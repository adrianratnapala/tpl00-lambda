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
        const AstNode *root = postfix + size - 1;

        DIE_IF(root->type != ANT_VAR, "Sorry, can only type VARs :(");

        fprintf(oot, "%c\n", root->VAR.token + 'A');
        return 0;
}
