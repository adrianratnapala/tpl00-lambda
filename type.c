#define _GNU_SOURCE
#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "lambda.h"
#include "untestable.h"

#define MAX_TOKS 26
#define MAX_DEPTH 16

typedef struct Type Type;
struct Type {
        Type *master_t;
        Type *arg_t, *ret_t;
};

typedef struct {
        const AstNode *postfix;
        uint32_t size;
        uint32_t pad;
        Type *bindings[MAX_TOKS];
        Type types[];
} TypeTree;

static Type *masterise(Type *t)
{
        Type *m = t->master_t;
        if (m == t) {
                return t;
        }
        return t->master_t = masterise(m);
}

static void coerce_to_fun_type(Type *fun_t, Type *arg_t, Type *ret_t)
{
        fun_t = masterise(fun_t);
        arg_t = masterise(arg_t);
        ret_t = masterise(ret_t);
        DIE_IF(fun_t->arg_t,
               "Unify() not implemented.  Cannot re-coerce function types.");
        fun_t->arg_t = arg_t;
        fun_t->ret_t = ret_t;
}

static void solve_types(TypeTree *ttree)
{
        const AstNode *exprs = ttree->postfix;
        Type *types = ttree->types;
        uint32_t size = ttree->size;

        const AstNode *f, *x;
        for (int k = 0; k < size; k++) {
                // FIX: make ast_call_unpack return a boolean for this sort of
                // test.
                if (exprs[k].type != ANT_CALL) {
                        DIE_IF(exprs[k].type != ANT_VAR,
                               "Not a VAR, not a CALL?");
                        unsigned tok = exprs[k].VAR.token;
                        DIE_IF(tok > MAX_TOKS, "Overbig token %u", tok);
                        if (ttree->bindings[tok]) {
                                types[k] = *ttree->bindings[tok];
                        } else {
                                ttree->bindings[tok] = types + k;
                        }
                        continue;
                }
                // FIX: here, it would be better if ast_call_unpack used ids.
                ast_call_unpack(exprs + k, &f, &x);
                uint32_t fidx = f - exprs;
                uint32_t xidx = x - exprs;
                coerce_to_fun_type(types + fidx, types + xidx, types + k);
        }
}

static TypeTree *build_type_tree(const Ast *ast)
{
        uint32_t size;
        const AstNode *postfix = ast_postfix(ast, &size);
        TypeTree *tree =
            realloc_or_die(HERE, 0, sizeof(TypeTree) + sizeof(Type) * size);
        *tree = (TypeTree){.postfix = postfix, .size = size};
        for (int k = 0; k < size; k++) {
                // FIX: merge this loop with the one in solve_tree
                Type *t = tree->types + k;
                *t = (Type){.master_t = t};
        }

        solve_types(tree);
        return tree;
}

// ------------------------------------------------------------------

typedef struct {
        FILE *oot;
        const AstNode *exprs; // FIX: pick a name, exprs or postfix
        Type *types; // FIX: make this const, let one-hop master be the rule.
        uint32_t depth;
        uint32_t ntypes;
        Type *stack[MAX_DEPTH];
} Unparser;

static bool unparse_push(Unparser *unp, Type *type)
{
        assert(type == type->master_t); // FIX: the names
        uint32_t depth = unp->depth, k = depth;
        while (k--)
                if (unp->stack[k] == type)
                        return false;
        unp->stack[depth] = type;
        unp->depth = depth + 1;
        return true;
}

static void unparse_pop(Unparser *unp)
{
        int depth = (int)unp->depth - 1;
        assert(depth >= 0);
        unp->depth = depth;
}

static void unparse_type_(Unparser *unp, Type *t)
{
        t = masterise(t);

        FILE *oot = unp->oot;
        int32_t idx = t - unp->types;

        const AstNode *expr = unp->exprs + idx;
        int k = 0;
        while (expr->type == ANT_CALL) {
                k++;
                const AstNode *arg_ignored;
                ast_call_unpack(expr, &expr, &arg_ignored);
        }

        fputc(expr->VAR.token + 'A', oot);
        while (k--) {
                fputc('r', oot);
        }

        Type ty = *t->master_t;
        if (!ty.arg_t) {
                // if it's not a function there is no structure to expand.
                return;
        }

        if (!unparse_push(unp, t)) {
                // Push failure means we have found recursion.
                return;
        }

        fputs("=(", oot);
        unparse_type_(unp, ty.arg_t);
        fputc(' ', oot);
        unparse_type_(unp, ty.ret_t);
        fputc(')', oot);
        unparse_pop(unp);
}

// FIX: make all the args const.
static void unparse_type(FILE *oot, TypeTree *tree, Type *t)
{
        Unparser unp = {
            .oot = oot,
            .exprs = tree->postfix, // FIX: pick a name, exprs or postfix.
            .types = tree->types,
        };

        unparse_type_(&unp, t);
}

int act_type(FILE *oot, const Ast *ast)
{
        TypeTree *ttree = build_type_tree(ast);
        for (size_t k = 0; k < ttree->size; k++) {
                unparse_type(oot, ttree, ttree->types + k);
                fputc('\n', oot);
        }

        free(ttree);
        fflush(oot);
        return 0;
}
