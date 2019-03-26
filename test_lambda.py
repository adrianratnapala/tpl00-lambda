#!/usr/bin/env -S -i python3

import re
import os
import pytest
import subprocess
import sys
from collections import namedtuple

def use_valgrind():
        uvg=os.environ.get('USE_VALGRIND', 'no')
        return uvg.lower() in ('true', 'yes')

class Config:
        valgrind_command = ['valgrind', '--leak-check=yes', '-q'] if use_valgrind() else []
        command = valgrind_command + ['b/lambda']
        seconds_per_command=0.5

config = Config()

Result = namedtuple('Result', [
        'out',
        'err',
], defaults = (None, None))

class R(Result):
        fname_re="[a-zA-Z0-9_.]+"
        number_re="[0-9]+"

        def match_err(s, errre):
                if len(s.err) < 1:
                        return s
                m = re.match(errre, s.err[0])
                if not m: return s
                return R(out = s.out, err=m.groups())

        parse_re = re.compile("(%s):(%s): Syntax error: (.*)[.]" %\
                (fname_re, number_re))

        def parse_err(s):
                return next(s._parse_errs())

        def parse_errs(s):
                return list(s._parse_errs())

        def _parse_errs(s):
                if len(s.err) < 1:
                        return s
                for line in s.err:
                        m = s.parse_re.match(line)
                        if not m:
                                raise ValueError('"%s" !~ /%r/'
                                        % (s.err, s.parse_re))
                        yield R(err = (m[1], int(m[2]), m[3]))


class X(Result):
        @classmethod
        def err(cls, *args): return R(err=tuple(args))

        @classmethod
        def read(cls, xout): return X(out='%d %s\n' % (len(xout), xout))

        @classmethod
        def ok(cls, xout): return X(out='%s\n' % xout)

        @classmethod
        def lines(cls, *args): return X(out=list(args))


def args_from(arg_dict):
        args = []
        if arg_dict is None:
                return args
        for k, v in arg_dict.items():
                prefix = '--'
                if v == False:
                        prefix += 'no-'
                        v = None
                if v == True:
                        v = None
                a = prefix + k.replace('_', '-')
                if v is not None:
                        a = '%s=%s' % (a, v)
                args.append(a)
        return args

def stderr_lines(text):
        for line in text.split('\n'):
                line = line.rstrip()
                if not line: continue
                if line.startswith('DBG: '): continue
                yield line


def run_lambda(input, faults_to_inject=(), args=None):
        env = dict()
        cmd = config.command + args_from(args)
        if faults_to_inject:
                for fault in faults_to_inject:
                        assert ',' not in fault
                env['INJECTED_FAULTS']=','.join(faults_to_inject)
        try:
                cp = subprocess.run(
                        cmd,
                        env = env if env else None,
                        text=True,
                        input=input,
                        capture_output=True,
                        timeout=config.seconds_per_command)
                cp.check_returncode()
        except subprocess.CalledProcessError as x:
                print("CalledProcessError = ", x)
                print("==> LAMBDA stderr <<<===\n%s" % cp.stderr)
                print("==> LAMBDA input <<<===\n%s\n=========" % input)
                return R(err=list(stderr_lines(cp.stderr)), out=None)
        for line in (l.strip() for l in cp.stderr.split('\n')):
                assert not list(stderr_lines(cp.stderr))
        return R(out=cp.stdout)


TEST_SOURCE_READ=dict(test_source_read=True)

def FILENAME():
        return "STDIN"

def test_bad_command_line_arg():
        assert X.err() == run_lambda('', args=dict(
                I_am_a_very_bad_command_line_arg=True
        )).match_err('.*unrecognized option.*')

def test_empty():
        assert X.read('') == run_lambda('', args=TEST_SOURCE_READ)

def test_little():
        assert X.read('little') == run_lambda('little', args=TEST_SOURCE_READ)

def test_big_error():
        big_str='\n'.join('%08s' % k for k in range(1,1000))
        assert X.read(big_str) == run_lambda(big_str, args=TEST_SOURCE_READ)

