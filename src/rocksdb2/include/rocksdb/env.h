// copyright (c) 2013, facebook, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
//
// an env is an interface used by the rocksdb implementation to access
// operating system functionality like the filesystem etc.  callers
// may wish to provide a custom env object when opening a database to
// get fine gain control; e.g., to rate limit file system operations.
//
// all env implementations are safe for concurrent access from
// multiple threads without any external synchronization.

#ifndef storage_rocksdb_include_env_h_
#define storage_rocksdb_include_env_h_

#include <cstdarg>
#include <string>
#include <memory>
#include <vector>
#include <stdint.h>
#include "rocksdb/status.h"

namespace rocksdb {

class filelock;
class logger;
class randomaccessfile;
class sequentialfile;
class slice;
class writablefile;
class randomrwfile;
class directory;
struct dboptions;
class ratelimiter;

using std::unique_ptr;
using std::shared_ptr;


// options while opening a file to read/write
struct envoptions {

  // construct with default options
  envoptions();

  // construct from options
  explicit envoptions(const dboptions& options);

  // if true, then allow caching of data in environment buffers
  bool use_os_buffer = true;

   // if true, then use mmap to read data
  bool use_mmap_reads = false;

   // if true, then use mmap to write data
  bool use_mmap_writes = true;

  // if true, set the fd_cloexec on open fd.
  bool set_fd_cloexec = true;

  // allows os to incrementally sync files to disk while they are being
  // written, in the background. issue one request for every bytes_per_sync
  // written. 0 turns it off.
  // default: 0
  uint64_t bytes_per_sync = 0;

  // if true, we will preallocate the file with falloc_fl_keep_size flag, which
  // means that file size won't change as part of preallocation.
  // if false, preallocation will also change the file size. this option will
  // improve the performance in workloads where you sync the data on every
  // write. by default, we set it to true for manifest writes and false for
  // wal writes
  bool fallocate_with_keep_size = true;

  // if not nullptr, write rate limiting is enabled for flush and compaction
  ratelimiter* rate_limiter = nullptr;
};

class env {
 public:
  env() { }
  virtual ~env();

  // return a default environment suitable for the current operating
  // system.  sophisticated users may wish to provide their own env
  // implementation instead of relying on this default environment.
  //
  // the result of default() belongs to rocksdb and must never be deleted.
  static env* default();

  // create a brand new sequentially-readable file with the specified name.
  // on success, stores a pointer to the new file in *result and returns ok.
  // on failure stores nullptr in *result and returns non-ok.  if the file does
  // not exist, returns a non-ok status.
  //
  // the returned file will only be accessed by one thread at a time.
  virtual status newsequentialfile(const std::string& fname,
                                   unique_ptr<sequentialfile>* result,
                                   const envoptions& options)
                                   = 0;

  // create a brand new random access read-only file with the
  // specified name.  on success, stores a pointer to the new file in
  // *result and returns ok.  on failure stores nullptr in *result and
  // returns non-ok.  if the file does not exist, returns a non-ok
  // status.
  //
  // the returned file may be concurrently accessed by multiple threads.
  virtual status newrandomaccessfile(const std::string& fname,
                                     unique_ptr<randomaccessfile>* result,
                                     const envoptions& options)
                                     = 0;

  // create an object that writes to a new file with the specified
  // name.  deletes any existing file with the same name and creates a
  // new file.  on success, stores a pointer to the new file in
  // *result and returns ok.  on failure stores nullptr in *result and
  // returns non-ok.
  //
  // the returned file will only be accessed by one thread at a time.
  virtual status newwritablefile(const std::string& fname,
                                 unique_ptr<writablefile>* result,
                                 const envoptions& options) = 0;

  // create an object that both reads and writes to a file on
  // specified offsets (random access). if file already exists,
  // does not overwrite it. on success, stores a pointer to the
  // new file in *result and returns ok. on failure stores nullptr
  // in *result and returns non-ok.
  virtual status newrandomrwfile(const std::string& fname,
                                 unique_ptr<randomrwfile>* result,
                                 const envoptions& options) = 0;

  // create an object that represents a directory. will fail if directory
  // doesn't exist. if the directory exists, it will open the directory
  // and create a new directory object.
  //
  // on success, stores a pointer to the new directory in
  // *result and returns ok. on failure stores nullptr in *result and
  // returns non-ok.
  virtual status newdirectory(const std::string& name,
                              unique_ptr<directory>* result) = 0;

