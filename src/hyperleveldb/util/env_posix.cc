// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include <deque>
#include <set>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#if defined(leveldb_platform_android)
#include <sys/stat.h>
#endif
#include "../hyperleveldb/env.h"
#include "../hyperleveldb/slice.h"
#include "../port/port.h"
#include "logging.h"
#include "mutexlock.h"
#include "posix_logger.h"

namespace hyperleveldb {

namespace {

static status ioerror(const std::string& context, int err_number) {
  return status::ioerror(context, strerror(err_number));
}

class posixsequentialfile: public sequentialfile {
 private:
  std::string filename_;
  file* file_;

 public:
  posixsequentialfile(const std::string& fname, file* f)
      : filename_(fname), file_(f) { }
  virtual ~posixsequentialfile() { fclose(file_); }

  virtual status read(size_t n, slice* result, char* scratch) {
    status s;
    size_t r = fread_unlocked(scratch, 1, n, file_);
    *result = slice(scratch, r);
    if (r < n) {
      if (feof(file_)) {
        // we leave status as ok if we hit the end of the file
      } else {
        // a partial read with an error: return a non-ok status
        s = ioerror(filename_, errno);
      }
    }
    return s;
  }

  virtual status skip(uint64_t n) {
    if (fseek(file_, n, seek_cur)) {
      return ioerror(filename_, errno);
    }
    return status::ok();
  }
};

// pread() based random-access
class posixrandomaccessfile: public randomaccessfile {
 private:
  std::string filename_;
  int fd_;

 public:
  posixrandomaccessfile(const std::string& fname, int fd)
      : filename_(fname), fd_(fd) { }
  virtual ~posixrandomaccessfile() { close(fd_); }

  virtual status read(uint64_t offset, size_t n, slice* result,
                      char* scratch) const {
    status s;
    ssize_t r = pread(fd_, scratch, n, static_cast<off_t>(offset));
    *result = slice(scratch, (r < 0) ? 0 : r);
    if (r < 0) {
      // an error: return a non-ok status
      s = ioerror(filename_, errno);
    }
    return s;
  }
};

// helper class to limit mmap file usage so that we do not end up
// running out virtual memory or running into kernel performance
// problems for very large databases.
class mmaplimiter {
 public:
  // up to 1000 mmaps for 64-bit binaries; none for smaller pointer sizes.
  mmaplimiter() {
    setallowed(sizeof(void*) >= 8 ? 1000 : 0);
  }

  // if another mmap slot is available, acquire it and return true.
  // else return false.
  bool acquire() {
    if (getallowed() <= 0) {
      return false;
    }
    mutexlock l(&mu_);
    intptr_t x = getallowed();
    if (x <= 0) {
      return false;
    } else {
      setallowed(x - 1);
      return true;
    }
  }

  // release a slot acquired by a previous call to acquire() that returned true.
  void release() {
    mutexlock l(&mu_);
    setallowed(getallowed() + 1);
  }

 private:
  port::mutex mu_;
  port::atomicpointer allowed_;

  intptr_t getallowed() const {
    return reinterpret_cast<intptr_t>(allowed_.acquire_load());
  }

  // requires: mu_ must be held
  void setallowed(intptr_t v) {
    allowed_.release_store(reinterpret_cast<void*>(v));
  }

  mmaplimiter(const mmaplimiter&);
  void operator=(const mmaplimiter&);
};

// mmap() based random-access
class posixmmapreadablefile: public randomaccessfile {
 private:
  std::string filename_;
  void* mmapped_region_;
  size_t length_;
  mmaplimiter* limiter_;

 public:
  // base[0,length-1] contains the mmapped contents of the file.
  posixmmapreadablefile(const std::string& fname, void* base, size_t length,
                        mmaplimiter* limiter)
      : filename_(fname), mmapped_region_(base), length_(length),
        limiter_(limiter) {
  }

  virtual ~posixmmapreadablefile() {
    munmap(mmapped_region_, length_);
    limiter_->release();
  }