def test_read_error():
        assert X.err() == run_lambda('bang! an EIO',
                faults_to_inject={'unreadable-bangs'}).match_err('Error reading.*')

def test_trivial_program():
        assert X.ok('x') == run_lambda('x')

def test_single_call():
        assert X.ok('(x y)') == run_lambda('x y')

def test_single_call_with_explicit_parens():
        assert X.ok('(x y)') == run_lambda('(x y)')

def test_auto_left_associated_call():
        assert X.ok('((x y) z)') == run_lambda('x y z')

def test_forced_right_associated_call():
        assert X.ok('((x y) z)') == run_lambda('x y z')

def MULTIBYTE_VAR_MSG(name):
        return "Multi-byte varnames aren't allowed.  '{}'".format(name)

def MULTIDIGIT_NUM_MSG(n):
        return "Multi-digit nums aren't allowed.  '{}'".format(n)

def UNMATCHED_MSG(thing):
        return"Unmatched '('".format(thing)

def EXPECTED_EXPR_MSG():
        return"Expected expr"

def EXPECTED_LAMBDA_BODY_MSG():
        return"Expected lambda body"

def test_parse_error_unmatched_paren():
        assert X.err(FILENAME(), 0, UNMATCHED_MSG('(')) == \
                run_lambda('(x').parse_err()

def test_parse_error_multi_byte_varname():
        assert X.err(FILENAME(), 0, MULTIBYTE_VAR_MSG('var')) == \
                run_lambda('var').parse_err()

def test_parse_error_expected_expr():
        assert X.err(FILENAME(), 0, EXPECTED_EXPR_MSG()) == \
                run_lambda('').parse_err()

def test_scan_after_expected_expr_error():
        assert run_lambda(')(').parse_errs() == [
                X.err(FILENAME(), 0, EXPECTED_EXPR_MSG()),
                X.err(FILENAME(), 1, UNMATCHED_MSG(')')),
        ]

def test_explicit_act_unparse():
        assert X.ok('x') == run_lambda('x', args={"unparse":True})

def test_bad_print_and_test_source_read():
        assert X.err() == run_lambda('x', args={
                "test_source_read":True,
                "unparse":True,
        }).match_err("--test-source-read means.*actions")

@pytest.fixture(params="xyz")
def xname(request):
        return request.param

@pytest.fixture(params=[0, 1, 5])
def padding(request):
        return request.param


from collections import namedtuple

TN = namedtuple('TN', ['N', 'T'])

def types(src):
        out, err = run_lambda(src, args={
                "type": True,
        })
        assert err == None
        lines = out.strip().split('\n')
        splits = (l.strip().split('=', 1) for l in lines)
        kv = [TN(*kv) if len(kv) == 2 else TN(kv[0], None) for kv in splits]

        found = {}
        for k, v in kv:
                print('run_type: ', k ,v)
                assert found.get(k, v) == v
                found[k] = v
        return kv

def run_type(src):
        # FIX: get rid of this.
        return dict(types(src))

def test_type_trivial_x(xname):
        xtype = xname.upper()
        assert dict(types(xname)) == {xtype: None}

def test_type_call():
        X, Y, Xr =  types("(x y)")
        assert X == ("X", "(Y Xr)")
        assert Y == ("Y", None)
        assert Xr == ("Xr", None)

def test_type_call_recursive():
        X, _, Xr = types("(x x)")
        assert X == ("X", "(X Xr)")
        assert Xr == ("Xr", None)

def test_deepish_type():
        A, B, Ar, C, Arr = types("((a b) c)")
        assert Arr == ('Arr', None)
        assert B == ('B', None)
        assert C == ('C', None)
        assert Ar == ('Ar', '(C Arr)')
        assert A == ('A', '(B Ar={})'.format(Ar.T))


def test_deepish_recursive_type():
        A, B, Ar, _A, Arr = types("((a b) a)")
        assert Arr == ('Arr', None)
        assert B == ('B', None)
        assert Ar == ('Ar', "(A=(B Ar) Arr)")

        assert A.N == 'A'
        assert A.T == "(B Ar={})".format(Ar.T.replace("=(B Ar)", ""))
        assert _A == A

