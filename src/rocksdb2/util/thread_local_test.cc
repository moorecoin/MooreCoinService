//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.

#include <atomic>

#include "rocksdb/env.h"
#include "port/port_posix.h"
#include "util/autovector.h"
#include "util/thread_local.h"
#include "util/testharness.h"
#include "util/testutil.h"

namespace rocksdb {

class threadlocaltest {
 public:
  threadlocaltest() : env_(env::default()) {}

  env* env_;
};

namespace {

struct params {
  params(port::mutex* m, port::condvar* c, int* unref, int n,
         unrefhandler handler = nullptr)
      : mu(m),
        cv(c),
        unref(unref),
        total(n),
        started(0),
        completed(0),
        dowrite(false),
        tls1(handler),
        tls2(nullptr) {}

  port::mutex* mu;
  port::condvar* cv;
  int* unref;
  int total;
  int started;
  int completed;
  bool dowrite;
  threadlocalptr tls1;
  threadlocalptr* tls2;
};

class idchecker : public threadlocalptr {
 public:
  static uint32_t peekid() { return instance()->peekid(); }
};

}  // anonymous namespace

test(threadlocaltest, uniqueidtest) {
  port::mutex mu;
  port::condvar cv(&mu);

  assert_eq(idchecker::peekid(), 0u);
  // new threadlocal instance bumps id by 1
  {
    // id used 0
    params p1(&mu, &cv, nullptr, 1u);
    assert_eq(idchecker::peekid(), 1u);
    // id used 1
    params p2(&mu, &cv, nullptr, 1u);
    assert_eq(idchecker::peekid(), 2u);
    // id used 2
    params p3(&mu, &cv, nullptr, 1u);
    assert_eq(idchecker::peekid(), 3u);
    // id used 3
    params p4(&mu, &cv, nullptr, 1u);
    assert_eq(idchecker::peekid(), 4u);
  }
  // id 3, 2, 1, 0 are in the free queue in order
  assert_eq(idchecker::peekid(), 0u);

  // pick up 0
  params p1(&mu, &cv, nullptr, 1u);
  assert_eq(idchecker::peekid(), 1u);
  // pick up 1
  params* p2 = new params(&mu, &cv, nullptr, 1u);
  assert_eq(idchecker::peekid(), 2u);
  // pick up 2
  params p3(&mu, &cv, nullptr, 1u);
  assert_eq(idchecker::peekid(), 3u);
  // return up 1
  delete p2;
  assert_eq(idchecker::peekid(), 1u);
  // now we have 3, 1 in queue
  // pick up 1
  params p4(&mu, &cv, nullptr, 1u);
  assert_eq(idchecker::peekid(), 3u);
  // pick up 3
  params p5(&mu, &cv, nullptr, 1u);
  // next new id
  assert_eq(idchecker::peekid(), 4u);
  // after exit, id sequence in queue:
  // 3, 1, 2, 0
}

test(threadlocaltest, sequentialreadwritetest) {
  // global id list carries over 3, 1, 2, 0
  assert_eq(idchecker::peekid(), 0u);

  port::mutex mu;
  port::condvar cv(&mu);
  params p(&mu, &cv, nullptr, 1);
  threadlocalptr tls2;
  p.tls2 = &tls2;

  auto func = [](void* ptr) {
    auto& p = *static_cast<params*>(ptr);

    assert_true(p.tls1.get() == nullptr);
    p.tls1.reset(reinterpret_cast<int*>(1));
    assert_true(p.tls1.get() == reinterpret_cast<int*>(1));
    p.tls1.reset(reinterpret_cast<int*>(2));
    assert_true(p.tls1.get() == reinterpret_cast<int*>(2));

    assert_true(p.tls2->get() == nullptr);
    p.tls2->reset(reinterpret_cast<int*>(1));
    assert_true(p.tls2->get() == reinterpret_cast<int*>(1));
    p.tls2->reset(reinterpret_cast<int*>(2));
    assert_true(p.tls2->get() == reinterpret_cast<int*>(2));

    p.mu->lock();
    ++(p.completed);
    p.cv->signalall();
    p.mu->unlock();
  };

  for (int iter = 0; iter < 1024; ++iter) {
    assert_eq(idchecker::peekid(), 1u);
    // another new thread, read/write should not see value from previous thread
    env_->startthread(func, static_cast<void*>(&p));
    mu.lock();
    while (p.completed != iter + 1) {
      cv.wait();
    }
    mu.unlock();
    assert_eq(idchecker::peekid(), 1u);
  }
}

test(threadlocaltest, concurrentreadwritetest) {
  // global id list carries over 3, 1, 2, 0
  assert_eq(idchecker::peekid(), 0u);

  threadlocalptr tls2;
  port::mutex mu1;
  port::condvar cv1(&mu1);
  params p1(&mu1, &cv1, nullptr, 16);
  p1.tls2 = &tls2;

  port::mutex mu2;
  port::condvar cv2(&mu2);
  params p2(&mu2, &cv2, nullptr, 16);
  p2.dowrite = true;
  p2.tls2 = &tls2;

  auto func = [](void* ptr) {
    auto& p = *static_cast<params*>(ptr);

    p.mu->lock();
    int own = ++(p.started);
    p.cv->signalall();
    while (p.started != p.total) {
      p.cv->wait();
    }
    p.mu->unlock();

    // let write threads write a different value from the read threads
    if (p.dowrite) {
      own += 8192;
    }

    assert_true(p.tls1.get() == nullptr);
    assert_true(p.tls2->get() == nullptr);

    auto* env = env::default();
    auto start = env->nowmicros();

    p.tls1.reset(reinterpret_cast<int*>(own));
    p.tls2->reset(reinterpret_cast<int*>(own + 1));
    // loop for 1 second
    while (env->nowmicros() - start < 1000 * 1000) {
      for (int iter = 0; iter < 100000; ++iter) {
        assert_true(p.tls1.get() == reinterpret_cast<int*>(own));
        assert_true(p.tls2->get() == reinterpret_cast<int*>(own + 1));
        if (p.dowrite) {
          p.tls1.reset(reinterpret_cast<int*>(own));
          p.tls2->reset(reinterpret_cast<int*>(own + 1));
        }
      }
    }

    p.mu->lock();
    ++(p.completed);
    p.cv->signalall();
    p.mu->unlock();
  };

  // initiate 2 instnaces: one keeps writing and one keeps reading.
  // the read instance should not see data from the write instance.
  // each thread local copy of the value are also different from each
  // other.
  for (int th = 0; th < p1.total; ++th) {
    env_->startthread(func, static_cast<void*>(&p1));
  }
  for (int th = 0; th < p2.total; ++th) {
    env_->startthread(func, static_cast<void*>(&p2));
  }

  mu1.lock();
  while (p1.completed != p1.total) {
    cv1.wait();
  }
  mu1.unlock();

  mu2.lock();
  while (p2.completed != p2.total) {
    cv2.wait();
  }
  mu2.unlock();

  assert_eq(idchecker::peekid(), 3u);
}

test(threadlocaltest, unref) {
  assert_eq(idchecker::peekid(), 0u);

  auto unref = [](void* ptr) {
    auto& p = *static_cast<params*>(ptr);
    p.mu->lock();
    ++(*p.unref);
    p.mu->unlock();
  };

  // case 0: no unref triggered if threadlocalptr is never accessed
  auto func0 = [](void* ptr) {
    auto& p = *static_cast<params*>(ptr);

    p.mu->lock();
    ++(p.started);
    p.cv->signalall();
    while (p.started != p.total) {
      p.cv->wait();
    }
    p.mu->unlock();
  };

  for (int th = 1; th <= 128; th += th) {
    port::mutex mu;
    port::condvar cv(&mu);
    int unref_count = 0;
    params p(&mu, &cv, &unref_count, th, unref);

    for (int i = 0; i < p.total; ++i) {
      env_->startthread(func0, static_cast<void*>(&p));
    }
    env_->waitforjoin();
    assert_eq(unref_count, 0);
  }

  // case 1: unref triggered by thread exit
  auto func1 = [](void* ptr) {
    auto& p = *static_cast<params*>(ptr);

    p.mu->lock();
    ++(p.started);
    p.cv->signalall();
    while (p.started != p.total) {
      p.cv->wait();
    }
    p.mu->unlock();

    assert_true(p.tls1.get() == nullptr);
    assert_true(p.tls2->get() == nullptr);

    p.tls1.reset(ptr);
    p.tls2->reset(ptr);

    p.tls1.reset(ptr);
    p.tls2->reset(ptr);
  };

  for (int th = 1; th <= 128; th += th) {
    port::mutex mu;
    port::condvar cv(&mu);
    int unref_count = 0;
    threadlocalptr tls2(unref);
    params p(&mu, &cv, &unref_count, th, unref);
    p.tls2 = &tls2;

    for (int i = 0; i < p.total; ++i) {
      env_->startthread(func1, static_cast<void*>(&p));
    }

    env_->waitforjoin();

    // n threads x 2 threadlocal instance cleanup on thread exit
    assert_eq(unref_count, 2 * p.total);
  }

  // case 2: unref triggered by threadlocal instance destruction
  auto func2 = [](void* ptr) {
    auto& p = *static_cast<params*>(ptr);

    p.mu->lock();
    ++(p.started);
    p.cv->signalall();
    while (p.started != p.total) {
      p.cv->wait();
    }
    p.mu->unlock();

    assert_true(p.tls1.get() == nullptr);
    assert_true(p.tls2->get() == nullptr);

    p.tls1.reset(ptr);
    p.tls2->reset(ptr);

    p.tls1.reset(ptr);
    p.tls2->reset(ptr);

    p.mu->lock();
    ++(p.completed);
    p.cv->signalall();

    // waiting for instruction to exit thread
    while (p.completed != 0) {
      p.cv->wait();
    }
    p.mu->unlock();
  };

  for (int th = 1; th <= 128; th += th) {
    port::mutex mu;
    port::condvar cv(&mu);
    int unref_count = 0;
    params p(&mu, &cv, &unref_count, th, unref);
    p.tls2 = new threadlocalptr(unref);

    for (int i = 0; i < p.total; ++i) {
      env_->startthread(func2, static_cast<void*>(&p));
    }

    // wait for all threads to finish using params
    mu.lock();
    while (p.completed != p.total) {
      cv.wait();
    }
    mu.unlock();

    // now destroy one threadlocal instance
    delete p.tls2;
    p.tls2 = nullptr;
    // instance destroy for n threads
    assert_eq(unref_count, p.total);

    // signal to exit
    mu.lock();
    p.completed = 0;
    cv.signalall();
    mu.unlock();
    env_->waitforjoin();
    // additional n threads exit unref for the left instance
    assert_eq(unref_count, 2 * p.total);
  }
}

test(threadlocaltest, swap) {
  threadlocalptr tls;
  tls.reset(reinterpret_cast<void*>(1));
  assert_eq(reinterpret_cast<int64_t>(tls.swap(nullptr)), 1);
  assert_true(tls.swap(reinterpret_cast<void*>(2)) == nullptr);
  assert_eq(reinterpret_cast<int64_t>(tls.get()), 2);
  assert_eq(reinterpret_cast<int64_t>(tls.swap(reinterpret_cast<void*>(3))), 2);
}

test(threadlocaltest, scrape) {
  auto unref = [](void* ptr) {
    auto& p = *static_cast<params*>(ptr);
    p.mu->lock();
    ++(*p.unref);
    p.mu->unlock();
  };

  auto func = [](void* ptr) {
    auto& p = *static_cast<params*>(ptr);

    assert_true(p.tls1.get() == nullptr);
    assert_true(p.tls2->get() == nullptr);

    p.tls1.reset(ptr);
    p.tls2->reset(ptr);

    p.tls1.reset(ptr);
    p.tls2->reset(ptr);

    p.mu->lock();
    ++(p.completed);
    p.cv->signalall();

    // waiting for instruction to exit thread
    while (p.completed != 0) {
      p.cv->wait();
    }
    p.mu->unlock();
  };

  for (int th = 1; th <= 128; th += th) {
    port::mutex mu;
    port::condvar cv(&mu);
    int unref_count = 0;
    params p(&mu, &cv, &unref_count, th, unref);
    p.tls2 = new threadlocalptr(unref);

    for (int i = 0; i < p.total; ++i) {
      env_->startthread(func, static_cast<void*>(&p));
    }

    // wait for all threads to finish using params
    mu.lock();
    while (p.completed != p.total) {
      cv.wait();
    }
    mu.unlock();

    assert_eq(unref_count, 0);

    // scrape all thread local data. no unref at thread
    // exit or threadlocalptr destruction
    autovector<void*> ptrs;
    p.tls1.scrape(&ptrs, nullptr);
    p.tls2->scrape(&ptrs, nullptr);
    delete p.tls2;
    // signal to exit
    mu.lock();
    p.completed = 0;
    cv.signalall();
    mu.unlock();
    env_->waitforjoin();

    assert_eq(unref_count, 0);
  }
}

test(threadlocaltest, compareandswap) {
  threadlocalptr tls;
  assert_true(tls.swap(reinterpret_cast<void*>(1)) == nullptr);
  void* expected = reinterpret_cast<void*>(1);
  // swap in 2
  assert_true(tls.compareandswap(reinterpret_cast<void*>(2), expected));
  expected = reinterpret_cast<void*>(100);
  // fail swap, still 2
  assert_true(!tls.compareandswap(reinterpret_cast<void*>(2), expected));
  assert_eq(expected, reinterpret_cast<void*>(2));
  // swap in 3
  expected = reinterpret_cast<void*>(2);
  assert_true(tls.compareandswap(reinterpret_cast<void*>(3), expected));
  assert_eq(tls.get(), reinterpret_cast<void*>(3));
}

}  // namespace rocksdb

int main(int argc, char** argv) {
  return rocksdb::test::runalltests();
}
