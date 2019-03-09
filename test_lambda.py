#!/usr/bin/env -S -i python3

import os
import subprocess
import sys
from collections import namedtuple

class Config:
        command = ['valgrind', '-q', 'b/lambda']
        seconds_per_command=0.5


config = Config()

Result = namedtuple('Result', [
        'out',
        'err',
], defaults = (None, None))

class R(Result): pass

class X(Result):
        @classmethod
        def err(cls, xerr=True): return R(err=xerr)

        @classmethod
        def read(cls, xout): return X(out='%d %s\n' % (len(xout), xout))

        @classmethod
        def ok(cls, xout): return X(out='%s\n' % xout)


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
                # FIX: do something more intelligent please.
                print("CalledProcessError = ", x)
                print("==> LAMBDA stderr <<<===\n%s\n=========" % cp.stderr)
                return R(err=True, out=None)
        assert cp.stderr == ''
        return R(out=cp.stdout)


TEST_SOURCE_READ=dict(test_source_read=True)

def test_bad_command_line_arg():
        assert X.err() == run_lambda('', args=dict(
                I_am_a_very_bad_command_line_arg=True
        ))

def test_empty():
        assert X.read('') == run_lambda('', args=TEST_SOURCE_READ)

def test_little():
        assert X.read('little') == run_lambda('little', args=TEST_SOURCE_READ)

def test_big_error():
        big_str='\n'.join('%08s' % k for k in range(1,1000))
        assert X.read(big_str) == run_lambda(big_str, args=TEST_SOURCE_READ)

def test_read_error():
        assert X.err() == run_lambda('bang! an EIO',
                faults_to_inject={'unreadable-bangs'})

def test_trivial_program():
        assert X.ok('(x)') == run_lambda('x')

