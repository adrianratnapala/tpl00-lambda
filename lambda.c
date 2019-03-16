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

static void ast_call_unpack(const AstNode *call, const AstNode **f,
                            const AstNode **x)
{
        DIE_IF(call->type != ANT_CALL, "%s called, on non-call node.",
               __func__);
        const AstNode *arg = call - 1;
        *x = arg;
        *f = arg - call->CALL.arg_size;
}

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
        unparse(oot, ast, ast_root(ast));
        fputc('\n', oot);
        fflush(oot);
        return 0;
}
