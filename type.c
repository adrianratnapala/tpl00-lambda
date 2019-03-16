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
        fprintf(oot, "X\n");
        return 0;
}
