//  copyright (c) 2014, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.

#include "util/log_buffer.h"

#include <sys/time.h>

namespace rocksdb {

logbuffer::logbuffer(const infologlevel log_level,
                     logger*info_log)
    : log_level_(log_level), info_log_(info_log) {}

void logbuffer::addlogtobuffer(const char* format, va_list ap) {
  if (!info_log_ || log_level_ < info_log_->getinfologlevel()) {
    // skip the level because of its level.
    return;
  }

  const size_t klogsizelimit = 512;
  char* alloc_mem = arena_.allocatealigned(klogsizelimit);
  bufferedlog* buffered_log = new (alloc_mem) bufferedlog();
  char* p = buffered_log->message;
  char* limit = alloc_mem + klogsizelimit - 1;

  // store the time
  gettimeofday(&(buffered_log->now_tv), nullptr);

  // print the message
  if (p < limit) {
    va_list backup_ap;
    va_copy(backup_ap, ap);
    auto n = vsnprintf(p, limit - p, format, backup_ap);
    assert(n >= 0);
    p += n;
    va_end(backup_ap);
  }

  if (p > limit) {
    p = limit;
  }

  // add '\0' to the end
  *p = '\0';

  logs_.push_back(buffered_log);
}

void logbuffer::flushbuffertolog() {
  for (bufferedlog* log : logs_) {
    const time_t seconds = log->now_tv.tv_sec;
    struct tm t;
    localtime_r(&seconds, &t);
    log(log_level_, info_log_,
        "(original log time %04d/%02d/%02d-%02d:%02d:%02d.%06d) %s",
        t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min,
        t.tm_sec, static_cast<int>(log->now_tv.tv_usec), log->message);
  }
  logs_.clear();
}

void logtobuffer(logbuffer* log_buffer, const char* format, ...) {
  if (log_buffer != nullptr) {
    va_list ap;
    va_start(ap, format);
    log_buffer->addlogtobuffer(format, ap);
    va_end(ap);
  }
}

}  // namespace rocksdb