  virtual status read(uint64_t offset, size_t n, slice* result,
                      char* scratch) const {
    status s;
    if (offset + n > length_) {
      *result = slice();
      s = ioerror(filename_, einval);
    } else {
      *result = slice(reinterpret_cast<char*>(mmapped_region_) + offset, n);
    }
    return s;
  }
};

// we preallocate up to an extra megabyte and use memcpy to append new
// data to the file.  this is safe since we either properly close the
// file before reading from it, or for log files, the reading code
// knows enough to skip zero suffixes.
// todo:  i use gcc intrinsics here.  i don't feel bad about this, but it
// hinders portability.
class posixmmapfile : public writablefile {
 private:
  struct mmapsegment {
    mmapsegment* next_;     // the next-lowest map segment in the file
    uint64_t file_offset_;  // offset of base_ in file
    uint64_t written_;      // the amount of data written to this segment
    uint64_t size_;         // the size of the mapped region
    char* base_;            // the mapped region
  };

  std::string filename_;    // path to the file
  int fd_;                  // the open file
  size_t page_size_;        // system page size
  uint64_t sync_offset_;    // offset of the last sync call
  uint64_t end_offset_;     // where does the file end?
  mmapsegment* segments_;   // mmap'ed regions of memory
  port::mutex mtx_;         // synchronize and shit

  // roundup x to a multiple of y
  static size_t roundup(size_t x, size_t y) {
    return ((x + y - 1) / y) * y;
  }

  mmapsegment* getsegment(uint64_t offset) {
    mutexlock l(&mtx_);
    while (true) {
      mmapsegment* seg = segments_;
      while (seg && seg->file_offset_ > offset) {
        seg = seg->next_;
      }
      if (!seg || seg->file_offset_ + seg->size_ <= offset) {
        assert(seg == segments_);
        mmapsegment* new_seg = new mmapsegment();
        new_seg->next_ = seg;
        new_seg->file_offset_ = seg ? seg->file_offset_ + seg-> size_ : 0;
        new_seg->written_ = 0;
        new_seg->size_ = seg ? seg->size_ : roundup(1 << 20, page_size_);
        if (ftruncate(fd_, new_seg->file_offset_ + new_seg->size_) < 0) {
          delete new_seg;
          return null;
        }
        void* ptr = mmap(null, new_seg->size_, prot_read | prot_write, map_shared,
                         fd_, new_seg->file_offset_);
        if (ptr == map_failed) {
          delete new_seg;
          return null;
        }
        new_seg->base_ = reinterpret_cast<char*>(ptr);
        segments_ = new_seg;
        continue;
      }
      assert(seg &&
             seg->file_offset_ <= offset &&
             seg->file_offset_ + seg->size_ > offset);
      return seg;
    }
  }

  bool releasesegment(mmapsegment* seg, bool full) {
    return true;
  }

 public:
  posixmmapfile(const std::string& fname, int fd, size_t page_size)
      : filename_(fname),
        fd_(fd),
        page_size_(page_size),
        sync_offset_(0),
        end_offset_(0),
        segments_(null),
        mtx_() {
    assert((page_size & (page_size - 1)) == 0);
  }

  ~posixmmapfile() {
    if (fd_ >= 0) {
      posixmmapfile::close();
    }
  }

  virtual status writeat(uint64_t offset, const slice& data) {
    uint64_t end = offset + data.size();
    const char* src = data.data();
    uint64_t left = data.size();
    while (left > 0) {
      mmapsegment* seg = getsegment(offset);
      if (!seg) {
        return ioerror(filename_, errno);
      }

      assert(offset >= seg->file_offset_);
      assert(offset < seg->file_offset_ + seg->size_);
      uint64_t local_offset = offset  - seg->file_offset_;
      uint64_t avail = seg->size_ - local_offset;
      uint64_t n = (left <= avail) ? left : avail;
      memcpy(seg->base_ + local_offset, src, n);
      src += n;
      left -= n;
      offset += n;
      uint64_t written = __sync_add_and_fetch(&seg->written_, n);

      if (!releasesegment(seg, written == seg->size_)) {
        return ioerror(filename_, errno);
      }
    }
    uint64_t old_end = end;
    do {
      old_end = __sync_val_compare_and_swap(&end_offset_, old_end, end);
    } while (old_end < end);
    return status::ok();
  }

  virtual status append(const slice& data) {
    uint64_t offset = __sync_val_compare_and_swap(&end_offset_, 0, 0);
    return writeat(offset, data);
  }

