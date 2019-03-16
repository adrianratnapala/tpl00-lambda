#ifndef LAMBDA_2018_03_07_H
#define LAMBDA_2018_03_07_H

#include <stdint.h>
#include <stdio.h>

#include "untestable.h"

// Tag-enum for the type of nodes in abstract syntax tree (AST).  ANT_XYZ
// corresponds to a field AstNode.XYZ of type AstXyz.
typedef enum
{
        ANT_VAR = 1,
        ANT_CALL,
} AstNodeType;

// FIX: rename to AstVar
// AstVar represents a named variable in the AST.
typedef struct {
        uint32_t token;
} AstVar;

// AstCall represents a call of a function.  This relies on post-fix ordering of
// Ast nodes:
//        AstNode *call = ...
//        assert(call->type == ANT_CALL)
//        AstNode *argument = call - 1;
//        AstNode *callee   = call - call->CALL.arg_size - 1;
//
typedef struct {
        uint32_t arg_size;
} AstCall;

// A node in the AST.
typedef struct {
        uint32_t type;

        union {
                AstCall CALL;
                AstVar VAR;
        };
} AstNode;

// Ast.  An opaque pointer to the result of parse().
typedef struct Ast Ast;

// Decodes an CALL AstNode into a function and argument pointer.
static inline void ast_call_unpack(const AstNode *call, const AstNode **f,
                                   const AstNode **x)
{
        DIE_IF(call->type != ANT_CALL, "%s called, on non-call node.",
               __func__);
        const AstNode *arg = call - 1;
        *x = arg;
        *f = arg - call->CALL.arg_size;
}

// --------------------------------------------------------------------------------------

// Parse nul-terminated source `zsrc` into an AST.  `zname` is the file-name
// for error messages and such.  malloc() failure while trying to allocate the
// AST will result in abort().  `parse()` will still succeed if there are
// syntax errors, but those errors will be recorded in the result and can be
// reported with report_syntax_errors.
Ast *parse(const char *zname, const char *zsrc);

// Return all the nodes as an array in post-fix order.  Ast retains ownership.
const AstNode *ast_postfix(const Ast *ast, uint32_t *size);

// Discard an Ast (including the stored error messages.)
void delete_ast(Ast *ast);

int report_syntax_errors(FILE *oot, Ast *ast);

// Print the lambda-program at zsrc, writing the result to `oot`.  The source
// is both counted and NUL terminated, i.e. `src_len == strlen(zsrc)`.  `zname`
// is a filename (used for error messages and such).  Returns the number of
// errors found.
extern int act_unparse(FILE *oot, const Ast *ast);

#endif // LAMBDA_2018_03_07_H
