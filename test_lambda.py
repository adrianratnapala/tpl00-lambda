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
                if len(s.err) < 1:
                        return s
                m = s.parse_re.match(s.err[0] or '')
                if not m:
                        raise ValueError('"%s" !~ /%r/'
                                % (s.err, s.parse_re))
                return R(err = (m[1], int(m[2]), m[3]))


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
                print("==> LAMBDA stderr <<<===\n%s\n=========" % cp.stderr)
                elines = (line.strip() for line in  cp.stderr.split('\n'))
                elines = [line for line in elines if line]
                return R(err=elines, out=None)
        assert cp.stderr == ''
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

def test_parse_error_unmatched_paren():
        assert X.err(FILENAME(), 0, "Unmatched '('") == \
                run_lambda('(x').parse_err()

def test_parse_error_multi_byte_varname():
        assert X.err(FILENAME(), 0, "Multi-byte varnames aren't allowed.  'var...'") == \
                run_lambda('var').parse_err()

def test_parse_error_expected_expr():
        assert X.err(FILENAME(), 0, "Expected expr") == run_lambda('').parse_err()

def test_explicit_act_unparse():
        assert X.ok('x') == run_lambda('x', args={"unparse":True})

def test_bad_print_and_test_source_read():
        assert X.err() == run_lambda('x', args={
                "test_source_read":True,
                "unparse":True,
        }).match_err("--test-source-read means.*actions:")

@pytest.fixture(params="xyz")
def xname(request):
        return request.param

def run_type(src):
        out, err = run_lambda(src, args={
                "type": True,
        })
        assert err == None
        types = {}
        lines = out.strip().split('\n')
        for l in (l.strip().split('=', 1) for l in lines):
                k = l[0]
                v = None
                if len(l) > 1:
                        v = l[1]
                print('run_type: ', l, k ,v)
                assert types.get(k, v) == v
                types[k] = v
        return types

def test_type_trivial_x(xname):
        xtype = xname.upper()
        assert run_type(xname) == {xtype: None}

def test_type_call():
        assert run_type("(x y)") == dict(
                X = "(Y Xr)",
                Y = None,
                Xr = None,
        )

def test_type_call_recursive():
        assert run_type("(x x)") == dict(
                X = "(X Xr)",
                Xr = None,
        )

def test_deepish_type():
        types = run_type("((a b) c)")
        A = types['A']
        B = types['B']
        C = types['C']
        Ar = types['Ar']
        Arr = types['Arr']
        assert Arr == None
        assert B == None
        assert C == None
        assert Ar == '(C Arr)'
        assert A == '(B Ar={})'.format(Ar)


def test_deepish_recursive_type():
        types=run_type("((a b) a)")
        A = types['A']
        B = types['B']
        Ar = types['Ar']
        Arr = types['Arr']
        assert Arr == None
        assert B == None
        assert Ar == "(A=(B Ar) Arr)"
        assert A == "(B Ar={})".format(Ar.replace("=(B Ar)", ""))

def test_deeper_recursive_type():
        types =  run_type("((((a b) c) d) a)")
        A = types['A']
        B = types['B']
        C = types['C']
        D = types['D']
        Ar = types['Ar']
        Arr = types['Arr']
        Arrr = types['Arrr']
        Arrrr = types['Arrrr']

        assert D == None
        assert B == None
        assert C == None
        assert Arrrr == None

        assert A == "(B Ar=(C Arr=(D Arrr=(A Arrrr))))"
        assert Arrr == "(A={} Arrrr)".format(A.replace('=(A Arrrr)', ''))
        assert Arr == "(D Arrr={})".format(Arrr.replace('=(D Arrr)', ''))
        assert Ar == "(C Arr={})".format(Arr.replace('=(C Arr)', ''))

