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

#include <google/protobuf/compiler/subprocess.h>

#include <algorithm>
#include <iostream>

#ifndef _win32
#include <errno.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <signal.h>
#endif

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/message.h>
#include <google/protobuf/stubs/substitute.h>

namespace google {
namespace protobuf {
namespace compiler {

#ifdef _win32

static void closehandleordie(handle handle) {
  if (!closehandle(handle)) {
    google_log(fatal) << "closehandle: "
                      << subprocess::win32errormessage(getlasterror());
  }
}

subprocess::subprocess()
    : process_start_error_(error_success),
      child_handle_(null), child_stdin_(null), child_stdout_(null) {}

subprocess::~subprocess() {
  if (child_stdin_ != null) {
    closehandleordie(child_stdin_);
  }
  if (child_stdout_ != null) {
    closehandleordie(child_stdout_);
  }
}

void subprocess::start(const string& program, searchmode search_mode) {
  // create the pipes.
  handle stdin_pipe_read;
  handle stdin_pipe_write;
  handle stdout_pipe_read;
  handle stdout_pipe_write;

  if (!createpipe(&stdin_pipe_read, &stdin_pipe_write, null, 0)) {
    google_log(fatal) << "createpipe: " << win32errormessage(getlasterror());
  }
  if (!createpipe(&stdout_pipe_read, &stdout_pipe_write, null, 0)) {
    google_log(fatal) << "createpipe: " << win32errormessage(getlasterror());
  }

  // make child side of the pipes inheritable.
  if (!sethandleinformation(stdin_pipe_read,
                            handle_flag_inherit, handle_flag_inherit)) {
    google_log(fatal) << "sethandleinformation: "
                      << win32errormessage(getlasterror());
  }
  if (!sethandleinformation(stdout_pipe_write,
                            handle_flag_inherit, handle_flag_inherit)) {
    google_log(fatal) << "sethandleinformation: "
                      << win32errormessage(getlasterror());
  }

  // setup startupinfo to redirect handles.
  startupinfoa startup_info;
  zeromemory(&startup_info, sizeof(startup_info));
  startup_info.cb = sizeof(startup_info);
  startup_info.dwflags = startf_usestdhandles;
  startup_info.hstdinput = stdin_pipe_read;
  startup_info.hstdoutput = stdout_pipe_write;
  startup_info.hstderror = getstdhandle(std_error_handle);

  if (startup_info.hstderror == invalid_handle_value) {
    google_log(fatal) << "getstdhandle: "
                      << win32errormessage(getlasterror());
  }

  // createprocess() mutates its second parameter.  wtf?
  char* name_copy = strdup(program.c_str());

  // create the process.
  process_information process_info;

  if (createprocessa((search_mode == search_path) ? null : program.c_str(),
                     (search_mode == search_path) ? name_copy : null,
                     null,  // process security attributes
                     null,  // thread security attributes
                     true,  // inherit handles?
                     0,     // obscure creation flags
                     null,  // environment (inherit from parent)
                     null,  // current directory (inherit from parent)
                     &startup_info,
                     &process_info)) {
    child_handle_ = process_info.hprocess;
    closehandleordie(process_info.hthread);
    child_stdin_ = stdin_pipe_write;
    child_stdout_ = stdout_pipe_read;
  } else {
    process_start_error_ = getlasterror();
    closehandleordie(stdin_pipe_write);
    closehandleordie(stdout_pipe_read);
  }

  closehandleordie(stdin_pipe_read);
  closehandleordie(stdout_pipe_write);
  free(name_copy);
}

bool subprocess::communicate(const message& input, message* output,
                             string* error) {
  if (process_start_error_ != error_success) {
    *error = win32errormessage(process_start_error_);
    return false;
  }

  google_check(child_handle_ != null) << "must call start() first.";

  string input_data = input.serializeasstring();
  string output_data;

  int input_pos = 0;

  while (child_stdout_ != null) {
    handle handles[2];
    int handle_count = 0;

    if (child_stdin_ != null) {
      handles[handle_count++] = child_stdin_;
    }
    if (child_stdout_ != null) {
      handles[handle_count++] = child_stdout_;
    }

    dword wait_result =
        waitformultipleobjects(handle_count, handles, false, infinite);

    handle signaled_handle;
    if (wait_result >= wait_object_0 &&
        wait_result < wait_object_0 + handle_count) {
      signaled_handle = handles[wait_result - wait_object_0];
    } else if (wait_result == wait_failed) {
      google_log(fatal) << "waitformultipleobjects: "
                        << win32errormessage(getlasterror());
    } else {
      google_log(fatal) << "waitformultipleobjects: unexpected return code: "
                        << wait_result;
    }

    if (signaled_handle == child_stdin_) {
      dword n;
      if (!writefile(child_stdin_,
                     input_data.data() + input_pos,
                     input_data.size() - input_pos,
                     &n, null)) {
        // child closed pipe.  presumably it will report an error later.
        // pretend we're done for now.
        input_pos = input_data.size();
      } else {
        input_pos += n;
      }

      if (input_pos == input_data.size()) {
        // we're done writing.  close.
        closehandleordie(child_stdin_);
        child_stdin_ = null;
      }
    } else if (signaled_handle == child_stdout_) {
      char buffer[4096];
      dword n;

      if (!readfile(child_stdout_, buffer, sizeof(buffer), &n, null)) {
        // we're done reading.  close.
        closehandleordie(child_stdout_);
        child_stdout_ = null;
      } else {
        output_data.append(buffer, n);
      }
    }
  }

  if (child_stdin_ != null) {
    // child did not finish reading input before it closed the output.
    // presumably it exited with an error.
    closehandleordie(child_stdin_);
    child_stdin_ = null;
  }

  dword wait_result = waitforsingleobject(child_handle_, infinite);

  if (wait_result == wait_failed) {
    google_log(fatal) << "waitforsingleobject: "
                      << win32errormessage(getlasterror());
  } else if (wait_result != wait_object_0) {
    google_log(fatal) << "waitforsingleobject: unexpected return code: "
                      << wait_result;
  }

  dword exit_code;
  if (!getexitcodeprocess(child_handle_, &exit_code)) {
    google_log(fatal) << "getexitcodeprocess: "
                      << win32errormessage(getlasterror());
  }

  closehandleordie(child_handle_);
  child_handle_ = null;

  if (exit_code != 0) {
    *error = strings::substitute(
        "plugin failed with status code $0.", exit_code);
    return false;
  }

  if (!output->parsefromstring(output_data)) {
    *error = "plugin output is unparseable: " + cescape(output_data);
    return false;
  }

  return true;
}

string subprocess::win32errormessage(dword error_code) {
  char* message;

  // wtf?
  formatmessage(format_message_allocate_buffer |
                format_message_from_system |
                format_message_ignore_inserts,
                null, error_code, 0,
                (lptstr)&message,  // not a bug!
                0, null);

  string result = message;
  localfree(message);
  return result;
}

// ===================================================================

#else  // _win32

subprocess::subprocess()
    : child_pid_(-1), child_stdin_(-1), child_stdout_(-1) {}

subprocess::~subprocess() {
  if (child_stdin_ != -1) {
    close(child_stdin_);
  }
  if (child_stdout_ != -1) {
    close(child_stdout_);
  }
}

void subprocess::start(const string& program, searchmode search_mode) {
  // note that we assume that there are no other threads, thus we don't have to
  // do crazy stuff like using socket pairs or avoiding libc locks.

  // [0] is read end, [1] is write end.
  int stdin_pipe[2];
  int stdout_pipe[2];

  google_check(pipe(stdin_pipe) != -1);
  google_check(pipe(stdout_pipe) != -1);

  char* argv[2] = { strdup(program.c_str()), null };

  child_pid_ = fork();
  if (child_pid_ == -1) {
    google_log(fatal) << "fork: " << strerror(errno);
  } else if (child_pid_ == 0) {
    // we are the child.
    dup2(stdin_pipe[0], stdin_fileno);
    dup2(stdout_pipe[1], stdout_fileno);

    close(stdin_pipe[0]);
    close(stdin_pipe[1]);
    close(stdout_pipe[0]);
    close(stdout_pipe[1]);

    switch (search_mode) {
      case search_path:
        execvp(argv[0], argv);
        break;
      case exact_name:
        execv(argv[0], argv);
        break;
    }

    // write directly to stderr_fileno to avoid stdio code paths that may do
    // stuff that is unsafe here.
    int ignored;
    ignored = write(stderr_fileno, argv[0], strlen(argv[0]));
    const char* message = ": program not found or is not executable\n";
    ignored = write(stderr_fileno, message, strlen(message));
    (void) ignored;

    // must use _exit() rather than exit() to avoid flushing output buffers
    // that will also be flushed by the parent.
    _exit(1);
  } else {
    free(argv[0]);

    close(stdin_pipe[0]);
    close(stdout_pipe[1]);

    child_stdin_ = stdin_pipe[1];
    child_stdout_ = stdout_pipe[0];
  }
}

bool subprocess::communicate(const message& input, message* output,
                             string* error) {

  google_check_ne(child_stdin_, -1) << "must call start() first.";

  // the "sighandler_t" typedef is gnu-specific, so define our own.
  typedef void signalhandler(int);

  // make sure sigpipe is disabled so that if the child dies it doesn't kill us.
  signalhandler* old_pipe_handler = signal(sigpipe, sig_ign);

  string input_data = input.serializeasstring();
  string output_data;

  int input_pos = 0;
  int max_fd = max(child_stdin_, child_stdout_);

  while (child_stdout_ != -1) {
    fd_set read_fds;
    fd_set write_fds;
    fd_zero(&read_fds);
    fd_zero(&write_fds);
    if (child_stdout_ != -1) {
      fd_set(child_stdout_, &read_fds);
    }
    if (child_stdin_ != -1) {
      fd_set(child_stdin_, &write_fds);
    }

    if (select(max_fd + 1, &read_fds, &write_fds, null, null) < 0) {
      if (errno == eintr) {
        // interrupted by signal.  try again.
        continue;
      } else {
        google_log(fatal) << "select: " << strerror(errno);
      }
    }

    if (child_stdin_ != -1 && fd_isset(child_stdin_, &write_fds)) {
      int n = write(child_stdin_, input_data.data() + input_pos,
                                  input_data.size() - input_pos);
      if (n < 0) {
        // child closed pipe.  presumably it will report an error later.
        // pretend we're done for now.
        input_pos = input_data.size();
      } else {
        input_pos += n;
      }

      if (input_pos == input_data.size()) {
        // we're done writing.  close.
        close(child_stdin_);
        child_stdin_ = -1;
      }
    }

    if (child_stdout_ != -1 && fd_isset(child_stdout_, &read_fds)) {
      char buffer[4096];
      int n = read(child_stdout_, buffer, sizeof(buffer));

      if (n > 0) {
        output_data.append(buffer, n);
      } else {
        // we're done reading.  close.
        close(child_stdout_);
        child_stdout_ = -1;
      }
    }
  }

  if (child_stdin_ != -1) {
    // child did not finish reading input before it closed the output.
    // presumably it exited with an error.
    close(child_stdin_);
    child_stdin_ = -1;
  }

  int status;
  while (waitpid(child_pid_, &status, 0) == -1) {
    if (errno != eintr) {
      google_log(fatal) << "waitpid: " << strerror(errno);
    }
  }

  // restore sigpipe handling.
  signal(sigpipe, old_pipe_handler);

  if (wifexited(status)) {
    if (wexitstatus(status) != 0) {
      int error_code = wexitstatus(status);
      *error = strings::substitute(
          "plugin failed with status code $0.", error_code);
      return false;
    }
  } else if (wifsignaled(status)) {
    int signal = wtermsig(status);
    *error = strings::substitute(
        "plugin killed by signal $0.", signal);
    return false;
  } else {
    *error = "neither wexitstatus nor wtermsig is true?";
    return false;
  }

  if (!output->parsefromstring(output_data)) {
    *error = "plugin output is unparseable.";
    return false;
  }

  return true;
}

#endif  // !_win32

}  // namespace compiler
}  // namespace protobuf
}  // namespace google
