//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//

#pragma once
#include <algorithm>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <iostream>
#include "rocksdb/env.h"
#include "rocksdb/status.h"

#ifdef use_hdfs
#include <hdfs.h>

namespace rocksdb {

// thrown during execution when there is an issue with the supplied
// arguments.
class hdfsusageexception : public std::exception { };

// a simple exception that indicates something went wrong that is not
// recoverable.  the intention is for the message to be printed (with
// nothing else) and the process terminate.
class hdfsfatalexception : public std::exception {
public:
  explicit hdfsfatalexception(const std::string& s) : what_(s) { }
  virtual ~hdfsfatalexception() throw() { }
  virtual const char* what() const throw() {
    return what_.c_str();
  }
private:
  const std::string what_;
};

//
// the hdfs environment for rocksdb. this class overrides all the
// file/dir access methods and delegates the thread-mgmt methods to the
// default posix environment.
//
class hdfsenv : public env {

 public:
  explicit hdfsenv(const std::string& fsname) : fsname_(fsname) {
    posixenv = env::default();
    filesys_ = connecttopath(fsname_);
  }

  virtual ~hdfsenv() {
    fprintf(stderr, "destroying hdfsenv::default()\n");
    hdfsdisconnect(filesys_);
  }

  virtual status newsequentialfile(const std::string& fname,
                                   std::unique_ptr<sequentialfile>* result,
                                   const envoptions& options);

  virtual status newrandomaccessfile(const std::string& fname,
                                     std::unique_ptr<randomaccessfile>* result,
                                     const envoptions& options);

  virtual status newwritablefile(const std::string& fname,
                                 std::unique_ptr<writablefile>* result,
                                 const envoptions& options);

  virtual status newrandomrwfile(const std::string& fname,
                                 std::unique_ptr<randomrwfile>* result,
                                 const envoptions& options);

  virtual status newdirectory(const std::string& name,
                              std::unique_ptr<directory>* result);

  virtual bool fileexists(const std::string& fname);

  virtual status getchildren(const std::string& path,
                             std::vector<std::string>* result);

  virtual status deletefile(const std::string& fname);

  virtual status createdir(const std::string& name);

  virtual status createdirifmissing(const std::string& name);

  virtual status deletedir(const std::string& name);

  virtual status getfilesize(const std::string& fname, uint64_t* size);

  virtual status getfilemodificationtime(const std::string& fname,
                                         uint64_t* file_mtime);

  virtual status renamefile(const std::string& src, const std::string& target);

  virtual status lockfile(const std::string& fname, filelock** lock);

  virtual status unlockfile(filelock* lock);

  virtual status newlogger(const std::string& fname,
                           std::shared_ptr<logger>* result);

  virtual void schedule(void (*function)(void* arg), void* arg,
                        priority pri = low) {
    posixenv->schedule(function, arg, pri);
  }

  virtual void startthread(void (*function)(void* arg), void* arg) {
    posixenv->startthread(function, arg);
  }

  virtual void waitforjoin() { posixenv->waitforjoin(); }

  virtual unsigned int getthreadpoolqueuelen(priority pri = low) const
      override {
    return posixenv->getthreadpoolqueuelen(pri);
  }

  virtual status gettestdirectory(std::string* path) {
    return posixenv->gettestdirectory(path);
  }

  virtual uint64_t nowmicros() {
    return posixenv->nowmicros();
  }

  virtual void sleepformicroseconds(int micros) {
    posixenv->sleepformicroseconds(micros);
  }

  virtual status gethostname(char* name, uint64_t len) {
    return posixenv->gethostname(name, len);
  }

  virtual status getcurrenttime(int64_t* unix_time) {
    return posixenv->getcurrenttime(unix_time);
  }

  virtual status getabsolutepath(const std::string& db_path,
      std::string* output_path) {
    return posixenv->getabsolutepath(db_path, output_path);
  }

  virtual void setbackgroundthreads(int number, priority pri = low) {
    posixenv->setbackgroundthreads(number, pri);
  }

  virtual std::string timetostring(uint64_t number) {
    return posixenv->timetostring(number);
  }

  static uint64_t gettid() {
    assert(sizeof(pthread_t) <= sizeof(uint64_t));
    return (uint64_t)pthread_self();
  }

 private:
  std::string fsname_;  // string of the form "hdfs://hostname:port/"
  hdfsfs filesys_;      //  a single filesystem object for all files
  env*  posixenv;       // this object is derived from env, but not from
                        // posixenv. we have posixnv as an encapsulated
                        // object here so that we can use posix timers,
                        // posix threads, etc.

  static const std::string kproto;
  static const std::string pathsep;

