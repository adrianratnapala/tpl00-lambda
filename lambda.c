#include <assert.h>
#include <string.h>

#include "lambda.h"

extern void interpret(FILE *oot, size_t src_len, const char *zsrc)
{
        assert(!zsrc[src_len]);
        assert(strlen(zsrc) == src_len);

        fputc('(', oot);
        fputs(zsrc, oot);
        fputc(')', oot);
        fputc('\n', oot);
        fflush(oot);
}
