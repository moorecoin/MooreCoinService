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
// emulates google3/testing/base/public/googletest.cc

#include <google/protobuf/testing/googletest.h>
#include <google/protobuf/testing/file.h>
#include <google/protobuf/stubs/strutil.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <stdlib.h>
#ifdef _msc_ver
#include <io.h>
#include <direct.h>
#else
#include <unistd.h>
#endif
#include <stdio.h>
#include <fcntl.h>
#include <iostream>
#include <fstream>

namespace google {
namespace protobuf {

#ifdef _win32
#define mkdir(name, mode) mkdir(name)
#endif

#ifndef o_binary
#ifdef _o_binary
#define o_binary _o_binary
#else
#define o_binary 0     // if this isn't defined, the platform doesn't need it.
#endif
#endif

string testsourcedir() {
#ifdef _msc_ver
  // look for the "src" directory.
  string prefix = ".";

  while (!file::exists(prefix + "/src/google/protobuf")) {
    if (!file::exists(prefix)) {
      google_log(fatal)
        << "could not find protobuf source code.  please run tests from "
           "somewhere within the protobuf source package.";
    }
    prefix += "/..";
  }
  return prefix + "/src";
#else
  // automake sets the "srcdir" environment variable.
  char* result = getenv("srcdir");
  if (result == null) {
    // otherwise, the test must be run from the source directory.
    return ".";
  } else {
    return result;
  }
#endif
}

namespace {

string gettemporarydirectoryname() {
  // tmpnam() is generally not considered safe but we're only using it for
  // testing.  we cannot use tmpfile() or mkstemp() since we're creating a
  // directory.
  char b[l_tmpnam + 1];     // hpux multithread return 0 if s is 0
  string result = tmpnam(b);
#ifdef _win32
  // on win32, tmpnam() returns a file prefixed with '\', but which is supposed
  // to be used in the current working directory.  wtf?
  if (hasprefixstring(result, "\\")) {
    result.erase(0, 1);
  }
#endif  // _win32
  return result;
}

// creates a temporary directory on demand and deletes it when the process
// quits.
class tempdirdeleter {
 public:
  tempdirdeleter() {}
  ~tempdirdeleter() {
    if (!name_.empty()) {
      file::deleterecursively(name_, null, null);
    }
  }

  string gettempdir() {
    if (name_.empty()) {
      name_ = gettemporarydirectoryname();
      google_check(mkdir(name_.c_str(), 0777) == 0) << strerror(errno);

      // stick a file in the directory that tells people what this is, in case
      // we abort and don't get a chance to delete it.
      file::writestringtofileordie("", name_ + "/temp_dir_for_protobuf_tests");
    }
    return name_;
  }

 private:
  string name_;
};

tempdirdeleter temp_dir_deleter_;

}  // namespace

string testtempdir() {
  return temp_dir_deleter_.gettempdir();
}

// todo(kenton):  share duplicated code below.  too busy/lazy for now.

static string stdout_capture_filename_;
static string stderr_capture_filename_;
static int original_stdout_ = -1;
static int original_stderr_ = -1;

void captureteststdout() {
  google_check_eq(original_stdout_, -1) << "already capturing.";

  stdout_capture_filename_ = testtempdir() + "/captured_stdout";

  int fd = open(stdout_capture_filename_.c_str(),
                o_wronly | o_creat | o_excl | o_binary, 0777);
  google_check(fd >= 0) << "open: " << strerror(errno);

  original_stdout_ = dup(1);
  close(1);
  dup2(fd, 1);
  close(fd);
}

void captureteststderr() {
  google_check_eq(original_stderr_, -1) << "already capturing.";

  stderr_capture_filename_ = testtempdir() + "/captured_stderr";

  int fd = open(stderr_capture_filename_.c_str(),
                o_wronly | o_creat | o_excl | o_binary, 0777);
  google_check(fd >= 0) << "open: " << strerror(errno);

  original_stderr_ = dup(2);
  close(2);
  dup2(fd, 2);
  close(fd);
}

string getcapturedteststdout() {
  google_check_ne(original_stdout_, -1) << "not capturing.";

  close(1);
  dup2(original_stdout_, 1);
  original_stdout_ = -1;

  string result;
  file::readfiletostringordie(stdout_capture_filename_, &result);

  remove(stdout_capture_filename_.c_str());

  return result;
}

string getcapturedteststderr() {
  google_check_ne(original_stderr_, -1) << "not capturing.";

  close(2);
  dup2(original_stderr_, 2);
  original_stderr_ = -1;

  string result;
  file::readfiletostringordie(stderr_capture_filename_, &result);

  remove(stderr_capture_filename_.c_str());

  return result;
}

scopedmemorylog* scopedmemorylog::active_log_ = null;

scopedmemorylog::scopedmemorylog() {
  google_check(active_log_ == null);
  active_log_ = this;
  old_handler_ = setloghandler(&handlelog);
}

scopedmemorylog::~scopedmemorylog() {
  setloghandler(old_handler_);
  active_log_ = null;
}

const vector<string>& scopedmemorylog::getmessages(loglevel level) {
  google_check(level == error ||
               level == warning);
  return messages_[level];
}

void scopedmemorylog::handlelog(loglevel level, const char* filename,
                                int line, const string& message) {
  google_check(active_log_ != null);
  if (level == error || level == warning) {
    active_log_->messages_[level].push_back(message);
  }
}

namespace {

// force shutdown at process exit so that we can test for memory leaks.  to
// actually check for leaks, i suggest using the heap checker included with
// google-perftools.  set it to "draconian" mode to ensure that every last
// call to malloc() has a corresponding free().
struct forceshutdown {
  ~forceshutdown() {
    shutdownprotobuflibrary();
  }
} force_shutdown;

}  // namespace

}  // namespace protobuf
}  // namespace google
