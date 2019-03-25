Terrible Programming Language 1: The lambda-calculus
====================================================

Author     : Adrian Ratnapala

Copyright (C) 2019, Adrian Ratnapala, published under the GNU Public License
version 2.  See file [LICENSE](LICENSE).

To build the interpreter, just run make:

        make

To run interpreter, do:

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

To step towards a real language, lets make it  implement left-associativity of
function application.

What?

We are trying to implement the [Lambda Calculus][wiki-lc], which is what [Alonzo
Church][wiki-ac] came up with when he showed how arbitrary computation could be
represented with nothing but function application.  The language contains
nothing but function-literals and variables (which are placeholders for
functions and their args).

[wiki-lc]: https://en.wikipedia.org/wiki/Lambda_calculus
[wiki-lc]: https://en.wikipedia.org/wiki/Alonzo_Church

Our first step is to drop even the functions, and have only variables which
behave like functions syntactically.  In particular stringing variables
together means applying them to each other as functions, left-associatively.  I
that mean if we have the program text:

        x y z

We get the output:

        ((x y) z)

This means: _"Call `x` with `y` as an argument.  The result is a function, call
it it with `z` as an argument."_ `y` got paired with `x` (to the left) rather
than `z` to the right.  We can override this default with explicit parentheses.
Thus input:

        x (y z)

yields the output:

        ((x y) z)


## Parsing

There might be a clever way to implement the above without parsing into an AST
and then printing out the AST; but we want an AST anyway so we might as well
write a parser.

Our parser implements the following grammar:

        varname         ::= [a-z]
        non-call-expr   ::= varname | '(' expr ')'
        expr            ::= non-call-expr | expr non-call-expr

The parser is a top-down parser in which each of the elements above corresponds
to a function with a name beginning with `parse_` or `lex_`.  These parse
whatever thing they are named after and return the result (into the AST for
`parse_*`, and into a caller-supplied location for `lex_*`).

So grammar rules and parser-internal functions map onto each other closely.  But
what about node types in the AST?  Each node is of the form:

        struct AstNode {
                uint32_t type;
                union {
                        AstCall CALL;
                        AstVar VAR;
                };
        };

So there are three data-types in all, `AstCall` and `AstVar` for the two types
of node, and `AstNode` itself which can be either.  These don't map very
obviously onto the syntax.

But lets consider the stricter grammar:

        varname ::= [a-z]
        call    ::= '(' expr expr ')'
        expr    ::= call | varname

Now we have a very clear mapping between data types and rules:

        varname -> AstVar
        call    -> AstCall
        expr    -> AstNode

Better still the stricter grammar, and the mapping are reflected in our code.
Not in the parser, but in the printer function `unparse`, which is:

        void unparse(FILE *oot, const Ast *ast, const AstNodeId root)
        {
                AstNode node = ast_node(ast, root);
                switch ((AstNodeType)node.type) {
                case ANT_VAR:
                        fputc(node.VAR.token + 'a', oot);
                        return;
                case ANT_CALL:
                        fputc('(', oot);
                        unparse(oot, ast, node.CALL.func);
                        fputc(' ', oot);
                        unparse(oot, ast, node.CALL.arg);
                        fputc(')', oot);
                        return;
                }
                DIE_LCOV_EXCL_LINE("unparsing found ast node with invalid type id %u",
                                   node.type);
        }

The two `case` clauses correspond to the `call` and `varname` rules, while the
function as a whole corresponds to the `expr` clause.

So set down the following tentative, informal, hypothesis:

### Hypothesis: Data-structures map closely to strict grammars.

For any given data structure we might like to serialise, there are strict
grammars whose rules map closely to type in the data structure.  Looser
grammars can be written for the same data structure, but they tend to be more
complicated because they need various catch-alls that don't represent
particular data types.

### The AST and its post-fixity.

How is the AST actually put together?  The `AstNode`s are stored contiguously
in memory along with some metadata.

        typedef struct {
                const char *zname;
                const char *zsrc;
                SyntaxError *error;
                uint32_t zsrc_len;
                uint32_t nnodes_alloced;
                uint32_t nnodes;
                AstNode nodes[];
        } Ast;

Also we have:

        typedef struct {
                uint32_t token;
        } AstVar;

Which means that variables are just represented by a number representing the
varname.  More interestingly we have:

        typedef struct {
                uint32_t arg_size;
        } AstCall;

What???

We are relying on a guarantee that nodes are stored in post-fix order.  That
means each sub-tree in the AST is stored contiguously and the root of every
tree is the last node in it.

Given a pointer to a CALL, we can calculate the pointer to the root of its
argument g by just subtracting 1.  But the full argument sub-tree has a
variable size, which we must store.  The root of the called function
is at

        pointer-to-called-function = pointer-to-CALL - arg_size - 1

## Typing

Even though we don't even do any computation yet, we can define a meaningful
type system.  Or language only has variables, but if one of them is used, we
know is a function of something, this is type information.  With the `--type`
argument, `lambda` will infer print out such types.  For example:

        >>$ b/lambda --type
        >>> x y
        X=(Y Xr)   # x is a function with arg type Y, returning Xr.
        Y          # y has type Y.
        Xr         # functions of type X return values of type Xr.


What we see here is that all types have names, derived from the first value
found of that type:

* If the value is a variable, `x` its type is just `X`.  This is a
  type-variable.
