#ifndef LAMBDA_2018_03_07_H
#define LAMBDA_2018_03_07_H

#include <stdint.h>
#include <stdio.h>

// Tag-enum for the type of nodes in abstract syntax tree (AST).  ANT_XYZ
// corresponds to a field AstNode.XYZ of type AstXyz.
typedef enum
{
        ANT_FREE = 1,
        ANT_CALL,
} AstNodeType;

// FIX: rename to AstVar
// AstFree represents a named variable in the AST.
typedef struct {
        uint32_t token;
} AstFree;

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
                AstFree FREE;
        };
} AstNode;

// Ast.  An opaque pointer to the result of parse().
typedef struct Ast Ast;

// Parse nul-terminated source `zsrc` into an AST.  `zname` is the file-name
// for error messages and such.  malloc() failure while trying to allocate the
// AST will result in abort().  `parse()` will still succeed if there are
// syntax errors, but those errors will be recorded in the result and can be
// reported with report_syntax_errors.
Ast *parse(const char *zname, const char *zsrc);

// The root node of the Ast.
const AstNode *ast_root(Ast *ast);

// Discard an Ast (including the stored error messages.)
void delete_ast(Ast *ast);

int report_syntax_errors(FILE *oot, Ast *ast);

// Execute the lambda-program at zsrc, writing the result to `oot`.  The source
// is both counted and NUL terminated, i.e. `src_len == strlen(zsrc)`.  `zname`
// is a filename (used for error messages and such).  Returns the number of
// errors found during parsing and/or running.
extern int interpret(FILE *oot, const char *zname, size_t src_len,
                     const char *zsrc);

#endif // LAMBDA_2018_03_07_H