  virtual status close() {
    status s;
    while (segments_) {
      mmapsegment* seg = segments_;
      segments_ = seg->next_;
      if (munmap(seg->base_, seg->size_) < 0) {
        s = ioerror(filename_, errno);
      }
      seg->base_ = null;
      delete seg;
    }

    if (ftruncate(fd_, end_offset_) < 0) {
      s = ioerror(filename_, errno);
    }

    if (close(fd_) < 0) {
      if (s.ok()) {
        s = ioerror(filename_, errno);
      }
    }

    fd_ = -1;
    return s;
  }

  status syncdirifmanifest() {
    const char* f = filename_.c_str();
    const char* sep = strrchr(f, '/');
    slice basename;
    std::string dir;
    if (sep == null) {
      dir = ".";
      basename = f;
    } else {
      dir = std::string(f, sep - f);
      basename = sep + 1;
    }
    status s;
    if (basename.starts_with("manifest")) {
      int fd = open(dir.c_str(), o_rdonly);
      if (fd < 0) {
        s = ioerror(dir, errno);
      } else {
        if (fsync(fd) < 0) {
          s = ioerror(dir, errno);
        }
        close(fd);
      }
    }
    return s;
  }

  virtual status sync() {
    // ensure new files referred to by the manifest are in the filesystem.
    status s = syncdirifmanifest();
    bool need_sync = false;

    if (!s.ok()) {
      return s;
    }

    {
      mutexlock l(&mtx_);
      need_sync = sync_offset_ != end_offset_;
      sync_offset_ = end_offset_;
    }

    if (need_sync) {
      // some unmapped data was not synced
      if (fdatasync(fd_) < 0) {
        s = ioerror(filename_, errno);
      }
    }

    return s;
  }
};

static int lockorunlock(int fd, bool lock) {
  errno = 0;
  struct flock f;
  memset(&f, 0, sizeof(f));
  f.l_type = (lock ? f_wrlck : f_unlck);
  f.l_whence = seek_set;
  f.l_start = 0;
  f.l_len = 0;        // lock/unlock entire file
  return fcntl(fd, f_setlk, &f);
}

class posixfilelock : public filelock {
 public:
  int fd_;
  std::string name_;
};

// set of locked files.  we keep a separate set instead of just
// relying on fcntrl(f_setlk) since fcntl(f_setlk) does not provide
// any protection against multiple uses from the same process.
class posixlocktable {
 private:
  port::mutex mu_;
  std::set<std::string> locked_files_;
 public:
  bool insert(const std::string& fname) {
    mutexlock l(&mu_);
    return locked_files_.insert(fname).second;
  }
  void remove(const std::string& fname) {
    mutexlock l(&mu_);
    locked_files_.erase(fname);
  }
};

class posixenv : public env {
 public:
  posixenv();
  virtual ~posixenv() {
    fprintf(stderr, "destroying env::default()\n");
    abort();
  }

  virtual status newsequentialfile(const std::string& fname,
                                   sequentialfile** result) {
    file* f = fopen(fname.c_str(), "r");
    if (f == null) {
      *result = null;
      return ioerror(fname, errno);
    } else {
      *result = new posixsequentialfile(fname, f);
      return status::ok();
    }
  }

  virtual status newrandomaccessfile(const std::string& fname,
                                     randomaccessfile** result) {
    *result = null;
    status s;
    int fd = open(fname.c_str(), o_rdonly);
    if (fd < 0) {
      s = ioerror(fname, errno);
    } else if (mmap_limit_.acquire()) {
      uint64_t size;
      s = getfilesize(fname, &size);
      if (s.ok()) {
        void* base = mmap(null, size, prot_read, map_shared, fd, 0);
        if (base != map_failed) {
          *result = new posixmmapreadablefile(fname, base, size, &mmap_limit_);
        } else {
          s = ioerror(fname, errno);
        }
      }
      close(fd);
      if (!s.ok()) {
        mmap_limit_.release();
      }
    } else {
      *result = new posixrandomaccessfile(fname, fd);
    }
    return s;
  }

  virtual status newwritablefile(const std::string& fname,
                                 writablefile** result) {
    status s;
    const int fd = open(fname.c_str(), o_creat | o_rdwr | o_trunc, 0644);
    if (fd < 0) {
      *result = null;
      s = ioerror(fname, errno);
    } else {
      *result = new posixmmapfile(fname, fd, page_size_);
    }
    return s;
  }

