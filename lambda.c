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
static void unparse(FILE *oot, const AstNode *nodes, uint32_t idx)
{
        uint32_t val;
        AstNodeType node_t = ast_unpack(nodes, idx, &val);
        switch (node_t) {
        case ANT_VAR:
                fputc(val + 'a', oot);
                return;
        case ANT_CALL:
                fputc('(', oot);
                unparse(oot, nodes, val);
                fputc(' ', oot);
                unparse(oot, nodes, ast_arg_idx(nodes, idx));
                fputc(')', oot);
                return;
        }
        DIE_LCOV_EXCL_LINE("Unparsing found Ast node %u with bad type id %u",
                           idx, node_t);
}

// ------------------------------------------------------------------

int act_unparse(FILE *oot, const Ast *ast)
{
        uint32_t size;
        const AstNode *ast0 = ast_postfix(ast, &size);

        unparse(oot, ast0, size - 1);
        fputc('\n', oot);
        fflush(oot);
        return 0;
}
