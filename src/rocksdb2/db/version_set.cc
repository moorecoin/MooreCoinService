//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "db/version_set.h"

#define __stdc_format_macros
#include <inttypes.h>
#include <algorithm>
#include <map>
#include <set>
#include <climits>
#include <unordered_map>
#include <vector>
#include <stdio.h>

#include "db/filename.h"
#include "db/log_reader.h"
#include "db/log_writer.h"
#include "db/memtable.h"
#include "db/merge_context.h"
#include "db/table_cache.h"
#include "db/compaction.h"
#include "rocksdb/env.h"
#include "rocksdb/merge_operator.h"
#include "table/table_reader.h"
#include "table/merger.h"
#include "table/two_level_iterator.h"
#include "table/format.h"
#include "table/plain_table_factory.h"
#include "table/meta_blocks.h"
#include "util/coding.h"
#include "util/logging.h"
#include "util/stop_watch.h"

namespace rocksdb {

namespace {

// find file in filelevel data structure
// within an index range defined by left and right
int findfileinrange(const internalkeycomparator& icmp,
    const filelevel& file_level,
    const slice& key,
    uint32_t left,
    uint32_t right) {
  while (left < right) {
    uint32_t mid = (left + right) / 2;
    const fdwithkeyrange& f = file_level.files[mid];
    if (icmp.internalkeycomparator::compare(f.largest_key, key) < 0) {
      // key at "mid.largest" is < "target".  therefore all
      // files at or before "mid" are uninteresting.
      left = mid + 1;
    } else {
      // key at "mid.largest" is >= "target".  therefore all files
      // after "mid" are uninteresting.
      right = mid;
    }
  }
  return right;
}

bool newestfirstbyseqno(filemetadata* a, filemetadata* b) {
  if (a->smallest_seqno != b->smallest_seqno) {
    return a->smallest_seqno > b->smallest_seqno;
  }
  if (a->largest_seqno != b->largest_seqno) {
    return a->largest_seqno > b->largest_seqno;
  }
  // break ties by file number
  return a->fd.getnumber() > b->fd.getnumber();
}

bool bysmallestkey(filemetadata* a, filemetadata* b,
                   const internalkeycomparator* cmp) {
  int r = cmp->compare(a->smallest, b->smallest);
  if (r != 0) {
    return (r < 0);
  }
  // break ties by file number
  return (a->fd.getnumber() < b->fd.getnumber());
}

// class to help choose the next file to search for the particular key.
// searches and returns files level by level.
// we can search level-by-level since entries never hop across
// levels. therefore we are guaranteed that if we find data
// in a smaller level, later levels are irrelevant (unless we
// are mergeinprogress).
class filepicker {
 public:
  filepicker(
      std::vector<filemetadata*>* files,
      const slice& user_key,
      const slice& ikey,
      autovector<filelevel>* file_levels,
      unsigned int num_levels,
      fileindexer* file_indexer,
      const comparator* user_comparator,
      const internalkeycomparator* internal_comparator)
      : num_levels_(num_levels),
        curr_level_(-1),
        search_left_bound_(0),
        search_right_bound_(fileindexer::klevelmaxindex),
#ifndef ndebug
        files_(files),
#endif
        file_levels_(file_levels),
        user_key_(user_key),
        ikey_(ikey),
        file_indexer_(file_indexer),
        user_comparator_(user_comparator),
        internal_comparator_(internal_comparator) {
    // setup member variables to search first level.
    search_ended_ = !preparenextlevel();
    if (!search_ended_) {
      // prefetch level 0 table data to avoid cache miss if possible.
      for (unsigned int i = 0; i < (*file_levels_)[0].num_files; ++i) {
        auto* r = (*file_levels_)[0].files[i].fd.table_reader;
        if (r) {
          r->prepare(ikey);
        }
      }
    }
  }

  fdwithkeyrange* getnextfile() {
    while (!search_ended_) {  // loops over different levels.
      while (curr_index_in_curr_level_ < curr_file_level_->num_files) {
        // loops over all files in current level.
        fdwithkeyrange* f = &curr_file_level_->files[curr_index_in_curr_level_];
        int cmp_largest = -1;

        // do key range filtering of files or/and fractional cascading if:
        // (1) not all the files are in level 0, or
        // (2) there are more than 3 level 0 files
        // if there are only 3 or less level 0 files in the system, we skip
        // the key range filtering. in this case, more likely, the system is
        // highly tuned to minimize number of tables queried by each query,
        // so it is unlikely that key range filtering is more efficient than
        // querying the files.
        if (num_levels_ > 1 || curr_file_level_->num_files > 3) {
          // check if key is within a file's range. if search left bound and
          // right bound point to the same find, we are sure key falls in
          // range.
          assert(
              curr_level_ == 0 ||
              curr_index_in_curr_level_ == start_index_in_curr_level_ ||
              user_comparator_->compare(user_key_,
                extractuserkey(f->smallest_key)) <= 0);

          int cmp_smallest = user_comparator_->compare(user_key_,
              extractuserkey(f->smallest_key));
          if (cmp_smallest >= 0) {
            cmp_largest = user_comparator_->compare(user_key_,
                extractuserkey(f->largest_key));
          }

          // setup file search bound for the next level based on the
          // comparison results
          if (curr_level_ > 0) {
            file_indexer_->getnextlevelindex(curr_level_,
                                            curr_index_in_curr_level_,
                                            cmp_smallest, cmp_largest,
                                            &search_left_bound_,
                                            &search_right_bound_);
          }
          // key falls out of current file's range
          if (cmp_smallest < 0 || cmp_largest > 0) {
            if (curr_level_ == 0) {
              ++curr_index_in_curr_level_;
              continue;
            } else {
              // search next level.
              break;
            }
          }
        }
#ifndef ndebug
        // sanity check to make sure that the files are correctly sorted
        if (prev_file_) {
          if (curr_level_ != 0) {
            int comp_sign = internal_comparator_->compare(
                prev_file_->largest_key, f->smallest_key);
            assert(comp_sign < 0);
          } else {
            // level == 0, the current file cannot be newer than the previous
            // one. use compressed data structure, has no attribute seqno
            assert(curr_index_in_curr_level_ > 0);
            assert(!newestfirstbyseqno(files_[0][curr_index_in_curr_level_],
                  files_[0][curr_index_in_curr_level_-1]));
          }
        }
        prev_file_ = f;
#endif
        if (curr_level_ > 0 && cmp_largest < 0) {
          // no more files to search in this level.
          search_ended_ = !preparenextlevel();
        } else {
          ++curr_index_in_curr_level_;
        }
        return f;
      }
      // start searching next level.
      search_ended_ = !preparenextlevel();
    }
    // search ended.
    return nullptr;
  }

 private:
  unsigned int num_levels_;
  unsigned int curr_level_;
  int search_left_bound_;
  int search_right_bound_;
#ifndef ndebug
  std::vector<filemetadata*>* files_;
#endif
  autovector<filelevel>* file_levels_;
  bool search_ended_;
  filelevel* curr_file_level_;
  unsigned int curr_index_in_curr_level_;
  unsigned int start_index_in_curr_level_;
  slice user_key_;
  slice ikey_;
  fileindexer* file_indexer_;
  const comparator* user_comparator_;
  const internalkeycomparator* internal_comparator_;
#ifndef ndebug
  fdwithkeyrange* prev_file_;
#endif

  // setup local variables to search next level.
  // returns false if there are no more levels to search.
  bool preparenextlevel() {
    curr_level_++;
    while (curr_level_ < num_levels_) {
      curr_file_level_ = &(*file_levels_)[curr_level_];
      if (curr_file_level_->num_files == 0) {
        // when current level is empty, the search bound generated from upper
        // level must be [0, -1] or [0, fileindexer::klevelmaxindex] if it is
        // also empty.
        assert(search_left_bound_ == 0);
        assert(search_right_bound_ == -1 ||
               search_right_bound_ == fileindexer::klevelmaxindex);
        // since current level is empty, it will need to search all files in
        // the next level
        search_left_bound_ = 0;
        search_right_bound_ = fileindexer::klevelmaxindex;
        curr_level_++;
        continue;
      }

      // some files may overlap each other. we find
      // all files that overlap user_key and process them in order from
      // newest to oldest. in the context of merge-operator, this can occur at
      // any level. otherwise, it only occurs at level-0 (since put/deletes
      // are always compacted into a single entry).
      int32_t start_index;
      if (curr_level_ == 0) {
        // on level-0, we read through all files to check for overlap.
        start_index = 0;
      } else {
        // on level-n (n>=1), files are sorted. binary search to find the
        // earliest file whose largest key >= ikey. search left bound and
        // right bound are used to narrow the range.
        if (search_left_bound_ == search_right_bound_) {
          start_index = search_left_bound_;
        } else if (search_left_bound_ < search_right_bound_) {
          if (search_right_bound_ == fileindexer::klevelmaxindex) {
            search_right_bound_ = curr_file_level_->num_files - 1;
          }
          start_index = findfileinrange(*internal_comparator_,
              *curr_file_level_, ikey_,
              search_left_bound_, search_right_bound_);
        } else {
          // search_left_bound > search_right_bound, key does not exist in
          // this level. since no comparision is done in this level, it will
          // need to search all files in the next level.
          search_left_bound_ = 0;
          search_right_bound_ = fileindexer::klevelmaxindex;
          curr_level_++;
          continue;
        }
      }
      start_index_in_curr_level_ = start_index;
      curr_index_in_curr_level_ = start_index;
#ifndef ndebug
      prev_file_ = nullptr;
#endif
      return true;
    }
    // curr_level_ = num_levels_. so, no more levels to search.
    return false;
  }
};
}  // anonymous namespace

version::~version() {
  assert(refs_ == 0);

  // remove from linked list
  prev_->next_ = next_;
  next_->prev_ = prev_;

  // drop references to files
  for (int level = 0; level < num_levels_; level++) {
    for (size_t i = 0; i < files_[level].size(); i++) {
      filemetadata* f = files_[level][i];
      assert(f->refs > 0);
      f->refs--;
      if (f->refs <= 0) {
        if (f->table_reader_handle) {
          cfd_->table_cache()->releasehandle(f->table_reader_handle);
          f->table_reader_handle = nullptr;
        }
        vset_->obsolete_files_.push_back(f);
      }
    }
  }
  delete[] files_;
}

int findfile(const internalkeycomparator& icmp,
             const filelevel& file_level,
             const slice& key) {
  return findfileinrange(icmp, file_level, key, 0, file_level.num_files);
}

void dogeneratefilelevel(filelevel* file_level,
        const std::vector<filemetadata*>& files,
        arena* arena) {
  assert(file_level);
  assert(files.size() >= 0);
  assert(arena);

  size_t num = files.size();
  file_level->num_files = num;
  char* mem = arena->allocatealigned(num * sizeof(fdwithkeyrange));
  file_level->files = new (mem)fdwithkeyrange[num];

  for (size_t i = 0; i < num; i++) {
    slice smallest_key = files[i]->smallest.encode();
    slice largest_key = files[i]->largest.encode();

    // copy key slice to sequential memory
    size_t smallest_size = smallest_key.size();
    size_t largest_size = largest_key.size();
    mem = arena->allocatealigned(smallest_size + largest_size);
    memcpy(mem, smallest_key.data(), smallest_size);
    memcpy(mem + smallest_size, largest_key.data(), largest_size);

    fdwithkeyrange& f = file_level->files[i];
    f.fd = files[i]->fd;
    f.smallest_key = slice(mem, smallest_size);
    f.largest_key = slice(mem + smallest_size, largest_size);
  }
}

static bool afterfile(const comparator* ucmp,
                      const slice* user_key, const fdwithkeyrange* f) {
  // nullptr user_key occurs before all keys and is therefore never after *f
  return (user_key != nullptr &&
          ucmp->compare(*user_key, extractuserkey(f->largest_key)) > 0);
}

static bool beforefile(const comparator* ucmp,
                       const slice* user_key, const fdwithkeyrange* f) {
  // nullptr user_key occurs after all keys and is therefore never before *f
  return (user_key != nullptr &&
          ucmp->compare(*user_key, extractuserkey(f->smallest_key)) < 0);
}

bool somefileoverlapsrange(
    const internalkeycomparator& icmp,
    bool disjoint_sorted_files,
    const filelevel& file_level,
    const slice* smallest_user_key,
    const slice* largest_user_key) {
  const comparator* ucmp = icmp.user_comparator();
  if (!disjoint_sorted_files) {
    // need to check against all files
    for (size_t i = 0; i < file_level.num_files; i++) {
      const fdwithkeyrange* f = &(file_level.files[i]);
      if (afterfile(ucmp, smallest_user_key, f) ||
          beforefile(ucmp, largest_user_key, f)) {
        // no overlap
      } else {
        return true;  // overlap
      }
    }
    return false;
  }

  // binary search over file list
  uint32_t index = 0;
  if (smallest_user_key != nullptr) {
    // find the earliest possible internal key for smallest_user_key
    internalkey small(*smallest_user_key, kmaxsequencenumber,kvaluetypeforseek);
    index = findfile(icmp, file_level, small.encode());
  }

  if (index >= file_level.num_files) {
    // beginning of range is after all files, so no overlap.
    return false;
  }

  return !beforefile(ucmp, largest_user_key, &file_level.files[index]);
}

// an internal iterator.  for a given version/level pair, yields
// information about the files in the level.  for a given entry, key()
// is the largest key that occurs in the file, and value() is an
// 16-byte value containing the file number and file size, both
// encoded using encodefixed64.
class version::levelfilenumiterator : public iterator {
 public:
  levelfilenumiterator(const internalkeycomparator& icmp,
                       const filelevel* flevel)
      : icmp_(icmp),
        flevel_(flevel),
        index_(flevel->num_files),
        current_value_(0, 0, 0) {  // marks as invalid
  }
  virtual bool valid() const {
    return index_ < flevel_->num_files;
  }
  virtual void seek(const slice& target) {
    index_ = findfile(icmp_, *flevel_, target);
  }
  virtual void seektofirst() { index_ = 0; }
  virtual void seektolast() {
    index_ = (flevel_->num_files == 0) ? 0 : flevel_->num_files - 1;
  }
  virtual void next() {
    assert(valid());
    index_++;
  }
  virtual void prev() {
    assert(valid());
    if (index_ == 0) {
      index_ = flevel_->num_files;  // marks as invalid
    } else {
      index_--;
    }
  }
  slice key() const {
    assert(valid());
    return flevel_->files[index_].largest_key;
  }
  slice value() const {
    assert(valid());

    auto file_meta = flevel_->files[index_];
    current_value_ = file_meta.fd;
    return slice(reinterpret_cast<const char*>(&current_value_),
                 sizeof(filedescriptor));
  }
  virtual status status() const { return status::ok(); }
 private:
  const internalkeycomparator icmp_;
  const filelevel* flevel_;
  uint32_t index_;
  mutable filedescriptor current_value_;
};

class version::levelfileiteratorstate : public twoleveliteratorstate {
 public:
  levelfileiteratorstate(tablecache* table_cache,
    const readoptions& read_options, const envoptions& env_options,
    const internalkeycomparator& icomparator, bool for_compaction,
    bool prefix_enabled)
    : twoleveliteratorstate(prefix_enabled),
      table_cache_(table_cache), read_options_(read_options),
      env_options_(env_options), icomparator_(icomparator),
      for_compaction_(for_compaction) {}

