//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
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
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#ifdef os_linux
#include <sys/statfs.h>
#include <sys/syscall.h>
#endif
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#if defined(os_linux)
#include <linux/fs.h>
#endif
#include <signal.h>
#include <algorithm>
#include "rocksdb/env.h"
#include "rocksdb/slice.h"
#include "port/port.h"
#include "util/coding.h"
#include "util/logging.h"
#include "util/posix_logger.h"
#include "util/random.h"
#include "util/iostats_context_imp.h"
#include "util/rate_limiter.h"

// get nano time for mach systems
#ifdef __mach__
#include <mach/clock.h>
#include <mach/mach.h>
#endif

#if !defined(tmpfs_magic)
#define tmpfs_magic 0x01021994
#endif
#if !defined(xfs_super_magic)
#define xfs_super_magic 0x58465342
#endif
#if !defined(ext4_super_magic)
#define ext4_super_magic 0xef53
#endif

// for non linux platform, the following macros are used only as place
// holder.
#ifndef os_linux
#define posix_fadv_normal 0 /* [mc1] no further special treatment */
#define posix_fadv_random 1 /* [mc1] expect random page refs */
#define posix_fadv_sequential 2 /* [mc1] expect sequential page refs */
#define posix_fadv_willneed 3 /* [mc1] will need these pages */
#define posix_fadv_dontneed 4 /* [mc1] dont need these pages */
#endif

// this is only set from db_stress.cc and for testing only.
// if non-zero, kill at various points in source code with probability 1/this
int rocksdb_kill_odds = 0;

namespace rocksdb {

namespace {

// a wrapper for fadvise, if the platform doesn't support fadvise,
// it will simply return status::notsupport.
int fadvise(int fd, off_t offset, size_t len, int advice) {
#ifdef os_linux
  return posix_fadvise(fd, offset, len, advice);
#else
  return 0;  // simply do nothing.
#endif
}

// list of pathnames that are locked
static std::set<std::string> lockedfiles;
static port::mutex mutex_lockedfiles;

static status ioerror(const std::string& context, int err_number) {
  return status::ioerror(context, strerror(err_number));
}

#ifdef ndebug
// empty in release build
#define test_kill_random(rocksdb_kill_odds)
#else

// kill the process with probablity 1/odds for testing.
static void testkillrandom(int odds, const std::string& srcfile,
                           int srcline) {
  time_t curtime = time(nullptr);
  random r((uint32_t)curtime);

  assert(odds > 0);
  bool crash = r.onein(odds);
  if (crash) {
    fprintf(stdout, "crashing at %s:%d\n", srcfile.c_str(), srcline);
    fflush(stdout);
    kill(getpid(), sigterm);
  }
}

// to avoid crashing always at some frequently executed codepaths (during
// kill random test), use this factor to reduce odds
#define reduce_odds 2
#define reduce_odds2 4

#define test_kill_random(rocksdb_kill_odds) {   \
  if (rocksdb_kill_odds > 0) { \
    testkillrandom(rocksdb_kill_odds, __file__, __line__);     \
  } \
}

#endif

#if defined(os_linux)
namespace {
  static size_t getuniqueidfromfile(int fd, char* id, size_t max_size) {
    if (max_size < kmaxvarint64length*3) {
      return 0;
    }

    struct stat buf;
    int result = fstat(fd, &buf);
    if (result == -1) {
      return 0;
    }

    long version = 0;
    result = ioctl(fd, fs_ioc_getversion, &version);
    if (result == -1) {
      return 0;
    }
    uint64_t uversion = (uint64_t)version;

    char* rid = id;
    rid = encodevarint64(rid, buf.st_dev);
    rid = encodevarint64(rid, buf.st_ino);
    rid = encodevarint64(rid, uversion);
    assert(rid >= id);
    return static_cast<size_t>(rid-id);
  }
}
#endif

class posixsequentialfile: public sequentialfile {
 private:
  std::string filename_;
  file* file_;
  int fd_;
  bool use_os_buffer_;

 public:
  posixsequentialfile(const std::string& fname, file* f,
      const envoptions& options)
      : filename_(fname), file_(f), fd_(fileno(f)),
        use_os_buffer_(options.use_os_buffer) {
  }
  virtual ~posixsequentialfile() { fclose(file_); }

  virtual status read(size_t n, slice* result, char* scratch) {
    status s;
    size_t r = 0;
    do {
      r = fread_unlocked(scratch, 1, n, file_);
    } while (r == 0 && ferror(file_) && errno == eintr);
    iostats_add(bytes_read, r);
    *result = slice(scratch, r);
    if (r < n) {
      if (feof(file_)) {
        // we leave status as ok if we hit the end of the file
        // we also clear the error so that the reads can continue
        // if a new data is written to the file
        clearerr(file_);
      } else {
        // a partial read with an error: return a non-ok status
        s = ioerror(filename_, errno);
      }
    }
    if (!use_os_buffer_) {
      // we need to fadvise away the entire range of pages because
      // we do not want readahead pages to be cached.
      fadvise(fd_, 0, 0, posix_fadv_dontneed); // free os pages
    }
    return s;
  }

  virtual status skip(uint64_t n) {
    if (fseek(file_, n, seek_cur)) {
      return ioerror(filename_, errno);
    }
    return status::ok();
  }

  virtual status invalidatecache(size_t offset, size_t length) {
#ifndef os_linux
    return status::ok();
#else
    // free os pages
    int ret = fadvise(fd_, offset, length, posix_fadv_dontneed);
    if (ret == 0) {
      return status::ok();
    }
    return ioerror(filename_, errno);
#endif
  }
};

// pread() based random-access
class posixrandomaccessfile: public randomaccessfile {
 private:
  std::string filename_;
  int fd_;
  bool use_os_buffer_;

 public:
  posixrandomaccessfile(const std::string& fname, int fd,
                        const envoptions& options)
      : filename_(fname), fd_(fd), use_os_buffer_(options.use_os_buffer) {
    assert(!options.use_mmap_reads || sizeof(void*) < 8);
  }
  virtual ~posixrandomaccessfile() { close(fd_); }

