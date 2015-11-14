//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
#ifdef use_hdfs
#ifndef rocksdb_hdfs_file_c
#define rocksdb_hdfs_file_c

#include <algorithm>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <iostream>
#include <sstream>
#include "rocksdb/env.h"
#include "rocksdb/status.h"
#include "hdfs/env_hdfs.h"

#define hdfs_exists 0
#define hdfs_doesnt_exist -1
#define hdfs_success 0

//
// this file defines an hdfs environment for rocksdb. it uses the libhdfs
// api to access hdfs. all hdfs files created by one instance of rocksdb
// will reside on the same hdfs cluster.
//

namespace rocksdb {

namespace {

// log error message
static status ioerror(const std::string& context, int err_number) {
  return status::ioerror(context, strerror(err_number));
}

// assume that there is one global logger for now. it is not thread-safe,
// but need not be because the logger is initialized at db-open time.
static logger* mylog = nullptr;

// used for reading a file from hdfs. it implements both sequential-read
// access methods as well as random read access methods.
class hdfsreadablefile : virtual public sequentialfile,
                         virtual public randomaccessfile {
 private:
  hdfsfs filesys_;
  std::string filename_;
  hdfsfile hfile_;

 public:
  hdfsreadablefile(hdfsfs filesys, const std::string& fname)
      : filesys_(filesys), filename_(fname), hfile_(nullptr) {
    log(mylog, "[hdfs] hdfsreadablefile opening file %s\n",
        filename_.c_str());
    hfile_ = hdfsopenfile(filesys_, filename_.c_str(), o_rdonly, 0, 0, 0);
    log(mylog, "[hdfs] hdfsreadablefile opened file %s hfile_=0x%p\n",
            filename_.c_str(), hfile_);
  }

  virtual ~hdfsreadablefile() {
    log(mylog, "[hdfs] hdfsreadablefile closing file %s\n",
       filename_.c_str());
    hdfsclosefile(filesys_, hfile_);
    log(mylog, "[hdfs] hdfsreadablefile closed file %s\n",
        filename_.c_str());
    hfile_ = nullptr;
  }

  bool isvalid() {
    return hfile_ != nullptr;
  }

  // sequential access, read data at current offset in file
  virtual status read(size_t n, slice* result, char* scratch) {
    status s;
    log(mylog, "[hdfs] hdfsreadablefile reading %s %ld\n",
        filename_.c_str(), n);

    char* buffer = scratch;
    size_t total_bytes_read = 0;
    tsize bytes_read = 0;
    tsize remaining_bytes = (tsize)n;

    // read a total of n bytes repeatedly until we hit error or eof
    while (remaining_bytes > 0) {
      bytes_read = hdfsread(filesys_, hfile_, buffer, remaining_bytes);
      if (bytes_read <= 0) {
        break;
      }
      assert(bytes_read <= remaining_bytes);

      total_bytes_read += bytes_read;
      remaining_bytes -= bytes_read;
      buffer += bytes_read;
    }
    assert(total_bytes_read <= n);

    log(mylog, "[hdfs] hdfsreadablefile read %s\n", filename_.c_str());

    if (bytes_read < 0) {
      s = ioerror(filename_, errno);
    } else {
      *result = slice(scratch, total_bytes_read);
    }

    return s;
  }

  // random access, read data from specified offset in file
  virtual status read(uint64_t offset, size_t n, slice* result,
                      char* scratch) const {
    status s;
    log(mylog, "[hdfs] hdfsreadablefile preading %s\n", filename_.c_str());
    ssize_t bytes_read = hdfspread(filesys_, hfile_, offset,
                                   (void*)scratch, (tsize)n);
    log(mylog, "[hdfs] hdfsreadablefile pread %s\n", filename_.c_str());
    *result = slice(scratch, (bytes_read < 0) ? 0 : bytes_read);
    if (bytes_read < 0) {
      // an error: return a non-ok status
      s = ioerror(filename_, errno);
    }
    return s;
  }

  virtual status skip(uint64_t n) {
    log(mylog, "[hdfs] hdfsreadablefile skip %s\n", filename_.c_str());
    // get current offset from file
    toffset current = hdfstell(filesys_, hfile_);
    if (current < 0) {
      return ioerror(filename_, errno);
    }
    // seek to new offset in file
    toffset newoffset = current + n;
    int val = hdfsseek(filesys_, hfile_, newoffset);
    if (val < 0) {
      return ioerror(filename_, errno);
    }
    return status::ok();
  }

 private:

  // returns true if we are at the end of file, false otherwise
  bool feof() {
    log(mylog, "[hdfs] hdfsreadablefile feof %s\n", filename_.c_str());
    if (hdfstell(filesys_, hfile_) == filesize()) {
      return true;
    }
    return false;
  }

  // the current size of the file
  toffset filesize() {
    log(mylog, "[hdfs] hdfsreadablefile filesize %s\n", filename_.c_str());
    hdfsfileinfo* pfileinfo = hdfsgetpathinfo(filesys_, filename_.c_str());
    toffset size = 0l;
    if (pfileinfo != nullptr) {
      size = pfileinfo->msize;
      hdfsfreefileinfo(pfileinfo, 1);
    } else {
      throw hdfsfatalexception("filesize on unknown file " + filename_);
    }
    return size;
  }
};

// appends to an existing file in hdfs.
class hdfswritablefile: public writablefile {
 private:
  hdfsfs filesys_;
  std::string filename_;
  hdfsfile hfile_;

