//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#pragma once
#include "port/port.h"

namespace rocksdb {

// helper class that locks a mutex on construction and unlocks the mutex when
// the destructor of the mutexlock object is invoked.
//
// typical usage:
//
//   void myclass::mymethod() {
//     mutexlock l(&mu_);       // mu_ is an instance variable
//     ... some complex code, possibly with multiple return paths ...
//   }

class mutexlock {
 public:
  explicit mutexlock(port::mutex *mu) : mu_(mu) {
    this->mu_->lock();
  }
  ~mutexlock() { this->mu_->unlock(); }

 private:
  port::mutex *const mu_;
  // no copying allowed
  mutexlock(const mutexlock&);
  void operator=(const mutexlock&);
};

//
// acquire a readlock on the specified rwmutex.
// the lock will be automatically released then the
// object goes out of scope.
//
class readlock {
 public:
  explicit readlock(port::rwmutex *mu) : mu_(mu) {
    this->mu_->readlock();
  }
  ~readlock() { this->mu_->readunlock(); }

 private:
  port::rwmutex *const mu_;
  // no copying allowed
  readlock(const readlock&);
  void operator=(const readlock&);
};


//
// acquire a writelock on the specified rwmutex.
// the lock will be automatically released then the
// object goes out of scope.
//
class writelock {
 public:
  explicit writelock(port::rwmutex *mu) : mu_(mu) {
    this->mu_->writelock();
  }
  ~writelock() { this->mu_->writeunlock(); }

 private:
  port::rwmutex *const mu_;
  // no copying allowed
  writelock(const writelock&);
  void operator=(const writelock&);
};

}  // namespace rocksdb
