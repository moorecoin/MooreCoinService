#!/bin/sh
#
# protocol buffers - google's data interchange format
# copyright 2009 google inc.  all rights reserved.
# http://code.google.com/p/protobuf/
#
# redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * neither the name of google inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# this software is provided by the copyright holders and contributors
# "as is" and any express or implied warranties, including, but not
# limited to, the implied warranties of merchantability and fitness for
# a particular purpose are disclaimed. in no event shall the copyright
# owner or contributors be liable for any direct, indirect, incidental,
# special, exemplary, or consequential damages (including, but not
# limited to, procurement of substitute goods or services; loss of use,
# data, or profits; or business interruption) however caused and on any
# theory of liability, whether in contract, strict liability, or tort
# (including negligence or otherwise) arising in any way out of the use
# of this software, even if advised of the possibility of such damage.

# author: kenton@google.com (kenton varda)
#
# test protoc's zip output mode.

fail() {
  echo "$@" >&2
  exit 1
}

test_tmpdir=.
protoc=./protoc

echo '
  syntax = "proto2";
  option java_multiple_files = true;
  option java_package = "test.jar";
  option java_outer_classname = "outer";
  message foo {}
  message bar {}
' > $test_tmpdir/testzip.proto

$protoc \
    --cpp_out=$test_tmpdir/testzip.zip --python_out=$test_tmpdir/testzip.zip \
    --java_out=$test_tmpdir/testzip.jar -i$test_tmpdir testzip.proto \
    || fail 'protoc failed.'

echo "testing output to zip..."
if unzip -h > /dev/null; then
  unzip -t $test_tmpdir/testzip.zip > $test_tmpdir/testzip.list || fail 'unzip failed.'

  grep 'testing: testzip\.pb\.cc *ok$' $test_tmpdir/testzip.list > /dev/null \
    || fail 'testzip.pb.cc not found in output zip.'
  grep 'testing: testzip\.pb\.h *ok$' $test_tmpdir/testzip.list > /dev/null \
    || fail 'testzip.pb.h not found in output zip.'
  grep 'testing: testzip_pb2\.py *ok$' $test_tmpdir/testzip.list > /dev/null \
    || fail 'testzip_pb2.py not found in output zip.'
  grep -i 'manifest' $test_tmpdir/testzip.list > /dev/null \
    && fail 'zip file contained manifest.'
else
  echo "warning:  'unzip' command not available.  skipping test."
fi

echo "testing output to jar..."
if jar c $test_tmpdir/testzip.proto > /dev/null; then
  jar tf $test_tmpdir/testzip.jar > $test_tmpdir/testzip.list || fail 'jar failed.'

  grep '^test/jar/foo\.java$' $test_tmpdir/testzip.list > /dev/null \
    || fail 'foo.java not found in output jar.'
  grep '^test/jar/bar\.java$' $test_tmpdir/testzip.list > /dev/null \
    || fail 'bar.java not found in output jar.'
  grep '^test/jar/outer\.java$' $test_tmpdir/testzip.list > /dev/null \
    || fail 'outer.java not found in output jar.'
  grep '^meta-inf/manifest\.mf$' $test_tmpdir/testzip.list > /dev/null \
    || fail 'manifest not found in output jar.'
else
  echo "warning:  'jar' command not available.  skipping test."
fi

echo pass
