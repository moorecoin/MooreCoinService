//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
#include "db/memtable_list.h"

#include <string>
#include "rocksdb/db.h"
#include "db/memtable.h"
#include "db/version_set.h"
#include "rocksdb/env.h"
#include "rocksdb/iterator.h"
#include "table/merger.h"
#include "util/coding.h"
#include "util/log_buffer.h"

namespace rocksdb {

class internalkeycomparator;
class mutex;
class versionset;

memtablelistversion::memtablelistversion(memtablelistversion* old) {
  if (old != nullptr) {
    memlist_ = old->memlist_;
    size_ = old->size_;
    for (auto& m : memlist_) {
      m->ref();
    }
  }
}

void memtablelistversion::ref() { ++refs_; }

void memtablelistversion::unref(autovector<memtable*>* to_delete) {
  assert(refs_ >= 1);
  --refs_;
  if (refs_ == 0) {
    // if to_delete is equal to nullptr it means we're confident
    // that refs_ will not be zero
    assert(to_delete != nullptr);
    for (const auto& m : memlist_) {
      memtable* x = m->unref();
      if (x != nullptr) {
        to_delete->push_back(x);
      }
    }
    delete this;
  }
}

int memtablelistversion::size() const { return size_; }

// returns the total number of memtables in the list
int memtablelist::size() const {
  assert(num_flush_not_started_ <= current_->size_);
  return current_->size_;
}

// search all the memtables starting from the most recent one.
// return the most recent value found, if any.
// operands stores the list of merge operations to apply, so far.
bool memtablelistversion::get(const lookupkey& key, std::string* value,
                              status* s, mergecontext& merge_context,
                              const options& options) {
  for (auto& memtable : memlist_) {
    if (memtable->get(key, value, s, merge_context, options)) {
      return true;
    }
  }
  return false;
}

void memtablelistversion::additerators(const readoptions& options,
                                       std::vector<iterator*>* iterator_list) {
  for (auto& m : memlist_) {
    iterator_list->push_back(m->newiterator(options));
  }
}

void memtablelistversion::additerators(
    const readoptions& options, mergeiteratorbuilder* merge_iter_builder) {
  for (auto& m : memlist_) {
    merge_iter_builder->additerator(
        m->newiterator(options, merge_iter_builder->getarena()));
  }
}

uint64_t memtablelistversion::gettotalnumentries() const {
  uint64_t total_num = 0;
  for (auto& m : memlist_) {
    total_num += m->getnumentries();
  }
  return total_num;
}

// caller is responsible for referencing m
void memtablelistversion::add(memtable* m) {
  assert(refs_ == 1);  // only when refs_ == 1 is memtablelistversion mutable
  memlist_.push_front(m);
  ++size_;
}

// caller is responsible for unreferencing m
void memtablelistversion::remove(memtable* m) {
  assert(refs_ == 1);  // only when refs_ == 1 is memtablelistversion mutable
  memlist_.remove(m);
  --size_;
}

// returns true if there is at least one memtable on which flush has
// not yet started.
bool memtablelist::isflushpending() const {
  if ((flush_requested_ && num_flush_not_started_ >= 1) ||
      (num_flush_not_started_ >= min_write_buffer_number_to_merge_)) {
    assert(imm_flush_needed.nobarrier_load() != nullptr);
    return true;
  }
  return false;
}

// returns the memtables that need to be flushed.
void memtablelist::pickmemtablestoflush(autovector<memtable*>* ret) {
  const auto& memlist = current_->memlist_;
  for (auto it = memlist.rbegin(); it != memlist.rend(); ++it) {
    memtable* m = *it;
    if (!m->flush_in_progress_) {
      assert(!m->flush_completed_);
      num_flush_not_started_--;
      if (num_flush_not_started_ == 0) {
        imm_flush_needed.release_store(nullptr);
      }
      m->flush_in_progress_ = true;  // flushing will start very soon
      ret->push_back(m);
    }
  }
  flush_requested_ = false;  // start-flush request is complete
}

void memtablelist::rollbackmemtableflush(const autovector<memtable*>& mems,
                                         uint64_t file_number,
                                         filenumtopathidmap* pending_outputs) {
  assert(!mems.empty());

  // if the flush was not successful, then just reset state.
  // maybe a suceeding attempt to flush will be successful.
  for (memtable* m : mems) {
    assert(m->flush_in_progress_);
    assert(m->file_number_ == 0);

    m->flush_in_progress_ = false;
    m->flush_completed_ = false;
    m->edit_.clear();
    num_flush_not_started_++;
  }
  pending_outputs->erase(file_number);
  imm_flush_needed.release_store(reinterpret_cast<void *>(1));
}

// record a successful flush in the manifest file
status memtablelist::installmemtableflushresults(
    columnfamilydata* cfd, const autovector<memtable*>& mems, versionset* vset,
    port::mutex* mu, logger* info_log, uint64_t file_number,
    filenumtopathidmap* pending_outputs, autovector<memtable*>* to_delete,
    directory* db_directory, logbuffer* log_buffer) {
  mu->assertheld();

  // flush was sucessful
  for (size_t i = 0; i < mems.size(); ++i) {
    // all the edits are associated with the first memtable of this batch.
    assert(i == 0 || mems[i]->getedits()->numentries() == 0);

    mems[i]->flush_completed_ = true;
    mems[i]->file_number_ = file_number;
  }

  // if some other thread is already commiting, then return
  status s;
  if (commit_in_progress_) {
    return s;
  }

  // only a single thread can be executing this piece of code
  commit_in_progress_ = true;

  // scan all memtables from the earliest, and commit those
  // (in that order) that have finished flushing. memetables
  // are always committed in the order that they were created.
  while (!current_->memlist_.empty() && s.ok()) {
    memtable* m = current_->memlist_.back();  // get the last element
    if (!m->flush_completed_) {
      break;
    }

    logtobuffer(log_buffer, "[%s] level-0 commit table #%lu started",
                cfd->getname().c_str(), (unsigned long)m->file_number_);

    // this can release and reacquire the mutex.
    s = vset->logandapply(cfd, &m->edit_, mu, db_directory);

    // we will be changing the version in the next code path,
    // so we better create a new one, since versions are immutable
    installnewversion();

    // all the later memtables that have the same filenum
    // are part of the same batch. they can be committed now.
    uint64_t mem_id = 1;  // how many memtables has been flushed.
    do {
      if (s.ok()) { // commit new state
        logtobuffer(log_buffer,
                    "[%s] level-0 commit table #%lu: memtable #%lu done",
                    cfd->getname().c_str(), (unsigned long)m->file_number_,
                    (unsigned long)mem_id);
        current_->remove(m);
        assert(m->file_number_ > 0);

        // pending_outputs can be cleared only after the newly created file
        // has been written to a committed version so that other concurrently
        // executing compaction threads do not mistakenly assume that this
        // file is not live.
        pending_outputs->erase(m->file_number_);
        if (m->unref() != nullptr) {
          to_delete->push_back(m);
        }
      } else {
        //commit failed. setup state so that we can flush again.
        log(info_log,
            "level-0 commit table #%lu: memtable #%lu failed",
            (unsigned long)m->file_number_,
            (unsigned long)mem_id);
        m->flush_completed_ = false;
        m->flush_in_progress_ = false;
        m->edit_.clear();
        num_flush_not_started_++;
        pending_outputs->erase(m->file_number_);
        m->file_number_ = 0;
        imm_flush_needed.release_store((void *)1);
      }
      ++mem_id;
    } while (!current_->memlist_.empty() && (m = current_->memlist_.back()) &&
             m->file_number_ == file_number);
  }
  commit_in_progress_ = false;
  return s;
}

// new memtables are inserted at the front of the list.
void memtablelist::add(memtable* m) {
  assert(current_->size_ >= num_flush_not_started_);
  installnewversion();
  // this method is used to move mutable memtable into an immutable list.
  // since mutable memtable is already refcounted by the dbimpl,
  // and when moving to the imutable list we don't unref it,
  // we don't have to ref the memtable here. we just take over the
  // reference from the dbimpl.
  current_->add(m);
  m->markimmutable();
  num_flush_not_started_++;
  if (num_flush_not_started_ == 1) {
    imm_flush_needed.release_store((void *)1);
  }
}

// returns an estimate of the number of bytes of data in use.
size_t memtablelist::approximatememoryusage() {
  size_t size = 0;
  for (auto& memtable : current_->memlist_) {
    size += memtable->approximatememoryusage();
  }
  return size;
}

void memtablelist::installnewversion() {
  if (current_->refs_ == 1) {
    // we're the only one using the version, just keep using it
  } else {
    // somebody else holds the current version, we need to create new one
    memtablelistversion* version = current_;
    current_ = new memtablelistversion(current_);
    current_->ref();
    version->unref();
  }
}

}  // namespace rocksdb