  virtual status read(uint64_t offset, size_t n, slice* result,
                      char* scratch) const {
    status s;
    ssize_t r = -1;
    size_t left = n;
    char* ptr = scratch;
    while (left > 0) {
      r = pread(fd_, ptr, left, static_cast<off_t>(offset));
      if (r <= 0) {
        if (errno == eintr) {
          continue;
        }
        break;
      }
      ptr += r;
      offset += r;
      left -= r;
    }

    iostats_add_if_positive(bytes_read, n - left);
    *result = slice(scratch, (r < 0) ? 0 : n - left);
    if (r < 0) {
      // an error: return a non-ok status
      s = ioerror(filename_, errno);
    }
    if (!use_os_buffer_) {
      // we need to fadvise away the entire range of pages because
      // we do not want readahead pages to be cached.
      fadvise(fd_, 0, 0, posix_fadv_dontneed); // free os pages
    }
    return s;
  }

#ifdef os_linux
  virtual size_t getuniqueid(char* id, size_t max_size) const {
    return getuniqueidfromfile(fd_, id, max_size);
  }
#endif

  virtual void hint(accesspattern pattern) {
    switch(pattern) {
      case normal:
        fadvise(fd_, 0, 0, posix_fadv_normal);
        break;
      case random:
        fadvise(fd_, 0, 0, posix_fadv_random);
        break;
      case sequential:
        fadvise(fd_, 0, 0, posix_fadv_sequential);
        break;
      case willneed:
        fadvise(fd_, 0, 0, posix_fadv_willneed);
        break;
      case dontneed:
        fadvise(fd_, 0, 0, posix_fadv_dontneed);
        break;
      default:
        assert(false);
        break;
    }
  }

  virtual status invalidatecache(size_t offset, size_t length) {
#ifndef os_linux
    return status::ok();
#else
    // free os pages
    int ret = fadvise(fd_, offset, length, posix_fadv_dontneed);
    if (ret == 0) {
      return status::ok();
    }
    return ioerror(filename_, errno);
#endif
  }
};

// mmap() based random-access
class posixmmapreadablefile: public randomaccessfile {
 private:
  int fd_;
  std::string filename_;
  void* mmapped_region_;
  size_t length_;

 public:
  // base[0,length-1] contains the mmapped contents of the file.
  posixmmapreadablefile(const int fd, const std::string& fname,
                        void* base, size_t length,
                        const envoptions& options)
      : fd_(fd), filename_(fname), mmapped_region_(base), length_(length) {
    fd_ = fd_ + 0;  // suppress the warning for used variables
    assert(options.use_mmap_reads);
    assert(options.use_os_buffer);
  }
  virtual ~posixmmapreadablefile() {
    int ret = munmap(mmapped_region_, length_);
    if (ret != 0) {
      fprintf(stdout, "failed to munmap %p length %zu \n",
              mmapped_region_, length_);
    }
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
  virtual status invalidatecache(size_t offset, size_t length) {
#ifndef os_linux
    return status::ok();
#else
    // free os pages
    int ret = fadvise(fd_, offset, length, posix_fadv_dontneed);
    if (ret == 0) {
      return status::ok();
    }
    return ioerror(filename_, errno);
#endif
  }
};

// we preallocate up to an extra megabyte and use memcpy to append new
// data to the file.  this is safe since we either properly close the
// file before reading from it, or for log files, the reading code
// knows enough to skip zero suffixes.
class posixmmapfile : public writablefile {
 private:
  std::string filename_;
  int fd_;
  size_t page_size_;
  size_t map_size_;       // how much extra memory to map at a time
  char* base_;            // the mapped region
  char* limit_;           // limit of the mapped region
  char* dst_;             // where to write next  (in range [base_,limit_])
  char* last_sync_;       // where have we synced up to
  uint64_t file_offset_;  // offset of base_ in file
  // have we done an munmap of unsynced data?
  bool pending_sync_;
#ifdef rocksdb_fallocate_present
  bool fallocate_with_keep_size_;
#endif

  // roundup x to a multiple of y
  static size_t roundup(size_t x, size_t y) {
    return ((x + y - 1) / y) * y;
  }

  size_t truncatetopageboundary(size_t s) {
    s -= (s & (page_size_ - 1));
    assert((s % page_size_) == 0);
    return s;
  }

  bool unmapcurrentregion() {
    bool result = true;
    test_kill_random(rocksdb_kill_odds);
    if (base_ != nullptr) {
      if (last_sync_ < limit_) {
        // defer syncing this data until next sync() call, if any
        pending_sync_ = true;
      }
      if (munmap(base_, limit_ - base_) != 0) {
        result = false;
      }
      file_offset_ += limit_ - base_;
      base_ = nullptr;
      limit_ = nullptr;
      last_sync_ = nullptr;
      dst_ = nullptr;

      // increase the amount we map the next time, but capped at 1mb
      if (map_size_ < (1<<20)) {
        map_size_ *= 2;
      }
    }
    return result;
  }

  status mapnewregion() {
#ifdef rocksdb_fallocate_present
    assert(base_ == nullptr);

    test_kill_random(rocksdb_kill_odds);
    // we can't fallocate with falloc_fl_keep_size here
    int alloc_status = fallocate(fd_, 0, file_offset_, map_size_);
    if (alloc_status != 0) {
      // fallback to posix_fallocate
      alloc_status = posix_fallocate(fd_, file_offset_, map_size_);
    }
    if (alloc_status != 0) {
      return status::ioerror("error allocating space to file : " + filename_ +
        "error : " + strerror(alloc_status));
    }

    test_kill_random(rocksdb_kill_odds);
    void* ptr = mmap(nullptr, map_size_, prot_read | prot_write, map_shared,
                     fd_, file_offset_);
    if (ptr == map_failed) {
      return status::ioerror("mmap failed on " + filename_);
    }

    test_kill_random(rocksdb_kill_odds);

    base_ = reinterpret_cast<char*>(ptr);
    limit_ = base_ + map_size_;
    dst_ = base_;
    last_sync_ = base_;
    return status::ok();
#else
    return status::notsupported("this platform doesn't support fallocate()");
#endif
  }

 public:
  posixmmapfile(const std::string& fname, int fd, size_t page_size,
                const envoptions& options)
      : filename_(fname),
        fd_(fd),
        page_size_(page_size),
        map_size_(roundup(65536, page_size)),
        base_(nullptr),
        limit_(nullptr),
        dst_(nullptr),
        last_sync_(nullptr),
        file_offset_(0),
        pending_sync_(false) {
#ifdef rocksdb_fallocate_present
    fallocate_with_keep_size_ = options.fallocate_with_keep_size;
#endif
    assert((page_size & (page_size - 1)) == 0);
    assert(options.use_mmap_writes);
  }


  ~posixmmapfile() {
    if (fd_ >= 0) {
      posixmmapfile::close();
    }
  }

