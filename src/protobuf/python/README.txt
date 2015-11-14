protocol buffers - google's data interchange format
copyright 2008 google inc.

this directory contains the python protocol buffers runtime library.

normally, this directory comes as part of the protobuf package, available
from:

  http://code.google.com/p/protobuf

the complete package includes the c++ source code, which includes the
protocol compiler (protoc).  if you downloaded this package from pypi
or some other python-specific source, you may have received only the
python part of the code.  in this case, you will need to obtain the
protocol compiler from some other source before you can use this
package.

development warning
===================

the python implementation of protocol buffers is not as mature as the c++
and java implementations.  it may be more buggy, and it is known to be
pretty slow at this time.  if you would like to help fix these issues,
join the protocol buffers discussion list and let us know!

installation
============

1) make sure you have python 2.4 or newer.  if in doubt, run:

     $ python -v

2) if you do not have setuptools installed, note that it will be
   downloaded and installed automatically as soon as you run setup.py.
   if you would rather install it manually, you may do so by following
   the instructions on this page:

     http://peak.telecommunity.com/devcenter/easyinstall#installation-instructions

3) build the c++ code, or install a binary distribution of protoc.  if
   you install a binary distribution, make sure that it is the same
   version as this package.  if in doubt, run:

     $ protoc --version

4) build and run the tests:

     $ python setup.py build
     $ python setup.py test

   if some tests fail, this library may not work correctly on your
   system.  continue at your own risk.

   please note that there is a known problem with some versions of
   python on cygwin which causes the tests to fail after printing the
   error:  "sem_init: resource temporarily unavailable".  this appears
   to be a bug either in cygwin or in python:
     http://www.cygwin.com/ml/cygwin/2005-07/msg01378.html
   we do not know if or when it might me fixed.  we also do not know
   how likely it is that this bug will affect users in practice.

5) install:

     $ python setup.py install

   this step may require superuser privileges.
   note: to use c++ implementation, you need to install c++ protobuf runtime
   library of the same version and export the environment variable before this
   step. see the "c++ implementation" section below for more details.

usage
=====

the complete documentation for protocol buffers is available via the
web at:

  http://code.google.com/apis/protocolbuffers/

c++ implementation
==================

warning: this is experimental and only available for cpython platforms.

the c++ implementation for python messages is built as a python extension to
improve the overall protobuf python performance.

to use the c++ implementation, you need to:
1) install the c++ protobuf runtime library, please see instructions in the
   parent directory.
2) export an environment variable:

  $ export protocol_buffers_python_implementation=cpp

you need to export this variable before running setup.py script to build and
install the extension.  you must also set the variable at runtime, otherwise
the pure-python implementation will be used. in a future release, we will
change the default so that c++ implementation is used whenever it is available.
it is strongly recommended to run `python setup.py test` after setting the
variable to "cpp", so the tests will be against c++ implemented python
messages.

