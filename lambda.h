#ifndef LAMBDA_2018_03_07_H
#define LAMBDA_2018_03_07_H

#include <stdint.h>
#include <stdio.h>

typedef struct {
        uint32_t arg_size;
} AstCall;

typedef struct {
        uint32_t token;
} AstFree;

typedef enum
{
        ANT_FREE = 1,
        ANT_CALL,
} AstNodeType;

typedef struct {
        uint32_t type;

        union {
                AstCall CALL;
                AstFree FREE;
        };
} AstNode;

typedef struct SyntaxError SyntaxError;

typedef struct Ast Ast;

Ast *parse(const char *zname, const char *zsrc);
int report_syntax_errors(FILE *oot, Ast *ast);
const AstNode *ast_root(Ast *ast);
void delete_ast(Ast *ast);

// Execute the lambda-program at zsrc, writing the result to `oot`.  The source
// is both counted and NUL terminated, i.e. `src_len == strlen(zsrc)`.  `zname`
// is a filename (used for error messages and such).  Returns the number of
// errors found during parsing and/or running.
extern int interpret(FILE *oot, const char *zname, size_t src_len,
                     const char *zsrc);

#endif // LAMBDA_2018_03_07_H