  virtual status append(const slice& data) {
    const char* src = data.data();
    size_t left = data.size();
    test_kill_random(rocksdb_kill_odds * reduce_odds);
    preparewrite(getfilesize(), left);
    while (left > 0) {
      assert(base_ <= dst_);
      assert(dst_ <= limit_);
      size_t avail = limit_ - dst_;
      if (avail == 0) {
        if (unmapcurrentregion()) {
          status s = mapnewregion();
          if (!s.ok()) {
            return s;
          }
          test_kill_random(rocksdb_kill_odds);
        }
      }

      size_t n = (left <= avail) ? left : avail;
      memcpy(dst_, src, n);
      iostats_add(bytes_written, n);
      dst_ += n;
      src += n;
      left -= n;
    }
    test_kill_random(rocksdb_kill_odds);
    return status::ok();
  }

  virtual status close() {
    status s;
    size_t unused = limit_ - dst_;

    test_kill_random(rocksdb_kill_odds);

    if (!unmapcurrentregion()) {
      s = ioerror(filename_, errno);
    } else if (unused > 0) {
      // trim the extra space at the end of the file
      if (ftruncate(fd_, file_offset_ - unused) < 0) {
        s = ioerror(filename_, errno);
      }
    }

    test_kill_random(rocksdb_kill_odds);

    if (close(fd_) < 0) {
      if (s.ok()) {
        s = ioerror(filename_, errno);
      }
    }

    fd_ = -1;
    base_ = nullptr;
    limit_ = nullptr;
    return s;
  }

  virtual status flush() {
    test_kill_random(rocksdb_kill_odds);
    return status::ok();
  }

  virtual status sync() {
    status s;

    if (pending_sync_) {
      // some unmapped data was not synced
      test_kill_random(rocksdb_kill_odds);
      pending_sync_ = false;
      if (fdatasync(fd_) < 0) {
        s = ioerror(filename_, errno);
      }
      test_kill_random(rocksdb_kill_odds * reduce_odds);
    }

    if (dst_ > last_sync_) {
      // find the beginnings of the pages that contain the first and last
      // bytes to be synced.
      size_t p1 = truncatetopageboundary(last_sync_ - base_);
      size_t p2 = truncatetopageboundary(dst_ - base_ - 1);
      last_sync_ = dst_;
      test_kill_random(rocksdb_kill_odds);
      if (msync(base_ + p1, p2 - p1 + page_size_, ms_sync) < 0) {
        s = ioerror(filename_, errno);
      }
      test_kill_random(rocksdb_kill_odds);
    }

    return s;
  }

  /**
   * flush data as well as metadata to stable storage.
   */
  virtual status fsync() {
    if (pending_sync_) {
      // some unmapped data was not synced
      test_kill_random(rocksdb_kill_odds);
      pending_sync_ = false;
      if (fsync(fd_) < 0) {
        return ioerror(filename_, errno);
      }
      test_kill_random(rocksdb_kill_odds);
    }
    // this invocation to sync will not issue the call to
    // fdatasync because pending_sync_ has already been cleared.
    return sync();
  }

  /**
   * get the size of valid data in the file. this will not match the
   * size that is returned from the filesystem because we use mmap
   * to extend file by map_size every time.
   */
  virtual uint64_t getfilesize() {
    size_t used = dst_ - base_;
    return file_offset_ + used;
  }

  virtual status invalidatecache(size_t offset, size_t length) {
#ifndef os_linux
    return status::ok();
#else
    // free os pages
    int ret = fadvise(fd_, offset, length, posix_fadv_dontneed);
    if (ret == 0) {
      return status::ok();
    }
    return ioerror(filename_, errno);
#endif
  }

#ifdef rocksdb_fallocate_present
  virtual status allocate(off_t offset, off_t len) {
    test_kill_random(rocksdb_kill_odds);
    int alloc_status = fallocate(
        fd_, fallocate_with_keep_size_ ? falloc_fl_keep_size : 0, offset, len);
    if (alloc_status == 0) {
      return status::ok();
    } else {
      return ioerror(filename_, errno);
    }
  }
#endif
};

// use posix write to write data to a file.
class posixwritablefile : public writablefile {
 private:
  const std::string filename_;
  int fd_;
  size_t cursize_;      // current size of cached data in buf_
  size_t capacity_;     // max size of buf_
  unique_ptr<char[]> buf_;           // a buffer to cache writes
  uint64_t filesize_;
  bool pending_sync_;
  bool pending_fsync_;
  uint64_t last_sync_size_;
  uint64_t bytes_per_sync_;
#ifdef rocksdb_fallocate_present
  bool fallocate_with_keep_size_;
#endif
  ratelimiter* rate_limiter_;

 public:
  posixwritablefile(const std::string& fname, int fd, size_t capacity,
                    const envoptions& options)
      : filename_(fname),
        fd_(fd),
        cursize_(0),
        capacity_(capacity),
        buf_(new char[capacity]),
        filesize_(0),
        pending_sync_(false),
        pending_fsync_(false),
        last_sync_size_(0),
        bytes_per_sync_(options.bytes_per_sync),
        rate_limiter_(options.rate_limiter) {
#ifdef rocksdb_fallocate_present
    fallocate_with_keep_size_ = options.fallocate_with_keep_size;
#endif
    assert(!options.use_mmap_writes);
  }

  ~posixwritablefile() {
    if (fd_ >= 0) {
      posixwritablefile::close();
    }
  }

  virtual status append(const slice& data) {
    const char* src = data.data();
    size_t left = data.size();
    status s;
    pending_sync_ = true;
    pending_fsync_ = true;

    test_kill_random(rocksdb_kill_odds * reduce_odds2);

    preparewrite(getfilesize(), left);
    // if there is no space in the cache, then flush
    if (cursize_ + left > capacity_) {
      s = flush();
      if (!s.ok()) {
        return s;
      }
      // increase the buffer size, but capped at 1mb
      if (capacity_ < (1<<20)) {
        capacity_ *= 2;
        buf_.reset(new char[capacity_]);
      }
      assert(cursize_ == 0);
    }

    // if the write fits into the cache, then write to cache
    // otherwise do a write() syscall to write to os buffers.
    if (cursize_ + left <= capacity_) {
      memcpy(buf_.get()+cursize_, src, left);
      cursize_ += left;
    } else {
      while (left != 0) {
        ssize_t done = write(fd_, src, requesttoken(left));
        if (done < 0) {
          if (errno == eintr) {
            continue;
          }
          return ioerror(filename_, errno);
        }
        iostats_add(bytes_written, done);
        test_kill_random(rocksdb_kill_odds);

        left -= done;
        src += done;
      }
    }
    filesize_ += data.size();
    return status::ok();
  }