 public:
  hdfswritablefile(hdfsfs filesys, const std::string& fname)
      : filesys_(filesys), filename_(fname) , hfile_(nullptr) {
    log(mylog, "[hdfs] hdfswritablefile opening %s\n", filename_.c_str());
    hfile_ = hdfsopenfile(filesys_, filename_.c_str(), o_wronly, 0, 0, 0);
    log(mylog, "[hdfs] hdfswritablefile opened %s\n", filename_.c_str());
    assert(hfile_ != nullptr);
  }
  virtual ~hdfswritablefile() {
    if (hfile_ != nullptr) {
      log(mylog, "[hdfs] hdfswritablefile closing %s\n", filename_.c_str());
      hdfsclosefile(filesys_, hfile_);
      log(mylog, "[hdfs] hdfswritablefile closed %s\n", filename_.c_str());
      hfile_ = nullptr;
    }
  }

  // if the file was successfully created, then this returns true.
  // otherwise returns false.
  bool isvalid() {
    return hfile_ != nullptr;
  }

  // the name of the file, mostly needed for debug logging.
  const std::string& getname() {
    return filename_;
  }

  virtual status append(const slice& data) {
    log(mylog, "[hdfs] hdfswritablefile append %s\n", filename_.c_str());
    const char* src = data.data();
    size_t left = data.size();
    size_t ret = hdfswrite(filesys_, hfile_, src, left);
    log(mylog, "[hdfs] hdfswritablefile appended %s\n", filename_.c_str());
    if (ret != left) {
      return ioerror(filename_, errno);
    }
    return status::ok();
  }

  virtual status flush() {
    return status::ok();
  }

  virtual status sync() {
    status s;
    log(mylog, "[hdfs] hdfswritablefile sync %s\n", filename_.c_str());
    if (hdfsflush(filesys_, hfile_) == -1) {
      return ioerror(filename_, errno);
    }
    if (hdfshsync(filesys_, hfile_) == -1) {
      return ioerror(filename_, errno);
    }
    log(mylog, "[hdfs] hdfswritablefile synced %s\n", filename_.c_str());
    return status::ok();
  }

  // this is used by hdfslogger to write data to the debug log file
  virtual status append(const char* src, size_t size) {
    if (hdfswrite(filesys_, hfile_, src, size) != (tsize)size) {
      return ioerror(filename_, errno);
    }
    return status::ok();
  }

  virtual status close() {
    log(mylog, "[hdfs] hdfswritablefile closing %s\n", filename_.c_str());
    if (hdfsclosefile(filesys_, hfile_) != 0) {
      return ioerror(filename_, errno);
    }
    log(mylog, "[hdfs] hdfswritablefile closed %s\n", filename_.c_str());
    hfile_ = nullptr;
    return status::ok();
  }
};

// the object that implements the debug logs to reside in hdfs.
class hdfslogger : public logger {
 private:
  hdfswritablefile* file_;
  uint64_t (*gettid_)();  // return the thread id for the current thread

 public:
  hdfslogger(hdfswritablefile* f, uint64_t (*gettid)())
      : file_(f), gettid_(gettid) {
    log(mylog, "[hdfs] hdfslogger opened %s\n",
            file_->getname().c_str());
  }

  virtual ~hdfslogger() {
    log(mylog, "[hdfs] hdfslogger closed %s\n",
            file_->getname().c_str());
    delete file_;
    if (mylog != nullptr && mylog == this) {
      mylog = nullptr;
    }
  }

