// copyright (c) 2013, facebook, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
//
// a writebatchwithindex with a binary searchable index built for all the keys
// inserted.

#pragma once

#include "rocksdb/status.h"
#include "rocksdb/slice.h"
#include "rocksdb/write_batch.h"

namespace rocksdb {

class columnfamilyhandle;
struct sliceparts;
class comparator;

enum writetype { kputrecord, kmergerecord, kdeleterecord, klogdatarecord };

// an entry for put, merge or delete entry for write batches. used in
// wbwiiterator.
struct writeentry {
  writetype type;
  slice key;
  slice value;
};

// iterator of one column family out of a writebatchwithindex.
class wbwiiterator {
 public:
  virtual ~wbwiiterator() {}

  virtual bool valid() const = 0;

  virtual void seek(const slice& key) = 0;

  virtual void next() = 0;

  virtual const writeentry& entry() const = 0;

  virtual status status() const = 0;
};

// a writebatchwithindex with a binary searchable index built for all the keys
// inserted.
// in put(), merge() or delete(), the same function of the wrapped will be
// called. at the same time, indexes will be built.
// by calling getwritebatch(), a user will get the writebatch for the data
// they inserted, which can be used for db::write().
// a user can call newiterator() to create an iterator.
class writebatchwithindex {
 public:
  // index_comparator indicates the order when iterating data in the write
  // batch. technically, it doesn't have to be the same as the one used in
  // the db.
  // reserved_bytes: reserved bytes in underlying writebatch
  explicit writebatchwithindex(const comparator* index_comparator,
                               size_t reserved_bytes = 0);
  virtual ~writebatchwithindex();

  writebatch* getwritebatch();

  virtual void put(columnfamilyhandle* column_family, const slice& key,
                   const slice& value);

  virtual void put(const slice& key, const slice& value);

  virtual void merge(columnfamilyhandle* column_family, const slice& key,
                     const slice& value);

  virtual void merge(const slice& key, const slice& value);

  virtual void putlogdata(const slice& blob);

  virtual void delete(columnfamilyhandle* column_family, const slice& key);
  virtual void delete(const slice& key);

  virtual void delete(columnfamilyhandle* column_family, const sliceparts& key);

  virtual void delete(const sliceparts& key);

  // create an iterator of a column family. user can call iterator.seek() to
  // search to the next entry of or after a key. keys will be iterated in the
  // order given by index_comparator. for multiple updates on the same key,
  // each update will be returned as a separate entry, in the order of update
  // time.
  virtual wbwiiterator* newiterator(columnfamilyhandle* column_family);
  // create an iterator of the default column family.
  virtual wbwiiterator* newiterator();

 private:
  struct rep;
  rep* rep;
};

}  // namespace rocksdb
