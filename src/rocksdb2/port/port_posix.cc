//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "port/port_posix.h"

#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <sys/time.h>
#include <string.h>
#include <cstdlib>
#include "util/logging.h"

namespace rocksdb {
namespace port {

static int pthreadcall(const char* label, int result) {
  if (result != 0 && result != etimedout) {
    fprintf(stderr, "pthread %s: %s\n", label, strerror(result));
    abort();
  }
  return result;
}

mutex::mutex(bool adaptive) {
#ifdef os_linux
  if (!adaptive) {
    pthreadcall("init mutex", pthread_mutex_init(&mu_, nullptr));
  } else {
    pthread_mutexattr_t mutex_attr;
    pthreadcall("init mutex attr", pthread_mutexattr_init(&mutex_attr));
    pthreadcall("set mutex attr",
                pthread_mutexattr_settype(&mutex_attr,
                                          pthread_mutex_adaptive_np));
    pthreadcall("init mutex", pthread_mutex_init(&mu_, &mutex_attr));
    pthreadcall("destroy mutex attr",
                pthread_mutexattr_destroy(&mutex_attr));
  }
#else // ignore adaptive for non-linux platform
  pthreadcall("init mutex", pthread_mutex_init(&mu_, nullptr));
#endif // os_linux
}

mutex::~mutex() { pthreadcall("destroy mutex", pthread_mutex_destroy(&mu_)); }

void mutex::lock() {
  pthreadcall("lock", pthread_mutex_lock(&mu_));
#ifndef ndebug
  locked_ = true;
#endif
}

void mutex::unlock() {
#ifndef ndebug
  locked_ = false;
#endif
  pthreadcall("unlock", pthread_mutex_unlock(&mu_));
}

void mutex::assertheld() {
#ifndef ndebug
  assert(locked_);
#endif
}

condvar::condvar(mutex* mu)
    : mu_(mu) {
    pthreadcall("init cv", pthread_cond_init(&cv_, nullptr));
}

condvar::~condvar() { pthreadcall("destroy cv", pthread_cond_destroy(&cv_)); }

void condvar::wait() {
#ifndef ndebug
  mu_->locked_ = false;
#endif
  pthreadcall("wait", pthread_cond_wait(&cv_, &mu_->mu_));
#ifndef ndebug
  mu_->locked_ = true;
#endif
}

bool condvar::timedwait(uint64_t abs_time_us) {
  struct timespec ts;
  ts.tv_sec = abs_time_us / 1000000;
  ts.tv_nsec = (abs_time_us % 1000000) * 1000;

#ifndef ndebug
  mu_->locked_ = false;
#endif
  int err = pthread_cond_timedwait(&cv_, &mu_->mu_, &ts);
#ifndef ndebug
  mu_->locked_ = true;
#endif
  if (err == etimedout) {
    return true;
  }
  if (err != 0) {
    pthreadcall("timedwait", err);
  }
  return false;
}

void condvar::signal() {
  pthreadcall("signal", pthread_cond_signal(&cv_));
}

void condvar::signalall() {
  pthreadcall("broadcast", pthread_cond_broadcast(&cv_));
}

rwmutex::rwmutex() {
  pthreadcall("init mutex", pthread_rwlock_init(&mu_, nullptr));
}

rwmutex::~rwmutex() { pthreadcall("destroy mutex", pthread_rwlock_destroy(&mu_)); }

void rwmutex::readlock() { pthreadcall("read lock", pthread_rwlock_rdlock(&mu_)); }

void rwmutex::writelock() { pthreadcall("write lock", pthread_rwlock_wrlock(&mu_)); }

void rwmutex::readunlock() { pthreadcall("read unlock", pthread_rwlock_unlock(&mu_)); }

void rwmutex::writeunlock() { pthreadcall("write unlock", pthread_rwlock_unlock(&mu_)); }

void initonce(oncetype* once, void (*initializer)()) {
  pthreadcall("once", pthread_once(once, initializer));
}

}  // namespace port
}  // namespace rocksdb
