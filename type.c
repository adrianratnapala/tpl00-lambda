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

typedef struct {
        const AstNode *exprs;
        uint32_t size;
        uint32_t pad;
        Type *bindings[MAX_TOKS];
        Type types[];
} TypeGraph;

static uint32_t one_step_master(const Type *types, uint32_t idx)
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

static void set_prior(Type *types, uint32_t target, int32_t prior)
{
        // FIX? so should we get rid of he unused arg.
        assert(prior < target);
        types[target] = (Type){.delta = prior - target};
}

static void print_typename(FILE *oot, const AstNode *exprs, int32_t idx)
{
        int k = 0;
        uint32_t val = idx;
        while (ANT_CALL == ast_unpack(exprs, val, &val)) {
                k++;
        }

        fputc(val + 'A', oot);
        while (k--) {
                fputc('r', oot);
        }
}

static void set_function(Type *types, uint32_t ifun, uint32_t iret)
{
        types[ifun].delta = iret - ifun;
}

static uint32_t arg_from_ret(const Type *types, uint32_t ret)
{
        return ret - 1;
}

static bool as_function(const Type *types, uint32_t idx, uint32_t *ret)
{
        Type t = types[idx];
        if (t.delta <= 0) {
                return false;
        }
        uint32_t iret = idx + t.delta;
        *ret = iret;
        return true;
}

static void unify(Type *types, uint32_t ia, uint32_t ib)
{
        ia = relink_to_first(types, ia);
        ib = relink_to_first(types, ib);
        if (ia == ib)
                return;

        uint32_t aret, bret;
        bool a_is_fun = as_function(types, ia, &aret);
        bool b_is_fun = as_function(types, ib, &bret);

        if (!a_is_fun && b_is_fun) {
                // Copy the contents of B into A, so that we can discard B.
                set_function(types, ia, bret);
        }
        set_prior(types, ib, ia);

        if (a_is_fun && b_is_fun) {
                uint32_t aarg = arg_from_ret(types, aret);
                uint32_t barg = arg_from_ret(types, bret);
                unify(types, aarg, barg);
                unify(types, aret, bret);
        }
}

static void coerce_callee(Type *types, uint32_t ifun, uint32_t iret)
{
        assert(ifun < iret);

        ifun = relink_to_first(types, ifun);
        uint32_t old_iret;
        if (as_function(types, ifun, &old_iret)) {
                uint32_t old_iarg = arg_from_ret(types, old_iret);
                unify(types, old_iarg, iret - 1);
                unify(types, old_iret, iret);
                return;
        }

        set_function(types, ifun, iret);
}

static void bind_to_typevar(TypeGraph *tg, uint32_t target, uint32_t tok)
{
        DIE_IF(tok > MAX_TOKS, "Overbig token %u", tok);
        Type *binding = tg->bindings[tok];
        if (binding) {
                set_prior(tg->types, target, binding - tg->types);
        } else {
                tg->bindings[tok] = tg->types + target;
        }
}

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

static void unparse_function_expansion(Unparser *unp, uint32_t idx);

static void unparse_type_(Unparser *unp, uint32_t idx)
{
        idx = one_step_master(unp->types, idx);
        print_typename(unp->oot, unp->exprs, idx);
        unparse_function_expansion(unp, idx);
}

static void unparse_function_expansion(Unparser *unp, uint32_t idx)
{
        uint32_t iret;
        if (!as_function(unp->types, idx, &iret)) {
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
