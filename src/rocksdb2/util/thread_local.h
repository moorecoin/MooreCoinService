//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#pragma once

#include <atomic>
#include <memory>
#include <unordered_map>
#include <vector>

#include "util/autovector.h"
#include "port/port_posix.h"
#include "util/thread_local.h"

namespace rocksdb {

// cleanup function that will be called for a stored thread local
// pointer (if not null) when one of the following happens:
// (1) a thread terminates
// (2) a threadlocalptr is destroyed
typedef void (*unrefhandler)(void* ptr);

// thread local storage that only stores value of pointer type. the storage
// distinguish data coming from different thread and different threadlocalptr
// instances. for example, if a regular thread_local variable a is declared
// in dbimpl, two dbimpl objects would share the same a. threadlocalptr avoids
// the confliction. the total storage size equals to # of threads * # of
// threadlocalptr instances. it is not efficient in terms of space, but it
// should serve most of our use cases well and keep code simple.
class threadlocalptr {
 public:
  explicit threadlocalptr(unrefhandler handler = nullptr);

  ~threadlocalptr();

  // return the current pointer stored in thread local
  void* get() const;

  // set a new pointer value to the thread local storage.
  void reset(void* ptr);

  // atomically swap the supplied ptr and return the previous value
  void* swap(void* ptr);

  // atomically compare the stored value with expected. set the new
  // pointer value to thread local only if the comparision is true.
  // otherwise, expected returns the stored value.
  // return true on success, false on failure
  bool compareandswap(void* ptr, void*& expected);

  // reset all thread local data to replacement, and return non-nullptr
  // data for all existing threads
  void scrape(autovector<void*>* ptrs, void* const replacement);

 protected:
  struct entry {
    entry() : ptr(nullptr) {}
    entry(const entry& e) : ptr(e.ptr.load(std::memory_order_relaxed)) {}
    std::atomic<void*> ptr;
  };

  // this is the structure that is declared as "thread_local" storage.
  // the vector keep list of atomic pointer for all instances for "current"
  // thread. the vector is indexed by an id that is unique in process and
  // associated with one threadlocalptr instance. the id is assigned by a
  // global staticmeta singleton. so if we instantiated 3 threadlocalptr
  // instances, each thread will have a threaddata with a vector of size 3:
  //     ---------------------------------------------------
  //     |          | instance 1 | instance 2 | instnace 3 |
  //     ---------------------------------------------------
  //     | thread 1 |    void*   |    void*   |    void*   | <- threaddata
  //     ---------------------------------------------------
  //     | thread 2 |    void*   |    void*   |    void*   | <- threaddata
  //     ---------------------------------------------------
  //     | thread 3 |    void*   |    void*   |    void*   | <- threaddata
  //     ---------------------------------------------------
  struct threaddata {
    threaddata() : entries() {}
    std::vector<entry> entries;
    threaddata* next;
    threaddata* prev;
  };

  class staticmeta {
   public:
    staticmeta();

    // return the next available id
    uint32_t getid();
    // return the next availabe id without claiming it
    uint32_t peekid() const;
    // return the given id back to the free pool. this also triggers
    // unrefhandler for associated pointer value (if not null) for all threads.
    void reclaimid(uint32_t id);

    // return the pointer value for the given id for the current thread.
    void* get(uint32_t id) const;
    // reset the pointer value for the given id for the current thread.
    // it triggers unrefhanlder if the id has existing pointer value.
    void reset(uint32_t id, void* ptr);
    // atomically swap the supplied ptr and return the previous value
    void* swap(uint32_t id, void* ptr);
    // atomically compare and swap the provided value only if it equals
    // to expected value.
    bool compareandswap(uint32_t id, void* ptr, void*& expected);
    // reset all thread local data to replacement, and return non-nullptr
    // data for all existing threads
    void scrape(uint32_t id, autovector<void*>* ptrs, void* const replacement);

    // register the unrefhandler for id
    void sethandler(uint32_t id, unrefhandler handler);

   private:
    // get unrefhandler for id with acquiring mutex
    // requires: mutex locked
    unrefhandler gethandler(uint32_t id);

    // triggered before a thread terminates
    static void onthreadexit(void* ptr);

    // add current thread's threaddata to the global chain
    // requires: mutex locked
    void addthreaddata(threaddata* d);

    // remove current thread's threaddata from the global chain
    // requires: mutex locked
    void removethreaddata(threaddata* d);

    static threaddata* getthreadlocal();

    uint32_t next_instance_id_;
    // used to recycle ids in case threadlocalptr is instantiated and destroyed
    // frequently. this also prevents it from blowing up the vector space.
    autovector<uint32_t> free_instance_ids_;
    // chain all thread local structure together. this is necessary since
    // when one threadlocalptr gets destroyed, we need to loop over each
    // thread's version of pointer corresponding to that instance and
    // call unrefhandler for it.
    threaddata head_;

    std::unordered_map<uint32_t, unrefhandler> handler_map_;

    // protect inst, next_instance_id_, free_instance_ids_, head_,
    // threaddata.entries
    static port::mutex mutex_;
#if !defined(os_macosx)
    // thread local storage
    static __thread threaddata* tls_;
#endif
    // used to make thread exit trigger possible if !defined(os_macosx).
    // otherwise, used to retrieve thread data.
    pthread_key_t pthread_key_;
  };

  static staticmeta* instance();

  const uint32_t id_;
};

}  // namespace rocksdb