  virtual bool fileexists(const std::string& fname) {
    return access(fname.c_str(), f_ok) == 0;
  }

  virtual status getchildren(const std::string& dir,
                             std::vector<std::string>* result) {
    result->clear();
    dir* d = opendir(dir.c_str());
    if (d == null) {
      return ioerror(dir, errno);
    }
    struct dirent* entry;
    while ((entry = readdir(d)) != null) {
      result->push_back(entry->d_name);
    }
    closedir(d);
    return status::ok();
  }

  virtual status deletefile(const std::string& fname) {
    status result;
    if (unlink(fname.c_str()) != 0) {
      result = ioerror(fname, errno);
    }
    return result;
  }

  virtual status createdir(const std::string& name) {
    status result;
    if (mkdir(name.c_str(), 0755) != 0) {
      result = ioerror(name, errno);
    }
    return result;
  }

  virtual status deletedir(const std::string& name) {
    status result;
    if (rmdir(name.c_str()) != 0) {
      result = ioerror(name, errno);
    }
    return result;
  }

  virtual status getfilesize(const std::string& fname, uint64_t* size) {
    status s;
    struct stat sbuf;
    if (stat(fname.c_str(), &sbuf) != 0) {
      *size = 0;
      s = ioerror(fname, errno);
    } else {
      *size = sbuf.st_size;
    }
    return s;
  }

  virtual status renamefile(const std::string& src, const std::string& target) {
    status result;
    if (rename(src.c_str(), target.c_str()) != 0) {
      result = ioerror(src, errno);
    }
    return result;
  }

  virtual status copyfile(const std::string& src, const std::string& target) {
    status result;
    int fd1;
    int fd2;

    if (result.ok() && (fd1 = open(src.c_str(), o_rdonly)) < 0) {
      result = ioerror(src, errno);
    }
    if (result.ok() && (fd2 = open(target.c_str(), o_wronly | o_creat | o_trunc, 0644)) < 0) {
      result = ioerror(target, errno);
    }

    ssize_t amt = 0;
    char buf[512];

    while (result.ok() && (amt = read(fd1, buf, 512)) > 0) {
      if (write(fd2, buf, amt) != amt) {
        result = ioerror(src, errno);
      }
    }

    if (result.ok() && amt < 0) {
      result = ioerror(src, errno);
    }

    if (fd1 >= 0 && close(fd1) < 0) {
      if (result.ok()) {
        result = ioerror(src, errno);
      }
    }

    if (fd2 >= 0 && close(fd2) < 0) {
      if (result.ok()) {
        result = ioerror(target, errno);
      }
    }

    return result;
  }

  virtual status linkfile(const std::string& src, const std::string& target) {
    status result;
    if (link(src.c_str(), target.c_str()) != 0) {
      result = ioerror(src, errno);
    }
    return result;
  }

  virtual status lockfile(const std::string& fname, filelock** lock) {
    *lock = null;
    status result;
    int fd = open(fname.c_str(), o_rdwr | o_creat, 0644);
    if (fd < 0) {
      result = ioerror(fname, errno);
    } else if (!locks_.insert(fname)) {
      close(fd);
      result = status::ioerror("lock " + fname, "already held by process");
    } else if (lockorunlock(fd, true) == -1) {
      result = ioerror("lock " + fname, errno);
      close(fd);
      locks_.remove(fname);
    } else {
      posixfilelock* my_lock = new posixfilelock;
      my_lock->fd_ = fd;
      my_lock->name_ = fname;
      *lock = my_lock;
    }
    return result;
  }

  virtual status unlockfile(filelock* lock) {
    posixfilelock* my_lock = reinterpret_cast<posixfilelock*>(lock);
    status result;
    if (lockorunlock(my_lock->fd_, false) == -1) {
      result = ioerror("unlock", errno);
    }
    locks_.remove(my_lock->name_);
    close(my_lock->fd_);
    delete my_lock;
    return result;
  }

  virtual void schedule(void (*function)(void*), void* arg);

  virtual void startthread(void (*function)(void* arg), void* arg);