  iterator* newsecondaryiterator(const slice& meta_handle) override {
    if (meta_handle.size() != sizeof(filedescriptor)) {
      return newerroriterator(
          status::corruption("filereader invoked with unexpected value"));
    } else {
      const filedescriptor* fd =
          reinterpret_cast<const filedescriptor*>(meta_handle.data());
      return table_cache_->newiterator(
          read_options_, env_options_, icomparator_, *fd,
          nullptr /* don't need reference to table*/, for_compaction_);
    }
  }

  bool prefixmaymatch(const slice& internal_key) override {
    return true;
  }

 private:
  tablecache* table_cache_;
  const readoptions read_options_;
  const envoptions& env_options_;
  const internalkeycomparator& icomparator_;
  bool for_compaction_;
};

status version::gettableproperties(std::shared_ptr<const tableproperties>* tp,
                                   const filemetadata* file_meta,
                                   const std::string* fname) {
  auto table_cache = cfd_->table_cache();
  auto options = cfd_->options();
  status s = table_cache->gettableproperties(
      vset_->storage_options_, cfd_->internal_comparator(), file_meta->fd,
      tp, true /* no io */);
  if (s.ok()) {
    return s;
  }

  // we only ignore error type `incomplete` since it's by design that we
  // disallow table when it's not in table cache.
  if (!s.isincomplete()) {
    return s;
  }

  // 2. table is not present in table cache, we'll read the table properties
  // directly from the properties block in the file.
  std::unique_ptr<randomaccessfile> file;
  if (fname != nullptr) {
    s = options->env->newrandomaccessfile(
        *fname, &file, vset_->storage_options_);
  } else {
    s = options->env->newrandomaccessfile(
        tablefilename(vset_->options_->db_paths, file_meta->fd.getnumber(),
                      file_meta->fd.getpathid()),
        &file, vset_->storage_options_);
  }
  if (!s.ok()) {
    return s;
  }

  tableproperties* raw_table_properties;
  // by setting the magic number to kinvalidtablemagicnumber, we can by
  // pass the magic number check in the footer.
  s = readtableproperties(
      file.get(), file_meta->fd.getfilesize(),
      footer::kinvalidtablemagicnumber /* table's magic number */,
      vset_->env_, options->info_log.get(), &raw_table_properties);
  if (!s.ok()) {
    return s;
  }
  recordtick(options->statistics.get(), number_direct_load_table_properties);

  *tp = std::shared_ptr<const tableproperties>(raw_table_properties);
  return s;
}

status version::getpropertiesofalltables(tablepropertiescollection* props) {
  for (int level = 0; level < num_levels_; level++) {
    for (const auto& file_meta : files_[level]) {
      auto fname =
          tablefilename(vset_->options_->db_paths, file_meta->fd.getnumber(),
                        file_meta->fd.getpathid());
      // 1. if the table is already present in table cache, load table
      // properties from there.
      std::shared_ptr<const tableproperties> table_properties;
      status s = gettableproperties(&table_properties, file_meta, &fname);
      if (s.ok()) {
        props->insert({fname, table_properties});
      } else {
        return s;
      }
    }
  }

  return status::ok();
}

size_t version::getmemoryusagebytablereaders() {
  size_t total_usage = 0;
  for (auto& file_level : file_levels_) {
    for (size_t i = 0; i < file_level.num_files; i++) {
      total_usage += cfd_->table_cache()->getmemoryusagebytablereader(
          vset_->storage_options_, cfd_->internal_comparator(),
          file_level.files[i].fd);
    }
  }
  return total_usage;
}

uint64_t version::getestimatedactivekeys() {
  // estimation will be not accurate when:
  // (1) there is merge keys
  // (2) keys are directly overwritten
  // (3) deletion on non-existing keys
  return num_non_deletions_ - num_deletions_;
}

void version::additerators(const readoptions& read_options,
                           const envoptions& soptions,
                           std::vector<iterator*>* iters) {
  // merge all level zero files together since they may overlap
  for (size_t i = 0; i < file_levels_[0].num_files; i++) {
    const auto& file = file_levels_[0].files[i];
    iters->push_back(cfd_->table_cache()->newiterator(
        read_options, soptions, cfd_->internal_comparator(), file.fd));
  }

  // for levels > 0, we can use a concatenating iterator that sequentially
  // walks through the non-overlapping files in the level, opening them
  // lazily.
  for (int level = 1; level < num_levels_; level++) {
    if (file_levels_[level].num_files != 0) {
      iters->push_back(newtwoleveliterator(new levelfileiteratorstate(
          cfd_->table_cache(), read_options, soptions,
          cfd_->internal_comparator(), false /* for_compaction */,
          cfd_->options()->prefix_extractor != nullptr),
        new levelfilenumiterator(cfd_->internal_comparator(),
            &file_levels_[level])));
    }
  }
}

void version::additerators(const readoptions& read_options,
                           const envoptions& soptions,
                           mergeiteratorbuilder* merge_iter_builder) {
  // merge all level zero files together since they may overlap
  for (size_t i = 0; i < file_levels_[0].num_files; i++) {
    const auto& file = file_levels_[0].files[i];
    merge_iter_builder->additerator(cfd_->table_cache()->newiterator(
        read_options, soptions, cfd_->internal_comparator(), file.fd, nullptr,
        false, merge_iter_builder->getarena()));
  }

  // for levels > 0, we can use a concatenating iterator that sequentially
  // walks through the non-overlapping files in the level, opening them
  // lazily.
  for (int level = 1; level < num_levels_; level++) {
    if (file_levels_[level].num_files != 0) {
      merge_iter_builder->additerator(newtwoleveliterator(
          new levelfileiteratorstate(
              cfd_->table_cache(), read_options, soptions,
              cfd_->internal_comparator(), false /* for_compaction */,
              cfd_->options()->prefix_extractor != nullptr),
          new levelfilenumiterator(cfd_->internal_comparator(),
              &file_levels_[level]), merge_iter_builder->getarena()));
    }
  }
}

// callback from tablecache::get()
enum saverstate {
  knotfound,
  kfound,
  kdeleted,
  kcorrupt,
  kmerge // saver contains the current merge result (the operands)
};

namespace version_set {
struct saver {
  saverstate state;
  const comparator* ucmp;
  slice user_key;
  bool* value_found; // is value set correctly? used by keymayexist
  std::string* value;
  const mergeoperator* merge_operator;
  // the merge operations encountered;
  mergecontext* merge_context;
  logger* logger;
  statistics* statistics;
};
} // namespace version_set

// called from tablecache::get and table::get when file/block in which
// key may  exist are not there in tablecache/blockcache respectively. in this
// case we  can't guarantee that key does not exist and are not permitted to do
// io to be  certain.set the status=kfound and value_found=false to let the
// caller know that key may exist but is not there in memory
static void markkeymayexist(void* arg) {
  version_set::saver* s = reinterpret_cast<version_set::saver*>(arg);
  s->state = kfound;
  if (s->value_found != nullptr) {
    *(s->value_found) = false;
  }
}

static bool savevalue(void* arg, const parsedinternalkey& parsed_key,
                      const slice& v) {
  version_set::saver* s = reinterpret_cast<version_set::saver*>(arg);
  mergecontext* merge_contex = s->merge_context;
  std::string merge_result;  // temporary area for merge results later

  assert(s != nullptr && merge_contex != nullptr);

  // todo: merge?
  if (s->ucmp->compare(parsed_key.user_key, s->user_key) == 0) {
    // key matches. process it
    switch (parsed_key.type) {
      case ktypevalue:
        if (knotfound == s->state) {
          s->state = kfound;
          s->value->assign(v.data(), v.size());
        } else if (kmerge == s->state) {
          assert(s->merge_operator != nullptr);
          s->state = kfound;
          if (!s->merge_operator->fullmerge(s->user_key, &v,
                                            merge_contex->getoperands(),
                                            s->value, s->logger)) {
            recordtick(s->statistics, number_merge_failures);
            s->state = kcorrupt;
          }
        } else {
          assert(false);
        }
        return false;

      case ktypedeletion:
        if (knotfound == s->state) {
          s->state = kdeleted;
        } else if (kmerge == s->state) {
          s->state = kfound;
          if (!s->merge_operator->fullmerge(s->user_key, nullptr,
                                            merge_contex->getoperands(),
                                            s->value, s->logger)) {
            recordtick(s->statistics, number_merge_failures);
            s->state = kcorrupt;
          }
        } else {
          assert(false);
        }
        return false;

      case ktypemerge:
        assert(s->state == knotfound || s->state == kmerge);
        s->state = kmerge;
        merge_contex->pushoperand(v);
        return true;

      default:
        assert(false);
        break;
    }
  }

  // s->state could be corrupt, merge or notfound

  return false;
}

version::version(columnfamilydata* cfd, versionset* vset,
                 uint64_t version_number)
    : cfd_(cfd),
      internal_comparator_((cfd == nullptr) ? nullptr
                                            : &cfd->internal_comparator()),
      user_comparator_(
          (cfd == nullptr) ? nullptr : internal_comparator_->user_comparator()),
      table_cache_((cfd == nullptr) ? nullptr : cfd->table_cache()),
      merge_operator_((cfd == nullptr) ? nullptr
                                       : cfd->options()->merge_operator.get()),
      info_log_((cfd == nullptr) ? nullptr : cfd->options()->info_log.get()),
      db_statistics_((cfd == nullptr) ? nullptr
                                      : cfd->options()->statistics.get()),
      // cfd is nullptr if version is dummy
      num_levels_(cfd == nullptr ? 0 : cfd->numberlevels()),
      num_non_empty_levels_(num_levels_),
      file_indexer_(cfd == nullptr
                        ? nullptr
                        : cfd->internal_comparator().user_comparator()),
      vset_(vset),
      next_(this),
      prev_(this),
      refs_(0),
      files_(new std::vector<filemetadata*>[num_levels_]),
      files_by_size_(num_levels_),
      next_file_to_compact_by_size_(num_levels_),
      compaction_score_(num_levels_),
      compaction_level_(num_levels_),
      version_number_(version_number),
      total_file_size_(0),
      total_raw_key_size_(0),
      total_raw_value_size_(0),
      num_non_deletions_(0),
      num_deletions_(0) {
  if (cfd != nullptr && cfd->current() != nullptr) {
      total_file_size_ = cfd->current()->total_file_size_;
      total_raw_key_size_ = cfd->current()->total_raw_key_size_;
      total_raw_value_size_ = cfd->current()->total_raw_value_size_;
      num_non_deletions_ = cfd->current()->num_non_deletions_;
      num_deletions_ = cfd->current()->num_deletions_;
  }
}

void version::get(const readoptions& options,
                  const lookupkey& k,
                  std::string* value,
                  status* status,
                  mergecontext* merge_context,
                  bool* value_found) {
  slice ikey = k.internal_key();
  slice user_key = k.user_key();

  assert(status->ok() || status->ismergeinprogress());
  version_set::saver saver;
  saver.state = status->ok()? knotfound : kmerge;
  saver.ucmp = user_comparator_;
  saver.user_key = user_key;
  saver.value_found = value_found;
  saver.value = value;
  saver.merge_operator = merge_operator_;
  saver.merge_context = merge_context;
  saver.logger = info_log_;
  saver.statistics = db_statistics_;

  filepicker fp(files_, user_key, ikey, &file_levels_, num_non_empty_levels_,
      &file_indexer_, user_comparator_, internal_comparator_);
  fdwithkeyrange* f = fp.getnextfile();
  while (f != nullptr) {
    *status = table_cache_->get(options, *internal_comparator_, f->fd, ikey,
                                &saver, savevalue, markkeymayexist);
    // todo: examine the behavior for corrupted key
    if (!status->ok()) {
      return;
    }

    switch (saver.state) {
      case knotfound:
        break;      // keep searching in other files
      case kfound:
        return;
      case kdeleted:
        *status = status::notfound();  // use empty error message for speed
        return;
      case kcorrupt:
        *status = status::corruption("corrupted key for ", user_key);
        return;
      case kmerge:
        break;
    }
    f = fp.getnextfile();
  }

  if (kmerge == saver.state) {
    if (!merge_operator_) {
      *status =  status::invalidargument(
          "merge_operator is not properly initialized.");
      return;
    }
    // merge_operands are in saver and we hit the beginning of the key history
    // do a final merge of nullptr and operands;
    if (merge_operator_->fullmerge(user_key, nullptr,
                                   saver.merge_context->getoperands(), value,
                                   info_log_)) {
      *status = status::ok();
    } else {
      recordtick(db_statistics_, number_merge_failures);
      *status = status::corruption("could not perform end-of-key merge for ",
                                   user_key);
    }
  } else {
    *status = status::notfound(); // use an empty error message for speed
  }
}

void version::generatefilelevels() {
  file_levels_.resize(num_non_empty_levels_);
  for (int level = 0; level < num_non_empty_levels_; level++) {
    dogeneratefilelevel(&file_levels_[level], files_[level], &arena_);
  }
}

void version::prepareapply(std::vector<uint64_t>& size_being_compacted) {
  updatetemporarystats();
  computecompactionscore(size_being_compacted);
  updatefilesbysize();
  updatenumnonemptylevels();
  file_indexer_.updateindex(&arena_, num_non_empty_levels_, files_);
  generatefilelevels();
}

bool version::maybeinitializefilemetadata(filemetadata* file_meta) {
  if (file_meta->init_stats_from_file) {
    return false;
  }
  std::shared_ptr<const tableproperties> tp;
  status s = gettableproperties(&tp, file_meta);
  file_meta->init_stats_from_file = true;
  if (!s.ok()) {
    log(vset_->options_->info_log,
        "unable to load table properties for file %" priu64 " --- %s\n",
        file_meta->fd.getnumber(), s.tostring().c_str());
    return false;
  }
  if (tp.get() == nullptr) return false;
  file_meta->num_entries = tp->num_entries;
  file_meta->num_deletions = getdeletedkeys(tp->user_collected_properties);
  file_meta->raw_value_size = tp->raw_value_size;
  file_meta->raw_key_size = tp->raw_key_size;

  return true;
}

void version::updatetemporarystats() {
  static const int kdeletionweightoncompaction = 2;

  // incrementally update the average value size by
  // including newly added files into the global stats
  int init_count = 0;
  int total_count = 0;
  for (int level = 0; level < num_levels_; level++) {
    for (auto* file_meta : files_[level]) {
      if (maybeinitializefilemetadata(file_meta)) {
        // each filemeta will be initialized only once.
        total_file_size_ += file_meta->fd.getfilesize();
        total_raw_key_size_ += file_meta->raw_key_size;
        total_raw_value_size_ += file_meta->raw_value_size;
        num_non_deletions_ +=
            file_meta->num_entries - file_meta->num_deletions;
        num_deletions_ += file_meta->num_deletions;
        init_count++;
      }
      total_count++;
    }
  }

  uint64_t average_value_size = getaveragevaluesize();

  // compute the compensated size
  for (int level = 0; level < num_levels_; level++) {
    for (auto* file_meta : files_[level]) {
      // here we only compute compensated_file_size for those file_meta
      // which compensated_file_size is uninitialized (== 0).
      if (file_meta->compensated_file_size == 0) {
        file_meta->compensated_file_size = file_meta->fd.getfilesize() +
            file_meta->num_deletions * average_value_size *
            kdeletionweightoncompaction;
      }
    }
  }
}

void version::computecompactionscore(
    std::vector<uint64_t>& size_being_compacted) {
  double max_score = 0;
  int max_score_level = 0;

  int max_input_level =
      cfd_->compaction_picker()->maxinputlevel(numberlevels());

  for (int level = 0; level <= max_input_level; level++) {
    double score;
    if (level == 0) {
      // we treat level-0 specially by bounding the number of files
      // instead of number of bytes for two reasons:
      //
      // (1) with larger write-buffer sizes, it is nice not to do too
      // many level-0 compactions.
      //
      // (2) the files in level-0 are merged on every read and
      // therefore we wish to avoid too many files when the individual
      // file size is small (perhaps because of a small write-buffer
      // setting, or very high compression ratios, or lots of
      // overwrites/deletions).
      int numfiles = 0;
      uint64_t total_size = 0;
      for (unsigned int i = 0; i < files_[level].size(); i++) {
        if (!files_[level][i]->being_compacted) {
          total_size += files_[level][i]->compensated_file_size;
          numfiles++;
        }
      }
      if (cfd_->options()->compaction_style == kcompactionstylefifo) {
        score = static_cast<double>(total_size) /
                cfd_->options()->compaction_options_fifo.max_table_files_size;
      } else if (numfiles >= cfd_->options()->level0_stop_writes_trigger) {
        // if we are slowing down writes, then we better compact that first
        score = 1000000;
      } else if (numfiles >= cfd_->options()->level0_slowdown_writes_trigger) {
        score = 10000;
      } else {
        score = static_cast<double>(numfiles) /
                cfd_->options()->level0_file_num_compaction_trigger;
      }
    } else {
      // compute the ratio of current size to size limit.
      const uint64_t level_bytes =
          totalcompensatedfilesize(files_[level]) - size_being_compacted[level];
      score = static_cast<double>(level_bytes) /
              cfd_->compaction_picker()->maxbytesforlevel(level);
      if (max_score < score) {
        max_score = score;
        max_score_level = level;
      }
    }
    compaction_level_[level] = level;
    compaction_score_[level] = score;
  }

  // update the max compaction score in levels 1 to n-1
  max_compaction_score_ = max_score;
  max_compaction_score_level_ = max_score_level;

  // sort all the levels based on their score. higher scores get listed
  // first. use bubble sort because the number of entries are small.
  for (int i = 0; i < numberlevels() - 2; i++) {
    for (int j = i + 1; j < numberlevels() - 1; j++) {
      if (compaction_score_[i] < compaction_score_[j]) {
        double score = compaction_score_[i];
        int level = compaction_level_[i];
        compaction_score_[i] = compaction_score_[j];
        compaction_level_[i] = compaction_level_[j];
        compaction_score_[j] = score;
        compaction_level_[j] = level;
      }
    }
  }
}

namespace {
// compator that is used to sort files based on their size
// in normal mode: descending size
bool comparecompensatedsizedescending(const version::fsize& first,
                                      const version::fsize& second) {
  return (first.file->compensated_file_size >
      second.file->compensated_file_size);
}
} // anonymous namespace

void version::updatenumnonemptylevels() {
  num_non_empty_levels_ = num_levels_;
  for (int i = num_levels_ - 1; i >= 0; i--) {
    if (files_[i].size() != 0) {
      return;
    } else {
      num_non_empty_levels_ = i;
    }
  }
}

void version::updatefilesbysize() {
  if (cfd_->options()->compaction_style == kcompactionstylefifo ||
      cfd_->options()->compaction_style == kcompactionstyleuniversal) {
    // don't need this
    return;
  }
  // no need to sort the highest level because it is never compacted.
  for (int level = 0; level < numberlevels() - 1; level++) {
    const std::vector<filemetadata*>& files = files_[level];
    auto& files_by_size = files_by_size_[level];
    assert(files_by_size.size() == 0);

    // populate a temp vector for sorting based on size
    std::vector<fsize> temp(files.size());
    for (unsigned int i = 0; i < files.size(); i++) {
      temp[i].index = i;
      temp[i].file = files[i];
    }

    // sort the top number_of_files_to_sort_ based on file size
    size_t num = version::number_of_files_to_sort_;
    if (num > temp.size()) {
      num = temp.size();
    }
    std::partial_sort(temp.begin(), temp.begin() + num, temp.end(),
                      comparecompensatedsizedescending);
    assert(temp.size() == files.size());

    // initialize files_by_size_
    for (unsigned int i = 0; i < temp.size(); i++) {
      files_by_size.push_back(temp[i].index);
    }
    next_file_to_compact_by_size_[level] = 0;
    assert(files_[level].size() == files_by_size_[level].size());
  }
}

void version::ref() {
  ++refs_;
}

bool version::unref() {
  assert(refs_ >= 1);
  --refs_;
  if (refs_ == 0) {
    delete this;
    return true;
  }
  return false;
}

bool version::needscompaction() const {
  // in universal compaction case, this check doesn't really
  // check the compaction condition, but checks num of files threshold
  // only. we are not going to miss any compaction opportunity
  // but it's likely that more compactions are scheduled but
  // ending up with nothing to do. we can improve it later.
  // todo(sdong): improve this function to be accurate for universal
  //              compactions.
  int max_input_level =
      cfd_->compaction_picker()->maxinputlevel(numberlevels());

  for (int i = 0; i <= max_input_level; i++) {
    if (compaction_score_[i] >= 1) {
      return true;
    }
  }
  return false;
}

bool version::overlapinlevel(int level,
                             const slice* smallest_user_key,
                             const slice* largest_user_key) {
  return somefileoverlapsrange(cfd_->internal_comparator(), (level > 0),
                               file_levels_[level], smallest_user_key,
                               largest_user_key);
}

int version::picklevelformemtableoutput(
    const slice& smallest_user_key,
    const slice& largest_user_key) {
  int level = 0;
  if (!overlapinlevel(0, &smallest_user_key, &largest_user_key)) {
    // push to next level if there is no overlap in next level,
    // and the #bytes overlapping in the level after that are limited.
    internalkey start(smallest_user_key, kmaxsequencenumber, kvaluetypeforseek);
    internalkey limit(largest_user_key, 0, static_cast<valuetype>(0));
    std::vector<filemetadata*> overlaps;
    int max_mem_compact_level = cfd_->options()->max_mem_compaction_level;
    while (max_mem_compact_level > 0 && level < max_mem_compact_level) {
      if (overlapinlevel(level + 1, &smallest_user_key, &largest_user_key)) {
        break;
      }
      if (level + 2 >= num_levels_) {
        level++;
        break;
      }
      getoverlappinginputs(level + 2, &start, &limit, &overlaps);
      const uint64_t sum = totalfilesize(overlaps);
      if (sum > cfd_->compaction_picker()->maxgrandparentoverlapbytes(level)) {
        break;
      }
      level++;
    }
  }

  return level;
}

// store in "*inputs" all files in "level" that overlap [begin,end]
// if hint_index is specified, then it points to a file in the
// overlapping range.
// the file_index returns a pointer to any file in an overlapping range.
void version::getoverlappinginputs(int level,
                                   const internalkey* begin,
                                   const internalkey* end,
                                   std::vector<filemetadata*>* inputs,
                                   int hint_index,
                                   int* file_index) {
  inputs->clear();
  slice user_begin, user_end;
  if (begin != nullptr) {
    user_begin = begin->user_key();
  }
  if (end != nullptr) {
    user_end = end->user_key();
  }
  if (file_index) {
    *file_index = -1;
  }
  const comparator* user_cmp = cfd_->internal_comparator().user_comparator();
  if (begin != nullptr && end != nullptr && level > 0) {
    getoverlappinginputsbinarysearch(level, user_begin, user_end, inputs,
      hint_index, file_index);
    return;
  }
  for (size_t i = 0; i < file_levels_[level].num_files; ) {
    fdwithkeyrange* f = &(file_levels_[level].files[i++]);
    const slice file_start = extractuserkey(f->smallest_key);
    const slice file_limit = extractuserkey(f->largest_key);
    if (begin != nullptr && user_cmp->compare(file_limit, user_begin) < 0) {
      // "f" is completely before specified range; skip it
    } else if (end != nullptr && user_cmp->compare(file_start, user_end) > 0) {
      // "f" is completely after specified range; skip it
    } else {
      inputs->push_back(files_[level][i-1]);
      if (level == 0) {
        // level-0 files may overlap each other.  so check if the newly
        // added file has expanded the range.  if so, restart search.
        if (begin != nullptr && user_cmp->compare(file_start, user_begin) < 0) {
          user_begin = file_start;
          inputs->clear();
          i = 0;
        } else if (end != nullptr
            && user_cmp->compare(file_limit, user_end) > 0) {
          user_end = file_limit;
          inputs->clear();
          i = 0;
        }
      } else if (file_index) {
        *file_index = i-1;
      }
    }
  }
}

// store in "*inputs" all files in "level" that overlap [begin,end]
// employ binary search to find at least one file that overlaps the
// specified range. from that file, iterate backwards and
// forwards to find all overlapping files.
void version::getoverlappinginputsbinarysearch(
    int level,
    const slice& user_begin,
    const slice& user_end,
    std::vector<filemetadata*>* inputs,
    int hint_index,
    int* file_index) {
  assert(level > 0);
  int min = 0;
  int mid = 0;
  int max = files_[level].size() -1;
  bool foundoverlap = false;
  const comparator* user_cmp = cfd_->internal_comparator().user_comparator();

  // if the caller already knows the index of a file that has overlap,
  // then we can skip the binary search.
  if (hint_index != -1) {
    mid = hint_index;
    foundoverlap = true;
  }

  while (!foundoverlap && min <= max) {
    mid = (min + max)/2;
    fdwithkeyrange* f = &(file_levels_[level].files[mid]);
    const slice file_start = extractuserkey(f->smallest_key);
    const slice file_limit = extractuserkey(f->largest_key);
    if (user_cmp->compare(file_limit, user_begin) < 0) {
      min = mid + 1;
    } else if (user_cmp->compare(user_end, file_start) < 0) {
      max = mid - 1;
    } else {
      foundoverlap = true;
      break;
    }
  }

  // if there were no overlapping files, return immediately.
  if (!foundoverlap) {
    return;
  }
  // returns the index where an overlap is found
  if (file_index) {
    *file_index = mid;
  }
  extendoverlappinginputs(level, user_begin, user_end, inputs, mid);
}

// store in "*inputs" all files in "level" that overlap [begin,end]
// the midindex specifies the index of at least one file that
// overlaps the specified range. from that file, iterate backward
// and forward to find all overlapping files.
// use filelevel in searching, make it faster
void version::extendoverlappinginputs(
    int level,
    const slice& user_begin,
    const slice& user_end,
    std::vector<filemetadata*>* inputs,
    unsigned int midindex) {

  const comparator* user_cmp = cfd_->internal_comparator().user_comparator();
  const fdwithkeyrange* files = file_levels_[level].files;
#ifndef ndebug
  {
    // assert that the file at midindex overlaps with the range
    assert(midindex < file_levels_[level].num_files);
    const fdwithkeyrange* f = &files[midindex];
    const slice fstart = extractuserkey(f->smallest_key);
    const slice flimit = extractuserkey(f->largest_key);
    if (user_cmp->compare(fstart, user_begin) >= 0) {
      assert(user_cmp->compare(fstart, user_end) <= 0);
    } else {
      assert(user_cmp->compare(flimit, user_begin) >= 0);
    }
  }
#endif
  int startindex = midindex + 1;
  int endindex = midindex;
  int count __attribute__((unused)) = 0;

  // check backwards from 'mid' to lower indices
  for (int i = midindex; i >= 0 ; i--) {
    const fdwithkeyrange* f = &files[i];
    const slice file_limit = extractuserkey(f->largest_key);
    if (user_cmp->compare(file_limit, user_begin) >= 0) {
      startindex = i;
      assert((count++, true));
    } else {
      break;
    }
  }
  // check forward from 'mid+1' to higher indices
  for (unsigned int i = midindex+1; i < file_levels_[level].num_files; i++) {
    const fdwithkeyrange* f = &files[i];
    const slice file_start = extractuserkey(f->smallest_key);
    if (user_cmp->compare(file_start, user_end) <= 0) {
      assert((count++, true));
      endindex = i;
    } else {
      break;
    }
  }
  assert(count == endindex - startindex + 1);

  // insert overlapping files into vector
  for (int i = startindex; i <= endindex; i++) {
    filemetadata* f = files_[level][i];
    inputs->push_back(f);
  }
}

// returns true iff the first or last file in inputs contains
// an overlapping user key to the file "just outside" of it (i.e.
// just after the last file, or just before the first file)
// requires: "*inputs" is a sorted list of non-overlapping files
bool version::hasoverlappinguserkey(
    const std::vector<filemetadata*>* inputs,
    int level) {

  // if inputs empty, there is no overlap.
  // if level == 0, it is assumed that all needed files were already included.
  if (inputs->empty() || level == 0){
    return false;
  }

  const comparator* user_cmp = cfd_->internal_comparator().user_comparator();
  const filelevel& file_level = file_levels_[level];
  const fdwithkeyrange* files = file_levels_[level].files;
  const size_t knumfiles = file_level.num_files;

  // check the last file in inputs against the file after it
  size_t last_file = findfile(cfd_->internal_comparator(), file_level,
                              inputs->back()->largest.encode());
  assert(0 <= last_file && last_file < knumfiles);  // file should exist!
  if (last_file < knumfiles-1) {                    // if not the last file
    const slice last_key_in_input = extractuserkey(
        files[last_file].largest_key);
    const slice first_key_after = extractuserkey(
        files[last_file+1].smallest_key);
    if (user_cmp->compare(last_key_in_input, first_key_after) == 0) {
      // the last user key in input overlaps with the next file's first key
      return true;
    }
  }

  // check the first file in inputs against the file just before it
  size_t first_file = findfile(cfd_->internal_comparator(), file_level,
                               inputs->front()->smallest.encode());
  assert(0 <= first_file && first_file <= last_file);   // file should exist!
  if (first_file > 0) {                                 // if not first file
    const slice& first_key_in_input = extractuserkey(
        files[first_file].smallest_key);
    const slice& last_key_before = extractuserkey(
        files[first_file-1].largest_key);
    if (user_cmp->compare(first_key_in_input, last_key_before) == 0) {
      // the first user key in input overlaps with the previous file's last key
      return true;
    }
  }

  return false;
}

int64_t version::numlevelbytes(int level) const {
  assert(level >= 0);
  assert(level < numberlevels());
  return totalfilesize(files_[level]);
}

const char* version::levelsummary(levelsummarystorage* scratch) const {
  int len = snprintf(scratch->buffer, sizeof(scratch->buffer), "files[");
  for (int i = 0; i < numberlevels(); i++) {
    int sz = sizeof(scratch->buffer) - len;
    int ret = snprintf(scratch->buffer + len, sz, "%d ", int(files_[i].size()));
    if (ret < 0 || ret >= sz) break;
    len += ret;
  }
  if (len > 0) {
    // overwrite the last space
    --len;
  }
  snprintf(scratch->buffer + len, sizeof(scratch->buffer) - len, "]");
  return scratch->buffer;
}

const char* version::levelfilesummary(filesummarystorage* scratch,
                                      int level) const {
  int len = snprintf(scratch->buffer, sizeof(scratch->buffer), "files_size[");
  for (const auto& f : files_[level]) {
    int sz = sizeof(scratch->buffer) - len;
    char sztxt[16];
    appendhumanbytes(f->fd.getfilesize(), sztxt, sizeof(sztxt));
    int ret = snprintf(scratch->buffer + len, sz,
                       "#%" priu64 "(seq=%" priu64 ",sz=%s,%d) ",
                       f->fd.getnumber(), f->smallest_seqno, sztxt,
                       static_cast<int>(f->being_compacted));
    if (ret < 0 || ret >= sz)
      break;
    len += ret;
  }
  // overwrite the last space (only if files_[level].size() is non-zero)
  if (files_[level].size() && len > 0) {
    --len;
  }
  snprintf(scratch->buffer + len, sizeof(scratch->buffer) - len, "]");
  return scratch->buffer;
}

int64_t version::maxnextleveloverlappingbytes() {
  uint64_t result = 0;
  std::vector<filemetadata*> overlaps;
  for (int level = 1; level < numberlevels() - 1; level++) {
    for (const auto& f : files_[level]) {
      getoverlappinginputs(level + 1, &f->smallest, &f->largest, &overlaps);
      const uint64_t sum = totalfilesize(overlaps);
      if (sum > result) {
        result = sum;
      }
    }
  }
  return result;
}

void version::addlivefiles(std::vector<filedescriptor>* live) {
  for (int level = 0; level < numberlevels(); level++) {
    const std::vector<filemetadata*>& files = files_[level];
    for (const auto& file : files) {
      live->push_back(file->fd);
    }
  }
}

std::string version::debugstring(bool hex) const {
  std::string r;
  for (int level = 0; level < num_levels_; level++) {
    // e.g.,
    //   --- level 1 ---
    //   17:123['a' .. 'd']
    //   20:43['e' .. 'g']
    r.append("--- level ");
    appendnumberto(&r, level);
    r.append(" --- version# ");
    appendnumberto(&r, version_number_);
    r.append(" ---\n");
    const std::vector<filemetadata*>& files = files_[level];
    for (size_t i = 0; i < files.size(); i++) {
      r.push_back(' ');
      appendnumberto(&r, files[i]->fd.getnumber());
      r.push_back(':');
      appendnumberto(&r, files[i]->fd.getfilesize());
      r.append("[");
      r.append(files[i]->smallest.debugstring(hex));
      r.append(" .. ");
      r.append(files[i]->largest.debugstring(hex));
      r.append("]\n");
    }
  }
  return r;
}

// this is used to batch writes to the manifest file
struct versionset::manifestwriter {
  status status;
  bool done;
  port::condvar cv;
  columnfamilydata* cfd;
  versionedit* edit;

