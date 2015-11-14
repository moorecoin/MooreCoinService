//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// logger implementation that can be shared by all environments
// where enough posix functionality is available.

#pragma once
#include "db/filename.h"
#include "port/port.h"
#include "util/posix_logger.h"

namespace rocksdb {

// rolls the log file by size and/or time
class autorolllogger : public logger {
 public:
  autorolllogger(env* env, const std::string& dbname,
                 const std::string& db_log_dir, size_t log_max_size,
                 size_t log_file_time_to_roll,
                 const infologlevel log_level = infologlevel::info_level)
      : logger(log_level),
        dbname_(dbname),
        db_log_dir_(db_log_dir),
        env_(env),
        status_(status::ok()),
        kmaxlogfilesize(log_max_size),
        klogfiletimetoroll(log_file_time_to_roll),
        cached_now(static_cast<uint64_t>(env_->nowmicros() * 1e-6)),
        ctime_(cached_now),
        cached_now_access_count(0),
        call_nowmicros_every_n_records_(100),
        mutex_() {
    env->getabsolutepath(dbname, &db_absolute_path_);
    log_fname_ = infologfilename(dbname_, db_absolute_path_, db_log_dir_);
    rolllogfile();
    resetlogger();
  }

  void logv(const char* format, va_list ap);

  // check if the logger has encountered any problem.
  status getstatus() {
    return status_;
  }

  size_t getlogfilesize() const {
    return logger_->getlogfilesize();
  }

  virtual ~autorolllogger() {
  }

  void setcallnowmicroseverynrecords(uint64_t call_nowmicros_every_n_records) {
    call_nowmicros_every_n_records_ = call_nowmicros_every_n_records;
  }

 private:

  bool logexpired();
  status resetlogger();
  void rolllogfile();

  std::string log_fname_; // current active info log's file name.
  std::string dbname_;
  std::string db_log_dir_;
  std::string db_absolute_path_;
  env* env_;
  std::shared_ptr<logger> logger_;
  // current status of the logger
  status status_;
  const size_t kmaxlogfilesize;
  const size_t klogfiletimetoroll;
  // to avoid frequent env->nowmicros() calls, we cached the current time
  uint64_t cached_now;
  uint64_t ctime_;
  uint64_t cached_now_access_count;
  uint64_t call_nowmicros_every_n_records_;
  port::mutex mutex_;
};

// facade to craete logger automatically
status createloggerfromoptions(
    const std::string& dbname,
    const std::string& db_log_dir,
    env* env,
    const dboptions& options,
    std::shared_ptr<logger>* logger);

}  // namespace rocksdb
