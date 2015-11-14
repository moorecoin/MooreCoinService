this directory contains project files for compiling protocol buffers using
msvc.  this is not the recommended way to do protocol buffer development --
we prefer to develop under a unix-like environment -- but it may be more
accessible to those who primarily work with msvc.

compiling and installing
========================

1) open protobuf.sln in microsoft visual studio.
2) choose "debug" or "release" configuration as desired.*
3) from the build menu, choose "build solution".  wait for compiling to finish.
4) from a command shell, run tests.exe and lite-test.exe and check that all
   tests pass.
5) run extract_includes.bat to copy all the public headers into a separate
   "include" directory (under the top-level package directory).
6) copy the contents of the include directory to wherever you want to put
   headers.
7) copy protoc.exe wherever you put build tools (probably somewhere in your
   path).
8) copy libprotobuf.lib, libprotobuf-lite.lib, and libprotoc.lib wherever you
   put libraries.

* to avoid conflicts between the msvc debug and release runtime libraries, when
  compiling a debug build of your application, you may need to link against a
  debug build of libprotobuf.lib.  similarly, release builds should link against
  release libs.

dlls vs. static linking
=======================

static linking is now the default for the protocol buffer libraries.  due to
issues with win32's use of a separate heap for each dll, as well as binary
compatibility issues between different versions of msvc's stl library, it is
recommended that you use static linkage only.  however, it is possible to
build libprotobuf and libprotoc as dlls if you really want.  to do this,
do the following:

  1) open protobuf.sln in msvc.
  2) for each of the projects libprotobuf, libprotobuf-lite, and libprotoc, do
     the following:
    2a) right-click the project and choose "properties".
    2b) from the side bar, choose "general", under "configuration properties".
    2c) change the "configuration type" to "dynamic library (.dll)".
    2d) from the side bar, choose "preprocessor", under "c/c++".
    2e) add protobuf_use_dlls to the list of preprocessor defines.
  3) when compiling your project, make sure to #define protobuf_use_dlls.

when distributing your software to end users, we strongly recommend that you
do not install libprotobuf.dll or libprotoc.dll to any shared location.
instead, keep these libraries next to your binaries, in your application's
own install directory.  c++ makes it very difficult to maintain binary
compatibility between releases, so it is likely that future versions of these
libraries will *not* be usable as drop-in replacements.

if your project is itself a dll intended for use by third-party software, we
recommend that you do not expose protocol buffer objects in your library's
public interface, and that you statically link protocol buffers into your
library.

zlib support
============

if you want to include gzipinputstream and gzipoutputstream
(google/protobuf/io/gzip_stream.h) in libprotoc, you will need to do a few
additional steps:

1) obtain a copy of the zlib library.  the pre-compiled dll at zlib.net works.
2) make sure zlib's two headers are in your include path and that the .lib file
   is in your library path.  you could place all three files directly into the
   vsproject directory to compile libprotobuf, but they need to be visible to
   your own project as well, so you should probably just put them into the
   vc shared icnlude and library directories.
3) right-click on the "tests" project and choose "properties".  navigate the
   sidebar to "configuration properties" -> "linker" -> "input".
4) under "additional dependencies", add the name of the zlib .lib file (e.g.
   zdll.lib).  make sure to update both the debug and release configurations.
5) if you are compiling libprotobuf and libprotoc as dlls (see previous
   section), repeat steps 2 and 3 for the libprotobuf and libprotoc projects.
   if you are compiling them as static libraries, then you will need to link
   against the zlib library directly from your own app.
6) edit config.h (in the vsprojects directory) and un-comment the line that
   #defines have_zlib.  (or, alternatively, define this macro via the project
   settings.)

notes on compiler warnings
==========================

the following warnings have been disabled while building the protobuf libraries
and compiler.  you may have to disable some of them in your own project as
well, or live with them.

c4018 - 'expression' : signed/unsigned mismatch
c4146 - unary minus operator applied to unsigned type, result still unsigned
c4244 - conversion from 'type1' to 'type2', possible loss of data.
c4251 - 'identifier' : class 'type' needs to have dll-interface to be used by
        clients of class 'type2'
c4267 - conversion from 'size_t' to 'type', possible loss of data.
c4305 - 'identifier' : truncation from 'type1' to 'type2'
c4355 - 'this' : used in base member initializer list
c4800 - 'type' : forcing value to bool 'true' or 'false' (performance warning)
c4996 - 'function': was declared deprecated

c4251 is of particular note, if you are compiling the protocol buffer library
as a dll (see previous section).  the protocol buffer library uses templates in
its public interfaces.  msvc does not provide any reasonable way to export
template classes from a dll.  however, in practice, it appears that exporting
templates is not necessary anyway.  since the complete definition of any
template is available in the header files, anyone importing the dll will just
end up compiling instances of the templates into their own binary.  the
protocol buffer implementation does not rely on static template members being
unique, so there should be no problem with this, but msvc prints warning
nevertheless.  so, we disable it.  unfortunately, this warning will also be
produced when compiling code which merely uses protocol buffers, meaning you
may have to disable it in your code too.