  explicit manifestwriter(port::mutex* mu, columnfamilydata* cfd,
                          versionedit* e)
      : done(false), cv(mu), cfd(cfd), edit(e) {}
};

// a helper class so we can efficiently apply a whole sequence
// of edits to a particular state without creating intermediate
// versions that contain full copies of the intermediate state.
class versionset::builder {
 private:
  // helper to sort v->files_
  // klevel0 -- newestfirstbyseqno
  // klevelnon0 -- bysmallestkey
  struct filecomparator {
    enum sortmethod {
      klevel0 = 0,
      klevelnon0 = 1,
    } sort_method;
    const internalkeycomparator* internal_comparator;

    bool operator()(filemetadata* f1, filemetadata* f2) const {
      switch (sort_method) {
        case klevel0:
          return newestfirstbyseqno(f1, f2);
        case klevelnon0:
          return bysmallestkey(f1, f2, internal_comparator);
      }
      assert(false);
      return false;
    }
  };

  typedef std::set<filemetadata*, filecomparator> fileset;
  struct levelstate {
    std::set<uint64_t> deleted_files;
    fileset* added_files;
  };

  columnfamilydata* cfd_;
  version* base_;
  levelstate* levels_;
  filecomparator level_zero_cmp_;
  filecomparator level_nonzero_cmp_;