  // returns true iff the named file exists.
  virtual bool fileexists(const std::string& fname) = 0;

  // store in *result the names of the children of the specified directory.
  // the names are relative to "dir".
  // original contents of *results are dropped.
  virtual status getchildren(const std::string& dir,
                             std::vector<std::string>* result) = 0;

  // delete the named file.
  virtual status deletefile(const std::string& fname) = 0;

  // create the specified directory. returns error if directory exists.
  virtual status createdir(const std::string& dirname) = 0;

  // creates directory if missing. return ok if it exists, or successful in
  // creating.
  virtual status createdirifmissing(const std::string& dirname) = 0;

  // delete the specified directory.
  virtual status deletedir(const std::string& dirname) = 0;

  // store the size of fname in *file_size.
  virtual status getfilesize(const std::string& fname, uint64_t* file_size) = 0;

  // store the last modification time of fname in *file_mtime.
  virtual status getfilemodificationtime(const std::string& fname,
                                         uint64_t* file_mtime) = 0;
  // rename file src to target.
  virtual status renamefile(const std::string& src,
                            const std::string& target) = 0;

  // lock the specified file.  used to prevent concurrent access to
  // the same db by multiple processes.  on failure, stores nullptr in
  // *lock and returns non-ok.
  //
  // on success, stores a pointer to the object that represents the
  // acquired lock in *lock and returns ok.  the caller should call
  // unlockfile(*lock) to release the lock.  if the process exits,
  // the lock will be automatically released.
  //
  // if somebody else already holds the lock, finishes immediately
  // with a failure.  i.e., this call does not wait for existing locks
  // to go away.
  //
  // may create the named file if it does not already exist.
  virtual status lockfile(const std::string& fname, filelock** lock) = 0;

  // release the lock acquired by a previous successful call to lockfile.
  // requires: lock was returned by a successful lockfile() call
  // requires: lock has not already been unlocked.
  virtual status unlockfile(filelock* lock) = 0;

  // priority for scheduling job in thread pool
  enum priority { low, high, total };

  // priority for requesting bytes in rate limiter scheduler
  enum iopriority {
    io_low = 0,
    io_high = 1,
    io_total = 2
  };

  // arrange to run "(*function)(arg)" once in a background thread, in
  // the thread pool specified by pri. by default, jobs go to the 'low'
  // priority thread pool.

  // "function" may run in an unspecified thread.  multiple functions
  // added to the same env may run concurrently in different threads.
  // i.e., the caller may not assume that background work items are
  // serialized.
  virtual void schedule(
      void (*function)(void* arg),
      void* arg,
      priority pri = low) = 0;

  // start a new thread, invoking "function(arg)" within the new thread.
  // when "function(arg)" returns, the thread will be destroyed.
  virtual void startthread(void (*function)(void* arg), void* arg) = 0;

  // wait for all threads started by startthread to terminate.
  virtual void waitforjoin() {}

  // get thread pool queue length for specific thrad pool.
  virtual unsigned int getthreadpoolqueuelen(priority pri = low) const {
    return 0;
  }

  // *path is set to a temporary directory that can be used for testing. it may
  // or many not have just been created. the directory may or may not differ
  // between runs of the same process, but subsequent calls will return the
  // same directory.
  virtual status gettestdirectory(std::string* path) = 0;

  // create and return a log file for storing informational messages.
  virtual status newlogger(const std::string& fname,
                           shared_ptr<logger>* result) = 0;

  // returns the number of micro-seconds since some fixed point in time. only
  // useful for computing deltas of time.
  virtual uint64_t nowmicros() = 0;

  // returns the number of nano-seconds since some fixed point in time. only
  // useful for computing deltas of time in one run.
  // default implementation simply relies on nowmicros
  virtual uint64_t nownanos() {
    return nowmicros() * 1000;
  }

  // sleep/delay the thread for the perscribed number of micro-seconds.
  virtual void sleepformicroseconds(int micros) = 0;

  // get the current host name.
  virtual status gethostname(char* name, uint64_t len) = 0;

  // get the number of seconds since the epoch, 1970-01-01 00:00:00 (utc).
  virtual status getcurrenttime(int64_t* unix_time) = 0;

  // get full directory name for this db.
  virtual status getabsolutepath(const std::string& db_path,
      std::string* output_path) = 0;