  /**
   * if the uri is specified of the form hdfs://server:port/path,
   * then connect to the specified cluster
   * else connect to default.
   */
  hdfsfs connecttopath(const std::string& uri) {
    if (uri.empty()) {
      return nullptr;
    }
    if (uri.find(kproto) != 0) {
      // uri doesn't start with hdfs:// -> use default:0, which is special
      // to libhdfs.
      return hdfsconnectnewinstance("default", 0);
    }
    const std::string hostport = uri.substr(kproto.length());

    std::vector <std::string> parts;
    split(hostport, ':', parts);
    if (parts.size() != 2) {
      throw hdfsfatalexception("bad uri for hdfs " + uri);
    }
    // parts[0] = hosts, parts[1] = port/xxx/yyy
    std::string host(parts[0]);
    std::string remaining(parts[1]);

    int rem = remaining.find(pathsep);
    std::string portstr = (rem == 0 ? remaining :
                           remaining.substr(0, rem));

    tport port;
    port = atoi(portstr.c_str());
    if (port == 0) {
      throw hdfsfatalexception("bad host-port for hdfs " + uri);
    }
    hdfsfs fs = hdfsconnectnewinstance(host.c_str(), port);
    return fs;
  }

  void split(const std::string &s, char delim,
             std::vector<std::string> &elems) {
    elems.clear();
    size_t prev = 0;
    size_t pos = s.find(delim);
    while (pos != std::string::npos) {
      elems.push_back(s.substr(prev, pos));
      prev = pos + 1;
      pos = s.find(delim, prev);
    }
    elems.push_back(s.substr(prev, s.size()));
  }
};

}  // namespace rocksdb

#else // use_hdfs


namespace rocksdb {

static const status notsup;

class hdfsenv : public env {

 public:
  explicit hdfsenv(const std::string& fsname) {
    fprintf(stderr, "you have not build rocksdb with hdfs support\n");
    fprintf(stderr, "please see hdfs/readme for details\n");
    throw std::exception();
  }

  virtual ~hdfsenv() {
  }

  virtual status newsequentialfile(const std::string& fname,
                                   unique_ptr<sequentialfile>* result,
                                   const envoptions& options);

  virtual status newrandomaccessfile(const std::string& fname,
                                     unique_ptr<randomaccessfile>* result,
                                     const envoptions& options) {
    return notsup;
  }

  virtual status newwritablefile(const std::string& fname,
                                 unique_ptr<writablefile>* result,
                                 const envoptions& options) {
    return notsup;
  }

  virtual status newrandomrwfile(const std::string& fname,
                                 unique_ptr<randomrwfile>* result,
                                 const envoptions& options) {
    return notsup;
  }

  virtual status newdirectory(const std::string& name,
                              unique_ptr<directory>* result) {
    return notsup;
  }

  virtual bool fileexists(const std::string& fname){return false;}

  virtual status getchildren(const std::string& path,
                             std::vector<std::string>* result){return notsup;}

  virtual status deletefile(const std::string& fname){return notsup;}

  virtual status createdir(const std::string& name){return notsup;}

  virtual status createdirifmissing(const std::string& name){return notsup;}

  virtual status deletedir(const std::string& name){return notsup;}

  virtual status getfilesize(const std::string& fname, uint64_t* size){return notsup;}

  virtual status getfilemodificationtime(const std::string& fname,
                                         uint64_t* time) {
    return notsup;
  }

  virtual status renamefile(const std::string& src, const std::string& target){return notsup;}

  virtual status lockfile(const std::string& fname, filelock** lock){return notsup;}

  virtual status unlockfile(filelock* lock){return notsup;}

  virtual status newlogger(const std::string& fname,
                           shared_ptr<logger>* result){return notsup;}

  virtual void schedule(void (*function)(void* arg), void* arg,
                        priority pri = low) {}

  virtual void startthread(void (*function)(void* arg), void* arg) {}

  virtual void waitforjoin() {}

  virtual unsigned int getthreadpoolqueuelen(priority pri = low) const {
    return 0;
  }

  virtual status gettestdirectory(std::string* path) {return notsup;}

  virtual uint64_t nowmicros() {return 0;}

  virtual void sleepformicroseconds(int micros) {}

  virtual status gethostname(char* name, uint64_t len) {return notsup;}

  virtual status getcurrenttime(int64_t* unix_time) {return notsup;}

  virtual status getabsolutepath(const std::string& db_path,
      std::string* outputpath) {return notsup;}

  virtual void setbackgroundthreads(int number, priority pri = low) {}

  virtual std::string timetostring(uint64_t number) { return "";}
};
}

#endif // use_hdfs
