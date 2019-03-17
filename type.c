#define _GNU_SOURCE
#include <assert.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "lambda.h"
#include "untestable.h"

typedef struct Type Type;
struct Type {
        Type *master_t;
        Type *arg_t, *ret_t;
};

typedef struct {
        const AstNode *postfix;
        uint32_t size;
        uint32_t pad;
        Type types[];
} TypeTree;

static Type *masterise(Type *t)
{
        Type *m = t->master_t;
        if(m == t) {
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
                if (exprs[k].type != ANT_CALL)
                        continue;
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
                Type *t = tree->types + k;
                *t = (Type){.master_t = t};
        }

        solve_types(tree);
        return tree;
}

// ------------------------------------------------------------------
static void unparse_type(FILE *oot, const TypeTree *ttree, const Type *t)
{
        Type ty = *t->master_t;

        size_t idx = t->master_t - ttree->types;
        const AstNode *expr = ttree->postfix + idx;
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

        if (ty.arg_t) {
                // it has an arg_t therefore it is a function
                fputs("=(", oot);
                unparse_type(oot, ttree, ty.arg_t);
                fputc(' ', oot);
                unparse_type(oot, ttree, ty.ret_t);
                fputc(')', oot);
                return;
        }
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
