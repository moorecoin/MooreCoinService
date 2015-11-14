//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "rocksdb/env.h"

#include <sys/time.h>
#include "rocksdb/options.h"
#include "util/arena.h"
#include "util/autovector.h"

namespace rocksdb {

env::~env() {
}

sequentialfile::~sequentialfile() {
}

randomaccessfile::~randomaccessfile() {
}

writablefile::~writablefile() {
}

logger::~logger() {
}

filelock::~filelock() {
}

void logflush(logger *info_log) {
  if (info_log) {
    info_log->flush();
  }
}

void log(logger* info_log, const char* format, ...) {
  if (info_log) {
    va_list ap;
    va_start(ap, format);
    info_log->logv(infologlevel::info_level, format, ap);
    va_end(ap);
  }
}

void log(const infologlevel log_level, logger* info_log, const char* format,
         ...) {
  if (info_log) {
    va_list ap;
    va_start(ap, format);
    info_log->logv(log_level, format, ap);
    va_end(ap);
  }
}

void debug(logger* info_log, const char* format, ...) {
  if (info_log) {
    va_list ap;
    va_start(ap, format);
    info_log->logv(infologlevel::debug_level, format, ap);
    va_end(ap);
  }
}

void info(logger* info_log, const char* format, ...) {
  if (info_log) {
    va_list ap;
    va_start(ap, format);
    info_log->logv(infologlevel::info_level, format, ap);
    va_end(ap);
  }
}

void warn(logger* info_log, const char* format, ...) {
  if (info_log) {
    va_list ap;
    va_start(ap, format);
    info_log->logv(infologlevel::warn_level, format, ap);
    va_end(ap);
  }
}
void error(logger* info_log, const char* format, ...) {
  if (info_log) {
    va_list ap;
    va_start(ap, format);
    info_log->logv(infologlevel::error_level, format, ap);
    va_end(ap);
  }
}
void fatal(logger* info_log, const char* format, ...) {
  if (info_log) {
    va_list ap;
    va_start(ap, format);
    info_log->logv(infologlevel::fatal_level, format, ap);
    va_end(ap);
  }
}

void logflush(const shared_ptr<logger>& info_log) {
  if (info_log) {
    info_log->flush();
  }
}

void log(const infologlevel log_level, const shared_ptr<logger>& info_log,
         const char* format, ...) {
  if (info_log) {
    va_list ap;
    va_start(ap, format);
    info_log->logv(log_level, format, ap);
    va_end(ap);
  }
}

void debug(const shared_ptr<logger>& info_log, const char* format, ...) {
  if (info_log) {
    va_list ap;
    va_start(ap, format);
    info_log->logv(infologlevel::debug_level, format, ap);
    va_end(ap);
  }
}

void info(const shared_ptr<logger>& info_log, const char* format, ...) {
  if (info_log) {
    va_list ap;
    va_start(ap, format);
    info_log->logv(infologlevel::info_level, format, ap);
    va_end(ap);
  }
}

void warn(const shared_ptr<logger>& info_log, const char* format, ...) {
  if (info_log) {
    va_list ap;
    va_start(ap, format);
    info_log->logv(infologlevel::warn_level, format, ap);
    va_end(ap);
  }
}

void error(const shared_ptr<logger>& info_log, const char* format, ...) {
  if (info_log) {
    va_list ap;
    va_start(ap, format);
    info_log->logv(infologlevel::error_level, format, ap);
    va_end(ap);
  }
}

void fatal(const shared_ptr<logger>& info_log, const char* format, ...) {
  if (info_log) {
    va_list ap;
    va_start(ap, format);
    info_log->logv(infologlevel::fatal_level, format, ap);
    va_end(ap);
  }
}

void log(const shared_ptr<logger>& info_log, const char* format, ...) {
  if (info_log) {
    va_list ap;
    va_start(ap, format);
    info_log->logv(infologlevel::info_level, format, ap);
    va_end(ap);
  }
}

status writestringtofile(env* env, const slice& data, const std::string& fname,
                         bool should_sync) {
  unique_ptr<writablefile> file;
  envoptions soptions;
  status s = env->newwritablefile(fname, &file, soptions);
  if (!s.ok()) {
    return s;
  }
  s = file->append(data);
  if (s.ok() && should_sync) {
    s = file->sync();
  }
  if (!s.ok()) {
    env->deletefile(fname);
  }
  return s;
}

status readfiletostring(env* env, const std::string& fname, std::string* data) {
  envoptions soptions;
  data->clear();
  unique_ptr<sequentialfile> file;
  status s = env->newsequentialfile(fname, &file, soptions);
  if (!s.ok()) {
    return s;
  }
  static const int kbuffersize = 8192;
  char* space = new char[kbuffersize];
  while (true) {
    slice fragment;
    s = file->read(kbuffersize, &fragment, space);
    if (!s.ok()) {
      break;
    }
    data->append(fragment.data(), fragment.size());
    if (fragment.empty()) {
      break;
    }
  }
  delete[] space;
  return s;
}

envwrapper::~envwrapper() {
}

namespace {  // anonymous namespace

void assignenvoptions(envoptions* env_options, const dboptions& options) {
  env_options->use_os_buffer = options.allow_os_buffer;
  env_options->use_mmap_reads = options.allow_mmap_reads;
  env_options->use_mmap_writes = options.allow_mmap_writes;
  env_options->set_fd_cloexec = options.is_fd_close_on_exec;
  env_options->bytes_per_sync = options.bytes_per_sync;
  env_options->rate_limiter = options.rate_limiter.get();
}

}

envoptions env::optimizeforlogwrite(const envoptions& env_options) const {
  return env_options;
}

envoptions env::optimizeformanifestwrite(const envoptions& env_options) const {
  return env_options;
}

envoptions::envoptions(const dboptions& options) {
  assignenvoptions(this, options);
}

envoptions::envoptions() {
  dboptions options;
  assignenvoptions(this, options);
}


}  // namespace rocksdb