  // the number of background worker threads of a specific thread pool
  // for this environment. 'low' is the default pool.
  // default number: 1
  virtual void setbackgroundthreads(int number, priority pri = low) = 0;

  // lower io priority for threads from the specified pool.
  virtual void lowerthreadpooliopriority(priority pool = low) {}

  // converts seconds-since-jan-01-1970 to a printable string
  virtual std::string timetostring(uint64_t time) = 0;

  // generates a unique id that can be used to identify a db
  virtual std::string generateuniqueid();

  // optimizeforlogwrite will create a new envoptions object that is a copy of
  // the envoptions in the parameters, but is optimized for writing log files.
  // default implementation returns the copy of the same object.
  virtual envoptions optimizeforlogwrite(const envoptions& env_options) const;
  // optimizeformanifestwrite will create a new envoptions object that is a copy
  // of the envoptions in the parameters, but is optimized for writing manifest
  // files. default implementation returns the copy of the same object.
  virtual envoptions optimizeformanifestwrite(const envoptions& env_options)
      const;

 private:
  // no copying allowed
  env(const env&);
  void operator=(const env&);
};

// a file abstraction for reading sequentially through a file
class sequentialfile {
 public:
  sequentialfile() { }
  virtual ~sequentialfile();

  // read up to "n" bytes from the file.  "scratch[0..n-1]" may be
  // written by this routine.  sets "*result" to the data that was
  // read (including if fewer than "n" bytes were successfully read).
  // may set "*result" to point at data in "scratch[0..n-1]", so
  // "scratch[0..n-1]" must be live when "*result" is used.
  // if an error was encountered, returns a non-ok status.
  //
  // requires: external synchronization
  virtual status read(size_t n, slice* result, char* scratch) = 0;

  // skip "n" bytes from the file. this is guaranteed to be no
  // slower that reading the same data, but may be faster.
  //
  // if end of file is reached, skipping will stop at the end of the
  // file, and skip will return ok.
  //
  // requires: external synchronization
  virtual status skip(uint64_t n) = 0;

  // remove any kind of caching of data from the offset to offset+length
  // of this file. if the length is 0, then it refers to the end of file.
  // if the system is not caching the file contents, then this is a noop.
  virtual status invalidatecache(size_t offset, size_t length) {
    return status::notsupported("invalidatecache not supported.");
  }
};

// a file abstraction for randomly reading the contents of a file.
class randomaccessfile {
 public:
  randomaccessfile() { }
  virtual ~randomaccessfile();

  // read up to "n" bytes from the file starting at "offset".
  // "scratch[0..n-1]" may be written by this routine.  sets "*result"
  // to the data that was read (including if fewer than "n" bytes were
  // successfully read).  may set "*result" to point at data in
  // "scratch[0..n-1]", so "scratch[0..n-1]" must be live when
  // "*result" is used.  if an error was encountered, returns a non-ok
  // status.
  //
  // safe for concurrent use by multiple threads.
  virtual status read(uint64_t offset, size_t n, slice* result,
                      char* scratch) const = 0;

  // tries to get an unique id for this file that will be the same each time
  // the file is opened (and will stay the same while the file is open).
  // furthermore, it tries to make this id at most "max_size" bytes. if such an
  // id can be created this function returns the length of the id and places it
  // in "id"; otherwise, this function returns 0, in which case "id"
  // may not have been modified.
  //
  // this function guarantees, for ids from a given environment, two unique ids
  // cannot be made equal to eachother by adding arbitrary bytes to one of
  // them. that is, no unique id is the prefix of another.
  //
  // this function guarantees that the returned id will not be interpretable as
  // a single varint.
  //
  // note: these ids are only valid for the duration of the process.
  virtual size_t getuniqueid(char* id, size_t max_size) const {
    return 0; // default implementation to prevent issues with backwards
              // compatibility.
  };


  enum accesspattern { normal, random, sequential, willneed, dontneed };

  virtual void hint(accesspattern pattern) {}

  // remove any kind of caching of data from the offset to offset+length
  // of this file. if the length is 0, then it refers to the end of file.
  // if the system is not caching the file contents, then this is a noop.
  virtual status invalidatecache(size_t offset, size_t length) {
    return status::notsupported("invalidatecache not supported.");
  }
};

// a file abstraction for sequential writing.  the implementation
// must provide buffering since callers may append small fragments
// at a time to the file.
class writablefile {
 public:
  writablefile()
    : last_preallocated_block_(0),
      preallocation_block_size_(0),
      io_priority_(env::io_total) {
  }
  virtual ~writablefile();

