// protocol buffers - google's data interchange format
// copyright 2008 google inc.  all rights reserved.
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

// author: kenton@google.com (kenton varda)
// emulates google3/file/base/file.h

#ifndef google_protobuf_testing_file_h__
#define google_protobuf_testing_file_h__

#include <google/protobuf/stubs/common.h>

namespace google {
namespace protobuf {

const int default_file_mode = 0777;

// protocol buffer code only uses a couple static methods of file, and only
// in tests.
class file {
 public:
  // check if the file exists.
  static bool exists(const string& name);

  // read an entire file to a string.  return true if successful, false
  // otherwise.
  static bool readfiletostring(const string& name, string* output);

  // same as above, but crash on failure.
  static void readfiletostringordie(const string& name, string* output);

  // create a file and write a string to it.
  static void writestringtofileordie(const string& contents,
                                     const string& name);

  // create a directory.
  static bool createdir(const string& name, int mode);

  // create a directory and all parent directories if necessary.
  static bool recursivelycreatedir(const string& path, int mode);

  // if "name" is a file, we delete it.  if it is a directory, we
  // call deleterecursively() for each file or directory (other than
  // dot and double-dot) within it, and then delete the directory itself.
  // the "dummy" parameters have a meaning in the original version of this
  // method but they are not used anywhere in protocol buffers.
  static void deleterecursively(const string& name,
                                void* dummy1, void* dummy2);

 private:
  google_disallow_evil_constructors(file);
};

}  // namespace protobuf
}  // namespace google

#endif  // google_protobuf_testing_file_h__
