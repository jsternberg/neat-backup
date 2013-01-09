#!/usr/bin/env python
# encoding: utf-8

import ninja_syntax
import sys, os, subprocess
from os.path import *

def find_llvm_config():
    for path in os.environ['PATH'].split(':'):
        for ext in ['', '-3.1']:
            fullpath = join(path, 'llvm-config'+ext)
            if exists(fullpath):
                return fullpath

def call(*k):
    return subprocess.check_output(k).strip()

llvm_config = find_llvm_config()
cflags = '-std=c++11 ' + call(llvm_config, '--cflags')
ldflags = call(llvm_config, '--ldflags') + ' ' + \
    call(llvm_config, '--libs', 'core', 'object', 'scalaropts')

n = ninja_syntax.Writer(open('build.ninja', 'w'))
n.variable('builddir', 'build')
n.variable('cxx', 'clang++')
n.newline()

n.variable('cflags', cflags)
n.variable('ldflags', ldflags)
n.newline()

n.rule('cxx', command='$cxx -MMD -MF $out.d $cflags -c $in -o $out',
       description='cxx $in', depfile='$out.d')
n.newline()

n.rule('link', command='$cxx $in $ldflags -o $out',
       description='link $out')
n.newline()

n.rule('re2c', command='re2c -b -i --no-generation-date -o $out $in',
       description='re2c $out')
n.newline()

objs = []
def cxx(src):
    objs.extend(n.build('$builddir/%s.o' % src, 'cxx', 'src/%s.cc' % src))

n.build('src/lexer.cc', 're2c', 'src/lexer.in.cc')
for x in ['neatc', 'ast', 'lexer', 'parse', 'scope', 'util']:
    cxx(x)

n.build('neatc', 'link', objs)
n.default('neatc')
n.newline()

n.variable('configure_args', ' '.join(sys.argv[1:]))
n.rule('configure', command='%s %s $configure_args' % (sys.executable, sys.argv[0]),
       description='configure $configure_args',
       generator=True)
n.newline()

n.build('build.ninja', 'configure', implicit='configure.py')
n.newline()

print 'wrote build.ninja.'