  virtual status append(const slice& data) = 0;
  virtual status close() = 0;
  virtual status flush() = 0;
  virtual status sync() = 0; // sync data

  /*
   * sync data and/or metadata as well.
   * by default, sync only data.
   * override this method for environments where we need to sync
   * metadata as well.
   */
  virtual status fsync() {
    return sync();
  }

  /*
   * change the priority in rate limiter if rate limiting is enabled.
   * if rate limiting is not enabled, this call has no effect.
   */
  virtual void setiopriority(env::iopriority pri) {
    io_priority_ = pri;
  }

  /*
   * get the size of valid data in the file.
   */
  virtual uint64_t getfilesize() {
    return 0;
  }

  /*
   * get and set the default pre-allocation block size for writes to
   * this file.  if non-zero, then allocate will be used to extend the
   * underlying storage of a file (generally via fallocate) if the env
   * instance supports it.
   */
  void setpreallocationblocksize(size_t size) {
    preallocation_block_size_ = size;
  }

  virtual void getpreallocationstatus(size_t* block_size,
                                      size_t* last_allocated_block) {
    *last_allocated_block = last_preallocated_block_;
    *block_size = preallocation_block_size_;
  }

  // for documentation, refer to randomaccessfile::getuniqueid()
  virtual size_t getuniqueid(char* id, size_t max_size) const {
    return 0; // default implementation to prevent issues with backwards
  }

  // remove any kind of caching of data from the offset to offset+length
  // of this file. if the length is 0, then it refers to the end of file.
  // if the system is not caching the file contents, then this is a noop.
  // this call has no effect on dirty pages in the cache.
  virtual status invalidatecache(size_t offset, size_t length) {
    return status::notsupported("invalidatecache not supported.");
  }

 protected:
  // preparewrite performs any necessary preparation for a write
  // before the write actually occurs.  this allows for pre-allocation
  // of space on devices where it can result in less file
  // fragmentation and/or less waste from over-zealous filesystem
  // pre-allocation.
  void preparewrite(size_t offset, size_t len) {
    if (preallocation_block_size_ == 0) {
      return;
    }
    // if this write would cross one or more preallocation blocks,
    // determine what the last preallocation block necesessary to
    // cover this write would be and allocate to that point.
    const auto block_size = preallocation_block_size_;
    size_t new_last_preallocated_block =
      (offset + len + block_size - 1) / block_size;
    if (new_last_preallocated_block > last_preallocated_block_) {
      size_t num_spanned_blocks =
        new_last_preallocated_block - last_preallocated_block_;
      allocate(block_size * last_preallocated_block_,
               block_size * num_spanned_blocks);
      last_preallocated_block_ = new_last_preallocated_block;
    }
  }

  /*
   * pre-allocate space for a file.
   */
  virtual status allocate(off_t offset, off_t len) {
    return status::ok();
  }

  // sync a file range with disk.
  // offset is the starting byte of the file range to be synchronized.
  // nbytes specifies the length of the range to be synchronized.
  // this asks the os to initiate flushing the cached data to disk,
  // without waiting for completion.
  // default implementation does nothing.
  virtual status rangesync(off_t offset, off_t nbytes) {
    return status::ok();
  }

 private:
  size_t last_preallocated_block_;
  size_t preallocation_block_size_;
  // no copying allowed
  writablefile(const writablefile&);
  void operator=(const writablefile&);

 protected:
  env::iopriority io_priority_;
};

// a file abstraction for random reading and writing.
class randomrwfile {
 public:
  randomrwfile() {}
  virtual ~randomrwfile() {}

  // write data from slice data to file starting from offset
  // returns ioerror on failure, but does not guarantee
  // atomicity of a write.  returns ok status on success.
  //
  // safe for concurrent use.
  virtual status write(uint64_t offset, const slice& data) = 0;
  // read up to "n" bytes from the file starting at "offset".
  // "scratch[0..n-1]" may be written by this routine.  sets "*result"
  // to the data that was read (including if fewer than "n" bytes were
  // successfully read).  may set "*result" to point at data in
  // "scratch[0..n-1]", so "scratch[0..n-1]" must be live when
  // "*result" is used.  if an error was encountered, returns a non-ok
  // status.
  //
  // safe for concurrent use by multiple threads.
  virtual status read(uint64_t offset, size_t n, slice* result,
                      char* scratch) const = 0;
  virtual status close() = 0; // closes the file
  virtual status sync() = 0; // sync data