 public:
  builder(columnfamilydata* cfd) : cfd_(cfd), base_(cfd->current()) {
    base_->ref();
    levels_ = new levelstate[base_->numberlevels()];
    level_zero_cmp_.sort_method = filecomparator::klevel0;
    level_nonzero_cmp_.sort_method = filecomparator::klevelnon0;
    level_nonzero_cmp_.internal_comparator = &cfd->internal_comparator();

    levels_[0].added_files = new fileset(level_zero_cmp_);
    for (int level = 1; level < base_->numberlevels(); level++) {
        levels_[level].added_files = new fileset(level_nonzero_cmp_);
    }
  }

  ~builder() {
    for (int level = 0; level < base_->numberlevels(); level++) {
      const fileset* added = levels_[level].added_files;
      std::vector<filemetadata*> to_unref;
      to_unref.reserve(added->size());
      for (fileset::const_iterator it = added->begin();
          it != added->end(); ++it) {
        to_unref.push_back(*it);
      }
      delete added;
      for (uint32_t i = 0; i < to_unref.size(); i++) {
        filemetadata* f = to_unref[i];
        f->refs--;
        if (f->refs <= 0) {
          if (f->table_reader_handle) {
            cfd_->table_cache()->releasehandle(f->table_reader_handle);
            f->table_reader_handle = nullptr;
          }
          delete f;
        }
      }
    }

    delete[] levels_;
    base_->unref();
  }

