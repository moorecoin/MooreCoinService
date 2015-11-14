// copyright (c) 2014, facebook, inc. all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.

#pragma once

#include "rocksdb/env.h"
#include "util/arena.h"
#include "util/autovector.h"
#include <ctime>

namespace rocksdb {

class logger;

// a class to buffer info log entries and flush them in the end.
class logbuffer {
 public:
  // log_level: the log level for all the logs
  // info_log:  logger to write the logs to
  logbuffer(const infologlevel log_level, logger* info_log);

  // add a log entry to the buffer.
  void addlogtobuffer(const char* format, va_list ap);

  size_t isempty() const { return logs_.empty(); }

  // flush all buffered log to the info log.
  void flushbuffertolog();

 private:
  // one log entry with its timestamp
  struct bufferedlog {
    struct timeval now_tv;  // timestamp of the log
    char message[1];        // beginning of log message
  };

  const infologlevel log_level_;
  logger* info_log_;
  arena arena_;
  autovector<bufferedlog*> logs_;
};

// add log to the logbuffer for a delayed info logging. it can be used when
// we want to add some logs inside a mutex.
extern void logtobuffer(logbuffer* log_buffer, const char* format, ...);

}  // namespace rocksdb
