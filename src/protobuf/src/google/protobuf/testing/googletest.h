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
// emulates google3/testing/base/public/googletest.h

#ifndef google_protobuf_googletest_h__
#define google_protobuf_googletest_h__

#include <map>
#include <vector>
#include <google/protobuf/stubs/common.h>

// disable death tests if we use exceptions in check().
#if !protobuf_use_exceptions && defined(gtest_has_death_test)
#define protobuf_has_death_test
#endif

namespace google {
namespace protobuf {

// when running unittests, get the directory containing the source code.
string testsourcedir();

// when running unittests, get a directory where temporary files may be
// placed.
string testtempdir();

// capture all text written to stdout or stderr.
void captureteststdout();
void captureteststderr();

// stop capturing stdout or stderr and return the text captured.
string getcapturedteststdout();
string getcapturedteststderr();

// for use with scopedmemorylog::getmessages().  inside google the loglevel
// constants don't have the loglevel_ prefix, so the code that used
// scopedmemorylog refers to loglevel_error as just error.
#undef error  // defend against promiscuous windows.h
static const loglevel error = loglevel_error;
static const loglevel warning = loglevel_warning;

// receives copies of all log(error) messages while in scope.  sample usage:
//   {
//     scopedmemorylog log;  // constructor registers object as a log sink
//     someroutinethatmaylogmessages();
//     const vector<string>& warnings = log.getmessages(error);
//   }  // destructor unregisters object as a log sink
// this is a dummy implementation which covers only what is used by protocol
// buffer unit tests.
class scopedmemorylog {
 public:
  scopedmemorylog();
  virtual ~scopedmemorylog();

  // fetches all messages with the given severity level.
  const vector<string>& getmessages(loglevel error);

 private:
  map<loglevel, vector<string> > messages_;
  loghandler* old_handler_;

  static void handlelog(loglevel level, const char* filename, int line,
                        const string& message);

  static scopedmemorylog* active_log_;

  google_disallow_evil_constructors(scopedmemorylog);
};

}  // namespace protobuf
}  // namespace google

#endif  // google_protobuf_googletest_h__
