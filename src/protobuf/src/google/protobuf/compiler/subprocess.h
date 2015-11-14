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

#ifndef google_protobuf_compiler_subprocess_h__
#define google_protobuf_compiler_subprocess_h__

#ifdef _win32
#define win32_lean_and_mean   // right...
#include <windows.h>
#else  // _win32
#include <sys/types.h>
#include <unistd.h>
#endif  // !_win32
#include <google/protobuf/stubs/common.h>

#include <string>


namespace google {
namespace protobuf {

class message;

namespace compiler {

// utility class for launching sub-processes.
class libprotoc_export subprocess {
 public:
  subprocess();
  ~subprocess();

  enum searchmode {
    search_path,   // use path environment variable.
    exact_name     // program is an exact file name; don't use the path.
  };

  // start the subprocess.  currently we don't provide a way to specify
  // arguments as protoc plugins don't have any.
  void start(const string& program, searchmode search_mode);

  // serialize the input message and pipe it to the subprocess's stdin, then
  // close the pipe.  meanwhile, read from the subprocess's stdout and parse
  // the data into *output.  all this is done carefully to avoid deadlocks.
  // returns true if successful.  on any sort of error, returns false and sets
  // *error to a description of the problem.
  bool communicate(const message& input, message* output, string* error);

#ifdef _win32
  // given an error code, returns a human-readable error message.  this is
  // defined here so that commandlineinterface can share it.
  static string win32errormessage(dword error_code);
#endif

 private:
#ifdef _win32
  dword process_start_error_;
  handle child_handle_;

  // the file handles for our end of the child's pipes.  we close each and
  // set it to null when no longer needed.
  handle child_stdin_;
  handle child_stdout_;

#else  // _win32
  pid_t child_pid_;

  // the file descriptors for our end of the child's pipes.  we close each and
  // set it to -1 when no longer needed.
  int child_stdin_;
  int child_stdout_;

#endif  // !_win32
};

}  // namespace compiler
}  // namespace protobuf

}  // namespace google
#endif  // google_protobuf_compiler_subprocess_h__
