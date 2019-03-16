#define _GNU_SOURCE
#include <assert.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "lambda.h"
#include "untestable.h"

int act_type(FILE *oot, const Ast *ast)
{
        uint32_t size;
        const AstNode *postfix = ast_postfix(ast, &size);

        for (size_t k = 0; k < size; k++) {
                const AstNode *expr = postfix + size - 1;
                DIE_IF(expr->type != ANT_VAR, "Sorry, can only type VARs :(");
                fprintf(oot, "%c\n", expr->VAR.token + 'A');
        }
        fflush(oot);

        return 0;
}