  virtual status close() {
    status s;
    s = flush(); // flush cache to os
    if (!s.ok()) {
      return s;
    }

    test_kill_random(rocksdb_kill_odds);

    size_t block_size;
    size_t last_allocated_block;
    getpreallocationstatus(&block_size, &last_allocated_block);
    if (last_allocated_block > 0) {
      // trim the extra space preallocated at the end of the file
      int dummy __attribute__((unused));
      dummy = ftruncate(fd_, filesize_);  // ignore errors
    }

    if (close(fd_) < 0) {
      if (s.ok()) {
        s = ioerror(filename_, errno);
      }
    }
    fd_ = -1;
    return s;
  }

  // write out the cached data to the os cache
  virtual status flush() {
    test_kill_random(rocksdb_kill_odds * reduce_odds2);
    size_t left = cursize_;
    char* src = buf_.get();
    while (left != 0) {
      ssize_t done = write(fd_, src, requesttoken(left));
      if (done < 0) {
        if (errno == eintr) {
          continue;
        }
        return ioerror(filename_, errno);
      }
      iostats_add(bytes_written, done);
      test_kill_random(rocksdb_kill_odds * reduce_odds2);
      left -= done;
      src += done;
    }
    cursize_ = 0;

    // sync os cache to disk for every bytes_per_sync_
    // todo: give log file and sst file different options (log
    // files could be potentially cached in os for their whole
    // life time, thus we might not want to flush at all).
    if (bytes_per_sync_ &&
        filesize_ - last_sync_size_ >= bytes_per_sync_) {
      rangesync(last_sync_size_, filesize_ - last_sync_size_);
      last_sync_size_ = filesize_;
    }

    return status::ok();
  }

  virtual status sync() {
    status s = flush();
    if (!s.ok()) {
      return s;
    }
    test_kill_random(rocksdb_kill_odds);
    if (pending_sync_ && fdatasync(fd_) < 0) {
      return ioerror(filename_, errno);
    }
    test_kill_random(rocksdb_kill_odds);
    pending_sync_ = false;
    return status::ok();
  }

  virtual status fsync() {
    status s = flush();
    if (!s.ok()) {
      return s;
    }
    test_kill_random(rocksdb_kill_odds);
    if (pending_fsync_ && fsync(fd_) < 0) {
      return ioerror(filename_, errno);
    }
    test_kill_random(rocksdb_kill_odds);
    pending_fsync_ = false;
    pending_sync_ = false;
    return status::ok();
  }

  virtual uint64_t getfilesize() {
    return filesize_;
  }

  virtual status invalidatecache(size_t offset, size_t length) {
#ifndef os_linux
    return status::ok();
#else
    // free os pages
    int ret = fadvise(fd_, offset, length, posix_fadv_dontneed);
    if (ret == 0) {
      return status::ok();
    }
    return ioerror(filename_, errno);
#endif
  }

#ifdef rocksdb_fallocate_present
  virtual status allocate(off_t offset, off_t len) {
    test_kill_random(rocksdb_kill_odds);
    int alloc_status = fallocate(
        fd_, fallocate_with_keep_size_ ? falloc_fl_keep_size : 0, offset, len);
    if (alloc_status == 0) {
      return status::ok();
    } else {
      return ioerror(filename_, errno);
    }
  }

  virtual status rangesync(off_t offset, off_t nbytes) {
    if (sync_file_range(fd_, offset, nbytes, sync_file_range_write) == 0) {
      return status::ok();
    } else {
      return ioerror(filename_, errno);
    }
  }
  virtual size_t getuniqueid(char* id, size_t max_size) const {
    return getuniqueidfromfile(fd_, id, max_size);
  }
#endif

 private:
  inline size_t requesttoken(size_t bytes) {
    if (rate_limiter_ && io_priority_ < env::io_total) {
      bytes = std::min(bytes,
          static_cast<size_t>(rate_limiter_->getsingleburstbytes()));
      rate_limiter_->request(bytes, io_priority_);
    }
    return bytes;
  }
};

class posixrandomrwfile : public randomrwfile {
 private:
  const std::string filename_;
  int fd_;
  bool pending_sync_;
  bool pending_fsync_;
#ifdef rocksdb_fallocate_present
  bool fallocate_with_keep_size_;
#endif

 public:
  posixrandomrwfile(const std::string& fname, int fd, const envoptions& options)
      : filename_(fname),
        fd_(fd),
        pending_sync_(false),
        pending_fsync_(false) {
#ifdef rocksdb_fallocate_present
    fallocate_with_keep_size_ = options.fallocate_with_keep_size;
#endif
    assert(!options.use_mmap_writes && !options.use_mmap_reads);
  }

  ~posixrandomrwfile() {
    if (fd_ >= 0) {
      close();
    }
  }

  virtual status write(uint64_t offset, const slice& data) {
    const char* src = data.data();
    size_t left = data.size();
    status s;
    pending_sync_ = true;
    pending_fsync_ = true;

    while (left != 0) {
      ssize_t done = pwrite(fd_, src, left, offset);
      if (done < 0) {
        if (errno == eintr) {
          continue;
        }
        return ioerror(filename_, errno);
      }
      iostats_add(bytes_written, done);

      left -= done;
      src += done;
      offset += done;
    }

    return status::ok();
  }

  virtual status read(uint64_t offset, size_t n, slice* result,
                      char* scratch) const {
    status s;
    ssize_t r = -1;
    size_t left = n;
    char* ptr = scratch;
    while (left > 0) {
      r = pread(fd_, ptr, left, static_cast<off_t>(offset));
      if (r <= 0) {
        if (errno == eintr) {
          continue;
        }
        break;
      }
      ptr += r;
      offset += r;
      left -= r;
    }
    iostats_add_if_positive(bytes_read, n - left);
    *result = slice(scratch, (r < 0) ? 0 : n - left);
    if (r < 0) {
      s = ioerror(filename_, errno);
    }
    return s;
  }

  virtual status close() {
    status s = status::ok();
    if (fd_ >= 0 && close(fd_) < 0) {
      s = ioerror(filename_, errno);
    }
    fd_ = -1;
    return s;
  }

