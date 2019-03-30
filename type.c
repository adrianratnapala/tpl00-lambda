#define _GNU_SOURCE
#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "lambda.h"
#include "untestable.h"

#define MAX_TOKS 26
#define MAX_DEPTH 16

typedef struct Type Type;
struct Type {
        int32_t delta;
};

static void print_typename(FILE *oot, const AstNode *exprs, int32_t idx)
{
        int k = 0;
        int32_t val = idx;
        while (ANT_CALL == ast_unpack(exprs, val, &val)) {
                k++;
        }

        fputc(val + 'A', oot);
        while (k--) {
                fputc('r', oot);
        }
}

// -----------------------------------------------------------------------------

typedef struct {
        const AstNode *exprs;
        uint32_t size;
        uint32_t pad;
        Type *bindings[MAX_TOKS];
        Type types[];
} TypeGraph;

static uint32_t first_occurrence(const Type *types, uint32_t idx)
{
        Type t = types[idx];
        if (t.delta < 0) {
                idx += t.delta;
        }
        assert(types[idx].delta >= 0);
        return idx;
}

static uint32_t relink_to_first(Type *types, uint32_t idx)
{
        Type t = types[idx];
        if (t.delta >= 0)
                return idx;

        assert(t.delta < 0);
        uint32_t first = relink_to_first(types, idx + t.delta);
        types[idx].delta = first - idx;
        assert(types[idx].delta < 0);

        return first;
}

static void replace_with_prior_link(Type *types, uint32_t idx, int32_t prior)
{
        assert(prior < idx);
        types[idx] = (Type){.delta = prior - idx};
}

static void replace_with_fun(Type *types, uint32_t ifun, uint32_t iarg,
                             uint32_t iret)
{
        assert(iarg + 1 == iret); // FIX: remove this constraint.
        types[ifun].delta = iret - ifun;
}

static bool as_fun_type(const Type *types, uint32_t idx, uint32_t *arg,
                        uint32_t *ret)
{
        Type t = types[idx];
        if (t.delta <= 0) {
                return false;
        }
        uint32_t iret = idx + t.delta;
        *ret = iret;
        *arg = iret - 1;
        return true;
}

static void unify(Type *types, uint32_t ia, uint32_t ib);

static void replace_subgraph_with_links(Type *types, uint32_t dest,
                                        uint32_t repl)
{
        uint32_t dest_ret, repl_ret;
        uint32_t dest_arg, repl_arg;
        bool dest_is_fun = as_fun_type(types, dest, &dest_arg, &dest_ret);
        bool repl_is_fun = as_fun_type(types, repl, &repl_arg, &repl_ret);

        if (!repl_is_fun && dest_is_fun) {
                replace_with_fun(types, repl, dest_arg, dest_ret);
        }

        replace_with_prior_link(types, dest, repl);
        if (repl_is_fun && dest_is_fun) {
                unify(types, repl_arg, dest_arg);
                unify(types, repl_ret, dest_ret);
        }
}

static void unify(Type *types, uint32_t ia, uint32_t ib)
{
        ia = relink_to_first(types, ia);
        ib = relink_to_first(types, ib);
        if (ia < ib)
                return replace_subgraph_with_links(types, ib, ia);
        if (ib < ia)
                return replace_subgraph_with_links(types, ia, ib);
}

static void coerce_callee(Type *types, uint32_t ifun, uint32_t iret)
{
        uint32_t iarg = iret - 1;
        assert(ifun < iret);

        ifun = relink_to_first(types, ifun);
        uint32_t old_iret, old_iarg;
        if (!as_fun_type(types, ifun, &old_iarg, &old_iret)) {
                replace_with_fun(types, ifun, iarg, iret);
                return;
        }

        unify(types, old_iarg, iarg);
        unify(types, old_iret, iret);
}

static void bind_to_typevar(TypeGraph *tg, uint32_t target, uint32_t tok)
{
        DIE_IF(tok > MAX_TOKS, "Overbig token %u", tok);
        Type *binding = tg->bindings[tok];
        if (binding) {
                replace_with_prior_link(tg->types, target, binding - tg->types);
        } else {
                tg->bindings[tok] = tg->types + target;
        }
}

static void infer_new_type(TypeGraph *tg, uint32_t idx)
{
        int32_t val;
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

static TypeGraph *build_type_graph(const Ast *ast)
{
        uint32_t size;
        const AstNode *exprs = ast_postfix(ast, &size);
        TypeGraph *tg =
            realloc_or_die(HERE, 0, sizeof(TypeGraph) + sizeof(Type) * size);
        *tg = (TypeGraph){.exprs = exprs, .size = size};

        Type *types = tg->types;
        for (uint32_t k = 0; k < size; k++) {
                types[k] = (Type){0};
                infer_new_type(tg, k);
        }

        for (uint32_t k = 0; k < size; k++) {
                relink_to_first(types, k);
        }

        return tg;
}

// ------------------------------------------------------------------

typedef struct {
        FILE *oot;
        const AstNode *exprs;
        const Type *types;
        uint32_t depth;
        uint32_t ntypes;
        uint32_t stack[MAX_DEPTH];
} Unparser;

typedef enum
{
        RECURSION_NOT_FOUND,
        RECURSION_FOUND,
} RecursionFound;

static RecursionFound unparse_push(Unparser *unp, uint32_t idx)
{
        uint32_t depth = unp->depth, k = depth;
        while (k--)
                if (unp->stack[k] == idx)
                        return RECURSION_FOUND;
        unp->stack[depth] = idx;
        unp->depth = depth + 1;
        return RECURSION_NOT_FOUND;
}

static void unparse_pop(Unparser *unp)
{
        int depth = (int)unp->depth - 1;
        assert(depth >= 0);
        unp->depth = depth;
}

static void unparse_fun_expansion(Unparser *unp, uint32_t idx);

static void unparse_type_(Unparser *unp, uint32_t idx)
{
        idx = first_occurrence(unp->types, idx);
        print_typename(unp->oot, unp->exprs, idx);
        unparse_fun_expansion(unp, idx);
}

static void unparse_fun_expansion(Unparser *unp, uint32_t idx)
{
        uint32_t iret, iarg;
        if (!as_fun_type(unp->types, idx, &iarg, &iret)) {
                return;
        }

        if (unparse_push(unp, idx) == RECURSION_FOUND) {
                return;
        }

        FILE *oot = unp->oot;
        fputs("=(", oot);
        unparse_type_(unp, iarg);
        fputc(' ', oot);
        unparse_type_(unp, iret);
        fputc(')', oot);
        unparse_pop(unp);
}

static void unparse_type(FILE *oot, const TypeGraph *tg, const Type *t)
{
        Unparser unp = {
            .oot = oot,
            .exprs = tg->exprs,
            .types = tg->types,
        };

        unparse_type_(&unp, t - tg->types);
}

int act_type(FILE *oot, const Ast *ast)
{
        TypeGraph *tg = build_type_graph(ast);

        for (size_t k = 0; k < tg->size; k++) {
                unparse_type(oot, tg, tg->types + k);
                fputc('\n', oot);
        }

        free(tg);
        fflush(oot);
        return 0;
}