def test_deeper_recursive_type():
        A, B, Ar, C, Arr, D, Arrr, _A, Arrrr = types("((((a b) c) d) a)")

        assert D == ("D", None)
        assert B == ("B", None)
        assert C == ("C", None)
        assert Arrrr == ("Arrrr", None)

        assert A.N == 'A'
        assert Ar.N == 'Ar'
        assert Arr.N == 'Arr'
        assert Arrr.N == 'Arrr'

        assert A.T == '(B Ar=(C Arr=(D Arrr=(A Arrrr))))'
        assert Ar.T == "(C Arr={})".format(Arr.T.replace('=(C Arr)', ''))
        assert Arr.T == "(D Arrr={})".format(Arrr.T.replace('=(D Arrr)', ''))
        assert Arrr.T == '(A={} Arrrr)'.format(A.T.replace('=(A Arrrr)', ''))

def test_unify_nonrecursive_functions_shallowly():
        # A and B don't have to be unified either.
        #                   0  1 234  5 678
        N,\
        A, X, Ar, Nr, \
        Y, A2, Yr, Nrr, \
        Y2, B, Yr2, Nrrr, \
        B2, X2, Br, Nrrrr = types("n (a x) (y a) (y b) (b x)")

        print("N=", N)

        assert B == A
        assert A2 == A
        assert B2 == B
        assert Y2 == Y
        assert Yr2 == Yr

        assert Ar == ("Ar", None)
        assert Yr == ("Yr", None)
        assert X == ("X", None)
        assert Nrrrr == ("Nrrrr", None)
        assert Yr == ("Yr", None)

        assert A == ("A", '(X Ar)')
        assert Y == ("Y", '(A={} Yr)'.format(A.T))
        assert Nrrr == ("Nrrr", '(Ar Nrrrr)')
        assert Nrr == ("Nrr", '(Yr Nrrr={})'.format(Nrrr.T))
        assert Nr == ("Nr", '(Yr Nrr={})'.format(Nrr.T))
        assert N == ("N", '(Ar Nr={})'.format(Nr.T))


def test_unify_nonrecursive_functions_deeply():
        a_and_b_are_functions = "n (a x) (b y)"
        a_and_b_are_equal = "(z a) (z b)"
        types = run_type(a_and_b_are_functions + " "  + a_and_b_are_equal)

        X = types['X']
        Z = types['Z']
        Zr = types['Zr']
        A = types['A']
        Ar = types['Ar']
        N = types['N']
        Nr = types['Nr']
        Nrr = types['Nrr']
        Nrrr = types['Nrrr']
        Nrrrr = types['Nrrrr']

        assert 'Y' not in types.keys()
        assert 'B' not in types.keys()
        assert 'Br' not in types.keys()

        assert  A == '(X Ar)'

        assert Ar == None
        assert Zr == None
        assert Nrrrr == None
        assert Nrrr == '(Zr Nrrrr)'
        assert Nrr == '(Zr Nrrr={})'.format(Nrrr)
        assert Nr == '(Ar Nrr={})'.format(Nrr)
        assert N == '(Ar Nr={})'.format(Nr)

def test_unify_out_of_order():
        a_before_b = "n (w a) (x b)"
        b_is_fun = "(b y)"
        a_equals_b = "(z a) (z b)"
        types = run_type(' '.join([a_before_b, b_is_fun, a_equals_b]))

        # `a` appears before `b` so the type gets the name `A`.
        assert 'B' not in types.keys()

        # But `A` only becomes a function by being unified with `B = (Y Br)`
        # and `Ar` never existed -- the return type is still `Br`.
        assert 'Ar' not in types.keys()
        A = types['A']
        assert A == '(Y Br)'

        W = types['W']
        X = types['X']
        Y = types['Y']
        Z = types['Z']
        Zr = types['Zr']
        Br = types['Br']

        assert Zr == None
        assert Br == None
        assert Y == None

        assert Z == '(A={} Zr)'.format(A)
        assert W == '(A={} Wr)'.format(A)
        assert X == '(A={} Xr)'.format(A)


