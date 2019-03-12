#define _GNU_SOURCE
#include <assert.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "lambda.h"
#include "untestable.h"

#define MAX_AST_NODES 10000

typedef struct AstNode AstNode;

typedef struct AstNodeId {
        int32_t n;
} AstNodeId;

typedef struct {
        AstNodeId func;
        AstNodeId arg;
} AstCall;

typedef struct {
        uint32_t token;
} AstFree;

typedef enum
{
        ANT_FREE = 1,
        ANT_CALL,
} AstNodeType;

struct AstNode {
        uint16_t type;
        uint16_t pad2_3;
        uint32_t pad5_8;

        union {
                AstCall CALL;
                AstFree FREE;
        };
};

static_assert(offsetof(AstNode, CALL) == sizeof(void *),
              "AstNode union is in an unexpected place.");

typedef struct SyntaxError SyntaxError;
struct SyntaxError {
        SyntaxError *prev;

        char *zmsg;
};

typedef struct {
        const char *zname;
        const char *zsrc;
        SyntaxError *error;
        uint32_t zsrc_len;
        AstNodeId root;
        uint32_t nnodes_alloced;
        uint32_t nnodes;
        AstNode nodes[];
} Ast;

// ------------------------------------------------------------------

static AstNodeId ast_node_id_from_ptr(const Ast *ast, AstNode *p)
{
        size_t n = p - ast->nodes;
        DIE_IF(n >= ast->nnodes, "Can't get ID for out-of bounds noode at %ld",
               n);

        return (AstNodeId){n};
}

static AstNode *ast_node_at(Ast *ast, AstNodeId id)
{
        DIE_IF(id.n >= ast->nnodes, "Out-of-bounds node id %ul", id.n);
        return ast->nodes + id.n;
}

static AstNode ast_node(const Ast *ast, AstNodeId id)
{
        return *ast_node_at((Ast *)ast, id);
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

static int report_syntax_errors(FILE *oot, Ast *ast)
{
        int n = 0;
        for (const SyntaxError *pe = ast->error; pe; pe = pe->prev, n++) {
                fputs(pe->zmsg, oot);
                fputc('\n', oot);
        }
        return n;
}

static void delete_ast(Ast *ast)
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

static const char *lex_varname(Ast *ast, uint32_t *idxptr, const char *z0)
{
        uint8_t idx = idx_from_letter(*z0);
        if (idx >= 26) {
                return NULL;
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

static const char *parse_expr(Ast *ast, const char *z0);

static const char *parse_non_call_expr(Ast *ast, const char *z0)
{
        uint32_t token;
        const char *zE = lex_varname(ast, &token, z0);
        if (zE) {
                DIE_IF(token + 'a' > 'z', "Bad token %u.", token);

                AstNode *pn = ast_node_alloc(ast, 1);
                *pn = (AstNode){
                    .type = ANT_FREE,
                    .FREE = {.token = token},
                };

                ast->root = ast_node_id_from_ptr(ast, pn);
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
                return z0;
        }

        for (;;) {
                AstNodeId func = ast->root;

                z = eat_white(z);
                const char *z1 = parse_non_call_expr(ast, z);
                if (!z1) {
                        return z;
                }
                z = z1;
                AstNode *call = ast_node_alloc(ast, 1);
                *call = (AstNode){.type = ANT_CALL,
                                  .CALL = {
                                      .func = func,
                                      .arg = ast->root,
                                  }};
                ast->root = ast_node_id_from_ptr(ast, call);
        }
}

static Ast *parse(const char *zname, const char *zsrc)
{
        size_t n = strlen(zsrc) * 20;
        // FIX: dynamically reallocate them
        if (n > MAX_AST_NODES) {
                die(HERE,
                    "Source of %s is too long.\n"
                    "  It has has %lu bytes.\n"
                    "  Max allowed is %u.",
                    zname, n, MAX_AST_NODES);
        }

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
        DIE_IF(*zE, "Unused bytes after program source: '%.*s...'", 10, zE);

        return ast;
}

// ------------------------------------------------------------------

void unparse(FILE *oot, const Ast *ast, const AstNodeId root)
{
        AstNode node = ast_node(ast, root);
        switch ((AstNodeType)node.type) {
        case ANT_FREE:
                fputc(node.FREE.token + 'a', oot);
                return;
        case ANT_CALL:
                fputc('(', oot);
                unparse(oot, ast, node.CALL.func);
                fputc(' ', oot);
                unparse(oot, ast, node.CALL.arg);
                fputc(')', oot);
                return;
        }
        DIE_LCOV_EXCL_LINE("unparsing found ast node with invalid type id %u",
                           node.type);
}

// ------------------------------------------------------------------

extern void interpret(FILE *oot, size_t src_len, const char *zsrc)
{
        assert(!zsrc[src_len]);
        assert(strlen(zsrc) == src_len);

        Ast *ast = parse("FIX", zsrc);
        int nerr = report_syntax_errors(stderr, ast);
        if (!nerr) {
                unparse(oot, ast, ast->root);
                fputc('\n', oot);
        }
        fflush(oot);
        delete_ast(ast);

        // FIX: return the error to main.
        if (nerr) {
                exit(1);
        }
}
