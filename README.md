Terrible Programming Language 1: The lambda-calculus
====================================================

Author     : Adrian Ratnapala

Copyright (C) 2019, Adrian Ratnapala, pusblished under the GNU Public License
version 2.  See file [LICENSE](LICENSE).

To the interpreter build, just run make:

        make

To result interpreter:

        b/lambda < YOUR_SOURCE_CODE

To run the tests, you can do:

        TEST_MODE=full make clean all test

But this requires lots of dependencies, such as `clang-format`, `valgrind`,
`gcovr`, `py.test` and maybe other things I have forgotten.


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


Part 1: Left-associativity
--------------------------

Lets make the language implement left-associativity of function application.

What?

I mean if we have the program text:

        x y z

We get the output:

        ((x y) z)

I.e. `y` got paired with `x` (to the left) rather than `z` to the right.
We can override this default with explicit parentheses.  Thus input:

        x (y z)

yields the output:

        ((x y) z)


## Parsing

There might be a clever way to implement the above without parsing into an AST
and then printing out the AST, but we want an AST anyway so we might as well
write a parser.

It implements the following grammar:

        varname         ::= [a-z]
        non-call-expr   ::= varname | '(' expr ')'
        expr            ::= non-call-expr | expr non-call-expr

FIX: While the words in this grammar correspond to parser function names,
neither corresponds to AST node types.  Fix or explain.

