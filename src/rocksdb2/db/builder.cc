//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "db/builder.h"

#include "db/dbformat.h"
#include "db/filename.h"
#include "db/merge_helper.h"
#include "db/table_cache.h"
#include "db/version_edit.h"
#include "rocksdb/db.h"
#include "rocksdb/env.h"
#include "rocksdb/iterator.h"
#include "rocksdb/options.h"
#include "rocksdb/table.h"
#include "table/block_based_table_builder.h"
#include "util/stop_watch.h"

namespace rocksdb {

class tablefactory;

tablebuilder* newtablebuilder(const options& options,
                              const internalkeycomparator& internal_comparator,
                              writablefile* file,
                              compressiontype compression_type) {
  return options.table_factory->newtablebuilder(options, internal_comparator,
                                                file, compression_type);
}

status buildtable(const std::string& dbname, env* env, const options& options,
                  const envoptions& soptions, tablecache* table_cache,
                  iterator* iter, filemetadata* meta,
                  const internalkeycomparator& internal_comparator,
                  const sequencenumber newest_snapshot,
                  const sequencenumber earliest_seqno_in_memtable,
                  const compressiontype compression,
                  const env::iopriority io_priority) {
  status s;
  meta->fd.file_size = 0;
  meta->smallest_seqno = meta->largest_seqno = 0;
  iter->seektofirst();

  // if the sequence number of the smallest entry in the memtable is
  // smaller than the most recent snapshot, then we do not trigger
  // removal of duplicate/deleted keys as part of this builder.
  bool purge = options.purge_redundant_kvs_while_flush;
  if (earliest_seqno_in_memtable <= newest_snapshot) {
    purge = false;
  }

  std::string fname = tablefilename(options.db_paths, meta->fd.getnumber(),
                                    meta->fd.getpathid());
  if (iter->valid()) {
    unique_ptr<writablefile> file;
    s = env->newwritablefile(fname, &file, soptions);
    if (!s.ok()) {
      return s;
    }
    file->setiopriority(io_priority);

    tablebuilder* builder =
        newtablebuilder(options, internal_comparator, file.get(), compression);

    // the first key is the smallest key
    slice key = iter->key();
    meta->smallest.decodefrom(key);
    meta->smallest_seqno = getinternalkeyseqno(key);
    meta->largest_seqno = meta->smallest_seqno;

    mergehelper merge(internal_comparator.user_comparator(),
                      options.merge_operator.get(), options.info_log.get(),
                      options.min_partial_merge_operands,
                      true /* internal key corruption is not ok */);

    if (purge) {
      // ugly walkaround to avoid compiler error for release build
      bool ok __attribute__((unused)) = true;

      // will write to builder if current key != prev key
      parsedinternalkey prev_ikey;
      std::string prev_key;
      bool is_first_key = true;    // also write if this is the very first key

      while (iter->valid()) {
        bool iterator_at_next = false;

        // get current key
        parsedinternalkey this_ikey;
        slice key = iter->key();
        slice value = iter->value();

        // in-memory key corruption is not ok;
        // todo: find a clean way to treat in memory key corruption
        ok = parseinternalkey(key, &this_ikey);
        assert(ok);
        assert(this_ikey.sequence >= earliest_seqno_in_memtable);

        // if the key is the same as the previous key (and it is not the
        // first key), then we skip it, since it is an older version.
        // otherwise we output the key and mark it as the "new" previous key.
        if (!is_first_key && !internal_comparator.user_comparator()->compare(
                                  prev_ikey.user_key, this_ikey.user_key)) {
          // seqno within the same key are in decreasing order
          assert(this_ikey.sequence < prev_ikey.sequence);
        } else {
          is_first_key = false;

          if (this_ikey.type == ktypemerge) {
            // todo(tbd): add a check here to prevent rocksdb from crash when
            // reopening a db w/o properly specifying the merge operator.  but
            // currently we observed a memory leak on failing in rocksdb
            // recovery, so we decide to let it crash instead of causing
            // memory leak for now before we have identified the real cause
            // of the memory leak.

            // handle merge-type keys using the mergehelper
            // todo: pass statistics to mergeuntil
            merge.mergeuntil(iter, 0 /* don't worry about snapshot */);
            iterator_at_next = true;
            if (merge.issuccess()) {
              // merge completed correctly.
              // add the resulting merge key/value and continue to next
              builder->add(merge.key(), merge.value());
              prev_key.assign(merge.key().data(), merge.key().size());
              ok = parseinternalkey(slice(prev_key), &prev_ikey);
              assert(ok);
            } else {
              // merge did not find a put/delete.
              // can not compact these merges into a kvaluetype.
              // write them out one-by-one. (proceed back() to front())
              const std::deque<std::string>& keys = merge.keys();
              const std::deque<std::string>& values = merge.values();
              assert(keys.size() == values.size() && keys.size() >= 1);
              std::deque<std::string>::const_reverse_iterator key_iter;
              std::deque<std::string>::const_reverse_iterator value_iter;
              for (key_iter=keys.rbegin(), value_iter = values.rbegin();
                   key_iter != keys.rend() && value_iter != values.rend();
                   ++key_iter, ++value_iter) {

                builder->add(slice(*key_iter), slice(*value_iter));
              }

              // sanity check. both iterators should end at the same time
              assert(key_iter == keys.rend() && value_iter == values.rend());

              prev_key.assign(keys.front());
              ok = parseinternalkey(slice(prev_key), &prev_ikey);
              assert(ok);
            }
          } else {
            // handle put/delete-type keys by simply writing them
            builder->add(key, value);
            prev_key.assign(key.data(), key.size());
            ok = parseinternalkey(slice(prev_key), &prev_ikey);
            assert(ok);
          }
        }

        if (!iterator_at_next) iter->next();
      }

      // the last key is the largest key
      meta->largest.decodefrom(slice(prev_key));
      sequencenumber seqno = getinternalkeyseqno(slice(prev_key));
      meta->smallest_seqno = std::min(meta->smallest_seqno, seqno);
      meta->largest_seqno = std::max(meta->largest_seqno, seqno);

    } else {
      for (; iter->valid(); iter->next()) {
        slice key = iter->key();
        meta->largest.decodefrom(key);
        builder->add(key, iter->value());
        sequencenumber seqno = getinternalkeyseqno(key);
        meta->smallest_seqno = std::min(meta->smallest_seqno, seqno);
        meta->largest_seqno = std::max(meta->largest_seqno, seqno);
      }
    }

    // finish and check for builder errors
    if (s.ok()) {
      s = builder->finish();
      if (s.ok()) {
        meta->fd.file_size = builder->filesize();
        assert(meta->fd.getfilesize() > 0);
      }
    } else {
      builder->abandon();
    }
    delete builder;

    // finish and check for file errors
    if (s.ok() && !options.disabledatasync) {
      if (options.use_fsync) {
        stopwatch sw(env, options.statistics.get(), table_sync_micros);
        s = file->fsync();
      } else {
        stopwatch sw(env, options.statistics.get(), table_sync_micros);
        s = file->sync();
      }
    }
    if (s.ok()) {
      s = file->close();
    }

    if (s.ok()) {
      // verify that the table is usable
      iterator* it = table_cache->newiterator(readoptions(), soptions,
                                              internal_comparator, meta->fd);
      s = it->status();
      delete it;
    }
  }

  // check for input iterator errors
  if (!iter->status().ok()) {
    s = iter->status();
  }

  if (s.ok() && meta->fd.getfilesize() > 0) {
    // keep it
  } else {
    env->deletefile(fname);
  }
  return s;
}

}  // namespace rocksdb