  void checkconsistency(version* v) {
#ifndef ndebug
    // make sure the files are sorted correctly
    for (int level = 0; level < v->numberlevels(); level++) {
      for (size_t i = 1; i < v->files_[level].size(); i++) {
        auto f1 = v->files_[level][i - 1];
        auto f2 = v->files_[level][i];
        if (level == 0) {
          assert(level_zero_cmp_(f1, f2));
          assert(f1->largest_seqno > f2->largest_seqno);
        } else {
          assert(level_nonzero_cmp_(f1, f2));

          // make sure there is no overlap in levels > 0
          if (cfd_->internal_comparator().compare(f1->largest, f2->smallest) >=
              0) {
            fprintf(stderr, "overlapping ranges in same level %s vs. %s\n",
                    (f1->largest).debugstring().c_str(),
                    (f2->smallest).debugstring().c_str());
            abort();
          }
        }
      }
    }
#endif
  }

  void checkconsistencyfordeletes(versionedit* edit, uint64_t number,
                                  int level) {
#ifndef ndebug
      // a file to be deleted better exist in the previous version
      bool found = false;
      for (int l = 0; !found && l < base_->numberlevels(); l++) {
        const std::vector<filemetadata*>& base_files = base_->files_[l];
        for (unsigned int i = 0; i < base_files.size(); i++) {
          filemetadata* f = base_files[i];
          if (f->fd.getnumber() == number) {
            found =  true;
            break;
          }
        }
      }
      // if the file did not exist in the previous version, then it
      // is possibly moved from lower level to higher level in current
      // version
      for (int l = level+1; !found && l < base_->numberlevels(); l++) {
        const fileset* added = levels_[l].added_files;
        for (fileset::const_iterator added_iter = added->begin();
             added_iter != added->end(); ++added_iter) {
          filemetadata* f = *added_iter;
          if (f->fd.getnumber() == number) {
            found = true;
            break;
          }
        }
      }

      // maybe this file was added in a previous edit that was applied
      if (!found) {
        const fileset* added = levels_[level].added_files;
        for (fileset::const_iterator added_iter = added->begin();
             added_iter != added->end(); ++added_iter) {
          filemetadata* f = *added_iter;
          if (f->fd.getnumber() == number) {
            found = true;
            break;
          }
        }
      }
      if (!found) {
        fprintf(stderr, "not found %" priu64 "\n", number);
      }
      assert(found);
#endif
  }

  // apply all of the edits in *edit to the current state.
  void apply(versionedit* edit) {
    checkconsistency(base_);

    // delete files
    const versionedit::deletedfileset& del = edit->deleted_files_;
    for (const auto& del_file : del) {
      const auto level = del_file.first;
      const auto number = del_file.second;
      levels_[level].deleted_files.insert(number);
      checkconsistencyfordeletes(edit, number, level);
    }

    // add new files
    for (const auto& new_file : edit->new_files_) {
      const int level = new_file.first;
      filemetadata* f = new filemetadata(new_file.second);
      f->refs = 1;

      levels_[level].deleted_files.erase(f->fd.getnumber());
      levels_[level].added_files->insert(f);
    }
  }

  // save the current state in *v.
  void saveto(version* v) {
    checkconsistency(base_);
    checkconsistency(v);

    for (int level = 0; level < base_->numberlevels(); level++) {
      const auto& cmp = (level == 0) ? level_zero_cmp_ : level_nonzero_cmp_;
      // merge the set of added files with the set of pre-existing files.
      // drop any deleted files.  store the result in *v.
      const auto& base_files = base_->files_[level];
      auto base_iter = base_files.begin();
      auto base_end = base_files.end();
      const auto& added_files = *levels_[level].added_files;
      v->files_[level].reserve(base_files.size() + added_files.size());

      for (const auto& added : added_files) {
        // add all smaller files listed in base_
        for (auto bpos = std::upper_bound(base_iter, base_end, added, cmp);
             base_iter != bpos;
             ++base_iter) {
          maybeaddfile(v, level, *base_iter);
        }

        maybeaddfile(v, level, added);
      }

      // add remaining base files
      for (; base_iter != base_end; ++base_iter) {
        maybeaddfile(v, level, *base_iter);
      }
    }

    checkconsistency(v);
  }

  void loadtablehandlers() {
    for (int level = 0; level < cfd_->numberlevels(); level++) {
      for (auto& file_meta : *(levels_[level].added_files)) {
        assert (!file_meta->table_reader_handle);
        cfd_->table_cache()->findtable(
            base_->vset_->storage_options_, cfd_->internal_comparator(),
            file_meta->fd, &file_meta->table_reader_handle, false);
        if (file_meta->table_reader_handle != nullptr) {
          // load table_reader
          file_meta->fd.table_reader =
              cfd_->table_cache()->gettablereaderfromhandle(
                  file_meta->table_reader_handle);
        }
      }
    }
  }

