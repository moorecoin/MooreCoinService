protocol buffers - google's data interchange format
copyright 2008 google inc.
http://code.google.com/apis/protocolbuffers/

c++ installation - unix
=======================

to build and install the c++ protocol buffer runtime and the protocol
buffer compiler (protoc) execute the following:

  $ ./configure
  $ make
  $ make check
  $ make install

if "make check" fails, you can still install, but it is likely that
some features of this library will not work correctly on your system.
proceed at your own risk.

"make install" may require superuser privileges.

for advanced usage information on configure and make, see install.txt.

** hint on install location **

  by default, the package will be installed to /usr/local.  however,
  on many platforms, /usr/local/lib is not part of ld_library_path.
  you can add it, but it may be easier to just install to /usr
  instead.  to do this, invoke configure as follows:

    ./configure --prefix=/usr

  if you already built the package with a different prefix, make sure
  to run "make clean" before building again.

** compiling dependent packages **

  to compile a package that uses protocol buffers, you need to pass
  various flags to your compiler and linker.  as of version 2.2.0,
  protocol buffers integrates with pkg-config to manage this.  if you
  have pkg-config installed, then you can invoke it to get a list of
  flags like so:

    pkg-config --cflags protobuf         # print compiler flags
    pkg-config --libs protobuf           # print linker flags
    pkg-config --cflags --libs protobuf  # print both

  for example:

    c++ my_program.cc my_proto.pb.cc `pkg-config --cflags --libs protobuf`

  note that packages written prior to the 2.2.0 release of protocol
  buffers may not yet integrate with pkg-config to get flags, and may
  not pass the correct set of flags to correctly link against
  libprotobuf.  if the package in question uses autoconf, you can
  often fix the problem by invoking its configure script like:

    configure cxxflags="$(pkg-config --cflags protobuf)" \
              libs="$(pkg-config --libs protobuf)"

  this will force it to use the correct flags.

  if you are writing an autoconf-based package that uses protocol
  buffers, you should probably use the pkg_check_modules macro in your
  configure script like:

    pkg_check_modules([protobuf], [protobuf])

  see the pkg-config man page for more info.

  if you only want protobuf-lite, substitute "protobuf-lite" in place
  of "protobuf" in these examples.

** note for cross-compiling **

  the makefiles normally invoke the protoc executable that they just
  built in order to build tests.  when cross-compiling, the protoc
  executable may not be executable on the host machine.  in this case,
  you must build a copy of protoc for the host machine first, then use
  the --with-protoc option to tell configure to use it instead.  for
  example:

    ./configure --with-protoc=protoc

  this will use the installed protoc (found in your $path) instead of
  trying to execute the one built during the build process.  you can
  also use an executable that hasn't been installed.  for example, if
  you built the protobuf package for your host machine in ../host,
  you might do:

    ./configure --with-protoc=../host/src/protoc

  either way, you must make sure that the protoc executable you use
  has the same version as the protobuf source code you are trying to
  use it with.

** note for solaris users **

  solaris 10 x86 has a bug that will make linking fail, complaining
  about libstdc++.la being invalid.  we have included a work-around
  in this package.  to use the work-around, run configure as follows:

    ./configure ldflags=-l$pwd/src/solaris

  see src/solaris/libstdc++.la for more info on this bug.

** note for hp c++ tru64 users **

  to compile invoke configure as follows:

    ./configure cxxflags="-o -std ansi -ieee -d__use_std_iostream"

  also, you will need to use gmake instead of make.

c++ installation - windows
==========================

if you are using microsoft visual c++, see vsprojects/readme.txt.

if you are using cygwin or mingw, follow the unix installation
instructions, above.

binary compatibility warning
============================

due to the nature of c++, it is unlikely that any two versions of the
protocol buffers c++ runtime libraries will have compatible abis.
that is, if you linked an executable against an older version of
libprotobuf, it is unlikely to work with a newer version without
re-compiling.  this problem, when it occurs, will normally be detected
immediately on startup of your app.  still, you may want to consider
using static linkage.  you can configure this package to install
static libraries only using:

  ./configure --disable-shared

java and python installation
============================

the java and python runtime libraries for protocol buffers are located
in the java and python directories.  see the readme file in each
directory for more information on how to compile and install them.
note that both of them require you to first install the protocol
buffer compiler (protoc), which is part of the c++ package.

usage
=====

the complete documentation for protocol buffers is available via the
web at:

  http://code.google.com/apis/protocolbuffers/
