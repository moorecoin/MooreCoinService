#!/bin/sh

# run this script to regenerate descriptor.pb.{h,cc} after the protocol
# compiler changes.  since these files are compiled into the protocol compiler
# itself, they cannot be generated automatically by a make rule.  "make check"
# will fail if these files do not match what the protocol compiler would
# generate.
#
# hint:  flags passed to generate_descriptor_proto.sh will be passed directly
#   to make when building protoc.  this is particularly useful for passing
#   -j4 to run 4 jobs simultaneously.

if test ! -e src/google/protobuf/stubs/common.h; then
  cat >&2 << __eof__
could not find source code.  make sure you are running this script from the
root of the distribution tree.
__eof__
  exit 1
fi

if test ! -e src/makefile; then
  cat >&2 << __eof__
could not find src/makefile.  you must run ./configure (and perhaps
./autogen.sh) first.
__eof__
  exit 1
fi

cd src
make $@ protoc &&
  ./protoc --cpp_out=dllexport_decl=libprotobuf_export:. google/protobuf/descriptor.proto && \
  ./protoc --cpp_out=dllexport_decl=libprotoc_export:. google/protobuf/compiler/plugin.proto
cd ..