  virtual void logv(const char* format, va_list ap) {
    const uint64_t thread_id = (*gettid_)();

    // we try twice: the first time with a fixed-size stack allocated buffer,
    // and the second time with a much larger dynamically allocated buffer.
    char buffer[500];
    for (int iter = 0; iter < 2; iter++) {
      char* base;
      int bufsize;
      if (iter == 0) {
        bufsize = sizeof(buffer);
        base = buffer;
      } else {
        bufsize = 30000;
        base = new char[bufsize];
      }
      char* p = base;
      char* limit = base + bufsize;

      struct timeval now_tv;
      gettimeofday(&now_tv, nullptr);
      const time_t seconds = now_tv.tv_sec;
      struct tm t;
      localtime_r(&seconds, &t);
      p += snprintf(p, limit - p,
                    "%04d/%02d/%02d-%02d:%02d:%02d.%06d %llx ",
                    t.tm_year + 1900,
                    t.tm_mon + 1,
                    t.tm_mday,
                    t.tm_hour,
                    t.tm_min,
                    t.tm_sec,
                    static_cast<int>(now_tv.tv_usec),
                    static_cast<long long unsigned int>(thread_id));

      // print the message
      if (p < limit) {
        va_list backup_ap;
        va_copy(backup_ap, ap);
        p += vsnprintf(p, limit - p, format, backup_ap);
        va_end(backup_ap);
      }

      // truncate to available space if necessary
      if (p >= limit) {
        if (iter == 0) {
          continue;       // try again with larger buffer
        } else {
          p = limit - 1;
        }
      }

      // add newline if necessary
      if (p == base || p[-1] != '\n') {
        *p++ = '\n';
      }

      assert(p <= limit);
      file_->append(base, p-base);
      file_->flush();
      if (base != buffer) {
        delete[] base;
      }
      break;
    }
  }
};

}  // namespace

// finally, the hdfs environment

const std::string hdfsenv::kproto = "hdfs://";
const std::string hdfsenv::pathsep = "/";

// open a file for sequential reading
status hdfsenv::newsequentialfile(const std::string& fname,
                                  unique_ptr<sequentialfile>* result,
                                  const envoptions& options) {
  result->reset();
  hdfsreadablefile* f = new hdfsreadablefile(filesys_, fname);
  if (f == nullptr || !f->isvalid()) {
    delete f;
    *result = nullptr;
    return ioerror(fname, errno);
  }
  result->reset(dynamic_cast<sequentialfile*>(f));
  return status::ok();
}

// open a file for random reading
status hdfsenv::newrandomaccessfile(const std::string& fname,
                                    unique_ptr<randomaccessfile>* result,
                                    const envoptions& options) {
  result->reset();
  hdfsreadablefile* f = new hdfsreadablefile(filesys_, fname);
  if (f == nullptr || !f->isvalid()) {
    delete f;
    *result = nullptr;
    return ioerror(fname, errno);
  }
  result->reset(dynamic_cast<randomaccessfile*>(f));
  return status::ok();
}

// create a new file for writing
status hdfsenv::newwritablefile(const std::string& fname,
                                unique_ptr<writablefile>* result,
                                const envoptions& options) {
  result->reset();
  status s;
  hdfswritablefile* f = new hdfswritablefile(filesys_, fname);
  if (f == nullptr || !f->isvalid()) {
    delete f;
    *result = nullptr;
    return ioerror(fname, errno);
  }
  result->reset(dynamic_cast<writablefile*>(f));
  return status::ok();
}

status hdfsenv::newrandomrwfile(const std::string& fname,
                                unique_ptr<randomrwfile>* result,
                                const envoptions& options) {
  return status::notsupported("newrandomrwfile not supported on hdfsenv");
}

class hdfsdirectory : public directory {
 public:
  explicit hdfsdirectory(int fd) : fd_(fd) {}
  ~hdfsdirectory() {}

  virtual status fsync() { return status::ok(); }