def test_unify_recursive():
        types = run_type("n (x a) (x b) (a b)")

        assert 'B' not in types
        A = types['A']
        Ar = types['Ar']
        X = types['X']
        Xr = types['Xr']

        assert A == '(A Ar)'
        assert X == '(A=(A Ar) Xr)'

def test_unify_shallow_but_long_recursion():
        # "long" means that the recursion is indirect (so the recursion is
        # deep in the type stack).  However it is shallow in the sense that all
        # do is coerce one typvar to a function in order to close the loop.
        chain_of_types = "n (a b) (b c) (c d)"
        closed_chain = chain_of_types + " (d a)"
        types = run_type(closed_chain)

        A = types['A']
        B = types['B']
        C = types['C']
        D = types['D']
        Ar = types['Ar']
        Br = types['Br']
        Cr = types['Cr']
        Dr = types['Dr']

        assert A == '(B=(C=(D=(A Dr) Cr) Br) Ar)'
        assert B == '(C=(D=(A=(B Ar) Dr) Cr) Br)'
        assert C == '(D=(A=(B=(C Br) Ar) Dr) Cr)'
        assert D == '(A=(B=(C=(D Cr) Br) Ar) Dr)'

def test_out_of_order_after_prior_links():
        prog = "n (x a)"  # A is some type
        prog += " (b p)"  # B is a function type
        prog += " (c q)"  # C is too
        prog += " (x c)"  # A == C
        prog += " (x b)"  # B == A

        N,\
        X,  A,  Xr, Nr, \
        B,  P,  Br, Nrr, \
        C,  Q,  Cr, Nrrr, \
        X2, C2, Xr2, Nrrrr, \
        X3, B2, Xr3, Nrrrrr, \
                = types(prog)

        assert P == ('P', None)
        assert Q == P

        assert A == ('A', '(P Br)');
        assert B == A;
        assert C == A;

def debruijn(src):
        R = run_lambda(src)
        if R.err is not None:
                return R
        assert R == run_lambda(R.out)
        return R

def test_parse_lambda_const():
        assert X.ok('[]z') == debruijn('[x]z')

def test_parse_error_unterminated_lambda():
        assert X.err(FILENAME(), 0, "Lambda '[)' doesn't end in ']'")\
                == debruijn('[)z').parse_err()

def test_parse_error_eof_instead_of_lambda_body(padding):
        stem = '[]'
        err_loc = len(stem) # error location ignores padding
        assert X.err(FILENAME(), err_loc, EXPECTED_LAMBDA_BODY_MSG()) \
                == debruijn(stem + ' '*padding).parse_err()

def test_lambda_binds_tighter_than_call():
        xout = '([]z y)'
        unparse_only = dict(unparse=True)
        assert X.ok(xout) == run_lambda('[]z y', args=unparse_only)
        assert X.ok(xout) == run_lambda(xout,  args=unparse_only)

def test_type_lambda_const():
        Z, X, Xf = types('[x]z')
        assert Z == ('Z', None)
        assert X == ('X', None)
        assert Xf == ('Xf', '[X](X Z)')

def test_parse_lambda_eye():
        assert X.ok('[]1') == debruijn('[]1')

def test_parse_error_multi_digit_boundvar():
        assert X.err(FILENAME(), 2, MULTIDIGIT_NUM_MSG('21')) == \
                debruijn('[]21').parse_err()

def test_parse_error_zero_is_invalid_debrunin_index():
        assert X.err(FILENAME(), 2, "0 is an invalid debrujin index") == \
                debruijn('[]0').parse_err()

def test_type_with_numerical_boundvars():
        _1, At, Atf = types('[]1')
        assert _1 == ('1', None)
        assert At == ('@', None)
        assert Atf == ('@f', '[@](@ 1)')
