//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#pragma once

#include <unordered_map>
#include <string>
#include <vector>
#include <atomic>

#include "rocksdb/options.h"
#include "rocksdb/db.h"
#include "rocksdb/env.h"
#include "db/memtable_list.h"
#include "db/write_batch_internal.h"
#include "db/table_cache.h"
#include "util/thread_local.h"

namespace rocksdb {

class version;
class versionset;
class memtable;
class memtablelistversion;
class compactionpicker;
class compaction;
class internalkey;
class internalstats;
class columnfamilydata;
class dbimpl;
class logbuffer;

// columnfamilyhandleimpl is the class that clients use to access different
// column families. it has non-trivial destructor, which gets called when client
// is done using the column family
class columnfamilyhandleimpl : public columnfamilyhandle {
 public:
  // create while holding the mutex
  columnfamilyhandleimpl(columnfamilydata* cfd, dbimpl* db, port::mutex* mutex);
  // destroy without mutex
  virtual ~columnfamilyhandleimpl();
  virtual columnfamilydata* cfd() const { return cfd_; }

  virtual uint32_t getid() const;

 private:
  columnfamilydata* cfd_;
  dbimpl* db_;
  port::mutex* mutex_;
};

// does not ref-count columnfamilydata
// we use this dummy columnfamilyhandleimpl because sometimes memtableinserter
// calls dbimpl methods. when this happens, memtableinserter need access to
// columnfamilyhandle (same as the client would need). in that case, we feed
// memtableinserter dummy columnfamilyhandle and enable it to call dbimpl
// methods
class columnfamilyhandleinternal : public columnfamilyhandleimpl {
 public:
  columnfamilyhandleinternal()
      : columnfamilyhandleimpl(nullptr, nullptr, nullptr) {}

  void setcfd(columnfamilydata* cfd) { internal_cfd_ = cfd; }
  virtual columnfamilydata* cfd() const override { return internal_cfd_; }

 private:
  columnfamilydata* internal_cfd_;
};

// holds references to memtable, all immutable memtables and version
struct superversion {
  memtable* mem;
  memtablelistversion* imm;
  version* current;
  std::atomic<uint32_t> refs;
  // we need to_delete because during cleanup(), imm->unref() returns
  // all memtables that we need to free through this vector. we then
  // delete all those memtables outside of mutex, during destruction
  autovector<memtable*> to_delete;
  // version number of the current superversion
  uint64_t version_number;
  port::mutex* db_mutex;

  // should be called outside the mutex
  superversion() = default;
  ~superversion();
  superversion* ref();

  bool unref();

  // call these two methods with db mutex held
  // cleanup unrefs mem, imm and current. also, it stores all memtables
  // that needs to be deleted in to_delete vector. unrefing those
  // objects needs to be done in the mutex
  void cleanup();
  void init(memtable* new_mem, memtablelistversion* new_imm,
            version* new_current);

  // the value of dummy is not actually used. ksvinuse takes its address as a
  // mark in the thread local storage to indicate the superversion is in use
  // by thread. this way, the value of ksvinuse is guaranteed to have no
  // conflict with superversion object address and portable on different
  // platform.
  static int dummy;
  static void* const ksvinuse;
  static void* const ksvobsolete;
};

extern columnfamilyoptions sanitizeoptions(const internalkeycomparator* icmp,
                                           const columnfamilyoptions& src);

class columnfamilyset;

// this class keeps all the data that a column family needs. it's mosly dumb and
// used just to provide access to metadata.
// most methods require db mutex held, unless otherwise noted
class columnfamilydata {
 public:
  ~columnfamilydata();

  // thread-safe
  uint32_t getid() const { return id_; }
  // thread-safe
  const std::string& getname() const { return name_; }

  void ref() { ++refs_; }
  // will just decrease reference count to 0, but will not delete it. returns
  // true if the ref count was decreased to zero. in that case, it can be
  // deleted by the caller immediatelly, or later, by calling
  // freedeadcolumnfamilies()
  bool unref() {
    assert(refs_ > 0);
    return --refs_ == 0;
  }

  // this can only be called from single-threaded versionset::logandapply()
  // after dropping column family no other operation on that column family
  // will be executed. all the files and memory will be, however, kept around
  // until client drops the column family handle. that way, client can still
  // access data from dropped column family.
  // column family can be dropped and still alive. in that state:
  // *) column family is not included in the iteration.
  // *) compaction and flush is not executed on the dropped column family.
  // *) client can continue writing and reading from column family. however, all
  // writes stay in the current memtable.
  // when the dropped column family is unreferenced, then we:
  // *) delete all memory associated with that column family
  // *) delete all the files associated with that column family
  void setdropped() {
    // can't drop default cf
    assert(id_ != 0);
    dropped_ = true;
  }
  bool isdropped() const { return dropped_; }

  // thread-safe
  int numberlevels() const { return options_.num_levels; }

  void setlognumber(uint64_t log_number) { log_number_ = log_number; }
  uint64_t getlognumber() const { return log_number_; }

  // thread-safe
  const options* options() const { return &options_; }
  const envoptions* soptions() const;

