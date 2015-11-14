#
# copyright (c) 2009  scott stafford
#
# permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "software"), to deal in the software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the software, and to
# permit persons to whom the software is furnished to do so, subject to
# the following conditions:
#
# the above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the software.
#
# the software is provided "as is", without warranty of any
# kind, express or implied, including but not limited to the
# warranties of merchantability, fitness for a particular purpose and
# noninfringement. in no event shall the authors or copyright holders be
# liable for any claim, damages or other liability, whether in an action
# of contract, tort or otherwise, arising from, out of or in connection
# with the software or the use or other dealings in the software.
#

#  author : scott stafford
#  date : 2009-12-09 20:36:14
#
#  changes : vinnie falco <vinnie.falco@gmail.com>
#  date : 2014--4-25

""" 
protoc.py: protoc builder for scons

this builder invokes protoc to generate c++ and python  from a .proto file.  

note: java is not currently supported.
"""

__author__ = "scott stafford"

import scons.action
import scons.builder
import scons.defaults
import scons.node.fs
import scons.util

from scons.script import file, dir

import os.path

protocs = 'protoc'

protocaction = scons.action.action('$protoccom', '$protoccomstr')

def protocemitter(target, source, env):
    protocoutdir = env['protocoutdir']
    protocpythonoutdir = env['protocpythonoutdir']
    for source_path in [str(x) for x in source]:
        base = os.path.splitext(os.path.basename(source_path))[0]
        if protocoutdir:
            target.extend([os.path.join(protocoutdir, base + '.pb.cc'),
                           os.path.join(protocoutdir, base + '.pb.h')])
        if protocpythonoutdir:
            target.append(os.path.join(protocpythonoutdir, base + '_pb2.py'))

    try:
        target.append(env['protocfdsout'])
    except keyerror:
        pass

    #print "protoc source:", [str(s) for s in source]
    #print "protoc target:", [str(s) for s in target]

    return target, source

protocbuilder = scons.builder.builder(
    action = protocaction,
    emitter = protocemitter,
    srcsuffix = '$protocsrcsuffix')

def generate(env):
    """add builders and construction variables for protoc to an environment."""
    try:
        bld = env['builders']['protoc']
    except keyerror:
        bld = protocbuilder
        env['builders']['protoc'] = bld

    env['protoc']        = env.detect(protocs) or 'protoc'
    env['protocflags']   = scons.util.clvar('')
    env['protocprotopath'] = scons.util.clvar('')
    env['protoccom']     = '$protoc ${["-i%s"%x for x in protocprotopath]} $protocflags --cpp_out=$protoccppoutflags$protocoutdir ${protocpythonoutdir and ("--python_out="+protocpythonoutdir) or ""} ${protocfdsout and ("-o"+protocfdsout) or ""} ${sources}'
    env['protocoutdir'] = '${source.dir}'
    env['protocpythonoutdir'] = "python"
    env['protocsrcsuffix']  = '.proto'

def exists(env):
    return env.detect(protocs)