//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "util/thread_local.h"
#include "util/mutexlock.h"
#include "port/likely.h"


namespace rocksdb {

port::mutex threadlocalptr::staticmeta::mutex_;
#if !defined(os_macosx)
__thread threadlocalptr::threaddata* threadlocalptr::staticmeta::tls_ = nullptr;
#endif

threadlocalptr::staticmeta* threadlocalptr::instance() {
  static threadlocalptr::staticmeta inst;
  return &inst;
}

void threadlocalptr::staticmeta::onthreadexit(void* ptr) {
  auto* tls = static_cast<threaddata*>(ptr);
  assert(tls != nullptr);

  auto* inst = instance();
  pthread_setspecific(inst->pthread_key_, nullptr);

  mutexlock l(&mutex_);
  inst->removethreaddata(tls);
  // unref stored pointers of current thread from all instances
  uint32_t id = 0;
  for (auto& e : tls->entries) {
    void* raw = e.ptr.load(std::memory_order_relaxed);
    if (raw != nullptr) {
      auto unref = inst->gethandler(id);
      if (unref != nullptr) {
        unref(raw);
      }
    }
    ++id;
  }
  // delete thread local structure no matter if it is mac platform
  delete tls;
}

threadlocalptr::staticmeta::staticmeta() : next_instance_id_(0) {
  if (pthread_key_create(&pthread_key_, &onthreadexit) != 0) {
    throw std::runtime_error("pthread_key_create failed");
  }
  head_.next = &head_;
  head_.prev = &head_;
}

void threadlocalptr::staticmeta::addthreaddata(threadlocalptr::threaddata* d) {
  mutex_.assertheld();
  d->next = &head_;
  d->prev = head_.prev;
  head_.prev->next = d;
  head_.prev = d;
}

void threadlocalptr::staticmeta::removethreaddata(
    threadlocalptr::threaddata* d) {
  mutex_.assertheld();
  d->next->prev = d->prev;
  d->prev->next = d->next;
  d->next = d->prev = d;
}

threadlocalptr::threaddata* threadlocalptr::staticmeta::getthreadlocal() {
#if defined(os_macosx)
  // make this local variable name look like a member variable so that we
  // can share all the code below
  threaddata* tls_ =
      static_cast<threaddata*>(pthread_getspecific(instance()->pthread_key_));
#endif

  if (unlikely(tls_ == nullptr)) {
    auto* inst = instance();
    tls_ = new threaddata();
    {
      // register it in the global chain, needs to be done before thread exit
      // handler registration
      mutexlock l(&mutex_);
      inst->addthreaddata(tls_);
    }
    // even it is not os_macosx, need to register value for pthread_key_ so that
    // its exit handler will be triggered.
    if (pthread_setspecific(inst->pthread_key_, tls_) != 0) {
      {
        mutexlock l(&mutex_);
        inst->removethreaddata(tls_);
      }
      delete tls_;
      throw std::runtime_error("pthread_setspecific failed");
    }
  }
  return tls_;
}

void* threadlocalptr::staticmeta::get(uint32_t id) const {
  auto* tls = getthreadlocal();
  if (unlikely(id >= tls->entries.size())) {
    return nullptr;
  }
  return tls->entries[id].ptr.load(std::memory_order_relaxed);
}

void threadlocalptr::staticmeta::reset(uint32_t id, void* ptr) {
  auto* tls = getthreadlocal();
  if (unlikely(id >= tls->entries.size())) {
    // need mutex to protect entries access within reclaimid
    mutexlock l(&mutex_);
    tls->entries.resize(id + 1);
  }
  tls->entries[id].ptr.store(ptr, std::memory_order_relaxed);
}

void* threadlocalptr::staticmeta::swap(uint32_t id, void* ptr) {
  auto* tls = getthreadlocal();
  if (unlikely(id >= tls->entries.size())) {
    // need mutex to protect entries access within reclaimid
    mutexlock l(&mutex_);
    tls->entries.resize(id + 1);
  }
  return tls->entries[id].ptr.exchange(ptr, std::memory_order_relaxed);
}

bool threadlocalptr::staticmeta::compareandswap(uint32_t id, void* ptr,
    void*& expected) {
  auto* tls = getthreadlocal();
  if (unlikely(id >= tls->entries.size())) {
    // need mutex to protect entries access within reclaimid
    mutexlock l(&mutex_);
    tls->entries.resize(id + 1);
  }
  return tls->entries[id].ptr.compare_exchange_strong(expected, ptr,
      std::memory_order_relaxed, std::memory_order_relaxed);
}

void threadlocalptr::staticmeta::scrape(uint32_t id, autovector<void*>* ptrs,
    void* const replacement) {
  mutexlock l(&mutex_);
  for (threaddata* t = head_.next; t != &head_; t = t->next) {
    if (id < t->entries.size()) {
      void* ptr =
          t->entries[id].ptr.exchange(replacement, std::memory_order_relaxed);
      if (ptr != nullptr) {
        ptrs->push_back(ptr);
      }
    }
  }
}

void threadlocalptr::staticmeta::sethandler(uint32_t id, unrefhandler handler) {
  mutexlock l(&mutex_);
  handler_map_[id] = handler;
}

unrefhandler threadlocalptr::staticmeta::gethandler(uint32_t id) {
  mutex_.assertheld();
  auto iter = handler_map_.find(id);
  if (iter == handler_map_.end()) {
    return nullptr;
  }
  return iter->second;
}

uint32_t threadlocalptr::staticmeta::getid() {
  mutexlock l(&mutex_);
  if (free_instance_ids_.empty()) {
    return next_instance_id_++;
  }

  uint32_t id = free_instance_ids_.back();
  free_instance_ids_.pop_back();
  return id;
}

uint32_t threadlocalptr::staticmeta::peekid() const {
  mutexlock l(&mutex_);
  if (!free_instance_ids_.empty()) {
    return free_instance_ids_.back();
  }
  return next_instance_id_;
}

void threadlocalptr::staticmeta::reclaimid(uint32_t id) {
  // this id is not used, go through all thread local data and release
  // corresponding value
  mutexlock l(&mutex_);
  auto unref = gethandler(id);
  for (threaddata* t = head_.next; t != &head_; t = t->next) {
    if (id < t->entries.size()) {
      void* ptr =
          t->entries[id].ptr.exchange(nullptr, std::memory_order_relaxed);
      if (ptr != nullptr && unref != nullptr) {
        unref(ptr);
      }
    }
  }
  handler_map_[id] = nullptr;
  free_instance_ids_.push_back(id);
}

threadlocalptr::threadlocalptr(unrefhandler handler)
    : id_(instance()->getid()) {
  if (handler != nullptr) {
    instance()->sethandler(id_, handler);
  }
}

threadlocalptr::~threadlocalptr() {
  instance()->reclaimid(id_);
}

void* threadlocalptr::get() const {
  return instance()->get(id_);
}

void threadlocalptr::reset(void* ptr) {
  instance()->reset(id_, ptr);
}

void* threadlocalptr::swap(void* ptr) {
  return instance()->swap(id_, ptr);
}

bool threadlocalptr::compareandswap(void* ptr, void*& expected) {
  return instance()->compareandswap(id_, ptr, expected);
}

void threadlocalptr::scrape(autovector<void*>* ptrs, void* const replacement) {
  instance()->scrape(id_, ptrs, replacement);
}

}  // namespace rocksdb