  void maybeaddfile(version* v, int level, filemetadata* f) {
    if (levels_[level].deleted_files.count(f->fd.getnumber()) > 0) {
      // file is deleted: do nothing
    } else {
      auto* files = &v->files_[level];
      if (level > 0 && !files->empty()) {
        // must not overlap
        assert(cfd_->internal_comparator().compare(
                   (*files)[files->size() - 1]->largest, f->smallest) < 0);
      }
      f->refs++;
      files->push_back(f);
    }
  }
};

versionset::versionset(const std::string& dbname, const dboptions* options,
                       const envoptions& storage_options, cache* table_cache)
    : column_family_set_(new columnfamilyset(dbname, options, storage_options,
                                             table_cache)),
      env_(options->env),
      dbname_(dbname),
      options_(options),
      next_file_number_(2),
      manifest_file_number_(0),  // filled by recover()
      pending_manifest_file_number_(0),
      last_sequence_(0),
      prev_log_number_(0),
      current_version_number_(0),
      manifest_file_size_(0),
      storage_options_(storage_options),
      storage_options_compactions_(storage_options_) {}

versionset::~versionset() {
  // we need to delete column_family_set_ because its destructor depends on
  // versionset
  column_family_set_.reset();
  for (auto file : obsolete_files_) {
    delete file;
  }
  obsolete_files_.clear();
}

void versionset::appendversion(columnfamilydata* column_family_data,
                               version* v) {
  // make "v" current
  assert(v->refs_ == 0);
  version* current = column_family_data->current();
  assert(v != current);
  if (current != nullptr) {
    assert(current->refs_ > 0);
    current->unref();
  }
  column_family_data->setcurrent(v);
  v->ref();

  // append to linked list
  v->prev_ = column_family_data->dummy_versions()->prev_;
  v->next_ = column_family_data->dummy_versions();
  v->prev_->next_ = v;
  v->next_->prev_ = v;
}

status versionset::logandapply(columnfamilydata* column_family_data,
                               versionedit* edit, port::mutex* mu,
                               directory* db_directory, bool new_descriptor_log,
                               const columnfamilyoptions* options) {
  mu->assertheld();

  // column_family_data can be nullptr only if this is column_family_add.
  // in that case, we also need to specify columnfamilyoptions
  if (column_family_data == nullptr) {
    assert(edit->is_column_family_add_);
    assert(options != nullptr);
  }

  // queue our request
  manifestwriter w(mu, column_family_data, edit);
  manifest_writers_.push_back(&w);
  while (!w.done && &w != manifest_writers_.front()) {
    w.cv.wait();
  }
  if (w.done) {
    return w.status;
  }
  if (column_family_data != nullptr && column_family_data->isdropped()) {
    // if column family is dropped by the time we get here, no need to write
    // anything to the manifest
    manifest_writers_.pop_front();
    // notify new head of write queue
    if (!manifest_writers_.empty()) {
      manifest_writers_.front()->cv.signal();
    }
    return status::ok();
  }

  std::vector<versionedit*> batch_edits;
  version* v = nullptr;
  std::unique_ptr<builder> builder(nullptr);

  // process all requests in the queue
  manifestwriter* last_writer = &w;
  assert(!manifest_writers_.empty());
  assert(manifest_writers_.front() == &w);
  if (edit->iscolumnfamilymanipulation()) {
    // no group commits for column family add or drop
    logandapplycfhelper(edit);
    batch_edits.push_back(edit);
  } else {
    v = new version(column_family_data, this, current_version_number_++);
    builder.reset(new builder(column_family_data));
    for (const auto& writer : manifest_writers_) {
      if (writer->edit->iscolumnfamilymanipulation() ||
          writer->cfd->getid() != column_family_data->getid()) {
        // no group commits for column family add or drop
        // also, group commits across column families are not supported
        break;
      }
      last_writer = writer;
      logandapplyhelper(column_family_data, builder.get(), v, last_writer->edit,
                        mu);
      batch_edits.push_back(last_writer->edit);
    }
    builder->saveto(v);
  }

  // initialize new descriptor log file if necessary by creating
  // a temporary file that contains a snapshot of the current version.
  uint64_t new_manifest_file_size = 0;
  status s;

  assert(pending_manifest_file_number_ == 0);
  if (!descriptor_log_ ||
      manifest_file_size_ > options_->max_manifest_file_size) {
    pending_manifest_file_number_ = newfilenumber();
    batch_edits.back()->setnextfile(next_file_number_);
    new_descriptor_log = true;
  } else {
    pending_manifest_file_number_ = manifest_file_number_;
  }

  if (new_descriptor_log) {
    // if we're writing out new snapshot make sure to persist max column family
    if (column_family_set_->getmaxcolumnfamily() > 0) {
      edit->setmaxcolumnfamily(column_family_set_->getmaxcolumnfamily());
    }
  }

  // unlock during expensive operations. new writes cannot get here
  // because &w is ensuring that all new writes get queued.
  {
    std::vector<uint64_t> size_being_compacted;
    if (!edit->iscolumnfamilymanipulation()) {
      size_being_compacted.resize(v->numberlevels() - 1);
      // calculate the amount of data being compacted at every level
      column_family_data->compaction_picker()->sizebeingcompacted(
          size_being_compacted);
    }

    mu->unlock();

    if (!edit->iscolumnfamilymanipulation() && options_->max_open_files == -1) {
      // unlimited table cache. pre-load table handle now.
      // need to do it out of the mutex.
      builder->loadtablehandlers();
    }

    // this is fine because everything inside of this block is serialized --
    // only one thread can be here at the same time
    if (new_descriptor_log) {
      // create manifest file
      log(options_->info_log,
          "creating manifest %" priu64 "\n", pending_manifest_file_number_);
      unique_ptr<writablefile> descriptor_file;
      s = env_->newwritablefile(
          descriptorfilename(dbname_, pending_manifest_file_number_),
          &descriptor_file, env_->optimizeformanifestwrite(storage_options_));
      if (s.ok()) {
        descriptor_file->setpreallocationblocksize(
            options_->manifest_preallocation_size);
        descriptor_log_.reset(new log::writer(std::move(descriptor_file)));
        s = writesnapshot(descriptor_log_.get());
      }
    }

    if (!edit->iscolumnfamilymanipulation()) {
      // this is cpu-heavy operations, which should be called outside mutex.
      v->prepareapply(size_being_compacted);
    }

    // write new record to manifest log
    if (s.ok()) {
      for (auto& e : batch_edits) {
        std::string record;
        e->encodeto(&record);
        s = descriptor_log_->addrecord(record);
        if (!s.ok()) {
          break;
        }
      }
      if (s.ok()) {
        if (options_->use_fsync) {
          stopwatch sw(env_, options_->statistics.get(),
                       manifest_file_sync_micros);
          s = descriptor_log_->file()->fsync();
        } else {
          stopwatch sw(env_, options_->statistics.get(),
                       manifest_file_sync_micros);
          s = descriptor_log_->file()->sync();
        }
      }
      if (!s.ok()) {
        log(options_->info_log, "manifest write: %s\n", s.tostring().c_str());
        bool all_records_in = true;
        for (auto& e : batch_edits) {
          std::string record;
          e->encodeto(&record);
          if (!manifestcontains(pending_manifest_file_number_, record)) {
            all_records_in = false;
            break;
          }
        }
        if (all_records_in) {
          log(options_->info_log,
              "manifest contains log record despite error; advancing to new "
              "version to prevent mismatch between in-memory and logged state"
              " if paranoid is set, then the db is now in readonly mode.");
          s = status::ok();
        }
      }
    }

    // if we just created a new descriptor file, install it by writing a
    // new current file that points to it.
    if (s.ok() && new_descriptor_log) {
      s = setcurrentfile(env_, dbname_, pending_manifest_file_number_,
                         db_directory);
      if (s.ok() && pending_manifest_file_number_ > manifest_file_number_) {
        // delete old manifest file
        log(options_->info_log,
            "deleting manifest %" priu64 " current manifest %" priu64 "\n",
            manifest_file_number_, pending_manifest_file_number_);
        // we don't care about an error here, purgeobsoletefiles will take care
        // of it later
        env_->deletefile(descriptorfilename(dbname_, manifest_file_number_));
      }
    }

    if (s.ok()) {
      // find offset in manifest file where this version is stored.
      new_manifest_file_size = descriptor_log_->file()->getfilesize();
    }

    logflush(options_->info_log);
    mu->lock();
  }

  // install the new version
  if (s.ok()) {
    if (edit->is_column_family_add_) {
      // no group commit on column family add
      assert(batch_edits.size() == 1);
      assert(options != nullptr);
      createcolumnfamily(*options, edit);
    } else if (edit->is_column_family_drop_) {
      assert(batch_edits.size() == 1);
      column_family_data->setdropped();
      if (column_family_data->unref()) {
        delete column_family_data;
      }
    } else {
      uint64_t max_log_number_in_batch  = 0;
      for (auto& e : batch_edits) {
        if (e->has_log_number_) {
          max_log_number_in_batch =
              std::max(max_log_number_in_batch, e->log_number_);
        }
      }
      if (max_log_number_in_batch != 0) {
        assert(column_family_data->getlognumber() <= max_log_number_in_batch);
        column_family_data->setlognumber(max_log_number_in_batch);
      }
      appendversion(column_family_data, v);
    }

    manifest_file_number_ = pending_manifest_file_number_;
    manifest_file_size_ = new_manifest_file_size;
    prev_log_number_ = edit->prev_log_number_;
  } else {
    log(options_->info_log, "error in committing version %lu to [%s]",
        (unsigned long)v->getversionnumber(),
        column_family_data->getname().c_str());
    delete v;
    if (new_descriptor_log) {
      log(options_->info_log,
        "deleting manifest %" priu64 " current manifest %" priu64 "\n",
        manifest_file_number_, pending_manifest_file_number_);
      descriptor_log_.reset();
      env_->deletefile(
          descriptorfilename(dbname_, pending_manifest_file_number_));
    }
  }
  pending_manifest_file_number_ = 0;

  // wake up all the waiting writers
  while (true) {
    manifestwriter* ready = manifest_writers_.front();
    manifest_writers_.pop_front();
    if (ready != &w) {
      ready->status = s;
      ready->done = true;
      ready->cv.signal();
    }
    if (ready == last_writer) break;
  }
  // notify new head of write queue
  if (!manifest_writers_.empty()) {
    manifest_writers_.front()->cv.signal();
  }
  return s;
}

void versionset::logandapplycfhelper(versionedit* edit) {
  assert(edit->iscolumnfamilymanipulation());
  edit->setnextfile(next_file_number_);
  edit->setlastsequence(last_sequence_);
  if (edit->is_column_family_drop_) {
    // if we drop column family, we have to make sure to save max column family,
    // so that we don't reuse existing id
    edit->setmaxcolumnfamily(column_family_set_->getmaxcolumnfamily());
  }
}

void versionset::logandapplyhelper(columnfamilydata* cfd, builder* builder,
                                   version* v, versionedit* edit,
                                   port::mutex* mu) {
  mu->assertheld();
  assert(!edit->iscolumnfamilymanipulation());

  if (edit->has_log_number_) {
    assert(edit->log_number_ >= cfd->getlognumber());
    assert(edit->log_number_ < next_file_number_);
  }

  if (!edit->has_prev_log_number_) {
    edit->setprevlognumber(prev_log_number_);
  }
  edit->setnextfile(next_file_number_);
  edit->setlastsequence(last_sequence_);

  builder->apply(edit);
}

status versionset::recover(
    const std::vector<columnfamilydescriptor>& column_families,
    bool read_only) {
  std::unordered_map<std::string, columnfamilyoptions> cf_name_to_options;
  for (auto cf : column_families) {
    cf_name_to_options.insert({cf.name, cf.options});
  }
  // keeps track of column families in manifest that were not found in
  // column families parameters. if those column families are not dropped
  // by subsequent manifest records, recover() will return failure status
  std::unordered_map<int, std::string> column_families_not_found;

  // read "current" file, which contains a pointer to the current manifest file
  std::string manifest_filename;
  status s = readfiletostring(
      env_, currentfilename(dbname_), &manifest_filename
  );
  if (!s.ok()) {
    return s;
  }
  if (manifest_filename.empty() ||
      manifest_filename.back() != '\n') {
    return status::corruption("current file does not end with newline");
  }
  // remove the trailing '\n'
  manifest_filename.resize(manifest_filename.size() - 1);
  filetype type;
  bool parse_ok =
      parsefilename(manifest_filename, &manifest_file_number_, &type);
  if (!parse_ok || type != kdescriptorfile) {
    return status::corruption("current file corrupted");
  }

  log(options_->info_log, "recovering from manifest file: %s\n",
      manifest_filename.c_str());

  manifest_filename = dbname_ + "/" + manifest_filename;
  unique_ptr<sequentialfile> manifest_file;
  s = env_->newsequentialfile(manifest_filename, &manifest_file,
                              storage_options_);
  if (!s.ok()) {
    return s;
  }
  uint64_t manifest_file_size;
  s = env_->getfilesize(manifest_filename, &manifest_file_size);
  if (!s.ok()) {
    return s;
  }

  bool have_log_number = false;
  bool have_prev_log_number = false;
  bool have_next_file = false;
  bool have_last_sequence = false;
  uint64_t next_file = 0;
  uint64_t last_sequence = 0;
  uint64_t log_number = 0;
  uint64_t prev_log_number = 0;
  uint32_t max_column_family = 0;
  std::unordered_map<uint32_t, builder*> builders;

  // add default column family
  auto default_cf_iter = cf_name_to_options.find(kdefaultcolumnfamilyname);
  if (default_cf_iter == cf_name_to_options.end()) {
    return status::invalidargument("default column family not specified");
  }
  versionedit default_cf_edit;
  default_cf_edit.addcolumnfamily(kdefaultcolumnfamilyname);
  default_cf_edit.setcolumnfamily(0);
  columnfamilydata* default_cfd =
      createcolumnfamily(default_cf_iter->second, &default_cf_edit);
  builders.insert({0, new builder(default_cfd)});

  {
    versionset::logreporter reporter;
    reporter.status = &s;
    log::reader reader(std::move(manifest_file), &reporter, true /*checksum*/,
                       0 /*initial_offset*/);
    slice record;
    std::string scratch;
    while (reader.readrecord(&record, &scratch) && s.ok()) {
      versionedit edit;
      s = edit.decodefrom(record);
      if (!s.ok()) {
        break;
      }

      // not found means that user didn't supply that column
      // family option and we encountered column family add
      // record. once we encounter column family drop record,
      // we will delete the column family from
      // column_families_not_found.
      bool cf_in_not_found =
          column_families_not_found.find(edit.column_family_) !=
          column_families_not_found.end();
      // in builders means that user supplied that column family
      // option and that we encountered column family add record
      bool cf_in_builders =
          builders.find(edit.column_family_) != builders.end();

      // they can't both be true
      assert(!(cf_in_not_found && cf_in_builders));

      columnfamilydata* cfd = nullptr;

      if (edit.is_column_family_add_) {
        if (cf_in_builders || cf_in_not_found) {
          s = status::corruption(
              "manifest adding the same column family twice");
          break;
        }
        auto cf_options = cf_name_to_options.find(edit.column_family_name_);
        if (cf_options == cf_name_to_options.end()) {
          column_families_not_found.insert(
              {edit.column_family_, edit.column_family_name_});
        } else {
          cfd = createcolumnfamily(cf_options->second, &edit);
          builders.insert({edit.column_family_, new builder(cfd)});
        }
      } else if (edit.is_column_family_drop_) {
        if (cf_in_builders) {
          auto builder = builders.find(edit.column_family_);
          assert(builder != builders.end());
          delete builder->second;
          builders.erase(builder);
          cfd = column_family_set_->getcolumnfamily(edit.column_family_);
          if (cfd->unref()) {
            delete cfd;
            cfd = nullptr;
          } else {
            // who else can have reference to cfd!?
            assert(false);
          }
        } else if (cf_in_not_found) {
          column_families_not_found.erase(edit.column_family_);
        } else {
          s = status::corruption(
              "manifest - dropping non-existing column family");
          break;
        }
      } else if (!cf_in_not_found) {
        if (!cf_in_builders) {
          s = status::corruption(
              "manifest record referencing unknown column family");
          break;
        }

        cfd = column_family_set_->getcolumnfamily(edit.column_family_);
        // this should never happen since cf_in_builders is true
        assert(cfd != nullptr);
        if (edit.max_level_ >= cfd->current()->numberlevels()) {
          s = status::invalidargument(
              "db has more levels than options.num_levels");
          break;
        }

        // if it is not column family add or column family drop,
        // then it's a file add/delete, which should be forwarded
        // to builder
        auto builder = builders.find(edit.column_family_);
        assert(builder != builders.end());
        builder->second->apply(&edit);
      }

      if (cfd != nullptr) {
        if (edit.has_log_number_) {
          if (cfd->getlognumber() > edit.log_number_) {
            log(options_->info_log,
                "manifest corruption detected, but ignored - log numbers in "
                "records not monotonically increasing");
          } else {
            cfd->setlognumber(edit.log_number_);
            have_log_number = true;
          }
        }
        if (edit.has_comparator_ &&
            edit.comparator_ != cfd->user_comparator()->name()) {
          s = status::invalidargument(
              cfd->user_comparator()->name(),
              "does not match existing comparator " + edit.comparator_);
          break;
        }
      }

      if (edit.has_prev_log_number_) {
        prev_log_number = edit.prev_log_number_;
        have_prev_log_number = true;
      }

      if (edit.has_next_file_number_) {
        next_file = edit.next_file_number_;
        have_next_file = true;
      }

      if (edit.has_max_column_family_) {
        max_column_family = edit.max_column_family_;
      }

      if (edit.has_last_sequence_) {
        last_sequence = edit.last_sequence_;
        have_last_sequence = true;
      }
    }
  }

  if (s.ok()) {
    if (!have_next_file) {
      s = status::corruption("no meta-nextfile entry in descriptor");
    } else if (!have_log_number) {
      s = status::corruption("no meta-lognumber entry in descriptor");
    } else if (!have_last_sequence) {
      s = status::corruption("no last-sequence-number entry in descriptor");
    }

    if (!have_prev_log_number) {
      prev_log_number = 0;
    }

    column_family_set_->updatemaxcolumnfamily(max_column_family);

    markfilenumberused(prev_log_number);
    markfilenumberused(log_number);
  }

  // there were some column families in the manifest that weren't specified
  // in the argument. this is ok in read_only mode
  if (read_only == false && column_families_not_found.size() > 0) {
    std::string list_of_not_found;
    for (const auto& cf : column_families_not_found) {
      list_of_not_found += ", " + cf.second;
    }
    list_of_not_found = list_of_not_found.substr(2);
    s = status::invalidargument(
        "you have to open all column families. column families not opened: " +
        list_of_not_found);
  }

  if (s.ok()) {
    for (auto cfd : *column_family_set_) {
      auto builders_iter = builders.find(cfd->getid());
      assert(builders_iter != builders.end());
      auto builder = builders_iter->second;

      if (options_->max_open_files == -1) {
      // unlimited table cache. pre-load table handle now.
      // need to do it out of the mutex.
        builder->loadtablehandlers();
      }

      version* v = new version(cfd, this, current_version_number_++);
      builder->saveto(v);

      // install recovered version
      std::vector<uint64_t> size_being_compacted(v->numberlevels() - 1);
      cfd->compaction_picker()->sizebeingcompacted(size_being_compacted);
      v->prepareapply(size_being_compacted);
      appendversion(cfd, v);
    }

    manifest_file_size_ = manifest_file_size;
    next_file_number_ = next_file + 1;
    last_sequence_ = last_sequence;
    prev_log_number_ = prev_log_number;

    log(options_->info_log,
        "recovered from manifest file:%s succeeded,"
        "manifest_file_number is %lu, next_file_number is %lu, "
        "last_sequence is %lu, log_number is %lu,"
        "prev_log_number is %lu,"
        "max_column_family is %u\n",
        manifest_filename.c_str(), (unsigned long)manifest_file_number_,
        (unsigned long)next_file_number_, (unsigned long)last_sequence_,
        (unsigned long)log_number, (unsigned long)prev_log_number_,
        column_family_set_->getmaxcolumnfamily());

    for (auto cfd : *column_family_set_) {
      log(options_->info_log,
          "column family [%s] (id %u), log number is %" priu64 "\n",
          cfd->getname().c_str(), cfd->getid(), cfd->getlognumber());
    }
  }

  for (auto builder : builders) {
    delete builder.second;
  }

  return s;
}

status versionset::listcolumnfamilies(std::vector<std::string>* column_families,
                                      const std::string& dbname, env* env) {
  // these are just for performance reasons, not correcntes,
  // so we're fine using the defaults
  envoptions soptions;
  // read "current" file, which contains a pointer to the current manifest file
  std::string current;
  status s = readfiletostring(env, currentfilename(dbname), &current);
  if (!s.ok()) {
    return s;
  }
  if (current.empty() || current[current.size()-1] != '\n') {
    return status::corruption("current file does not end with newline");
  }
  current.resize(current.size() - 1);

  std::string dscname = dbname + "/" + current;
  unique_ptr<sequentialfile> file;
  s = env->newsequentialfile(dscname, &file, soptions);
  if (!s.ok()) {
    return s;
  }

  std::map<uint32_t, std::string> column_family_names;
  // default column family is always implicitly there
  column_family_names.insert({0, kdefaultcolumnfamilyname});
  versionset::logreporter reporter;
  reporter.status = &s;
  log::reader reader(std::move(file), &reporter, true /*checksum*/,
                     0 /*initial_offset*/);
  slice record;
  std::string scratch;
  while (reader.readrecord(&record, &scratch) && s.ok()) {
    versionedit edit;
    s = edit.decodefrom(record);
    if (!s.ok()) {
      break;
    }
    if (edit.is_column_family_add_) {
      if (column_family_names.find(edit.column_family_) !=
          column_family_names.end()) {
        s = status::corruption("manifest adding the same column family twice");
        break;
      }
      column_family_names.insert(
          {edit.column_family_, edit.column_family_name_});
    } else if (edit.is_column_family_drop_) {
      if (column_family_names.find(edit.column_family_) ==
          column_family_names.end()) {
        s = status::corruption(
            "manifest - dropping non-existing column family");
        break;
      }
      column_family_names.erase(edit.column_family_);
    }
  }

  column_families->clear();
  if (s.ok()) {
    for (const auto& iter : column_family_names) {
      column_families->push_back(iter.second);
    }
  }

  return s;
}

#ifndef rocksdb_lite
status versionset::reducenumberoflevels(const std::string& dbname,
                                        const options* options,
                                        const envoptions& storage_options,
                                        int new_levels) {
  if (new_levels <= 1) {
    return status::invalidargument(
        "number of levels needs to be bigger than 1");
  }

  columnfamilyoptions cf_options(*options);
  std::shared_ptr<cache> tc(newlrucache(
      options->max_open_files - 10, options->table_cache_numshardbits,
      options->table_cache_remove_scan_count_limit));
  versionset versions(dbname, options, storage_options, tc.get());
  status status;

  std::vector<columnfamilydescriptor> dummy;
  columnfamilydescriptor dummy_descriptor(kdefaultcolumnfamilyname,
                                          columnfamilyoptions(*options));
  dummy.push_back(dummy_descriptor);
  status = versions.recover(dummy);
  if (!status.ok()) {
    return status;
  }

  version* current_version =
      versions.getcolumnfamilyset()->getdefault()->current();
  int current_levels = current_version->numberlevels();

  if (current_levels <= new_levels) {
    return status::ok();
  }

  // make sure there are file only on one level from
  // (new_levels-1) to (current_levels-1)
  int first_nonempty_level = -1;
  int first_nonempty_level_filenum = 0;
  for (int i = new_levels - 1; i < current_levels; i++) {
    int file_num = current_version->numlevelfiles(i);
    if (file_num != 0) {
      if (first_nonempty_level < 0) {
        first_nonempty_level = i;
        first_nonempty_level_filenum = file_num;
      } else {
        char msg[255];
        snprintf(msg, sizeof(msg),
                 "found at least two levels containing files: "
                 "[%d:%d],[%d:%d].\n",
                 first_nonempty_level, first_nonempty_level_filenum, i,
                 file_num);
        return status::invalidargument(msg);
      }
    }
  }

  std::vector<filemetadata*>* old_files_list = current_version->files_;
  // we need to allocate an array with the old number of levels size to
  // avoid sigsegv in writesnapshot()
  // however, all levels bigger or equal to new_levels will be empty
  std::vector<filemetadata*>* new_files_list =
      new std::vector<filemetadata*>[current_levels];
  for (int i = 0; i < new_levels - 1; i++) {
    new_files_list[i] = old_files_list[i];
  }

  if (first_nonempty_level > 0) {
    new_files_list[new_levels - 1] = old_files_list[first_nonempty_level];
  }

  delete[] current_version->files_;
  current_version->files_ = new_files_list;
  current_version->num_levels_ = new_levels;

  versionedit ve;
  port::mutex dummy_mutex;
  mutexlock l(&dummy_mutex);
  return versions.logandapply(versions.getcolumnfamilyset()->getdefault(), &ve,
                              &dummy_mutex, nullptr, true);
}

status versionset::dumpmanifest(options& options, std::string& dscname,
                                bool verbose, bool hex) {
  // open the specified manifest file.
  unique_ptr<sequentialfile> file;
  status s = options.env->newsequentialfile(dscname, &file, storage_options_);
  if (!s.ok()) {
    return s;
  }

  bool have_prev_log_number = false;
  bool have_next_file = false;
  bool have_last_sequence = false;
  uint64_t next_file = 0;
  uint64_t last_sequence = 0;
  uint64_t prev_log_number = 0;
  int count = 0;
  std::unordered_map<uint32_t, std::string> comparators;
  std::unordered_map<uint32_t, builder*> builders;

  // add default column family
  versionedit default_cf_edit;
  default_cf_edit.addcolumnfamily(kdefaultcolumnfamilyname);
  default_cf_edit.setcolumnfamily(0);
  columnfamilydata* default_cfd =
      createcolumnfamily(columnfamilyoptions(options), &default_cf_edit);
  builders.insert({0, new builder(default_cfd)});

  {
    versionset::logreporter reporter;
    reporter.status = &s;
    log::reader reader(std::move(file), &reporter, true/*checksum*/,
                       0/*initial_offset*/);
    slice record;
    std::string scratch;
    while (reader.readrecord(&record, &scratch) && s.ok()) {
      versionedit edit;
      s = edit.decodefrom(record);
      if (!s.ok()) {
        break;
      }

      // write out each individual edit
      if (verbose) {
        printf("*************************edit[%d] = %s\n",
                count, edit.debugstring(hex).c_str());
      }
      count++;

      bool cf_in_builders =
          builders.find(edit.column_family_) != builders.end();

      if (edit.has_comparator_) {
        comparators.insert({edit.column_family_, edit.comparator_});
      }

      columnfamilydata* cfd = nullptr;

      if (edit.is_column_family_add_) {
        if (cf_in_builders) {
          s = status::corruption(
              "manifest adding the same column family twice");
          break;
        }
        cfd = createcolumnfamily(columnfamilyoptions(options), &edit);
        builders.insert({edit.column_family_, new builder(cfd)});
      } else if (edit.is_column_family_drop_) {
        if (!cf_in_builders) {
          s = status::corruption(
              "manifest - dropping non-existing column family");
          break;
        }
        auto builder_iter = builders.find(edit.column_family_);
        delete builder_iter->second;
        builders.erase(builder_iter);
        comparators.erase(edit.column_family_);
        cfd = column_family_set_->getcolumnfamily(edit.column_family_);
        assert(cfd != nullptr);
        cfd->unref();
        delete cfd;
        cfd = nullptr;
      } else {
        if (!cf_in_builders) {
          s = status::corruption(
              "manifest record referencing unknown column family");
          break;
        }

        cfd = column_family_set_->getcolumnfamily(edit.column_family_);
        // this should never happen since cf_in_builders is true
        assert(cfd != nullptr);

        // if it is not column family add or column family drop,
        // then it's a file add/delete, which should be forwarded
        // to builder
        auto builder = builders.find(edit.column_family_);
        assert(builder != builders.end());
        builder->second->apply(&edit);
      }

      if (cfd != nullptr && edit.has_log_number_) {
        cfd->setlognumber(edit.log_number_);
      }

      if (edit.has_prev_log_number_) {
        prev_log_number = edit.prev_log_number_;
        have_prev_log_number = true;
      }

      if (edit.has_next_file_number_) {
        next_file = edit.next_file_number_;
        have_next_file = true;
      }

      if (edit.has_last_sequence_) {
        last_sequence = edit.last_sequence_;
        have_last_sequence = true;
      }

      if (edit.has_max_column_family_) {
        column_family_set_->updatemaxcolumnfamily(edit.max_column_family_);
      }
    }
  }
  file.reset();

  if (s.ok()) {
    if (!have_next_file) {
      s = status::corruption("no meta-nextfile entry in descriptor");
      printf("no meta-nextfile entry in descriptor");
    } else if (!have_last_sequence) {
      printf("no last-sequence-number entry in descriptor");
      s = status::corruption("no last-sequence-number entry in descriptor");
    }

    if (!have_prev_log_number) {
      prev_log_number = 0;
    }
  }

  if (s.ok()) {
    for (auto cfd : *column_family_set_) {
      auto builders_iter = builders.find(cfd->getid());
      assert(builders_iter != builders.end());
      auto builder = builders_iter->second;

      version* v = new version(cfd, this, current_version_number_++);
      builder->saveto(v);
      std::vector<uint64_t> size_being_compacted(v->numberlevels() - 1);
      cfd->compaction_picker()->sizebeingcompacted(size_being_compacted);
      v->prepareapply(size_being_compacted);
      delete builder;

      printf("--------------- column family \"%s\"  (id %u) --------------\n",
             cfd->getname().c_str(), (unsigned int)cfd->getid());
      printf("log number: %lu\n", (unsigned long)cfd->getlognumber());
      auto comparator = comparators.find(cfd->getid());
      if (comparator != comparators.end()) {
        printf("comparator: %s\n", comparator->second.c_str());
      } else {
        printf("comparator: <no comparator>\n");
      }
      printf("%s \n", v->debugstring(hex).c_str());
      delete v;
    }

    next_file_number_ = next_file + 1;
    last_sequence_ = last_sequence;
    prev_log_number_ = prev_log_number;

    printf(
        "next_file_number %lu last_sequence "
        "%lu  prev_log_number %lu max_column_family %u\n",
        (unsigned long)next_file_number_, (unsigned long)last_sequence,
        (unsigned long)prev_log_number,
        column_family_set_->getmaxcolumnfamily());
  }

  return s;
}
#endif  // rocksdb_lite

void versionset::markfilenumberused(uint64_t number) {
  if (next_file_number_ <= number) {
    next_file_number_ = number + 1;
  }
}

status versionset::writesnapshot(log::writer* log) {
  // todo: break up into multiple records to reduce memory usage on recovery?

  // warning: this method doesn't hold a mutex!!

  // this is done without db mutex lock held, but only within single-threaded
  // logandapply. column family manipulations can only happen within logandapply
  // (the same single thread), so we're safe to iterate.
  for (auto cfd : *column_family_set_) {
    {
      // store column family info
      versionedit edit;
      if (cfd->getid() != 0) {
        // default column family is always there,
        // no need to explicitly write it
        edit.addcolumnfamily(cfd->getname());
        edit.setcolumnfamily(cfd->getid());
      }
      edit.setcomparatorname(
          cfd->internal_comparator().user_comparator()->name());
      std::string record;
      edit.encodeto(&record);
      status s = log->addrecord(record);
      if (!s.ok()) {
        return s;
      }
    }

    {
      // save files
      versionedit edit;
      edit.setcolumnfamily(cfd->getid());

      for (int level = 0; level < cfd->numberlevels(); level++) {
        for (const auto& f : cfd->current()->files_[level]) {
          edit.addfile(level, f->fd.getnumber(), f->fd.getpathid(),
                       f->fd.getfilesize(), f->smallest, f->largest,
                       f->smallest_seqno, f->largest_seqno);
        }
      }
      edit.setlognumber(cfd->getlognumber());
      std::string record;
      edit.encodeto(&record);
      status s = log->addrecord(record);
      if (!s.ok()) {
        return s;
      }
    }
  }

  return status::ok();
}

// opens the mainfest file and reads all records
// till it finds the record we are looking for.
bool versionset::manifestcontains(uint64_t manifest_file_number,
                                  const std::string& record) const {
  std::string fname =
      descriptorfilename(dbname_, manifest_file_number);
  log(options_->info_log, "manifestcontains: checking %s\n", fname.c_str());
  unique_ptr<sequentialfile> file;
  status s = env_->newsequentialfile(fname, &file, storage_options_);
  if (!s.ok()) {
    log(options_->info_log, "manifestcontains: %s\n", s.tostring().c_str());
    log(options_->info_log,
        "manifestcontains: is unable to reopen the manifest file  %s",
        fname.c_str());
    return false;
  }
  log::reader reader(std::move(file), nullptr, true/*checksum*/, 0);
  slice r;
  std::string scratch;
  bool result = false;
  while (reader.readrecord(&r, &scratch)) {
    if (r == slice(record)) {
      result = true;
      break;
    }
  }
  log(options_->info_log, "manifestcontains: result = %d\n", result ? 1 : 0);
  return result;
}


uint64_t versionset::approximateoffsetof(version* v, const internalkey& ikey) {
  uint64_t result = 0;
  for (int level = 0; level < v->numberlevels(); level++) {
    const std::vector<filemetadata*>& files = v->files_[level];
    for (size_t i = 0; i < files.size(); i++) {
      if (v->cfd_->internal_comparator().compare(files[i]->largest, ikey) <=
          0) {
        // entire file is before "ikey", so just add the file size
        result += files[i]->fd.getfilesize();
      } else if (v->cfd_->internal_comparator().compare(files[i]->smallest,
                                                        ikey) > 0) {
        // entire file is after "ikey", so ignore
        if (level > 0) {
          // files other than level 0 are sorted by meta->smallest, so
          // no further files in this level will contain data for
          // "ikey".
          break;
        }
      } else {
        // "ikey" falls in the range for this table.  add the
        // approximate offset of "ikey" within the table.
        tablereader* table_reader_ptr;
        iterator* iter = v->cfd_->table_cache()->newiterator(
            readoptions(), storage_options_, v->cfd_->internal_comparator(),
            files[i]->fd, &table_reader_ptr);
        if (table_reader_ptr != nullptr) {
          result += table_reader_ptr->approximateoffsetof(ikey.encode());
        }
        delete iter;
      }
    }
  }
  return result;
}

void versionset::addlivefiles(std::vector<filedescriptor>* live_list) {
  // pre-calculate space requirement
  int64_t total_files = 0;
  for (auto cfd : *column_family_set_) {
    version* dummy_versions = cfd->dummy_versions();
    for (version* v = dummy_versions->next_; v != dummy_versions;
         v = v->next_) {
      for (int level = 0; level < v->numberlevels(); level++) {
        total_files += v->files_[level].size();
      }
    }
  }

  // just one time extension to the right size
  live_list->reserve(live_list->size() + total_files);

  for (auto cfd : *column_family_set_) {
    version* dummy_versions = cfd->dummy_versions();
    for (version* v = dummy_versions->next_; v != dummy_versions;
         v = v->next_) {
      for (int level = 0; level < v->numberlevels(); level++) {
        for (const auto& f : v->files_[level]) {
          live_list->push_back(f->fd);
        }
      }
    }
  }
}

iterator* versionset::makeinputiterator(compaction* c) {
  auto cfd = c->column_family_data();
  readoptions read_options;
  read_options.verify_checksums =
    cfd->options()->verify_checksums_in_compaction;
  read_options.fill_cache = false;

  // level-0 files have to be merged together.  for other levels,
  // we will make a concatenating iterator per level.
  // todo(opt): use concatenating iterator for level-0 if there is no overlap
  const int space = (c->level() == 0 ?
      c->input_levels(0)->num_files + c->num_input_levels() - 1:
      c->num_input_levels());
  iterator** list = new iterator*[space];
  int num = 0;
  for (int which = 0; which < c->num_input_levels(); which++) {
    if (c->input_levels(which)->num_files != 0) {
      if (c->level(which) == 0) {
        const filelevel* flevel = c->input_levels(which);
        for (size_t i = 0; i < flevel->num_files; i++) {
          list[num++] = cfd->table_cache()->newiterator(
              read_options, storage_options_compactions_,
              cfd->internal_comparator(), flevel->files[i].fd, nullptr,
              true /* for compaction */);
        }
      } else {
        // create concatenating iterator for the files from this level
        list[num++] = newtwoleveliterator(new version::levelfileiteratorstate(
              cfd->table_cache(), read_options, storage_options_,
              cfd->internal_comparator(), true /* for_compaction */,
              false /* prefix enabled */),
            new version::levelfilenumiterator(cfd->internal_comparator(),
                                              c->input_levels(which)));
      }
    }
  }
  assert(num <= space);
  iterator* result = newmergingiterator(
      &c->column_family_data()->internal_comparator(), list, num);
  delete[] list;
  return result;
}

// verify that the files listed in this compaction are present
// in the current version
bool versionset::verifycompactionfileconsistency(compaction* c) {
#ifndef ndebug
  version* version = c->column_family_data()->current();
  if (c->input_version() != version) {
    log(options_->info_log,
        "[%s] verifycompactionfileconsistency version mismatch",
        c->column_family_data()->getname().c_str());
  }

  // verify files in level
  int level = c->level();
  for (int i = 0; i < c->num_input_files(0); i++) {
    uint64_t number = c->input(0, i)->fd.getnumber();

    // look for this file in the current version
    bool found = false;
    for (unsigned int j = 0; j < version->files_[level].size(); j++) {
      filemetadata* f = version->files_[level][j];
      if (f->fd.getnumber() == number) {
        found = true;
        break;
      }
    }
    if (!found) {
      return false; // input files non existant in current version
    }
  }
  // verify level+1 files
  level++;
  for (int i = 0; i < c->num_input_files(1); i++) {
    uint64_t number = c->input(1, i)->fd.getnumber();

    // look for this file in the current version
    bool found = false;
    for (unsigned int j = 0; j < version->files_[level].size(); j++) {
      filemetadata* f = version->files_[level][j];
      if (f->fd.getnumber() == number) {
        found = true;
        break;
      }
    }
    if (!found) {
      return false; // input files non existant in current version
    }
  }
#endif
  return true;     // everything good
}

status versionset::getmetadataforfile(uint64_t number, int* filelevel,
                                      filemetadata** meta,
                                      columnfamilydata** cfd) {
  for (auto cfd_iter : *column_family_set_) {
    version* version = cfd_iter->current();
    for (int level = 0; level < version->numberlevels(); level++) {
      for (const auto& file : version->files_[level]) {
        if (file->fd.getnumber() == number) {
          *meta = file;
          *filelevel = level;
          *cfd = cfd_iter;
          return status::ok();
        }
      }
    }
  }
  return status::notfound("file not present in any level");
}

void versionset::getlivefilesmetadata(std::vector<livefilemetadata>* metadata) {
  for (auto cfd : *column_family_set_) {
    for (int level = 0; level < cfd->numberlevels(); level++) {
      for (const auto& file : cfd->current()->files_[level]) {
        livefilemetadata filemetadata;
        filemetadata.column_family_name = cfd->getname();
        uint32_t path_id = file->fd.getpathid();
        if (path_id < options_->db_paths.size()) {
          filemetadata.db_path = options_->db_paths[path_id].path;
        } else {
          assert(!options_->db_paths.empty());
          filemetadata.db_path = options_->db_paths.back().path;
        }
        filemetadata.name = maketablefilename("", file->fd.getnumber());
        filemetadata.level = level;
        filemetadata.size = file->fd.getfilesize();
        filemetadata.smallestkey = file->smallest.user_key().tostring();
        filemetadata.largestkey = file->largest.user_key().tostring();
        filemetadata.smallest_seqno = file->smallest_seqno;
        filemetadata.largest_seqno = file->largest_seqno;
        metadata->push_back(filemetadata);
      }
    }
  }
}

void versionset::getobsoletefiles(std::vector<filemetadata*>* files) {
  files->insert(files->end(), obsolete_files_.begin(), obsolete_files_.end());
  obsolete_files_.clear();
}

columnfamilydata* versionset::createcolumnfamily(
    const columnfamilyoptions& options, versionedit* edit) {
  assert(edit->is_column_family_add_);

  version* dummy_versions = new version(nullptr, this);
  auto new_cfd = column_family_set_->createcolumnfamily(
      edit->column_family_name_, edit->column_family_, dummy_versions, options);

  version* v = new version(new_cfd, this, current_version_number_++);

  appendversion(new_cfd, v);
  new_cfd->createnewmemtable();
  new_cfd->setlognumber(edit->log_number_);
  return new_cfd;
}

}  // namespace rocksdb
