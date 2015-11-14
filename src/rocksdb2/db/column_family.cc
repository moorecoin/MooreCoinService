//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "db/column_family.h"

#include <vector>
#include <string>
#include <algorithm>
#include <limits>

#include "db/db_impl.h"
#include "db/version_set.h"
#include "db/internal_stats.h"
#include "db/compaction_picker.h"
#include "db/table_properties_collector.h"
#include "util/autovector.h"
#include "util/hash_skiplist_rep.h"

namespace rocksdb {

columnfamilyhandleimpl::columnfamilyhandleimpl(columnfamilydata* cfd,
                                               dbimpl* db, port::mutex* mutex)
    : cfd_(cfd), db_(db), mutex_(mutex) {
  if (cfd_ != nullptr) {
    cfd_->ref();
  }
}

columnfamilyhandleimpl::~columnfamilyhandleimpl() {
  if (cfd_ != nullptr) {
    dbimpl::deletionstate deletion_state;
    mutex_->lock();
    if (cfd_->unref()) {
      delete cfd_;
    }
    db_->findobsoletefiles(deletion_state, false, true);
    mutex_->unlock();
    if (deletion_state.havesomethingtodelete()) {
      db_->purgeobsoletefiles(deletion_state);
    }
  }
}

uint32_t columnfamilyhandleimpl::getid() const { return cfd()->getid(); }

columnfamilyoptions sanitizeoptions(const internalkeycomparator* icmp,
                                    const columnfamilyoptions& src) {
  columnfamilyoptions result = src;
  result.comparator = icmp;
#ifdef os_macosx
  // todo(icanadi) make write_buffer_size uint64_t instead of size_t
  cliptorange(&result.write_buffer_size, ((size_t)64) << 10, ((size_t)1) << 30);
#else
  cliptorange(&result.write_buffer_size,
              ((size_t)64) << 10, ((size_t)64) << 30);
#endif
  // if user sets arena_block_size, we trust user to use this value. otherwise,
  // calculate a proper value from writer_buffer_size;
  if (result.arena_block_size <= 0) {
    result.arena_block_size = result.write_buffer_size / 10;
  }
  result.min_write_buffer_number_to_merge =
      std::min(result.min_write_buffer_number_to_merge,
               result.max_write_buffer_number - 1);
  result.compression_per_level = src.compression_per_level;
  if (result.max_mem_compaction_level >= result.num_levels) {
    result.max_mem_compaction_level = result.num_levels - 1;
  }
  if (result.soft_rate_limit > result.hard_rate_limit) {
    result.soft_rate_limit = result.hard_rate_limit;
  }
  if (result.max_write_buffer_number < 2) {
    result.max_write_buffer_number = 2;
  }
  if (!result.prefix_extractor) {
    assert(result.memtable_factory);
    slice name = result.memtable_factory->name();
    if (name.compare("hashskiplistrepfactory") == 0 ||
        name.compare("hashlinklistrepfactory") == 0) {
      result.memtable_factory = std::make_shared<skiplistfactory>();
    }
  }

  // -- sanitize the table properties collector
  // all user defined properties collectors will be wrapped by
  // userkeytablepropertiescollector since for them they only have the
  // knowledge of the user keys; internal keys are invisible to them.
  auto& collector_factories = result.table_properties_collector_factories;
  for (size_t i = 0; i < result.table_properties_collector_factories.size();
       ++i) {
    assert(collector_factories[i]);
    collector_factories[i] =
        std::make_shared<userkeytablepropertiescollectorfactory>(
            collector_factories[i]);
  }
  // add collector to collect internal key statistics
  collector_factories.push_back(
      std::make_shared<internalkeypropertiescollectorfactory>());

  if (result.compaction_style == kcompactionstylefifo) {
    result.num_levels = 1;
    // since we delete level0 files in fifo compaction when there are too many
    // of them, these options don't really mean anything
    result.level0_file_num_compaction_trigger = std::numeric_limits<int>::max();
    result.level0_slowdown_writes_trigger = std::numeric_limits<int>::max();
    result.level0_stop_writes_trigger = std::numeric_limits<int>::max();
  }

  return result;
}

int superversion::dummy = 0;
void* const superversion::ksvinuse = &superversion::dummy;
void* const superversion::ksvobsolete = nullptr;

superversion::~superversion() {
  for (auto td : to_delete) {
    delete td;
  }
}

superversion* superversion::ref() {
  refs.fetch_add(1, std::memory_order_relaxed);
  return this;
}

bool superversion::unref() {
  // fetch_sub returns the previous value of ref
  uint32_t previous_refs = refs.fetch_sub(1, std::memory_order_relaxed);
  assert(previous_refs > 0);
  return previous_refs == 1;
}

void superversion::cleanup() {
  assert(refs.load(std::memory_order_relaxed) == 0);
  imm->unref(&to_delete);
  memtable* m = mem->unref();
  if (m != nullptr) {
    to_delete.push_back(m);
  }
  current->unref();
}

void superversion::init(memtable* new_mem, memtablelistversion* new_imm,
                        version* new_current) {
  mem = new_mem;
  imm = new_imm;
  current = new_current;
  mem->ref();
  imm->ref();
  current->ref();
  refs.store(1, std::memory_order_relaxed);
}

namespace {
void superversionunrefhandle(void* ptr) {
  // unrefhandle is called when a thread exists or a threadlocalptr gets
  // destroyed. when former happens, the thread shouldn't see ksvinuse.
  // when latter happens, we are in ~columnfamilydata(), no get should happen as
  // well.
  superversion* sv = static_cast<superversion*>(ptr);
  if (sv->unref()) {
    sv->db_mutex->lock();
    sv->cleanup();
    sv->db_mutex->unlock();
    delete sv;
  }
}
}  // anonymous namespace

columnfamilydata::columnfamilydata(uint32_t id, const std::string& name,
                                   version* dummy_versions, cache* table_cache,
                                   const columnfamilyoptions& options,
                                   const dboptions* db_options,
                                   const envoptions& storage_options,
                                   columnfamilyset* column_family_set)
    : id_(id),
      name_(name),
      dummy_versions_(dummy_versions),
      current_(nullptr),
      refs_(0),
      dropped_(false),
      internal_comparator_(options.comparator),
      options_(*db_options, sanitizeoptions(&internal_comparator_, options)),
      mem_(nullptr),
      imm_(options_.min_write_buffer_number_to_merge),
      super_version_(nullptr),
      super_version_number_(0),
      local_sv_(new threadlocalptr(&superversionunrefhandle)),
      next_(nullptr),
      prev_(nullptr),
      log_number_(0),
      need_slowdown_for_num_level0_files_(false),
      column_family_set_(column_family_set) {
  ref();

  // if dummy_versions is nullptr, then this is a dummy column family.
  if (dummy_versions != nullptr) {
    internal_stats_.reset(
        new internalstats(options_.num_levels, db_options->env, this));
    table_cache_.reset(new tablecache(&options_, storage_options, table_cache));
    if (options_.compaction_style == kcompactionstyleuniversal) {
      compaction_picker_.reset(
          new universalcompactionpicker(&options_, &internal_comparator_));
    } else if (options_.compaction_style == kcompactionstylelevel) {
      compaction_picker_.reset(
          new levelcompactionpicker(&options_, &internal_comparator_));
    } else {
      assert(options_.compaction_style == kcompactionstylefifo);
      compaction_picker_.reset(
          new fifocompactionpicker(&options_, &internal_comparator_));
    }

    log(options_.info_log, "options for column family \"%s\":\n",
        name.c_str());
    const columnfamilyoptions* cf_options = &options_;
    cf_options->dump(options_.info_log.get());
  }

  recalculatewritestallconditions();
}

// db mutex held
columnfamilydata::~columnfamilydata() {
  assert(refs_ == 0);
  // remove from linked list
  auto prev = prev_;
  auto next = next_;
  prev->next_ = next;
  next->prev_ = prev;

  // it's nullptr for dummy cfd
  if (column_family_set_ != nullptr) {
    // remove from column_family_set
    column_family_set_->removecolumnfamily(this);
  }

  if (current_ != nullptr) {
    current_->unref();
  }

  if (super_version_ != nullptr) {
    // release superversion reference kept in threadlocalptr.
    // this must be done outside of mutex_ since unref handler can lock mutex.
    super_version_->db_mutex->unlock();
    local_sv_.reset();
    super_version_->db_mutex->lock();

    bool is_last_reference __attribute__((unused));
    is_last_reference = super_version_->unref();
    assert(is_last_reference);
    super_version_->cleanup();
    delete super_version_;
    super_version_ = nullptr;
  }

  if (dummy_versions_ != nullptr) {
    // list must be empty
    assert(dummy_versions_->next_ == dummy_versions_);
    delete dummy_versions_;
  }

  if (mem_ != nullptr) {
    delete mem_->unref();
  }
  autovector<memtable*> to_delete;
  imm_.current()->unref(&to_delete);
  for (memtable* m : to_delete) {
    delete m;
  }
}

void columnfamilydata::recalculatewritestallconditions() {
  need_wait_for_num_memtables_ =
    (imm()->size() == options()->max_write_buffer_number - 1);

  if (current_ != nullptr) {
    need_wait_for_num_level0_files_ =
      (current_->numlevelfiles(0) >= options()->level0_stop_writes_trigger);
  } else {
    need_wait_for_num_level0_files_ = false;
  }

  recalculatewritestallratelimitsconditions();
}

void columnfamilydata::recalculatewritestallratelimitsconditions() {
  if (current_ != nullptr) {
    exceeds_hard_rate_limit_ =
        (options()->hard_rate_limit > 1.0 &&
         current_->maxcompactionscore() > options()->hard_rate_limit);

    exceeds_soft_rate_limit_ =
        (options()->soft_rate_limit > 0.0 &&
         current_->maxcompactionscore() > options()->soft_rate_limit);
  } else {
    exceeds_hard_rate_limit_ = false;
    exceeds_soft_rate_limit_ = false;
  }
}

const envoptions* columnfamilydata::soptions() const {
  return &(column_family_set_->storage_options_);
}

void columnfamilydata::setcurrent(version* current) {
  current_ = current;
  need_slowdown_for_num_level0_files_ =
      (options_.level0_slowdown_writes_trigger >= 0 &&
       current_->numlevelfiles(0) >= options_.level0_slowdown_writes_trigger);
}

void columnfamilydata::createnewmemtable() {
  assert(current_ != nullptr);
  if (mem_ != nullptr) {
    delete mem_->unref();
  }
  mem_ = new memtable(internal_comparator_, options_);
  mem_->ref();
}

compaction* columnfamilydata::pickcompaction(logbuffer* log_buffer) {
  auto result = compaction_picker_->pickcompaction(current_, log_buffer);
  recalculatewritestallratelimitsconditions();
  return result;
}

compaction* columnfamilydata::compactrange(int input_level, int output_level,
                                           uint32_t output_path_id,
                                           const internalkey* begin,
                                           const internalkey* end,
                                           internalkey** compaction_end) {
  return compaction_picker_->compactrange(current_, input_level, output_level,
                                          output_path_id, begin, end,
                                          compaction_end);
}

superversion* columnfamilydata::getreferencedsuperversion(
    port::mutex* db_mutex) {
  superversion* sv = nullptr;
  if (likely(column_family_set_->db_options_->allow_thread_local)) {
    sv = getthreadlocalsuperversion(db_mutex);
    sv->ref();
    if (!returnthreadlocalsuperversion(sv)) {
      sv->unref();
    }
  } else {
    db_mutex->lock();
    sv = super_version_->ref();
    db_mutex->unlock();
  }
  return sv;
}

superversion* columnfamilydata::getthreadlocalsuperversion(
    port::mutex* db_mutex) {
  superversion* sv = nullptr;
  // the superversion is cached in thread local storage to avoid acquiring
  // mutex when superversion does not change since the last use. when a new
  // superversion is installed, the compaction or flush thread cleans up
  // cached superversion in all existing thread local storage. to avoid
  // acquiring mutex for this operation, we use atomic swap() on the thread
  // local pointer to guarantee exclusive access. if the thread local pointer
  // is being used while a new superversion is installed, the cached
  // superversion can become stale. in that case, the background thread would
  // have swapped in ksvobsolete. we re-check the value at when returning
  // superversion back to thread local, with an atomic compare and swap.
  // the superversion will need to be released if detected to be stale.
  void* ptr = local_sv_->swap(superversion::ksvinuse);
  // invariant:
  // (1) scrape (always) installs ksvobsolete in threadlocal storage
  // (2) the swap above (always) installs ksvinuse, threadlocal storage
  // should only keep ksvinuse before returnthreadlocalsuperversion call
  // (if no scrape happens).
  assert(ptr != superversion::ksvinuse);
  sv = static_cast<superversion*>(ptr);
  if (sv == superversion::ksvobsolete ||
      sv->version_number != super_version_number_.load()) {
    recordtick(options_.statistics.get(), number_superversion_acquires);
    superversion* sv_to_delete = nullptr;

    if (sv && sv->unref()) {
      recordtick(options_.statistics.get(), number_superversion_cleanups);
      db_mutex->lock();
      // note: underlying resources held by superversion (sst files) might
      // not be released until the next background job.
      sv->cleanup();
      sv_to_delete = sv;
    } else {
      db_mutex->lock();
    }
    sv = super_version_->ref();
    db_mutex->unlock();

    delete sv_to_delete;
  }
  assert(sv != nullptr);
  return sv;
}

bool columnfamilydata::returnthreadlocalsuperversion(superversion* sv) {
  assert(sv != nullptr);
  // put the superversion back
  void* expected = superversion::ksvinuse;
  if (local_sv_->compareandswap(static_cast<void*>(sv), expected)) {
    // when we see ksvinuse in the threadlocal, we are sure threadlocal
    // storage has not been altered and no scrape has happend. the
    // superversion is still current.
    return true;
  } else {
    // threadlocal scrape happened in the process of this getimpl call (after
    // thread local swap() at the beginning and before compareandswap()).
    // this means the superversion it holds is obsolete.
    assert(expected == superversion::ksvobsolete);
  }
  return false;
}

superversion* columnfamilydata::installsuperversion(
    superversion* new_superversion, port::mutex* db_mutex) {
  new_superversion->db_mutex = db_mutex;
  new_superversion->init(mem_, imm_.current(), current_);
  superversion* old_superversion = super_version_;
  super_version_ = new_superversion;
  ++super_version_number_;
  super_version_->version_number = super_version_number_;
  // reset superversions cached in thread local storage
  if (column_family_set_->db_options_->allow_thread_local) {
    resetthreadlocalsuperversions();
  }

  recalculatewritestallconditions();

  if (old_superversion != nullptr && old_superversion->unref()) {
    old_superversion->cleanup();
    return old_superversion;  // will let caller delete outside of mutex
  }
  return nullptr;
}

void columnfamilydata::resetthreadlocalsuperversions() {
  autovector<void*> sv_ptrs;
  local_sv_->scrape(&sv_ptrs, superversion::ksvobsolete);
  for (auto ptr : sv_ptrs) {
    assert(ptr);
    if (ptr == superversion::ksvinuse) {
      continue;
    }
    auto sv = static_cast<superversion*>(ptr);
    if (sv->unref()) {
      sv->cleanup();
      delete sv;
    }
  }
}

columnfamilyset::columnfamilyset(const std::string& dbname,
                                 const dboptions* db_options,
                                 const envoptions& storage_options,
                                 cache* table_cache)
    : max_column_family_(0),
      dummy_cfd_(new columnfamilydata(0, "", nullptr, nullptr,
                                      columnfamilyoptions(), db_options,
                                      storage_options_, nullptr)),
      default_cfd_cache_(nullptr),
      db_name_(dbname),
      db_options_(db_options),
      storage_options_(storage_options),
      table_cache_(table_cache),
      spin_lock_(atomic_flag_init) {
  // initialize linked list
  dummy_cfd_->prev_ = dummy_cfd_;
  dummy_cfd_->next_ = dummy_cfd_;
}

columnfamilyset::~columnfamilyset() {
  while (column_family_data_.size() > 0) {
    // cfd destructor will delete itself from column_family_data_
    auto cfd = column_family_data_.begin()->second;
    cfd->unref();
    delete cfd;
  }
  dummy_cfd_->unref();
  delete dummy_cfd_;
}

columnfamilydata* columnfamilyset::getdefault() const {
  assert(default_cfd_cache_ != nullptr);
  return default_cfd_cache_;
}

columnfamilydata* columnfamilyset::getcolumnfamily(uint32_t id) const {
  auto cfd_iter = column_family_data_.find(id);
  if (cfd_iter != column_family_data_.end()) {
    return cfd_iter->second;
  } else {
    return nullptr;
  }
}

columnfamilydata* columnfamilyset::getcolumnfamily(const std::string& name)
    const {
  auto cfd_iter = column_families_.find(name);
  if (cfd_iter != column_families_.end()) {
    auto cfd = getcolumnfamily(cfd_iter->second);
    assert(cfd != nullptr);
    return cfd;
  } else {
    return nullptr;
  }
}

uint32_t columnfamilyset::getnextcolumnfamilyid() {
  return ++max_column_family_;
}

uint32_t columnfamilyset::getmaxcolumnfamily() { return max_column_family_; }

void columnfamilyset::updatemaxcolumnfamily(uint32_t new_max_column_family) {
  max_column_family_ = std::max(new_max_column_family, max_column_family_);
}

size_t columnfamilyset::numberofcolumnfamilies() const {
  return column_families_.size();
}

// under a db mutex
columnfamilydata* columnfamilyset::createcolumnfamily(
    const std::string& name, uint32_t id, version* dummy_versions,
    const columnfamilyoptions& options) {
  assert(column_families_.find(name) == column_families_.end());
  columnfamilydata* new_cfd =
      new columnfamilydata(id, name, dummy_versions, table_cache_, options,
                           db_options_, storage_options_, this);
  lock();
  column_families_.insert({name, id});
  column_family_data_.insert({id, new_cfd});
  unlock();
  max_column_family_ = std::max(max_column_family_, id);
  // add to linked list
  new_cfd->next_ = dummy_cfd_;
  auto prev = dummy_cfd_->prev_;
  new_cfd->prev_ = prev;
  prev->next_ = new_cfd;
  dummy_cfd_->prev_ = new_cfd;
  if (id == 0) {
    default_cfd_cache_ = new_cfd;
  }
  return new_cfd;
}

void columnfamilyset::lock() {
  // spin lock
  while (spin_lock_.test_and_set(std::memory_order_acquire)) {
  }
}

void columnfamilyset::unlock() { spin_lock_.clear(std::memory_order_release); }

// requires: db mutex held
void columnfamilyset::freedeadcolumnfamilies() {
  autovector<columnfamilydata*> to_delete;
  for (auto cfd = dummy_cfd_->next_; cfd != dummy_cfd_; cfd = cfd->next_) {
    if (cfd->refs_ == 0) {
      to_delete.push_back(cfd);
    }
  }
  for (auto cfd : to_delete) {
    // this is very rare, so it's not a problem that we do it under a mutex
    delete cfd;
  }
}

// under a db mutex
void columnfamilyset::removecolumnfamily(columnfamilydata* cfd) {
  auto cfd_iter = column_family_data_.find(cfd->getid());
  assert(cfd_iter != column_family_data_.end());
  lock();
  column_family_data_.erase(cfd_iter);
  column_families_.erase(cfd->getname());
  unlock();
}

bool columnfamilymemtablesimpl::seek(uint32_t column_family_id) {
  if (column_family_id == 0) {
    // optimization for common case
    current_ = column_family_set_->getdefault();
  } else {
    // maybe outside of db mutex, should lock
    column_family_set_->lock();
    current_ = column_family_set_->getcolumnfamily(column_family_id);
    column_family_set_->unlock();
  }
  handle_.setcfd(current_);
  return current_ != nullptr;
}

uint64_t columnfamilymemtablesimpl::getlognumber() const {
  assert(current_ != nullptr);
  return current_->getlognumber();
}

memtable* columnfamilymemtablesimpl::getmemtable() const {
  assert(current_ != nullptr);
  return current_->mem();
}

const options* columnfamilymemtablesimpl::getoptions() const {
  assert(current_ != nullptr);
  return current_->options();
}

columnfamilyhandle* columnfamilymemtablesimpl::getcolumnfamilyhandle() {
  assert(current_ != nullptr);
  return &handle_;
}

uint32_t getcolumnfamilyid(columnfamilyhandle* column_family) {
  uint32_t column_family_id = 0;
  if (column_family != nullptr) {
    auto cfh = reinterpret_cast<columnfamilyhandleimpl*>(column_family);
    column_family_id = cfh->getid();
  }
  return column_family_id;
}

}  // namespace rocksdb
