#!/bin/sh

# run this script to generate the configure script and other files that will
# be included in the distribution.  these files are not checked in because they
# are automatically generated.

set -e

# check that we're being run from the right directory.
if test ! -f src/google/protobuf/stubs/common.h; then
  cat >&2 << __eof__
could not find source code.  make sure you are running this script from the
root of the distribution tree.
__eof__
  exit 1
fi

# check that gtest is present.  usually it is already there since the
# directory is set up as an svn external.
if test ! -e gtest; then
  echo "google test not present.  fetching gtest-1.5.0 from the web..."
  curl http://googletest.googlecode.com/files/gtest-1.5.0.tar.bz2 | tar jx
  mv gtest-1.5.0 gtest
fi

set -ex

# temporary hack:  must change c runtime library to "multi-threaded dll",
#   otherwise it will be set to "multi-threaded static" when msvc upgrades
#   the project file to msvc 2005/2008.  vladl of google test says gtest will
#   probably change their default to match, then this will be unnecessary.
#   one of these mappings converts the debug configuration and the other
#   converts the release configuration.  i don't know which is which.
sed -i -e 's/runtimelibrary="5"/runtimelibrary="3"/g;
           s/runtimelibrary="4"/runtimelibrary="2"/g;' gtest/msvc/*.vcproj

# todo(kenton):  remove the ",no-obsolete" part and fix the resulting warnings.
autoreconf -f -i -wall,no-obsolete

rm -rf autom4te.cache config.h.in~
exit 0
