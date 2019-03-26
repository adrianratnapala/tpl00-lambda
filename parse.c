#define _GNU_SOURCE
#include <assert.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "lambda.h"
#include "untestable.h"

typedef struct SyntaxError SyntaxError;
struct SyntaxError {
        SyntaxError *prev;
        char *zmsg;
};

struct Ast {
        const char *zname;
        const char *zsrc;
        SyntaxError *error;
        uint32_t zsrc_len;
        uint32_t nnodes_alloced;
        uint32_t nnodes;
        AstNode nodes[];
};

// ------------------------------------------------------------------

const AstNode *ast_postfix(const Ast *ast, uint32_t *size_ret)
{
        uint32_t nnodes = ast->nnodes;
        DIE_IF(!nnodes, "An empty AST is postfix.");
        *size_ret = nnodes;
        return ast->nodes;
}

static const AstNode *ast_root(const Ast *ast)
{
        uint32_t nnodes = ast->nnodes;
        DIE_IF(!nnodes, "Empty AST has no root");
        return ast->nodes + nnodes - 1;
}

static AstNode *ast_node_alloc(Ast *ast, size_t n)
{
        size_t u = ast->nnodes;
        size_t nu = u + n;
        DIE_IF(nu > ast->nnodes_alloced,
               "BUG: %s is using %lu Ast nodes, only %d are alloced",
               ast->zname, nu, ast->nnodes_alloced);

        ast->nnodes = nu;
        return ast->nodes + u;
}

static SyntaxError *add_syntax_error(Ast *ast, const char *zloc,
                                     const char *zfmt, ...)
{
        size_t n = zloc - ast->zsrc;
        DIE_IF(n > ast->zsrc_len, "Creating error at invalid source loc %ld",
               n);
        SyntaxError *e = realloc_or_die(HERE, 0, sizeof(SyntaxError));
        *e = (SyntaxError){.prev = ast->error};

        char *prefix = NULL, *suffix = NULL;

        int nprefix =
            asprintf(&prefix, "%s:%lu: Syntax error: ", ast->zname, n);
        DIE_IF(nprefix < 0 || !prefix, "Couldn't format syntax_error location");

        va_list va;
        va_start(va, zfmt);
        int nsuffix = vasprintf(&suffix, zfmt, va);
        va_end(va);
        DIE_IF(nsuffix < 0 || !suffix, "Couldn't format %s%s...", prefix, zfmt);

        size_t len = nprefix + nsuffix + 1;
        prefix = realloc_or_die(HERE, prefix, len + 1);
        memcpy(prefix + nprefix, suffix, nsuffix);
        prefix[len - 1] = '.';
        prefix[len] = 0;

        e->zmsg = prefix;
        return ast->error = e;
}

int report_syntax_errors(FILE *oot, Ast *ast)
{
        int n = 0;
        for (const SyntaxError *pe = ast->error; pe; pe = pe->prev, n++) {
                fputs(pe->zmsg, oot);
                fputc('\n', oot);
        }
        return n;
}

void delete_ast(Ast *ast)
{
        SyntaxError *e, *pe = ast->error;
        while ((e = pe)) {
                pe = e->prev;
                free(e->zmsg);
                free(e);
        }
        free(ast);
}

// ------------------------------------------------------------------

static const char *eat_white(const char *z0)
{
        for (;; z0++) {
                char ch = *z0;
                switch (ch) {
                case ' ':
                case '\t':
                case '\n':
                        continue;
                default:
                        return z0;
                }
        }
}

static uint8_t idx_from_letter(char c) { return (uint8_t)c - (uint8_t)'a'; }

static const char *lex_varname(Ast *ast, int32_t *idxptr, const char *z0)
{
        uint8_t idx = idx_from_letter(*z0);
        if (idx >= 26) {
                *idxptr = -1;
                return z0;
        }
        *idxptr = idx;

        const char *z = z0 + 1;
        if (idx_from_letter(*z) >= 26) {
                return z;
        }

        add_syntax_error(
            ast, z0, "Multi-byte varnames aren't allowed.  '%.*s...'", 10, z0);
        do
                z++;
        while (idx_from_letter(*z) < 26);
        return z;
}

static void push_varname(Ast *ast, int32_t token)
{
        DIE_IF(token + 'a' > 'z', "Bad token %u.", token);

        AstNode *pn = ast_node_alloc(ast, 1);
        *pn = (AstNode){
            .type = ANT_VAR,
            .VAR = {.token = token},
        };
}

static const char *parse_expr(Ast *ast, const char *z0);

static const char *parse_non_call_expr(Ast *ast, const char *z0)
{
        int32_t token;
        const char *zE = lex_varname(ast, &token, z0);
        if (token >= 0) {
                push_varname(ast, token);
                return zE;
        }

        switch (*z0) {
        case '(':
                zE = parse_expr(ast, z0 + 1);
                if (!zE || *zE != ')') {
                        add_syntax_error(ast, z0, "Unmatched '('");
                        return zE;
                }
                return zE + 1;
                // ... more cases here later ...
        }

        return NULL;
}

static const char *parse_expr(Ast *ast, const char *z0)
{
        z0 = eat_white(z0);

        const char *z = parse_non_call_expr(ast, z0);
        if (!z) {
                add_syntax_error(ast, z0, "Expected expr");
                return NULL;
        }

        for (;;) {
                const AstNode *func = ast_root(ast);
                z = eat_white(z);
                const char *z1 = parse_non_call_expr(ast, z);
                size_t arg_size = ast_root(ast) - func;
                if (!z1) {
                        return z;
                }
                DIE_IF(arg_size > INT32_MAX,
                       "Huge arg parsed %lu nodes, why no ENOMEM?", arg_size);
                z = z1;
                AstNode *call = ast_node_alloc(ast, 1);
                *call =
                    (AstNode){.type = ANT_CALL, .CALL = {.arg_size = arg_size}};
        }
}

Ast *parse(const char *zname, const char *zsrc)
{
        size_t n = strlen(zsrc) + 8;

        Ast *ast = realloc_or_die(HERE, 0, sizeof(Ast) + sizeof(AstNode) * n);
        *ast = (Ast){
            .zname = zname,
            .zsrc = zsrc,
            .zsrc_len = (int32_t)n,
            .nnodes_alloced = n,
        };
        for (int k = 0; k < n; k++) {
                ast->nodes[k] = (AstNode){0};
        }

        const char *zE = parse_expr(ast, zsrc);
        DIE_IF(zE && *zE, "Unused bytes after program source: '%.*s...'", 10,
               zE);

        return ast;
}
