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

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/once.h>
#include <stdio.h>
#include <errno.h>
#include <vector>

#include "config.h"

#ifdef _win32
#define win32_lean_and_mean  // we only need minimal includes
#include <windows.h>
#define snprintf _snprintf    // see comment in strutil.cc
#elif defined(have_pthread)
#include <pthread.h>
#else
#error "no suitable threading library available."
#endif

namespace google {
namespace protobuf {

namespace internal {

void verifyversion(int headerversion,
                   int minlibraryversion,
                   const char* filename) {
  if (google_protobuf_version < minlibraryversion) {
    // library is too old for headers.
    google_log(fatal)
      << "this program requires version " << versionstring(minlibraryversion)
      << " of the protocol buffer runtime library, but the installed version "
         "is " << versionstring(google_protobuf_version) << ".  please update "
         "your library.  if you compiled the program yourself, make sure that "
         "your headers are from the same version of protocol buffers as your "
         "link-time library.  (version verification failed in \""
      << filename << "\".)";
  }
  if (headerversion < kminheaderversionforlibrary) {
    // headers are too old for library.
    google_log(fatal)
      << "this program was compiled against version "
      << versionstring(headerversion) << " of the protocol buffer runtime "
         "library, which is not compatible with the installed version ("
      << versionstring(google_protobuf_version) <<  ").  contact the program "
         "author for an update.  if you compiled the program yourself, make "
         "sure that your headers are from the same version of protocol buffers "
         "as your link-time library.  (version verification failed in \""
      << filename << "\".)";
  }
}

string versionstring(int version) {
  int major = version / 1000000;
  int minor = (version / 1000) % 1000;
  int micro = version % 1000;

  // 128 bytes should always be enough, but we use snprintf() anyway to be
  // safe.
  char buffer[128];
  snprintf(buffer, sizeof(buffer), "%d.%d.%d", major, minor, micro);

  // guard against broken msvc snprintf().
  buffer[sizeof(buffer)-1] = '\0';

  return buffer;
}

}  // namespace internal

// ===================================================================
// emulates google3/base/logging.cc

namespace internal {

void defaultloghandler(loglevel level, const char* filename, int line,
                       const string& message) {
  static const char* level_names[] = { "info", "warning", "error", "fatal" };

  // we use fprintf() instead of cerr because we want this to work at static
  // initialization time.
  fprintf(stderr, "[libprotobuf %s %s:%d] %s\n",
          level_names[level], filename, line, message.c_str());
  fflush(stderr);  // needed on msvc.
}

void nullloghandler(loglevel level, const char* filename, int line,
                    const string& message) {
  // nothing.
}

static loghandler* log_handler_ = &defaultloghandler;
static int log_silencer_count_ = 0;

static mutex* log_silencer_count_mutex_ = null;
google_protobuf_declare_once(log_silencer_count_init_);

void deletelogsilencercount() {
  delete log_silencer_count_mutex_;
  log_silencer_count_mutex_ = null;
}
void initlogsilencercount() {
  log_silencer_count_mutex_ = new mutex;
  onshutdown(&deletelogsilencercount);
}
void initlogsilencercountonce() {
  googleonceinit(&log_silencer_count_init_, &initlogsilencercount);
}

logmessage& logmessage::operator<<(const string& value) {
  message_ += value;
  return *this;
}

logmessage& logmessage::operator<<(const char* value) {
  message_ += value;
  return *this;
}

// since this is just for logging, we don't care if the current locale changes
// the results -- in fact, we probably prefer that.  so we use snprintf()
// instead of simple*toa().
#undef declare_stream_operator
#define declare_stream_operator(type, format)                       \
  logmessage& logmessage::operator<<(type value) {                  \
    /* 128 bytes should be big enough for any of the primitive */   \
    /* values which we print with this, but well use snprintf() */  \
    /* anyway to be extra safe. */                                  \
    char buffer[128];                                               \
    snprintf(buffer, sizeof(buffer), format, value);                \
    /* guard against broken msvc snprintf(). */                     \
    buffer[sizeof(buffer)-1] = '\0';                                \
    message_ += buffer;                                             \
    return *this;                                                   \
  }

declare_stream_operator(char         , "%c" )
declare_stream_operator(int          , "%d" )
declare_stream_operator(uint         , "%u" )
declare_stream_operator(long         , "%ld")
declare_stream_operator(unsigned long, "%lu")
declare_stream_operator(double       , "%g" )
#undef declare_stream_operator

logmessage::logmessage(loglevel level, const char* filename, int line)
  : level_(level), filename_(filename), line_(line) {}
logmessage::~logmessage() {}

void logmessage::finish() {
  bool suppress = false;

  if (level_ != loglevel_fatal) {
    initlogsilencercountonce();
    mutexlock lock(log_silencer_count_mutex_);
    suppress = log_silencer_count_ > 0;
  }

  if (!suppress) {
    log_handler_(level_, filename_, line_, message_);
  }

  if (level_ == loglevel_fatal) {
#if protobuf_use_exceptions
    throw fatalexception(filename_, line_, message_);
#else
    abort();
#endif
  }
}

void logfinisher::operator=(logmessage& other) {
  other.finish();
}

}  // namespace internal

loghandler* setloghandler(loghandler* new_func) {
  loghandler* old = internal::log_handler_;
  if (old == &internal::nullloghandler) {
    old = null;
  }
  if (new_func == null) {
    internal::log_handler_ = &internal::nullloghandler;
  } else {
    internal::log_handler_ = new_func;
  }
  return old;
}

logsilencer::logsilencer() {
  internal::initlogsilencercountonce();
  mutexlock lock(internal::log_silencer_count_mutex_);
  ++internal::log_silencer_count_;
};

logsilencer::~logsilencer() {
  internal::initlogsilencercountonce();
  mutexlock lock(internal::log_silencer_count_mutex_);
  --internal::log_silencer_count_;
};

// ===================================================================
// emulates google3/base/callback.cc

closure::~closure() {}

namespace internal { functionclosure0::~functionclosure0() {} }

void donothing() {}

// ===================================================================
// emulates google3/base/mutex.cc

#ifdef _win32

struct mutex::internal {
  critical_section mutex;
#ifndef ndebug
  // used only to implement assertheld().
  dword thread_id;
#endif
};

mutex::mutex()
  : minternal(new internal) {
  initializecriticalsection(&minternal->mutex);
}

mutex::~mutex() {
  deletecriticalsection(&minternal->mutex);
  delete minternal;
}

void mutex::lock() {
  entercriticalsection(&minternal->mutex);
#ifndef ndebug
  minternal->thread_id = getcurrentthreadid();
#endif
}

void mutex::unlock() {
#ifndef ndebug
  minternal->thread_id = 0;
#endif
  leavecriticalsection(&minternal->mutex);
}

void mutex::assertheld() {
#ifndef ndebug
  google_dcheck_eq(minternal->thread_id, getcurrentthreadid());
#endif
}

#elif defined(have_pthread)

struct mutex::internal {
  pthread_mutex_t mutex;
};

mutex::mutex()
  : minternal(new internal) {
  pthread_mutex_init(&minternal->mutex, null);
}

mutex::~mutex() {
  pthread_mutex_destroy(&minternal->mutex);
  delete minternal;
}

void mutex::lock() {
  int result = pthread_mutex_lock(&minternal->mutex);
  if (result != 0) {
    google_log(fatal) << "pthread_mutex_lock: " << strerror(result);
  }
}

void mutex::unlock() {
  int result = pthread_mutex_unlock(&minternal->mutex);
  if (result != 0) {
    google_log(fatal) << "pthread_mutex_unlock: " << strerror(result);
  }
}

void mutex::assertheld() {
  // pthreads dosn't provide a way to check which thread holds the mutex.
  // todo(kenton):  maybe keep track of locking thread id like with win32?
}

#endif

// ===================================================================
// emulates google3/util/endian/endian.h
//
// todo(xiaofeng): protobuf_little_endian is unfortunately defined in
// google/protobuf/io/coded_stream.h and therefore can not be used here.
// maybe move that macro definition here in the furture.
uint32 ghtonl(uint32 x) {
  union {
    uint32 result;
    uint8 result_array[4];
  };
  result_array[0] = static_cast<uint8>(x >> 24);
  result_array[1] = static_cast<uint8>((x >> 16) & 0xff);
  result_array[2] = static_cast<uint8>((x >> 8) & 0xff);
  result_array[3] = static_cast<uint8>(x & 0xff);
  return result;
}

// ===================================================================
// shutdown support.

namespace internal {

typedef void onshutdownfunc();
vector<void (*)()>* shutdown_functions = null;
mutex* shutdown_functions_mutex = null;
google_protobuf_declare_once(shutdown_functions_init);

void initshutdownfunctions() {
  shutdown_functions = new vector<void (*)()>;
  shutdown_functions_mutex = new mutex;
}

inline void initshutdownfunctionsonce() {
  googleonceinit(&shutdown_functions_init, &initshutdownfunctions);
}

void onshutdown(void (*func)()) {
  initshutdownfunctionsonce();
  mutexlock lock(shutdown_functions_mutex);
  shutdown_functions->push_back(func);
}

}  // namespace internal

void shutdownprotobuflibrary() {
  internal::initshutdownfunctionsonce();

  // we don't need to lock shutdown_functions_mutex because it's up to the
  // caller to make sure that no one is using the library before this is
  // called.

  // make it safe to call this multiple times.
  if (internal::shutdown_functions == null) return;

  for (int i = 0; i < internal::shutdown_functions->size(); i++) {
    internal::shutdown_functions->at(i)();
  }
  delete internal::shutdown_functions;
  internal::shutdown_functions = null;
  delete internal::shutdown_functions_mutex;
  internal::shutdown_functions_mutex = null;
}

#if protobuf_use_exceptions
fatalexception::~fatalexception() throw() {}

const char* fatalexception::what() const throw() {
  return message_.c_str();
}
#endif

}  // namespace protobuf
}  // namespace google
