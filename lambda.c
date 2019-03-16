#define _GNU_SOURCE
#include <assert.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "lambda.h"
#include "untestable.h"

// ------------------------------------------------------------------
static void unparse(FILE *oot, const Ast *ast, const AstNode *root)
{
        const AstNode *f, *x;
        AstNode node = *root;
        switch ((AstNodeType)node.type) {
        case ANT_VAR:
                fputc(node.VAR.token + 'a', oot);
                return;
        case ANT_CALL:
                ast_call_unpack(root, &f, &x);
                fputc('(', oot);
                unparse(oot, ast, f);
                fputc(' ', oot);
                unparse(oot, ast, x);
                fputc(')', oot);
                return;
        }
        DIE_LCOV_EXCL_LINE("unparsing found ast node with invalid type id %u",
                           node.type);
}

// ------------------------------------------------------------------

int act_unparse(FILE *oot, const Ast *ast)
{
        uint32_t size;
        const AstNode *ast0 = ast_postfix(ast, &size);

        unparse(oot, ast, ast0 + size - 1);
        fputc('\n', oot);
        fflush(oot);
        return 0;
}
