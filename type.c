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
} TypeTree;

static uint32_t one_step_master(const Type *types, uint32_t idx)
{
        Type t = types[idx];
        if (t.delta < 0) {
                idx += t.delta;
        }
        assert(types[idx].delta >= 0);
        return idx;
}

static uint32_t masterise(Type *types, uint32_t idx)
{
        Type t = types[idx];
        if (t.delta >= 0)
                return idx;

        assert(t.delta < 0);
        uint32_t first = masterise(types, idx + t.delta);
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

static bool as_function(const Type *types, uint32_t idx, uint32_t *arg,
                        uint32_t *ret)
{
        Type t = types[idx];
        if (t.delta <= 0) {
                return false;
        }
        uint32_t iret = idx + t.delta;
        *arg = iret - 1;
        *ret = iret;
        return true;
}

static void unify(TypeTree *ttree, uint32_t ia, uint32_t ib)
{
        Type *types = ttree->types;

        ia = masterise(types, ia);
        ib = masterise(types, ib);
        if (ia == ib)
                return;

        uint32_t aarg, aret;
        uint32_t barg, bret;

        bool a_is_fun = as_function(types, ia, &aarg, &aret);
        bool b_is_fun = as_function(types, ib, &barg, &bret);

        if (!a_is_fun && b_is_fun) {
                set_function(types, ia, bret);
                set_prior(types, ib, ia);
                return;
        }
        set_prior(types, ib, ia);

        if (a_is_fun && b_is_fun) {
                unify(ttree, aarg, barg);
                unify(ttree, aret, bret);
        }
}

static void coerce_to_fun_type(TypeTree *ttree, uint32_t ifun, uint32_t icall)
{
        Type *types = ttree->types;
        assert(ifun < icall);

        uint32_t iarg = ast_arg_idx(ttree->exprs, icall);
        uint32_t iret = icall;
        // fputs("DBG: ", stderr);
        // print_typename(stderr, ttree->postfix, ifun);
        // fprintf(stderr, " <= new fun %d at from %d, %d\n", ifun, iarg, iret);

        ifun = masterise(types, ifun);
        uint32_t old_iarg, old_iret;
        if (as_function(types, ifun, &old_iarg, &old_iret)) {
                unify(ttree, old_iarg, iarg);
                unify(ttree, old_iret, icall);
                return;
        }

        set_function(types, ifun, iret);
}

static void bind_to_typevar(TypeTree *tree, uint32_t target, uint32_t tok)
{
        DIE_IF(tok > MAX_TOKS, "Overbig token %u", tok);
        Type *binding = tree->bindings[tok];
        if (binding) {
                set_prior(tree->types, target, binding - tree->types);
        } else {
                tree->bindings[tok] = tree->types + target;
        }
}

static void solve_types(TypeTree *ttree)
{
        const AstNode *exprs = ttree->exprs;
        uint32_t size = ttree->size;

        uint32_t val;
        for (int k = 0; k < size; k++)
                switch (ast_unpack(exprs, k, &val)) {
                case ANT_VAR:
                        bind_to_typevar(ttree, k, val);
                        continue;
                case ANT_CALL:
                        coerce_to_fun_type(ttree, val, k);
                        continue;
                }
}

static TypeTree *build_type_tree(const Ast *ast)
{
        uint32_t size;
        const AstNode *exprs = ast_postfix(ast, &size);
        TypeTree *tree =
            realloc_or_die(HERE, 0, sizeof(TypeTree) + sizeof(Type) * size);
        *tree = (TypeTree){.exprs = exprs, .size = size};
        for (uint32_t k = 0; k < size; k++) {
                tree->types[k] = (Type){0};
        }

        solve_types(tree);

        for (uint32_t k = 0; k < size; k++) {
                masterise(tree->types, k);
        }

        return tree;
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

typedef enum {
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
        uint32_t iarg, iret;
        if (!as_function(unp->types, idx, &iarg, &iret)) {
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

static void unparse_type(FILE *oot, const TypeTree *tree, const Type *t)
{
        Unparser unp = {
            .oot = oot,
            .exprs = tree->exprs,
            .types = tree->types,
        };

        unparse_type_(&unp, t - tree->types);
}

int act_type(FILE *oot, const Ast *ast)
{
        TypeTree *ttree = build_type_tree(ast);

        for (size_t k = 0; k < ttree->size; k++) {
                unparse_type(oot, ttree, ttree->types + k);
                fputc('\n', oot);
        }

        free(ttree);
        fflush(oot);
        return 0;
}