  virtual status sync() {
    if (pending_sync_ && fdatasync(fd_) < 0) {
      return ioerror(filename_, errno);
    }
    pending_sync_ = false;
    return status::ok();
  }

  virtual status fsync() {
    if (pending_fsync_ && fsync(fd_) < 0) {
      return ioerror(filename_, errno);
    }
    pending_fsync_ = false;
    pending_sync_ = false;
    return status::ok();
  }

#ifdef rocksdb_fallocate_present
  virtual status allocate(off_t offset, off_t len) {
    test_kill_random(rocksdb_kill_odds);
    int alloc_status = fallocate(
        fd_, fallocate_with_keep_size_ ? falloc_fl_keep_size : 0, offset, len);
    if (alloc_status == 0) {
      return status::ok();
    } else {
      return ioerror(filename_, errno);
    }
  }
#endif
};

class posixdirectory : public directory {
 public:
  explicit posixdirectory(int fd) : fd_(fd) {}
  ~posixdirectory() {
    close(fd_);
  }

  virtual status fsync() {
    if (fsync(fd_) == -1) {
      return ioerror("directory", errno);
    }
    return status::ok();
  }

 private:
  int fd_;
};

static int lockorunlock(const std::string& fname, int fd, bool lock) {
  mutex_lockedfiles.lock();
  if (lock) {
    // if it already exists in the lockedfiles set, then it is already locked,
    // and fail this lock attempt. otherwise, insert it into lockedfiles.
    // this check is needed because fcntl() does not detect lock conflict
    // if the fcntl is issued by the same thread that earlier acquired
    // this lock.
    if (lockedfiles.insert(fname).second == false) {
      mutex_lockedfiles.unlock();
      errno = enolck;
      return -1;
    }
  } else {
    // if we are unlocking, then verify that we had locked it earlier,
    // it should already exist in lockedfiles. remove it from lockedfiles.
    if (lockedfiles.erase(fname) != 1) {
      mutex_lockedfiles.unlock();
      errno = enolck;
      return -1;
    }
  }
  errno = 0;
  struct flock f;
  memset(&f, 0, sizeof(f));
  f.l_type = (lock ? f_wrlck : f_unlck);
  f.l_whence = seek_set;
  f.l_start = 0;
  f.l_len = 0;        // lock/unlock entire file
  int value = fcntl(fd, f_setlk, &f);
  if (value == -1 && lock) {
    // if there is an error in locking, then remove the pathname from lockedfiles
    lockedfiles.erase(fname);
  }
  mutex_lockedfiles.unlock();
  return value;
}

class posixfilelock : public filelock {
 public:
  int fd_;
  std::string filename;
};

void pthreadcall(const char* label, int result) {
  if (result != 0) {
    fprintf(stderr, "pthread %s: %s\n", label, strerror(result));
    exit(1);
  }
}

class posixenv : public env {
 public:
  posixenv();

  virtual ~posixenv(){
    for (const auto tid : threads_to_join_) {
      pthread_join(tid, nullptr);
    }
  }

  void setfd_cloexec(int fd, const envoptions* options) {
    if ((options == nullptr || options->set_fd_cloexec) && fd > 0) {
      fcntl(fd, f_setfd, fcntl(fd, f_getfd) | fd_cloexec);
    }
  }

  virtual status newsequentialfile(const std::string& fname,
                                   unique_ptr<sequentialfile>* result,
                                   const envoptions& options) {
    result->reset();
    file* f = nullptr;
    do {
      f = fopen(fname.c_str(), "r");
    } while (f == nullptr && errno == eintr);
    if (f == nullptr) {
      *result = nullptr;
      return ioerror(fname, errno);
    } else {
      int fd = fileno(f);
      setfd_cloexec(fd, &options);
      result->reset(new posixsequentialfile(fname, f, options));
      return status::ok();
    }
  }

  virtual status newrandomaccessfile(const std::string& fname,
                                     unique_ptr<randomaccessfile>* result,
                                     const envoptions& options) {
    result->reset();
    status s;
    int fd = open(fname.c_str(), o_rdonly);
    setfd_cloexec(fd, &options);
    if (fd < 0) {
      s = ioerror(fname, errno);
    } else if (options.use_mmap_reads && sizeof(void*) >= 8) {
      // use of mmap for random reads has been removed because it
      // kills performance when storage is fast.
      // use mmap when virtual address-space is plentiful.
      uint64_t size;
      s = getfilesize(fname, &size);
      if (s.ok()) {
        void* base = mmap(nullptr, size, prot_read, map_shared, fd, 0);
        if (base != map_failed) {
          result->reset(new posixmmapreadablefile(fd, fname, base,
                                                  size, options));
        } else {
          s = ioerror(fname, errno);
        }
      }
      close(fd);
    } else {
      result->reset(new posixrandomaccessfile(fname, fd, options));
    }
    return s;
  }

  virtual status newwritablefile(const std::string& fname,
                                 unique_ptr<writablefile>* result,
                                 const envoptions& options) {
    result->reset();
    status s;
    int fd = -1;
    do {
      fd = open(fname.c_str(), o_creat | o_rdwr | o_trunc, 0644);
    } while (fd < 0 && errno == eintr);
    if (fd < 0) {
      s = ioerror(fname, errno);
    } else {
      setfd_cloexec(fd, &options);
      if (options.use_mmap_writes) {
        if (!checkeddiskformmap_) {
          // this will be executed once in the program's lifetime.
          // do not use mmapwrite on non ext-3/xfs/tmpfs systems.
          if (!supportsfastallocate(fname)) {
            forcemmapoff = true;
          }
          checkeddiskformmap_ = true;
        }
      }
      if (options.use_mmap_writes && !forcemmapoff) {
        result->reset(new posixmmapfile(fname, fd, page_size_, options));
      } else {
        // disable mmap writes
        envoptions no_mmap_writes_options = options;
        no_mmap_writes_options.use_mmap_writes = false;

        result->reset(
            new posixwritablefile(fname, fd, 65536, no_mmap_writes_options)
        );
      }
    }
    return s;
  }

  virtual status newrandomrwfile(const std::string& fname,
                                 unique_ptr<randomrwfile>* result,
                                 const envoptions& options) {
    result->reset();
    // no support for mmap yet
    if (options.use_mmap_writes || options.use_mmap_reads) {
      return status::notsupported("no support for mmap read/write yet");
    }
    status s;
    const int fd = open(fname.c_str(), o_creat | o_rdwr, 0644);
    if (fd < 0) {
      s = ioerror(fname, errno);
    } else {
      setfd_cloexec(fd, &options);
      result->reset(new posixrandomrwfile(fname, fd, options));
    }
    return s;
  }

