Terrible Programming Language 1: The lambda-calculus
====================================================

Author     : Adrian Ratnapala

Copyright (C) 2019, Adrian Ratnapala, pusblished under the GNU Public License
version 2.  See file [LICENSE](LICENSE).


Part 0: Brack-cat
-----------------

Let's make a language that does almost nothing.  It just puts parentheses
around its input.  That means the source:

        x

Is the program that prints:

        (x)

To standard output.


The point of writing this is to set down some infrastructure.  Things like the
build system (`make`), the test framework (`py.test`) and coverage analysis
(`gcc` and `gcovr`).  None of that is boring; but none of it is the topic of
this README.

The code that is on topic is in `lambda.c`, which is very exciting:


        void interpret(FILE *oot, size_t src_len, const char *zsrc)
        {
                assert(!zsrc[src_len]);
                assert(strlen(zsrc) == src_len);

                fputc('(', oot);
                fputs(zsrc, oot);
                fputc(')', oot);
                fputc('\n', oot);
                fflush(oot);
        }
