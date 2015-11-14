// protocol buffers - google's data interchange format
// copyright 2009 google inc.  all rights reserved.
// http://code.google.com/p/protobuf/
//
// redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * neither the name of google inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// this software is provided by the copyright holders and contributors
// "as is" and any express or implied warranties, including, but not
// limited to, the implied warranties of merchantability and fitness for
// a particular purpose are disclaimed. in no event shall the copyright
// owner or contributors be liable for any direct, indirect, incidental,
// special, exemplary, or consequential damages (including, but not
// limited to, procurement of substitute goods or services; loss of use,
// data, or profits; or business interruption) however caused and on any
// theory of liability, whether in contract, strict liability, or tort
// (including negligence or otherwise) arising in any way out of the use
// of this software, even if advised of the possibility of such damage.

// author: brianolson@google.com (brian olson)
//  based on original protocol buffers design by
//  sanjay ghemawat, jeff dean, and others.
//
// test program to verify that gzipoutputstream is compatible with command line
// gzip or java.util.zip.gzipoutputstream
//
// reads data on standard input and writes compressed gzip stream to standard
// output.

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#include <google/protobuf/io/gzip_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

using google::protobuf::io::fileoutputstream;
using google::protobuf::io::gzipoutputstream;

int main(int argc, const char** argv) {
  fileoutputstream fout(stdout_fileno);
  gzipoutputstream out(&fout);
  int readlen;

  while (true) {
    void* outptr;
    int outlen;
    bool ok;
    do {
      ok = out.next(&outptr, &outlen);
      if (!ok) {
        break;
      }
    } while (outlen <= 0);
    readlen = read(stdin_fileno, outptr, outlen);
    if (readlen <= 0) {
      out.backup(outlen);
      break;
    }
    if (readlen < outlen) {
      out.backup(outlen - readlen);
    }
  }

  return 0;
}