  /*
   * sync data and/or metadata as well.
   * by default, sync only data.
   * override this method for environments where we need to sync
   * metadata as well.
   */
  virtual status fsync() {
    return sync();
  }

  /*
   * pre-allocate space for a file.
   */
  virtual status allocate(off_t offset, off_t len) {
    return status::ok();
  }

 private:
  // no copying allowed
  randomrwfile(const randomrwfile&);
  void operator=(const randomrwfile&);
};

// directory object represents collection of files and implements
// filesystem operations that can be executed on directories.
class directory {
 public:
  virtual ~directory() {}
  // fsync directory
  virtual status fsync() = 0;
};

enum infologlevel : unsigned char {
  debug_level = 0,
  info_level,
  warn_level,
  error_level,
  fatal_level,
  num_info_log_levels,
};

// an interface for writing log messages.
class logger {
 public:
  enum { do_not_support_get_log_file_size = -1 };
  explicit logger(const infologlevel log_level = infologlevel::info_level)
      : log_level_(log_level) {}
  virtual ~logger();

  // write an entry to the log file with the specified format.
  virtual void logv(const char* format, va_list ap) = 0;

  // write an entry to the log file with the specified log level
  // and format.  any log with level under the internal log level
  // of *this (see @setinfologlevel and @getinfologlevel) will not be
  // printed.
  void logv(const infologlevel log_level, const char* format, va_list ap) {
    static const char* kinfologlevelnames[5] = {"debug", "info", "warn",
                                                "error", "fatal"};
    if (log_level < log_level_) {
      return;
    }

    if (log_level == infologlevel::info_level) {
      // doesn't print log level if it is info level.
      // this is to avoid unexpected performance regression after we add
      // the feature of log level. all the logs before we add the feature
      // are info level. we don't want to add extra costs to those existing
      // logging.
      logv(format, ap);
    } else {
      char new_format[500];
      snprintf(new_format, sizeof(new_format) - 1, "[%s] %s",
               kinfologlevelnames[log_level], format);
      logv(new_format, ap);
    }
  }
  virtual size_t getlogfilesize() const {
    return do_not_support_get_log_file_size;
  }
  // flush to the os buffers
  virtual void flush() {}
  virtual infologlevel getinfologlevel() const { return log_level_; }
  virtual void setinfologlevel(const infologlevel log_level) {
    log_level_ = log_level;
  }

 private:
  // no copying allowed
  logger(const logger&);
  void operator=(const logger&);
  infologlevel log_level_;
};


// identifies a locked file.
class filelock {
 public:
  filelock() { }
  virtual ~filelock();
 private:
  // no copying allowed
  filelock(const filelock&);
  void operator=(const filelock&);
};

extern void logflush(const shared_ptr<logger>& info_log);

extern void log(const infologlevel log_level,
                const shared_ptr<logger>& info_log, const char* format, ...);

// a set of log functions with different log levels.
extern void debug(const shared_ptr<logger>& info_log, const char* format, ...);
extern void info(const shared_ptr<logger>& info_log, const char* format, ...);
extern void warn(const shared_ptr<logger>& info_log, const char* format, ...);
extern void error(const shared_ptr<logger>& info_log, const char* format, ...);
extern void fatal(const shared_ptr<logger>& info_log, const char* format, ...);

// log the specified data to *info_log if info_log is non-nullptr.
// the default info log level is infologlevel::error.
extern void log(const shared_ptr<logger>& info_log, const char* format, ...)
#   if defined(__gnuc__) || defined(__clang__)
    __attribute__((__format__ (__printf__, 2, 3)))
#   endif
    ;

extern void logflush(logger *info_log);

extern void log(const infologlevel log_level, logger* info_log,
                const char* format, ...);

// the default info log level is infologlevel::error.
extern void log(logger* info_log, const char* format, ...)
#   if defined(__gnuc__) || defined(__clang__)
    __attribute__((__format__ (__printf__, 2, 3)))
#   endif
    ;

// a set of log functions with different log levels.
extern void debug(logger* info_log, const char* format, ...);
extern void info(logger* info_log, const char* format, ...);
extern void warn(logger* info_log, const char* format, ...);
extern void error(logger* info_log, const char* format, ...);
extern void fatal(logger* info_log, const char* format, ...);

// a utility routine: write "data" to the named file.
extern status writestringtofile(env* env, const slice& data,
                                const std::string& fname,
                                bool should_sync = false);

// a utility routine: read contents of named file into *data
extern status readfiletostring(env* env, const std::string& fname,
                               std::string* data);

// an implementation of env that forwards all calls to another env.
// may be useful to clients who wish to override just part of the
// functionality of another env.
class envwrapper : public env {
 public:
  // initialize an envwrapper that delegates all calls to *t
  explicit envwrapper(env* t) : target_(t) { }
  virtual ~envwrapper();

