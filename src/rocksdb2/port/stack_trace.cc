//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
#include "port/stack_trace.h"

namespace rocksdb {
namespace port {

#if defined(rocksdb_lite) || !(defined(os_linux) || defined(os_macosx))

// noop

void installstacktracehandler() {}
void printstack(int first_frames_to_skip) {}

#else

#include <execinfo.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <cxxabi.h>

namespace {

#ifdef os_linux
const char* getexecutablename() {
  static char name[1024];

  char link[1024];
  snprintf(link, sizeof(link), "/proc/%d/exe", getpid());
  auto read = readlink(link, name, sizeof(name));
  if (-1 == read) {
    return nullptr;
  } else {
    name[read] = 0;
    return name;
  }
}

void printstacktraceline(const char* symbol, void* frame) {
  static const char* executable = getexecutablename();
  if (symbol) {
    fprintf(stderr, "%s ", symbol);
  }
  if (executable) {
    // out source to addr2line, for the address translation
    const int klinemax = 256;
    char cmd[klinemax];
    snprintf(cmd, klinemax, "addr2line %p -e %s -f -c 2>&1", frame, executable);
    auto f = popen(cmd, "r");
    if (f) {
      char line[klinemax];
      while (fgets(line, sizeof(line), f)) {
        line[strlen(line) - 1] = 0;  // remove newline
        fprintf(stderr, "%s\t", line);
      }
      pclose(f);
    }
  } else {
    fprintf(stderr, " %p", frame);
  }

  fprintf(stderr, "\n");
}
#elif os_macosx

void printstacktraceline(const char* symbol, void* frame) {
  static int pid = getpid();
  // out source to atos, for the address translation
  const int klinemax = 256;
  char cmd[klinemax];
  snprintf(cmd, klinemax, "xcrun atos %p -p %d  2>&1", frame, pid);
  auto f = popen(cmd, "r");
  if (f) {
    char line[klinemax];
    while (fgets(line, sizeof(line), f)) {
      line[strlen(line) - 1] = 0;  // remove newline
      fprintf(stderr, "%s\t", line);
    }
    pclose(f);
  } else if (symbol) {
    fprintf(stderr, "%s ", symbol);
  }

  fprintf(stderr, "\n");
}

#endif

}  // namespace

void printstack(int first_frames_to_skip) {
  const int kmaxframes = 100;
  void* frames[kmaxframes];

  auto num_frames = backtrace(frames, kmaxframes);
  auto symbols = backtrace_symbols(frames, num_frames);

  for (int i = first_frames_to_skip; i < num_frames; ++i) {
    fprintf(stderr, "#%-2d  ", i - first_frames_to_skip);
    printstacktraceline((symbols != nullptr) ? symbols[i] : nullptr, frames[i]);
  }
}

static void stacktracehandler(int sig) {
  // reset to default handler
  signal(sig, sig_dfl);
  fprintf(stderr, "received signal %d (%s)\n", sig, strsignal(sig));
  // skip the top three signal handler related frames
  printstack(3);
  // re-signal to default handler (so we still get core dump if needed...)
  raise(sig);
}

void installstacktracehandler() {
  // just use the plain old signal as it's simple and sufficient
  // for this use case
  signal(sigill, stacktracehandler);
  signal(sigsegv, stacktracehandler);
  signal(sigbus, stacktracehandler);
  signal(sigabrt, stacktracehandler);
}

#endif

}  // namespace port
}  // namespace rocksdb