 private:
  int fd_;
};

status hdfsenv::newdirectory(const std::string& name,
                             unique_ptr<directory>* result) {
  int value = hdfsexists(filesys_, name.c_str());
  switch (value) {
    case hdfs_exists:
      result->reset(new hdfsdirectory(0));
      return status::ok();
    default:  // fail if the directory doesn't exist
      log(mylog, "newdirectory hdfsexists call failed");
      throw hdfsfatalexception("hdfsexists call failed with error " +
                               std::to_string(value) + " on path " + name +
                               ".\n");
  }
}

bool hdfsenv::fileexists(const std::string& fname) {

  int value = hdfsexists(filesys_, fname.c_str());
  switch (value) {
    case hdfs_exists:
    return true;
    case hdfs_doesnt_exist:
      return false;
    default:  // anything else should be an error
      log(mylog, "fileexists hdfsexists call failed");
      throw hdfsfatalexception("hdfsexists call failed with error " +
                               std::to_string(value) + " on path " + fname +
                               ".\n");
  }
}

status hdfsenv::getchildren(const std::string& path,
                            std::vector<std::string>* result) {
  int value = hdfsexists(filesys_, path.c_str());
  switch (value) {
    case hdfs_exists: {  // directory exists
    int numentries = 0;
    hdfsfileinfo* phdfsfileinfo = 0;
    phdfsfileinfo = hdfslistdirectory(filesys_, path.c_str(), &numentries);
    if (numentries >= 0) {
      for(int i = 0; i < numentries; i++) {
        char* pathname = phdfsfileinfo[i].mname;
        char* filename = rindex(pathname, '/');
        if (filename != nullptr) {
          result->push_back(filename+1);
        }
      }
      if (phdfsfileinfo != nullptr) {
        hdfsfreefileinfo(phdfsfileinfo, numentries);
      }
    } else {
      // numentries < 0 indicates error
      log(mylog, "hdfslistdirectory call failed with error ");
      throw hdfsfatalexception(
          "hdfslistdirectory call failed negative error.\n");
    }
    break;
  }
  case hdfs_doesnt_exist:  // directory does not exist, exit
    break;
  default:          // anything else should be an error
    log(mylog, "getchildren hdfsexists call failed");
    throw hdfsfatalexception("hdfsexists call failed with error " +
                             std::to_string(value) + ".\n");
  }
  return status::ok();
}

status hdfsenv::deletefile(const std::string& fname) {
  if (hdfsdelete(filesys_, fname.c_str(), 1) == 0) {
    return status::ok();
  }
  return ioerror(fname, errno);
};

status hdfsenv::createdir(const std::string& name) {
  if (hdfscreatedirectory(filesys_, name.c_str()) == 0) {
    return status::ok();
  }
  return ioerror(name, errno);
};

status hdfsenv::createdirifmissing(const std::string& name) {
  const int value = hdfsexists(filesys_, name.c_str());
  //  not atomic. state might change b/w hdfsexists and createdir.
  switch (value) {
    case hdfs_exists:
    return status::ok();
    case hdfs_doesnt_exist:
    return createdir(name);
    default:  // anything else should be an error
      log(mylog, "createdirifmissing hdfsexists call failed");
      throw hdfsfatalexception("hdfsexists call failed with error " +
                               std::to_string(value) + ".\n");
  }
};

status hdfsenv::deletedir(const std::string& name) {
  return deletefile(name);
};

status hdfsenv::getfilesize(const std::string& fname, uint64_t* size) {
  *size = 0l;
  hdfsfileinfo* pfileinfo = hdfsgetpathinfo(filesys_, fname.c_str());
  if (pfileinfo != nullptr) {
    *size = pfileinfo->msize;
    hdfsfreefileinfo(pfileinfo, 1);
    return status::ok();
  }
  return ioerror(fname, errno);
}

status hdfsenv::getfilemodificationtime(const std::string& fname,
                                        uint64_t* time) {
  hdfsfileinfo* pfileinfo = hdfsgetpathinfo(filesys_, fname.c_str());
  if (pfileinfo != nullptr) {
    *time = static_cast<uint64_t>(pfileinfo->mlastmod);
    hdfsfreefileinfo(pfileinfo, 1);
    return status::ok();
  }
  return ioerror(fname, errno);

}

// the rename is not atomic. hdfs does not allow a renaming if the
// target already exists. so, we delete the target before attemting the
// rename.
status hdfsenv::renamefile(const std::string& src, const std::string& target) {
  hdfsdelete(filesys_, target.c_str(), 1);
  if (hdfsrename(filesys_, src.c_str(), target.c_str()) == 0) {
    return status::ok();
  }
  return ioerror(src, errno);
}

status hdfsenv::lockfile(const std::string& fname, filelock** lock) {
  // there isn's a very good way to atomically check and create
  // a file via libhdfs
  *lock = nullptr;
  return status::ok();
}

status hdfsenv::unlockfile(filelock* lock) {
  return status::ok();
}

status hdfsenv::newlogger(const std::string& fname,
                          shared_ptr<logger>* result) {
  hdfswritablefile* f = new hdfswritablefile(filesys_, fname);
  if (f == nullptr || !f->isvalid()) {
    delete f;
    *result = nullptr;
    return ioerror(fname, errno);
  }
  hdfslogger* h = new hdfslogger(f, &hdfsenv::gettid);
  result->reset(h);
  if (mylog == nullptr) {
    // mylog = h; // uncomment this for detailed logging
  }
  return status::ok();
}

}  // namespace rocksdb

#endif // rocksdb_hdfs_file_c

#else // use_hdfs

// dummy placeholders used when hdfs is not available
#include "rocksdb/env.h"
#include "hdfs/env_hdfs.h"
namespace rocksdb {
 status hdfsenv::newsequentialfile(const std::string& fname,
                                   unique_ptr<sequentialfile>* result,
                                   const envoptions& options) {
   return status::notsupported("not compiled with hdfs support");
 }
}

#endif