  virtual status gettestdirectory(std::string* result) {
    const char* env = getenv("test_tmpdir");
    if (env && env[0] != '\0') {
      *result = env;
    } else {
      char buf[100];
      snprintf(buf, sizeof(buf), "/tmp/leveldbtest-%d", int(geteuid()));
      *result = buf;
    }
    // directory may already exist
    createdir(*result);
    return status::ok();
  }

  static uint64_t gettid() {
    pthread_t tid = pthread_self();
    uint64_t thread_id = 0;
    memcpy(&thread_id, &tid, std::min(sizeof(thread_id), sizeof(tid)));
    return thread_id;
  }

  virtual status newlogger(const std::string& fname, logger** result) {
    file* f = fopen(fname.c_str(), "w");
    if (f == null) {
      *result = null;
      return ioerror(fname, errno);
    } else {
      *result = new posixlogger(f, &posixenv::gettid);
      return status::ok();
    }
  }

  virtual uint64_t nowmicros() {
    struct timeval tv;
    gettimeofday(&tv, null);
    return static_cast<uint64_t>(tv.tv_sec) * 1000000 + tv.tv_usec;
  }

  virtual void sleepformicroseconds(int micros) {
    usleep(micros);
  }

 private:
  void pthreadcall(const char* label, int result) {
    if (result != 0) {
      fprintf(stderr, "pthread %s: %s\n", label, strerror(result));
      abort();
    }
  }

  // bgthread() is the body of the background thread
  void bgthread();
  static void* bgthreadwrapper(void* arg) {
    reinterpret_cast<posixenv*>(arg)->bgthread();
    return null;
  }

  size_t page_size_;
  pthread_mutex_t mu_;
  pthread_cond_t bgsignal_;
  pthread_t bgthread_;
  bool started_bgthread_;

  // entry per schedule() call
  struct bgitem { void* arg; void (*function)(void*); };
  typedef std::deque<bgitem> bgqueue;
  bgqueue queue_;

  posixlocktable locks_;
  mmaplimiter mmap_limit_;
};

posixenv::posixenv() : page_size_(getpagesize()),
                       started_bgthread_(false) {
  pthreadcall("mutex_init", pthread_mutex_init(&mu_, null));
  pthreadcall("cvar_init", pthread_cond_init(&bgsignal_, null));
}

void posixenv::schedule(void (*function)(void*), void* arg) {
  pthreadcall("lock", pthread_mutex_lock(&mu_));

  // start background thread if necessary
  if (!started_bgthread_) {
    started_bgthread_ = true;
    pthreadcall(
        "create thread",
        pthread_create(&bgthread_, null,  &posixenv::bgthreadwrapper, this));
  }

  // if the queue is currently empty, the background thread may currently be
  // waiting.
  if (queue_.empty()) {
    pthreadcall("signal", pthread_cond_signal(&bgsignal_));
  }

  // add to priority queue
  queue_.push_back(bgitem());
  queue_.back().function = function;
  queue_.back().arg = arg;

  pthreadcall("unlock", pthread_mutex_unlock(&mu_));
}

void posixenv::bgthread() {
  while (true) {
    // wait until there is an item that is ready to run
    pthreadcall("lock", pthread_mutex_lock(&mu_));
    while (queue_.empty()) {
      pthreadcall("wait", pthread_cond_wait(&bgsignal_, &mu_));
    }

    void (*function)(void*) = queue_.front().function;
    void* arg = queue_.front().arg;
    queue_.pop_front();

    pthreadcall("unlock", pthread_mutex_unlock(&mu_));
    (*function)(arg);
  }
}

namespace {
struct startthreadstate {
  void (*user_function)(void*);
  void* arg;
};
}
static void* startthreadwrapper(void* arg) {
  startthreadstate* state = reinterpret_cast<startthreadstate*>(arg);
  state->user_function(state->arg);
  delete state;
  return null;
}

void posixenv::startthread(void (*function)(void* arg), void* arg) {
  pthread_t t;
  startthreadstate* state = new startthreadstate;
  state->user_function = function;
  state->arg = arg;
  pthreadcall("start thread",
              pthread_create(&t, null,  &startthreadwrapper, state));
}

}  // namespace

static pthread_once_t onceposix = pthread_once_init;
static env* default_env;
static void initdefaultenv() { default_env = new posixenv; }

env* env::default() {
  pthread_once(&onceposix, initdefaultenv);
  return default_env;
}

}  // namespace hyperleveldb