  internalstats* internal_stats() { return internal_stats_.get(); }

  memtablelist* imm() { return &imm_; }
  memtable* mem() { return mem_; }
  version* current() { return current_; }
  version* dummy_versions() { return dummy_versions_; }
  void setmemtable(memtable* new_mem) { mem_ = new_mem; }
  void setcurrent(version* current);
  void createnewmemtable();

  tablecache* table_cache() const { return table_cache_.get(); }

  // see documentation in compaction_picker.h
  compaction* pickcompaction(logbuffer* log_buffer);
  compaction* compactrange(int input_level, int output_level,
                           uint32_t output_path_id, const internalkey* begin,
                           const internalkey* end,
                           internalkey** compaction_end);

  compactionpicker* compaction_picker() { return compaction_picker_.get(); }
  // thread-safe
  const comparator* user_comparator() const {
    return internal_comparator_.user_comparator();
  }
  // thread-safe
  const internalkeycomparator& internal_comparator() const {
    return internal_comparator_;
  }

  superversion* getsuperversion() { return super_version_; }
  // thread-safe
  // return a already referenced superversion to be used safely.
  superversion* getreferencedsuperversion(port::mutex* db_mutex);
  // thread-safe
  // get superversion stored in thread local storage. if it does not exist,
  // get a reference from a current superversion.
  superversion* getthreadlocalsuperversion(port::mutex* db_mutex);
  // try to return superversion back to thread local storage. retrun true on
  // success and false on failure. it fails when the thread local storage
  // contains anything other than superversion::ksvinuse flag.
  bool returnthreadlocalsuperversion(superversion* sv);
  // thread-safe
  uint64_t getsuperversionnumber() const {
    return super_version_number_.load();
  }
  // will return a pointer to superversion* if previous superversion
  // if its reference count is zero and needs deletion or nullptr if not
  // as argument takes a pointer to allocated superversion to enable
  // the clients to allocate superversion outside of mutex.
  superversion* installsuperversion(superversion* new_superversion,
                                    port::mutex* db_mutex);

  void resetthreadlocalsuperversions();

  // a flag indicating whether write needs to slowdown because of there are
  // too many number of level0 files.
  bool needslowdownfornumlevel0files() const {
    return need_slowdown_for_num_level0_files_;
  }

  bool needwaitfornumlevel0files() const {
    return need_wait_for_num_level0_files_;
  }

  bool needwaitfornummemtables() const {
    return need_wait_for_num_memtables_;
  }

  bool exceedssoftratelimit() const {
    return exceeds_soft_rate_limit_;
  }

  bool exceedshardratelimit() const {
    return exceeds_hard_rate_limit_;
  }

 private:
  friend class columnfamilyset;
  columnfamilydata(uint32_t id, const std::string& name,
                   version* dummy_versions, cache* table_cache,
                   const columnfamilyoptions& options,
                   const dboptions* db_options,
                   const envoptions& storage_options,
                   columnfamilyset* column_family_set);

  // recalculate some small conditions, which are changed only during
  // compaction, adding new memtable and/or
  // recalculation of compaction score. these values are used in
  // dbimpl::makeroomforwrite function to decide, if it need to make
  // a write stall
  void recalculatewritestallconditions();
  void recalculatewritestallratelimitsconditions();

  uint32_t id_;
  const std::string name_;
  version* dummy_versions_;  // head of circular doubly-linked list of versions.
  version* current_;         // == dummy_versions->prev_

  int refs_;                   // outstanding references to columnfamilydata
  bool dropped_;               // true if client dropped it

  const internalkeycomparator internal_comparator_;

  options const options_;

  std::unique_ptr<tablecache> table_cache_;

  std::unique_ptr<internalstats> internal_stats_;

  memtable* mem_;
  memtablelist imm_;
  superversion* super_version_;

  // an ordinal representing the current superversion. updated by
  // installsuperversion(), i.e. incremented every time super_version_
  // changes.
  std::atomic<uint64_t> super_version_number_;

  // thread's local copy of superversion pointer
  // this needs to be destructed before mutex_
  std::unique_ptr<threadlocalptr> local_sv_;

  // pointers for a circular linked list. we use it to support iterations
  // that can be concurrent with writes
  columnfamilydata* next_;
  columnfamilydata* prev_;

  // this is the earliest log file number that contains data from this
  // column family. all earlier log files must be ignored and not
  // recovered from
  uint64_t log_number_;

  // a flag indicating whether we should delay writes because
  // we have too many level 0 files
  bool need_slowdown_for_num_level0_files_;

  // these 4 variables are updated only after compaction,
  // adding new memtable, flushing memtables to files
  // and/or add recalculation of compaction score.
  // that's why theirs values are cached in columnfamilydata.
  // recalculation is made by recalculatewritestallconditions and
  // recalculatewritestallratelimitsconditions function. they are used
  // in dbimpl::makeroomforwrite function to decide, if it need
  // to sleep during write operation
  bool need_wait_for_num_memtables_;

  bool need_wait_for_num_level0_files_;

  bool exceeds_hard_rate_limit_;