  // return the target to which this env forwards all calls
  env* target() const { return target_; }

  // the following text is boilerplate that forwards all methods to target()
  status newsequentialfile(const std::string& f,
                           unique_ptr<sequentialfile>* r,
                           const envoptions& options) {
    return target_->newsequentialfile(f, r, options);
  }
  status newrandomaccessfile(const std::string& f,
                             unique_ptr<randomaccessfile>* r,
                             const envoptions& options) {
    return target_->newrandomaccessfile(f, r, options);
  }
  status newwritablefile(const std::string& f, unique_ptr<writablefile>* r,
                         const envoptions& options) {
    return target_->newwritablefile(f, r, options);
  }
  status newrandomrwfile(const std::string& f, unique_ptr<randomrwfile>* r,
                         const envoptions& options) {
    return target_->newrandomrwfile(f, r, options);
  }
  virtual status newdirectory(const std::string& name,
                              unique_ptr<directory>* result) {
    return target_->newdirectory(name, result);
  }
  bool fileexists(const std::string& f) { return target_->fileexists(f); }
  status getchildren(const std::string& dir, std::vector<std::string>* r) {
    return target_->getchildren(dir, r);
  }
  status deletefile(const std::string& f) { return target_->deletefile(f); }
  status createdir(const std::string& d) { return target_->createdir(d); }
  status createdirifmissing(const std::string& d) {
    return target_->createdirifmissing(d);
  }
  status deletedir(const std::string& d) { return target_->deletedir(d); }
  status getfilesize(const std::string& f, uint64_t* s) {
    return target_->getfilesize(f, s);
  }

  status getfilemodificationtime(const std::string& fname,
                                 uint64_t* file_mtime) {
    return target_->getfilemodificationtime(fname, file_mtime);
  }

  status renamefile(const std::string& s, const std::string& t) {
    return target_->renamefile(s, t);
  }
  status lockfile(const std::string& f, filelock** l) {
    return target_->lockfile(f, l);
  }
  status unlockfile(filelock* l) { return target_->unlockfile(l); }
  void schedule(void (*f)(void*), void* a, priority pri) {
    return target_->schedule(f, a, pri);
  }
  void startthread(void (*f)(void*), void* a) {
    return target_->startthread(f, a);
  }
  void waitforjoin() { return target_->waitforjoin(); }
  virtual unsigned int getthreadpoolqueuelen(priority pri = low) const {
    return target_->getthreadpoolqueuelen(pri);
  }
  virtual status gettestdirectory(std::string* path) {
    return target_->gettestdirectory(path);
  }
  virtual status newlogger(const std::string& fname,
                           shared_ptr<logger>* result) {
    return target_->newlogger(fname, result);
  }
  uint64_t nowmicros() {
    return target_->nowmicros();
  }
  void sleepformicroseconds(int micros) {
    target_->sleepformicroseconds(micros);
  }
  status gethostname(char* name, uint64_t len) {
    return target_->gethostname(name, len);
  }
  status getcurrenttime(int64_t* unix_time) {
    return target_->getcurrenttime(unix_time);
  }
  status getabsolutepath(const std::string& db_path,
      std::string* output_path) {
    return target_->getabsolutepath(db_path, output_path);
  }
  void setbackgroundthreads(int num, priority pri) {
    return target_->setbackgroundthreads(num, pri);
  }
  void lowerthreadpooliopriority(priority pool = low) override {
    target_->lowerthreadpooliopriority(pool);
  }
  std::string timetostring(uint64_t time) {
    return target_->timetostring(time);
  }

 private:
  env* target_;
};

// returns a new environment that stores its data in memory and delegates
// all non-file-storage tasks to base_env. the caller must delete the result
// when it is no longer needed.
// *base_env must remain live while the result is in use.
env* newmemenv(env* base_env);

}  // namespace rocksdb

#endif  // storage_rocksdb_include_env_h_