  virtual status newdirectory(const std::string& name,
                              unique_ptr<directory>* result) {
    result->reset();
    const int fd = open(name.c_str(), 0);
    if (fd < 0) {
      return ioerror(name, errno);
    } else {
      result->reset(new posixdirectory(fd));
    }
    return status::ok();
  }

  virtual bool fileexists(const std::string& fname) {
    return access(fname.c_str(), f_ok) == 0;
  }

  virtual status getchildren(const std::string& dir,
                             std::vector<std::string>* result) {
    result->clear();
    dir* d = opendir(dir.c_str());
    if (d == nullptr) {
      return ioerror(dir, errno);
    }
    struct dirent* entry;
    while ((entry = readdir(d)) != nullptr) {
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
  };

  virtual status createdir(const std::string& name) {
    status result;
    if (mkdir(name.c_str(), 0755) != 0) {
      result = ioerror(name, errno);
    }
    return result;
  };

  virtual status createdirifmissing(const std::string& name) {
    status result;
    if (mkdir(name.c_str(), 0755) != 0) {
      if (errno != eexist) {
        result = ioerror(name, errno);
      } else if (!direxists(name)) { // check that name is actually a
                                     // directory.
        // message is taken from mkdir
        result = status::ioerror("`"+name+"' exists but is not a directory");
      }
    }
    return result;
  };

  virtual status deletedir(const std::string& name) {
    status result;
    if (rmdir(name.c_str()) != 0) {
      result = ioerror(name, errno);
    }
    return result;
  };

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

  virtual status getfilemodificationtime(const std::string& fname,
                                         uint64_t* file_mtime) {
    struct stat s;
    if (stat(fname.c_str(), &s) !=0) {
      return ioerror(fname, errno);
    }
    *file_mtime = static_cast<uint64_t>(s.st_mtime);
    return status::ok();
  }
  virtual status renamefile(const std::string& src, const std::string& target) {
    status result;
    if (rename(src.c_str(), target.c_str()) != 0) {
      result = ioerror(src, errno);
    }
    return result;
  }

  virtual status lockfile(const std::string& fname, filelock** lock) {
    *lock = nullptr;
    status result;
    int fd = open(fname.c_str(), o_rdwr | o_creat, 0644);
    if (fd < 0) {
      result = ioerror(fname, errno);
    } else if (lockorunlock(fname, fd, true) == -1) {
      result = ioerror("lock " + fname, errno);
      close(fd);
    } else {
      setfd_cloexec(fd, nullptr);
      posixfilelock* my_lock = new posixfilelock;
      my_lock->fd_ = fd;
      my_lock->filename = fname;
      *lock = my_lock;
    }
    return result;
  }

  virtual status unlockfile(filelock* lock) {
    posixfilelock* my_lock = reinterpret_cast<posixfilelock*>(lock);
    status result;
    if (lockorunlock(my_lock->filename, my_lock->fd_, false) == -1) {
      result = ioerror("unlock", errno);
    }
    close(my_lock->fd_);
    delete my_lock;
    return result;
  }

  virtual void schedule(void (*function)(void*), void* arg, priority pri = low);

  virtual void startthread(void (*function)(void* arg), void* arg);

  virtual void waitforjoin();

  virtual unsigned int getthreadpoolqueuelen(priority pri = low) const override;

  virtual status gettestdirectory(std::string* result) {
    const char* env = getenv("test_tmpdir");
    if (env && env[0] != '\0') {
      *result = env;
    } else {
      char buf[100];
      snprintf(buf, sizeof(buf), "/tmp/rocksdbtest-%d", int(geteuid()));
      *result = buf;
    }
    // directory may already exist
    createdir(*result);
    return status::ok();
  }

  static uint64_t gettid(pthread_t tid) {
    uint64_t thread_id = 0;
    memcpy(&thread_id, &tid, std::min(sizeof(thread_id), sizeof(tid)));
    return thread_id;
  }

  static uint64_t gettid() {
    pthread_t tid = pthread_self();
    return gettid(tid);
  }

  virtual status newlogger(const std::string& fname,
                           shared_ptr<logger>* result) {
    file* f = fopen(fname.c_str(), "w");
    if (f == nullptr) {
      result->reset();
      return ioerror(fname, errno);
    } else {
      int fd = fileno(f);
      setfd_cloexec(fd, nullptr);
      result->reset(new posixlogger(f, &posixenv::gettid, this));
      return status::ok();
    }
  }

  virtual uint64_t nowmicros() {
    struct timeval tv;
    // todo(kailiu) mac don't have this
    gettimeofday(&tv, nullptr);
    return static_cast<uint64_t>(tv.tv_sec) * 1000000 + tv.tv_usec;
  }

  virtual uint64_t nownanos() {
#ifdef os_linux
    struct timespec ts;
    clock_gettime(clock_monotonic, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000000000 + ts.tv_nsec;
#elif __mach__
    clock_serv_t cclock;
    mach_timespec_t ts;
    host_get_clock_service(mach_host_self(), calendar_clock, &cclock);
    clock_get_time(cclock, &ts);
    mach_port_deallocate(mach_task_self(), cclock);
#endif
    return static_cast<uint64_t>(ts.tv_sec) * 1000000000 + ts.tv_nsec;
  }

  virtual void sleepformicroseconds(int micros) {
    usleep(micros);
  }

  virtual status gethostname(char* name, uint64_t len) {
    int ret = gethostname(name, len);
    if (ret < 0) {
      if (errno == efault || errno == einval)
        return status::invalidargument(strerror(errno));
      else
        return ioerror("gethostname", errno);
    }
    return status::ok();
  }

  virtual status getcurrenttime(int64_t* unix_time) {
    time_t ret = time(nullptr);
    if (ret == (time_t) -1) {
      return ioerror("getcurrenttime", errno);
    }
    *unix_time = (int64_t) ret;
    return status::ok();
  }

  virtual status getabsolutepath(const std::string& db_path,
      std::string* output_path) {
    if (db_path.find('/') == 0) {
      *output_path = db_path;
      return status::ok();
    }

    char the_path[256];
    char* ret = getcwd(the_path, 256);
    if (ret == nullptr) {
      return status::ioerror(strerror(errno));
    }

    *output_path = ret;
    return status::ok();
  }

  // allow increasing the number of worker threads.
  virtual void setbackgroundthreads(int num, priority pri) {
    assert(pri >= priority::low && pri <= priority::high);
    thread_pools_[pri].setbackgroundthreads(num);
  }

  virtual void lowerthreadpooliopriority(priority pool = low) override {
    assert(pool >= priority::low && pool <= priority::high);
#ifdef os_linux
    thread_pools_[pool].loweriopriority();
#endif
  }

  virtual std::string timetostring(uint64_t secondssince1970) {
    const time_t seconds = (time_t)secondssince1970;
    struct tm t;
    int maxsize = 64;
    std::string dummy;
    dummy.reserve(maxsize);
    dummy.resize(maxsize);
    char* p = &dummy[0];
    localtime_r(&seconds, &t);
    snprintf(p, maxsize,
             "%04d/%02d/%02d-%02d:%02d:%02d ",
             t.tm_year + 1900,
             t.tm_mon + 1,
             t.tm_mday,
             t.tm_hour,
             t.tm_min,
             t.tm_sec);
    return dummy;
  }

  envoptions optimizeforlogwrite(const envoptions& env_options) const {
    envoptions optimized = env_options;
    optimized.use_mmap_writes = false;
    // todo(icanadi) it's faster if fallocate_with_keep_size is false, but it
    // breaks transactionlogiteratorstallatlastrecord unit test. fix the unit
    // test and make this false
    optimized.fallocate_with_keep_size = true;
    return optimized;
  }

  envoptions optimizeformanifestwrite(const envoptions& env_options) const {
    envoptions optimized = env_options;
    optimized.use_mmap_writes = false;
    optimized.fallocate_with_keep_size = true;
    return optimized;
  }

 private:
  bool checkeddiskformmap_;
  bool forcemmapoff; // do we override env options?


  // returns true iff the named directory exists and is a directory.
  virtual bool direxists(const std::string& dname) {
    struct stat statbuf;
    if (stat(dname.c_str(), &statbuf) == 0) {
      return s_isdir(statbuf.st_mode);
    }
    return false; // stat() failed return false
  }

  bool supportsfastallocate(const std::string& path) {
#ifdef rocksdb_fallocate_present
    struct statfs s;
    if (statfs(path.c_str(), &s)){
      return false;
    }
    switch (s.f_type) {
      case ext4_super_magic:
        return true;
      case xfs_super_magic:
        return true;
      case tmpfs_magic:
        return true;
      default:
        return false;
    }
#else
    return false;
#endif
  }

  size_t page_size_;


  class threadpool {
   public:
    threadpool()
        : total_threads_limit_(1),
          bgthreads_(0),
          queue_(),
          queue_len_(0),
          exit_all_threads_(false),
          low_io_priority_(false) {
      pthreadcall("mutex_init", pthread_mutex_init(&mu_, nullptr));
      pthreadcall("cvar_init", pthread_cond_init(&bgsignal_, nullptr));
    }

    ~threadpool() {
      pthreadcall("lock", pthread_mutex_lock(&mu_));
      assert(!exit_all_threads_);
      exit_all_threads_ = true;
      pthreadcall("signalall", pthread_cond_broadcast(&bgsignal_));
      pthreadcall("unlock", pthread_mutex_unlock(&mu_));
      for (const auto tid : bgthreads_) {
        pthread_join(tid, nullptr);
      }
    }

    void loweriopriority() {
#ifdef os_linux
      pthreadcall("lock", pthread_mutex_lock(&mu_));
      low_io_priority_ = true;
      pthreadcall("unlock", pthread_mutex_unlock(&mu_));
#endif
    }

    // return true if there is at least one thread needs to terminate.
    bool hasexcessivethread() {
      return static_cast<int>(bgthreads_.size()) > total_threads_limit_;
    }

    // return true iff the current thread is the excessive thread to terminate.
    // always terminate the running thread that is added last, even if there are
    // more than one thread to terminate.
    bool islastexcessivethread(size_t thread_id) {
      return hasexcessivethread() && thread_id == bgthreads_.size() - 1;
    }

    // is one of the threads to terminate.
    bool isexcessivethread(size_t thread_id) {
      return static_cast<int>(thread_id) >= total_threads_limit_;
    }

    void bgthread(size_t thread_id) {
      bool low_io_priority = false;
      while (true) {
        // wait until there is an item that is ready to run
        pthreadcall("lock", pthread_mutex_lock(&mu_));
        // stop waiting if the thread needs to do work or needs to terminate.
        while (!exit_all_threads_ && !islastexcessivethread(thread_id) &&
               (queue_.empty() || isexcessivethread(thread_id))) {
          pthreadcall("wait", pthread_cond_wait(&bgsignal_, &mu_));
        }
        if (exit_all_threads_) { // mechanism to let bg threads exit safely
          pthreadcall("unlock", pthread_mutex_unlock(&mu_));
          break;
        }
        if (islastexcessivethread(thread_id)) {
          // current thread is the last generated one and is excessive.
          // we always terminate excessive thread in the reverse order of
          // generation time.
          auto terminating_thread = bgthreads_.back();
          pthread_detach(terminating_thread);
          bgthreads_.pop_back();
          if (hasexcessivethread()) {
            // there is still at least more excessive thread to terminate.
            wakeupallthreads();
          }
          pthreadcall("unlock", pthread_mutex_unlock(&mu_));
          // todo(sdong): temp logging. need to help debugging. remove it when
          // the feature is proved to be stable.
          fprintf(stdout, "bg thread %zu terminates %llx\n", thread_id,
                  static_cast<long long unsigned int>(gettid()));
          break;
        }
        void (*function)(void*) = queue_.front().function;
        void* arg = queue_.front().arg;
        queue_.pop_front();
        queue_len_.store(queue_.size(), std::memory_order_relaxed);

        bool decrease_io_priority = (low_io_priority != low_io_priority_);
        pthreadcall("unlock", pthread_mutex_unlock(&mu_));

#ifdef os_linux
        if (decrease_io_priority) {
          #define ioprio_class_shift               (13)
          #define ioprio_prio_value(class, data)   \
              (((class) << ioprio_class_shift) | data)
          // put schedule into ioprio_class_idle class (lowest)
          // these system calls only have an effect when used in conjunction
          // with an i/o scheduler that supports i/o priorities. as at
          // kernel 2.6.17 the only such scheduler is the completely
          // fair queuing (cfq) i/o scheduler.
          // to change scheduler:
          //  echo cfq > /sys/block/<device_name>/queue/schedule
          // tunables to consider:
          //  /sys/block/<device_name>/queue/slice_idle
          //  /sys/block/<device_name>/queue/slice_sync
          syscall(sys_ioprio_set,
                  1,  // ioprio_who_process
                  0,  // current thread
                  ioprio_prio_value(3, 0));
          low_io_priority = true;
        }
#else
        (void)decrease_io_priority; // avoid 'unused variable' error
#endif
        (*function)(arg);
      }
    }

    // helper struct for passing arguments when creating threads.
    struct bgthreadmetadata {
      threadpool* thread_pool_;
      size_t thread_id_;  // thread count in the thread.
      explicit bgthreadmetadata(threadpool* thread_pool, size_t thread_id)
          : thread_pool_(thread_pool), thread_id_(thread_id) {}
    };

    static void* bgthreadwrapper(void* arg) {
      bgthreadmetadata* meta = reinterpret_cast<bgthreadmetadata*>(arg);
      size_t thread_id = meta->thread_id_;
      threadpool* tp = meta->thread_pool_;
      delete meta;
      tp->bgthread(thread_id);
      return nullptr;
    }

    void wakeupallthreads() {
      pthreadcall("signalall", pthread_cond_broadcast(&bgsignal_));
    }

    void setbackgroundthreads(int num) {
      pthreadcall("lock", pthread_mutex_lock(&mu_));
      if (exit_all_threads_) {
        pthreadcall("unlock", pthread_mutex_unlock(&mu_));
        return;
      }
      if (num != total_threads_limit_) {
        total_threads_limit_ = num;
        wakeupallthreads();
        startbgthreads();
      }
      assert(total_threads_limit_ > 0);
      pthreadcall("unlock", pthread_mutex_unlock(&mu_));
    }

    void startbgthreads() {
      // start background thread if necessary
      while ((int)bgthreads_.size() < total_threads_limit_) {
        pthread_t t;
        pthreadcall(
            "create thread",
            pthread_create(&t, nullptr, &threadpool::bgthreadwrapper,
                           new bgthreadmetadata(this, bgthreads_.size())));

        // set the thread name to aid debugging
#if defined(_gnu_source) && defined(__glibc_prereq)
#if __glibc_prereq(2, 12)
        char name_buf[16];
        snprintf(name_buf, sizeof name_buf, "rocksdb:bg%zu", bgthreads_.size());
        name_buf[sizeof name_buf - 1] = '\0';
        pthread_setname_np(t, name_buf);
#endif
#endif

        bgthreads_.push_back(t);
      }
    }

    void schedule(void (*function)(void*), void* arg) {
      pthreadcall("lock", pthread_mutex_lock(&mu_));

      if (exit_all_threads_) {
        pthreadcall("unlock", pthread_mutex_unlock(&mu_));
        return;
      }

      startbgthreads();

      // add to priority queue
      queue_.push_back(bgitem());
      queue_.back().function = function;
      queue_.back().arg = arg;
      queue_len_.store(queue_.size(), std::memory_order_relaxed);

      if (!hasexcessivethread()) {
        // wake up at least one waiting thread.
        pthreadcall("signal", pthread_cond_signal(&bgsignal_));
      } else {
        // need to wake up all threads to make sure the one woken
        // up is not the one to terminate.
        wakeupallthreads();
      }

      pthreadcall("unlock", pthread_mutex_unlock(&mu_));
    }

    unsigned int getqueuelen() const {
      return queue_len_.load(std::memory_order_relaxed);
    }

   private:
    // entry per schedule() call
    struct bgitem { void* arg; void (*function)(void*); };
    typedef std::deque<bgitem> bgqueue;

    pthread_mutex_t mu_;
    pthread_cond_t bgsignal_;
    int total_threads_limit_;
    std::vector<pthread_t> bgthreads_;
    bgqueue queue_;
    std::atomic_uint queue_len_;  // queue length. used for stats reporting
    bool exit_all_threads_;
    bool low_io_priority_;
  };

  std::vector<threadpool> thread_pools_;

  pthread_mutex_t mu_;
  std::vector<pthread_t> threads_to_join_;

};

posixenv::posixenv() : checkeddiskformmap_(false),
                       forcemmapoff(false),
                       page_size_(getpagesize()),
                       thread_pools_(priority::total) {
  pthreadcall("mutex_init", pthread_mutex_init(&mu_, nullptr));
}

void posixenv::schedule(void (*function)(void*), void* arg, priority pri) {
  assert(pri >= priority::low && pri <= priority::high);
  thread_pools_[pri].schedule(function, arg);
}

unsigned int posixenv::getthreadpoolqueuelen(priority pri) const {
  assert(pri >= priority::low && pri <= priority::high);
  return thread_pools_[pri].getqueuelen();
}

struct startthreadstate {
  void (*user_function)(void*);
  void* arg;
};

static void* startthreadwrapper(void* arg) {
  startthreadstate* state = reinterpret_cast<startthreadstate*>(arg);
  state->user_function(state->arg);
  delete state;
  return nullptr;
}

void posixenv::startthread(void (*function)(void* arg), void* arg) {
  pthread_t t;
  startthreadstate* state = new startthreadstate;
  state->user_function = function;
  state->arg = arg;
  pthreadcall("start thread",
              pthread_create(&t, nullptr,  &startthreadwrapper, state));
  pthreadcall("lock", pthread_mutex_lock(&mu_));
  threads_to_join_.push_back(t);
  pthreadcall("unlock", pthread_mutex_unlock(&mu_));
}

void posixenv::waitforjoin() {
  for (const auto tid : threads_to_join_) {
    pthread_join(tid, nullptr);
  }
  threads_to_join_.clear();
}

}  // namespace

std::string env::generateuniqueid() {
  std::string uuid_file = "/proc/sys/kernel/random/uuid";
  if (fileexists(uuid_file)) {
    std::string uuid;
    status s = readfiletostring(this, uuid_file, &uuid);
    if (s.ok()) {
      return uuid;
    }
  }
  // could not read uuid_file - generate uuid using "nanos-random"
  random64 r(time(nullptr));
  uint64_t random_uuid_portion =
    r.uniform(std::numeric_limits<uint64_t>::max());
  uint64_t nanos_uuid_portion = nownanos();
  char uuid2[200];
  snprintf(uuid2,
           200,
           "%lx-%lx",
           (unsigned long)nanos_uuid_portion,
           (unsigned long)random_uuid_portion);
  return uuid2;
}

env* env::default() {
  static posixenv default_env;
  return &default_env;
}

}  // namespace rocksdb