  bool exceeds_soft_rate_limit_;

  // an object that keeps all the compaction stats
  // and picks the next compaction
  std::unique_ptr<compactionpicker> compaction_picker_;

  columnfamilyset* column_family_set_;
};

// columnfamilyset has interesting thread-safety requirements
// * createcolumnfamily() or removecolumnfamily() -- need to protect by db
// mutex. inside, column_family_data_ and column_families_ will be protected
// by lock() and unlock(). createcolumnfamily() should only be called from
// versionset::logandapply() in the normal runtime. it is also called
// during recovery and in dumpmanifest(). removecolumnfamily() is called
// from columnfamilydata destructor
// * iteration -- hold db mutex, but you can release it in the body of
// iteration. if you release db mutex in body, reference the column
// family before the mutex and unreference after you unlock, since the column
// family might get dropped when the db mutex is released
// * getdefault() -- thread safe
// * getcolumnfamily() -- either inside of db mutex or call lock() <-> unlock()
// * getnextcolumnfamilyid(), getmaxcolumnfamily(), updatemaxcolumnfamily(),
// numberofcolumnfamilies -- inside of db mutex
class columnfamilyset {
 public:
  // columnfamilyset supports iteration
  class iterator {
   public:
    explicit iterator(columnfamilydata* cfd)
        : current_(cfd) {}
    iterator& operator++() {
      // dummy is never dead or dropped, so this will never be infinite
      do {
        current_ = current_->next_;
      } while (current_->refs_ == 0 || current_->isdropped());
      return *this;
    }
    bool operator!=(const iterator& other) {
      return this->current_ != other.current_;
    }
    columnfamilydata* operator*() { return current_; }

   private:
    columnfamilydata* current_;
  };

  columnfamilyset(const std::string& dbname, const dboptions* db_options,
                  const envoptions& storage_options, cache* table_cache);
  ~columnfamilyset();

  columnfamilydata* getdefault() const;
  // getcolumnfamily() calls return nullptr if column family is not found
  columnfamilydata* getcolumnfamily(uint32_t id) const;
  columnfamilydata* getcolumnfamily(const std::string& name) const;
  // this call will return the next available column family id. it guarantees
  // that there is no column family with id greater than or equal to the
  // returned value in the current running instance or anytime in rocksdb
  // instance history.
  uint32_t getnextcolumnfamilyid();
  uint32_t getmaxcolumnfamily();
  void updatemaxcolumnfamily(uint32_t new_max_column_family);
  size_t numberofcolumnfamilies() const;

  columnfamilydata* createcolumnfamily(const std::string& name, uint32_t id,
                                       version* dummy_version,
                                       const columnfamilyoptions& options);

  iterator begin() { return iterator(dummy_cfd_->next_); }
  iterator end() { return iterator(dummy_cfd_); }

  void lock();
  void unlock();

  // requires: db mutex held
  // don't call while iterating over columnfamilyset
  void freedeadcolumnfamilies();

 private:
  friend class columnfamilydata;
  // helper function that gets called from cfd destructor
  // requires: db mutex held
  void removecolumnfamily(columnfamilydata* cfd);

  // column_families_ and column_family_data_ need to be protected:
  // * when mutating: 1. db mutex locked first, 2. spinlock locked second
  // * when reading, either: 1. lock db mutex, or 2. lock spinlock
  //  (if both, respect the ordering to avoid deadlock!)
  std::unordered_map<std::string, uint32_t> column_families_;
  std::unordered_map<uint32_t, columnfamilydata*> column_family_data_;

  uint32_t max_column_family_;
  columnfamilydata* dummy_cfd_;
  // we don't hold the refcount here, since default column family always exists
  // we are also not responsible for cleaning up default_cfd_cache_. this is
  // just a cache that makes common case (accessing default column family)
  // faster
  columnfamilydata* default_cfd_cache_;

  const std::string db_name_;
  const dboptions* const db_options_;
  const envoptions storage_options_;
  cache* table_cache_;
  std::atomic_flag spin_lock_;
};

// we use columnfamilymemtablesimpl to provide writebatch a way to access
// memtables of different column families (specified by id in the write batch)
class columnfamilymemtablesimpl : public columnfamilymemtables {
 public:
  explicit columnfamilymemtablesimpl(columnfamilyset* column_family_set)
      : column_family_set_(column_family_set), current_(nullptr) {}

  // sets current_ to columnfamilydata with column_family_id
  // returns false if column family doesn't exist
  bool seek(uint32_t column_family_id) override;

  // returns log number of the selected column family
  uint64_t getlognumber() const override;

  // requires: seek() called first
  virtual memtable* getmemtable() const override;

  // returns options for selected column family
  // requires: seek() called first
  virtual const options* getoptions() const override;

  // returns column family handle for the selected column family
  virtual columnfamilyhandle* getcolumnfamilyhandle() override;

 private:
  columnfamilyset* column_family_set_;
  columnfamilydata* current_;
  columnfamilyhandleinternal handle_;
};

extern uint32_t getcolumnfamilyid(columnfamilyhandle* column_family);

}  // namespace rocksdb
