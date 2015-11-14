//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "db/table_cache.h"

#include "db/filename.h"
#include "db/version_edit.h"

#include "rocksdb/statistics.h"
#include "table/iterator_wrapper.h"
#include "table/table_reader.h"
#include "util/coding.h"
#include "util/stop_watch.h"

namespace rocksdb {

static void deleteentry(const slice& key, void* value) {
  tablereader* table_reader = reinterpret_cast<tablereader*>(value);
  delete table_reader;
}

static void unrefentry(void* arg1, void* arg2) {
  cache* cache = reinterpret_cast<cache*>(arg1);
  cache::handle* h = reinterpret_cast<cache::handle*>(arg2);
  cache->release(h);
}

static slice getsliceforfilenumber(const uint64_t* file_number) {
  return slice(reinterpret_cast<const char*>(file_number),
               sizeof(*file_number));
}

tablecache::tablecache(const options* options,
                       const envoptions& storage_options, cache* const cache)
    : env_(options->env),
      db_paths_(options->db_paths),
      options_(options),
      storage_options_(storage_options),
      cache_(cache) {}

tablecache::~tablecache() {
}

tablereader* tablecache::gettablereaderfromhandle(cache::handle* handle) {
  return reinterpret_cast<tablereader*>(cache_->value(handle));
}

void tablecache::releasehandle(cache::handle* handle) {
  cache_->release(handle);
}

status tablecache::findtable(const envoptions& toptions,
                             const internalkeycomparator& internal_comparator,
                             const filedescriptor& fd, cache::handle** handle,
                             const bool no_io) {
  status s;
  uint64_t number = fd.getnumber();
  slice key = getsliceforfilenumber(&number);
  *handle = cache_->lookup(key);
  if (*handle == nullptr) {
    if (no_io) { // dont do io and return a not-found status
      return status::incomplete("table not found in table_cache, no_io is set");
    }
    std::string fname =
        tablefilename(db_paths_, fd.getnumber(), fd.getpathid());
    unique_ptr<randomaccessfile> file;
    unique_ptr<tablereader> table_reader;
    s = env_->newrandomaccessfile(fname, &file, toptions);
    recordtick(options_->statistics.get(), no_file_opens);
    if (s.ok()) {
      if (options_->advise_random_on_open) {
        file->hint(randomaccessfile::random);
      }
      stopwatch sw(env_, options_->statistics.get(), table_open_io_micros);
      s = options_->table_factory->newtablereader(
          *options_, toptions, internal_comparator, std::move(file),
          fd.getfilesize(), &table_reader);
    }

    if (!s.ok()) {
      assert(table_reader == nullptr);
      recordtick(options_->statistics.get(), no_file_errors);
      // we do not cache error results so that if the error is transient,
      // or somebody repairs the file, we recover automatically.
    } else {
      assert(file.get() == nullptr);
      *handle = cache_->insert(key, table_reader.release(), 1, &deleteentry);
    }
  }
  return s;
}

iterator* tablecache::newiterator(const readoptions& options,
                                  const envoptions& toptions,
                                  const internalkeycomparator& icomparator,
                                  const filedescriptor& fd,
                                  tablereader** table_reader_ptr,
                                  bool for_compaction, arena* arena) {
  if (table_reader_ptr != nullptr) {
    *table_reader_ptr = nullptr;
  }
  tablereader* table_reader = fd.table_reader;
  cache::handle* handle = nullptr;
  status s;
  if (table_reader == nullptr) {
    s = findtable(toptions, icomparator, fd, &handle,
                  options.read_tier == kblockcachetier);
    if (!s.ok()) {
      return newerroriterator(s, arena);
    }
    table_reader = gettablereaderfromhandle(handle);
  }

  iterator* result = table_reader->newiterator(options, arena);
  if (handle != nullptr) {
    result->registercleanup(&unrefentry, cache_, handle);
  }
  if (table_reader_ptr != nullptr) {
    *table_reader_ptr = table_reader;
  }

  if (for_compaction) {
    table_reader->setupforcompaction();
  }

  return result;
}

status tablecache::get(const readoptions& options,
                       const internalkeycomparator& internal_comparator,
                       const filedescriptor& fd, const slice& k, void* arg,
                       bool (*saver)(void*, const parsedinternalkey&,
                                     const slice&),
                       void (*mark_key_may_exist)(void*)) {
  tablereader* t = fd.table_reader;
  status s;
  cache::handle* handle = nullptr;
  if (!t) {
    s = findtable(storage_options_, internal_comparator, fd, &handle,
                  options.read_tier == kblockcachetier);
    if (s.ok()) {
      t = gettablereaderfromhandle(handle);
    }
  }
  if (s.ok()) {
    s = t->get(options, k, arg, saver, mark_key_may_exist);
    if (handle != nullptr) {
      releasehandle(handle);
    }
  } else if (options.read_tier && s.isincomplete()) {
    // couldnt find table in cache but treat as kfound if no_io set
    (*mark_key_may_exist)(arg);
    return status::ok();
  }
  return s;
}
status tablecache::gettableproperties(
    const envoptions& toptions,
    const internalkeycomparator& internal_comparator, const filedescriptor& fd,
    std::shared_ptr<const tableproperties>* properties, bool no_io) {
  status s;
  auto table_reader = fd.table_reader;
  // table already been pre-loaded?
  if (table_reader) {
    *properties = table_reader->gettableproperties();

    return s;
  }

  cache::handle* table_handle = nullptr;
  s = findtable(toptions, internal_comparator, fd, &table_handle, no_io);
  if (!s.ok()) {
    return s;
  }
  assert(table_handle);
  auto table = gettablereaderfromhandle(table_handle);
  *properties = table->gettableproperties();
  releasehandle(table_handle);
  return s;
}

size_t tablecache::getmemoryusagebytablereader(
    const envoptions& toptions,
    const internalkeycomparator& internal_comparator,
    const filedescriptor& fd) {
  status s;
  auto table_reader = fd.table_reader;
  // table already been pre-loaded?
  if (table_reader) {
    return table_reader->approximatememoryusage();
  }

  cache::handle* table_handle = nullptr;
  s = findtable(toptions, internal_comparator, fd, &table_handle, true);
  if (!s.ok()) {
    return 0;
  }
  assert(table_handle);
  auto table = gettablereaderfromhandle(table_handle);
  auto ret = table->approximatememoryusage();
  releasehandle(table_handle);
  return ret;
}

void tablecache::evict(cache* cache, uint64_t file_number) {
  cache->erase(getsliceforfilenumber(&file_number));
}

}  // namespace rocksdb
