// protocol buffers - google's data interchange format
// copyright 2008 google inc.  all rights reserved.
// http://code.google.com/p/protobuf/
//
// redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * neither the name of google inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// this software is provided by the copyright holders and contributors
// "as is" and any express or implied warranties, including, but not
// limited to, the implied warranties of merchantability and fitness for
// a particular purpose are disclaimed. in no event shall the copyright
// owner or contributors be liable for any direct, indirect, incidental,
// special, exemplary, or consequential damages (including, but not
// limited to, procurement of substitute goods or services; loss of use,
// data, or profits; or business interruption) however caused and on any
// theory of liability, whether in contract, strict liability, or tort
// (including negligence or otherwise) arising in any way out of the use
// of this software, even if advised of the possibility of such damage.

// author: kenton@google.com (kenton varda)

#ifdef _win32
#include <windows.h>
#else
#include <unistd.h>
#include <pthread.h>
#endif

#include <google/protobuf/stubs/once.h>
#include <google/protobuf/testing/googletest.h>
#include <gtest/gtest.h>

namespace google {
namespace protobuf {
namespace {

class onceinittest : public testing::test {
 protected:
  void setup() {
    state_ = init_not_started;
    current_test_ = this;
  }

  // since protobufoncetype is only allowed to be allocated in static storage,
  // each test must use a different pair of protobufoncetype objects which it
  // must declare itself.
  void setonces(protobufoncetype* once, protobufoncetype* recursive_once) {
    once_ = once;
    recursive_once_ = recursive_once;
  }

  void initonce() {
    googleonceinit(once_, &initstatic);
  }
  void initrecursiveonce() {
    googleonceinit(recursive_once_, &initrecursivestatic);
  }

  void blockinit() { init_blocker_.lock(); }
  void unblockinit() { init_blocker_.unlock(); }

  class testthread {
   public:
    testthread(closure* callback)
        : done_(false), joined_(false), callback_(callback) {
#ifdef _win32
      thread_ = createthread(null, 0, &start, this, 0, null);
#else
      pthread_create(&thread_, null, &start, this);
#endif
    }
    ~testthread() {
      if (!joined_) join();
    }

    bool isdone() {
      mutexlock lock(&done_mutex_);
      return done_;
    }
    void join() {
      joined_ = true;
#ifdef _win32
      waitforsingleobject(thread_, infinite);
      closehandle(thread_);
#else
      pthread_join(thread_, null);
#endif
    }

   private:
#ifdef _win32
    handle thread_;
#else
    pthread_t thread_;
#endif

    mutex done_mutex_;
    bool done_;
    bool joined_;
    closure* callback_;

#ifdef _win32
    static dword winapi start(lpvoid arg) {
#else
    static void* start(void* arg) {
#endif
      reinterpret_cast<testthread*>(arg)->run();
      return 0;
    }

    void run() {
      callback_->run();
      mutexlock lock(&done_mutex_);
      done_ = true;
    }
  };

  testthread* runinitonceinnewthread() {
    return new testthread(newcallback(this, &onceinittest::initonce));
  }
  testthread* runinitrecursiveonceinnewthread() {
    return new testthread(newcallback(this, &onceinittest::initrecursiveonce));
  }

  enum state {
    init_not_started,
    init_started,
    init_done
  };
  state currentstate() {
    mutexlock lock(&mutex_);
    return state_;
  }

  void waitabit() {
#ifdef _win32
    sleep(1000);
#else
    sleep(1);
#endif
  }

 private:
  mutex mutex_;
  mutex init_blocker_;
  state state_;
  protobufoncetype* once_;
  protobufoncetype* recursive_once_;

  void init() {
    mutexlock lock(&mutex_);
    expect_eq(init_not_started, state_);
    state_ = init_started;
    mutex_.unlock();
    init_blocker_.lock();
    init_blocker_.unlock();
    mutex_.lock();
    state_ = init_done;
  }

  static onceinittest* current_test_;
  static void initstatic() { current_test_->init(); }
  static void initrecursivestatic() { current_test_->initonce(); }
};

onceinittest* onceinittest::current_test_ = null;

google_protobuf_declare_once(simple_once);

test_f(onceinittest, simple) {
  setonces(&simple_once, null);

  expect_eq(init_not_started, currentstate());
  initonce();
  expect_eq(init_done, currentstate());

  // calling again has no effect.
  initonce();
  expect_eq(init_done, currentstate());
}

google_protobuf_declare_once(recursive_once1);
google_protobuf_declare_once(recursive_once2);

test_f(onceinittest, recursive) {
  setonces(&recursive_once1, &recursive_once2);

  expect_eq(init_not_started, currentstate());
  initrecursiveonce();
  expect_eq(init_done, currentstate());
}

google_protobuf_declare_once(multiple_threads_once);

test_f(onceinittest, multiplethreads) {
  setonces(&multiple_threads_once, null);

  scoped_ptr<testthread> threads[4];
  expect_eq(init_not_started, currentstate());
  for (int i = 0; i < 4; i++) {
    threads[i].reset(runinitonceinnewthread());
  }
  for (int i = 0; i < 4; i++) {
    threads[i]->join();
  }
  expect_eq(init_done, currentstate());
}

google_protobuf_declare_once(multiple_threads_blocked_once1);
google_protobuf_declare_once(multiple_threads_blocked_once2);

test_f(onceinittest, multiplethreadsblocked) {
  setonces(&multiple_threads_blocked_once1, &multiple_threads_blocked_once2);

  scoped_ptr<testthread> threads[8];
  expect_eq(init_not_started, currentstate());

  blockinit();
  for (int i = 0; i < 4; i++) {
    threads[i].reset(runinitonceinnewthread());
  }
  for (int i = 4; i < 8; i++) {
    threads[i].reset(runinitrecursiveonceinnewthread());
  }

  waitabit();

  // we should now have one thread blocked inside init(), four blocked waiting
  // for init() to complete, and three blocked waiting for initrecursive() to
  // complete.
  expect_eq(init_started, currentstate());
  unblockinit();

  for (int i = 0; i < 8; i++) {
    threads[i]->join();
  }
  expect_eq(init_done, currentstate());
}

}  // anonymous namespace
}  // namespace protobuf
}  // namespace google
