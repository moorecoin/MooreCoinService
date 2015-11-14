#! /usr/bin/python
#
# see readme for usage instructions.
import sys
import os
import subprocess

# we must use setuptools, not distutils, because we need to use the
# namespace_packages option for the "google" package.
try:
  from setuptools import setup, extension
except importerror:
  try:
    from ez_setup import use_setuptools
    use_setuptools()
    from setuptools import setup, extension
  except importerror:
    sys.stderr.write(
        "could not import setuptools; make sure you have setuptools or "
        "ez_setup installed.\n")
    raise
from distutils.command.clean import clean as _clean
from distutils.command.build_py import build_py as _build_py
from distutils.spawn import find_executable

maintainer_email = "protobuf@googlegroups.com"

# find the protocol compiler.
if 'protoc' in os.environ and os.path.exists(os.environ['protoc']):
  protoc = os.environ['protoc']
elif os.path.exists("../src/protoc"):
  protoc = "../src/protoc"
elif os.path.exists("../src/protoc.exe"):
  protoc = "../src/protoc.exe"
elif os.path.exists("../vsprojects/debug/protoc.exe"):
  protoc = "../vsprojects/debug/protoc.exe"
elif os.path.exists("../vsprojects/release/protoc.exe"):
  protoc = "../vsprojects/release/protoc.exe"
else:
  protoc = find_executable("protoc")

def generate_proto(source):
  """invokes the protocol compiler to generate a _pb2.py from the given
  .proto file.  does nothing if the output already exists and is newer than
  the input."""

  output = source.replace(".proto", "_pb2.py").replace("../src/", "")

  if (not os.path.exists(output) or
      (os.path.exists(source) and
       os.path.getmtime(source) > os.path.getmtime(output))):
    print "generating %s..." % output

    if not os.path.exists(source):
      sys.stderr.write("can't find required file: %s\n" % source)
      sys.exit(-1)

    if protoc == none:
      sys.stderr.write(
          "protoc is not installed nor found in ../src.  please compile it "
          "or install the binary package.\n")
      sys.exit(-1)

    protoc_command = [ protoc, "-i../src", "-i.", "--python_out=.", source ]
    if subprocess.call(protoc_command) != 0:
      sys.exit(-1)

def generateunittestprotos():
  generate_proto("../src/google/protobuf/unittest.proto")
  generate_proto("../src/google/protobuf/unittest_custom_options.proto")
  generate_proto("../src/google/protobuf/unittest_import.proto")
  generate_proto("../src/google/protobuf/unittest_import_public.proto")
  generate_proto("../src/google/protobuf/unittest_mset.proto")
  generate_proto("../src/google/protobuf/unittest_no_generic_services.proto")
  generate_proto("google/protobuf/internal/test_bad_identifiers.proto")
  generate_proto("google/protobuf/internal/more_extensions.proto")
  generate_proto("google/protobuf/internal/more_extensions_dynamic.proto")
  generate_proto("google/protobuf/internal/more_messages.proto")
  generate_proto("google/protobuf/internal/factory_test1.proto")
  generate_proto("google/protobuf/internal/factory_test2.proto")

def maketestsuite():
  # this is apparently needed on some systems to make sure that the tests
  # work even if a previous version is already installed.
  if 'google' in sys.modules:
    del sys.modules['google']
  generateunittestprotos()

  import unittest
  import google.protobuf.internal.generator_test     as generator_test
  import google.protobuf.internal.descriptor_test    as descriptor_test
  import google.protobuf.internal.reflection_test    as reflection_test
  import google.protobuf.internal.service_reflection_test \
    as service_reflection_test
  import google.protobuf.internal.text_format_test   as text_format_test
  import google.protobuf.internal.wire_format_test   as wire_format_test
  import google.protobuf.internal.unknown_fields_test as unknown_fields_test
  import google.protobuf.internal.descriptor_database_test \
      as descriptor_database_test
  import google.protobuf.internal.descriptor_pool_test as descriptor_pool_test
  import google.protobuf.internal.message_factory_test as message_factory_test
  import google.protobuf.internal.message_cpp_test as message_cpp_test
  import google.protobuf.internal.reflection_cpp_generated_test \
      as reflection_cpp_generated_test

  loader = unittest.defaulttestloader
  suite = unittest.testsuite()
  for test in [ generator_test,
                descriptor_test,
                reflection_test,
                service_reflection_test,
                text_format_test,
                wire_format_test ]:
    suite.addtest(loader.loadtestsfrommodule(test))

  return suite


class clean(_clean):
  def run(self):
    # delete generated files in the code tree.
    for (dirpath, dirnames, filenames) in os.walk("."):
      for filename in filenames:
        filepath = os.path.join(dirpath, filename)
        if filepath.endswith("_pb2.py") or filepath.endswith(".pyc") or \
          filepath.endswith(".so") or filepath.endswith(".o") or \
          filepath.endswith('google/protobuf/compiler/__init__.py'):
          os.remove(filepath)
    # _clean is an old-style class, so super() doesn't work.
    _clean.run(self)

class build_py(_build_py):
  def run(self):
    # generate necessary .proto file if it doesn't exist.
    generate_proto("../src/google/protobuf/descriptor.proto")
    generate_proto("../src/google/protobuf/compiler/plugin.proto")

    # make sure google.protobuf.compiler is a valid package.
    open('google/protobuf/compiler/__init__.py', 'a').close()
    # _build_py is an old-style class, so super() doesn't work.
    _build_py.run(self)

if __name__ == '__main__':
  ext_module_list = []

  # c++ implementation extension
  if os.getenv("protocol_buffers_python_implementation", "python") == "cpp":
    print "using experimental c++ implmenetation."
    ext_module_list.append(extension(
        "google.protobuf.internal._net_proto2___python",
        [ "google/protobuf/pyext/python_descriptor.cc",
          "google/protobuf/pyext/python_protobuf.cc",
          "google/protobuf/pyext/python-proto2.cc" ],
        include_dirs = [ "." ],
        libraries = [ "protobuf" ]))

  setup(name = 'protobuf',
        version = '2.5.1-pre',
        packages = [ 'google' ],
        namespace_packages = [ 'google' ],
        test_suite = 'setup.maketestsuite',
        # must list modules explicitly so that we don't install tests.
        py_modules = [
          'google.protobuf.internal.api_implementation',
          'google.protobuf.internal.containers',
          'google.protobuf.internal.cpp_message',
          'google.protobuf.internal.decoder',
          'google.protobuf.internal.encoder',
          'google.protobuf.internal.enum_type_wrapper',
          'google.protobuf.internal.message_listener',
          'google.protobuf.internal.python_message',
          'google.protobuf.internal.type_checkers',
          'google.protobuf.internal.wire_format',
          'google.protobuf.descriptor',
          'google.protobuf.descriptor_pb2',
          'google.protobuf.compiler.plugin_pb2',
          'google.protobuf.message',
          'google.protobuf.descriptor_database',
          'google.protobuf.descriptor_pool',
          'google.protobuf.message_factory',
          'google.protobuf.reflection',
          'google.protobuf.service',
          'google.protobuf.service_reflection',
          'google.protobuf.text_format' ],
        cmdclass = { 'clean': clean, 'build_py': build_py },
        install_requires = ['setuptools'],
        ext_modules = ext_module_list,
        url = 'http://code.google.com/p/protobuf/',
        maintainer = maintainer_email,
        maintainer_email = 'protobuf@googlegroups.com',
        license = 'new bsd license',
        description = 'protocol buffers',
        long_description =
          "protocol buffers are google's data interchange format.",
        )
