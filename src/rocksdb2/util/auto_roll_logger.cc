//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
#include "util/auto_roll_logger.h"
#include "util/mutexlock.h"

using namespace std;

namespace rocksdb {

// -- autorolllogger
status autorolllogger::resetlogger() {
  status_ = env_->newlogger(log_fname_, &logger_);

  if (!status_.ok()) {
    return status_;
  }

  if (logger_->getlogfilesize() ==
      (size_t)logger::do_not_support_get_log_file_size) {
    status_ = status::notsupported(
        "the underlying logger doesn't support getlogfilesize()");
  }
  if (status_.ok()) {
    cached_now = static_cast<uint64_t>(env_->nowmicros() * 1e-6);
    ctime_ = cached_now;
    cached_now_access_count = 0;
  }

  return status_;
}

void autorolllogger::rolllogfile() {
  std::string old_fname = oldinfologfilename(
      dbname_, env_->nowmicros(), db_absolute_path_, db_log_dir_);
  env_->renamefile(log_fname_, old_fname);
}

void autorolllogger::logv(const char* format, va_list ap) {
  assert(getstatus().ok());

  std::shared_ptr<logger> logger;
  {
    mutexlock l(&mutex_);
    if ((klogfiletimetoroll > 0 && logexpired()) ||
        (kmaxlogfilesize > 0 && logger_->getlogfilesize() >= kmaxlogfilesize)) {
      rolllogfile();
      status s = resetlogger();
      if (!s.ok()) {
        // can't really log the error if creating a new log file failed
        return;
      }
    }

    // pin down the current logger_ instance before releasing the mutex.
    logger = logger_;
  }

  // another thread could have put a new logger instance into logger_ by now.
  // however, since logger is still hanging on to the previous instance
  // (reference count is not zero), we don't have to worry about it being
  // deleted while we are accessing it.
  // note that logv itself is not mutex protected to allow maximum concurrency,
  // as thread safety should have been handled by the underlying logger.
  logger->logv(format, ap);
}

bool autorolllogger::logexpired() {
  if (cached_now_access_count >= call_nowmicros_every_n_records_) {
    cached_now = static_cast<uint64_t>(env_->nowmicros() * 1e-6);
    cached_now_access_count = 0;
  }

  ++cached_now_access_count;
  return cached_now >= ctime_ + klogfiletimetoroll;
}

status createloggerfromoptions(
    const std::string& dbname,
    const std::string& db_log_dir,
    env* env,
    const dboptions& options,
    std::shared_ptr<logger>* logger) {
  std::string db_absolute_path;
  env->getabsolutepath(dbname, &db_absolute_path);
  std::string fname = infologfilename(dbname, db_absolute_path, db_log_dir);

  env->createdirifmissing(dbname);  // in case it does not exist
  // currently we only support roll by time-to-roll and log size
  if (options.log_file_time_to_roll > 0 || options.max_log_file_size > 0) {
    autorolllogger* result = new autorolllogger(
        env, dbname, db_log_dir,
        options.max_log_file_size,
        options.log_file_time_to_roll, options.info_log_level);
    status s = result->getstatus();
    if (!s.ok()) {
      delete result;
    } else {
      logger->reset(result);
    }
    return s;
  } else {
    // open a log file in the same directory as the db
    env->renamefile(fname, oldinfologfilename(dbname, env->nowmicros(),
                                              db_absolute_path, db_log_dir));
    auto s = env->newlogger(fname, logger);
    if (logger->get() != nullptr) {
      (*logger)->setinfologlevel(options.info_log_level);
    }
    return s;
  }
}

}  // namespace rocksdb