* If the value is a call of a function of type `F`, the return-type is `Fr`.

The type of a function  is called a "fun-type".  Fun-types have
more structure than type-variables, their full serialisation is:

        FuncName=(ArgType RetType)

Where if either of `ArgType` and `RetType` is fun-type it also have its own
`=(...)` expansion, unless it is a recursive invocation of the overall type.

Thus:

        >>$ b/lambda --type
        >>> x x
        X=(X Xr)   # x is a function with arg type X, returning Xr.
        X=(X Xr)   # The arg x, has the same type
        Xr         # functions of type X return values of type Xr.

### The type graph

We want to build a digraph of types.  There is a vertex for each expression in
the Ast.  Only the first vertex for a given type is a "real" type, all others
are references to it.  If a real type is a fun-type, then it has edges pointing
(perhaps indirectly) to its argument and return types.  Thus we have three
kinds of edge:

* A *prior link* means the source vertex is a reference to the destination
  (which might itself be a reference).

* A *arg link* is an edge from a fun-type to the type of its argument.

* A *ret link* is an edge from a fun-type to the type of its return value.

As ever, this structure is best captured by the serialisation code:


        static void unparse_type_(Unparser *unp, uint32_t idx)
        {
                idx = first_occurrence(unp->types, idx);
                print_typename(unp->oot, unp->exprs, idx);
                unparse_fun_expansion(unp, idx);
        }

        static void unparse_fun_expansion(Unparser *unp, uint32_t idx)
        {
                uint32_t iret;
                if (!as_fun_type(unp->types, idx, &iret)) {
                        return;
                }

                if (unparse_push(unp, idx) == RECURSION_FOUND) {
                        return;
                }

                uint32_t iarg = arg_from_ret(unp->types, iret);
                FILE *oot = unp->oot;
                fputs("=(", oot);
                unparse_type_(unp, iarg);
                fputc(' ', oot);
                unparse_type_(unp, iret);
                fputc(')', oot);
                unparse_pop(unp);
        }


So this is a depth-first traversal of the type graph.

* `first_occurrence` chases prior links.  (In fact assert()s that a single
  hop is enough.  The graph is post-processed to collapse longer chains).

* `as_fun_type` returns true if it handed a fun-type, in which case it
  chases the ret-link.

* `arg_from_ret` chases the arg-link using satanic invariants of the graph
  representation to derive the arg type from the ret type.

This is a depth-first traversal of the graph.  The required stack is really the
CPU-stack, `unparse_push` and `unparse_pop` are really there to remember which
nodes are between the root at the current point.

### How to type

The typing algorithm is straightforward but not trivial.  To build a graph of
types we scan the expression tree in postfix order assigning a new type to each
expression.  The types never really change, but occasionally we find that
previously distinct types are actually, they have to be unified.

The scan is just

                .
                . // allocate memory for the type graph
                .
                for (uint32_t k = 0; k < size; k++) {
                        types[k] = (Type){0};
                        infer_new_type(tg, k);
                }


This produces a valid type-graph.  But it is sub-optimal because there can be
chains of multiple prior-links.  So we collapse them with a utility function
that we need anyway:

                for (uint32_t k = 0; k < size; k++) {
                        relink_to_first(types, k);
                }



The type inference is more interesting, but it begins with a dumb-old switch:

        static void infer_new_type(TypeGraph *tg, uint32_t idx)
        {
                uint32_t val;
                AstNodeType tag = ast_unpack(tg->exprs, idx, &val);
                switch (tag) {
                case ANT_VAR:
                        bind_to_typevar(tg, idx, val);
                        return;
                case ANT_CALL:
                        coerce_callee(tg->types, val, idx);
                        return;
                }
                DIE_LCOV_EXCL_LINE("Typing found expr %u with bad tag %d", idx, tag);
        }

`bind_to_typevar` is straightforward.  The `ANT_CALL` case is trickier in part
because the type-graph and the Ast have different shapes.  In the Ast, a
function call is:

            ret-val
           /       \
          /         \
         V           V
      fun-val     arg-val


But the corresponding types link as:


        ret-type
           ^
          /
         /
      fun-type ----> arg-type


So we `(val, idx)` in `infer_new_type` becomes `(ifun, iret)` in
`coerce_to_callee`.  This means `iret` is the new type (implicitly a
type-variable) while `ifun` is a previously discovered type which can be a
type-var or a fun-type.

Thus `coerce_to_callee` is:

        static void coerce_callee(Type *types, uint32_t ifun, uint32_t iret)
        {
                assert(ifun < iret);

                ifun = relink_to_first(types, ifun);
                uint32_t old_iret;
                if (!as_fun_type(types, ifun, &old_iret)) {
                        replace_with_fun_returning(types, ifun, iret);
                        return;
                }

                uint32_t old_iarg = arg_from_ret(types, old_iret);
                uint32_t iarg = arg_from_ret(types, iret);
                unify(types, old_iarg, iarg);
                unify(types, old_iret, iret);
        }


If `ifun` was simply a type-var, all we do is expand it out using the newly
discovered `iret` (remember the arg-type can be inferred from this).

If `ifun` was already a function, it does not change.  However we have now
discovered that the newly discovered ret- and arg- are references to old ones.

`unify()` is the thing that *recursively* replaces one type with prior-links to
another one.
