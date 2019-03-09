#ifndef LAMBDA_2018_03_07_H
#define LAMBDA_2018_03_07_H

#include <stdio.h>

// Execute the lambda-program at zsrc, writing the result to `oot`.  The source
// is both counted and NUL terminated, i.e. `src_len == strlen(zsrc)`.
extern void interpret(FILE *oot, size_t src_len, const char *zsrc);

#endif // LAMBDA_2018_03_07_H
