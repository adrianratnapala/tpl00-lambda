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

        return tree;
}

// ------------------------------------------------------------------
static void unparse_type(FILE *oot, const TypeTree *ttree, const Type *t)
{
        Type ty = *t->master_t;

        if (ty.arg_t) {
                // it has an arg_t therefore it is a function
                fputc('(', oot);
                unparse_type(oot, ttree, ty.ret_t);
                fputc(' ', oot);
                unparse_type(oot, ttree, ty.arg_t);
                fputc(')', oot);
                return;
        }

        // It's a typevar, but to name it correctly we must look at the AST.
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
}

int act_type(FILE *oot, const Ast *ast)
{
        /*
        uint32_t size;
        const AstNode *postfix = ast_postfix(ast, &size);

        for (size_t k = 0; k < size; k++) {
                const AstNode *expr = postfix + size - 1;
                DIE_IF(expr->type != ANT_VAR, "Sorry, can only type VARs :(");
                fprintf(oot, "%c\n", expr->VAR.token + 'A');
        }
        */

        TypeTree *ttree = build_type_tree(ast);
        for (size_t k = 0; k < ttree->size; k++) {
                unparse_type(oot, ttree, ttree->types + k);
                fputc('\n', oot);
        }

        free(ttree);

        fflush(oot);

        return 0;
}
