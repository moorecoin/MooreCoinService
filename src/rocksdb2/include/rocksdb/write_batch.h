// copyright (c) 2013, facebook, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
//
// writebatch holds a collection of updates to apply atomically to a db.
//
// the updates are applied in the order in which they are added
// to the writebatch.  for example, the value of "key" will be "v3"
// after the following batch is written:
//
//    batch.put("key", "v1");
//    batch.delete("key");
//    batch.put("key", "v2");
//    batch.put("key", "v3");
//
// multiple threads can invoke const methods on a writebatch without
// external synchronization, but if any of the threads may call a
// non-const method, all threads accessing the same writebatch must use
// external synchronization.

#ifndef storage_rocksdb_include_write_batch_h_
#define storage_rocksdb_include_write_batch_h_

#include <string>
#include "rocksdb/status.h"

namespace rocksdb {

class slice;
class columnfamilyhandle;
struct sliceparts;

class writebatch {
 public:
  explicit writebatch(size_t reserved_bytes = 0);
  ~writebatch();

  // store the mapping "key->value" in the database.
  void put(columnfamilyhandle* column_family, const slice& key,
           const slice& value);
  void put(const slice& key, const slice& value) {
    put(nullptr, key, value);
  }

  // variant of put() that gathers output like writev(2).  the key and value
  // that will be written to the database are concatentations of arrays of
  // slices.
  void put(columnfamilyhandle* column_family, const sliceparts& key,
           const sliceparts& value);
  void put(const sliceparts& key, const sliceparts& value) {
    put(nullptr, key, value);
  }

  // merge "value" with the existing value of "key" in the database.
  // "key->merge(existing, value)"
  void merge(columnfamilyhandle* column_family, const slice& key,
             const slice& value);
  void merge(const slice& key, const slice& value) {
    merge(nullptr, key, value);
  }

  // if the database contains a mapping for "key", erase it.  else do nothing.
  void delete(columnfamilyhandle* column_family, const slice& key);
  void delete(const slice& key) { delete(nullptr, key); }

  // variant that takes sliceparts
  void delete(columnfamilyhandle* column_family, const sliceparts& key);
  void delete(const sliceparts& key) { delete(nullptr, key); }

  // append a blob of arbitrary size to the records in this batch. the blob will
  // be stored in the transaction log but not in any other file. in particular,
  // it will not be persisted to the sst files. when iterating over this
  // writebatch, writebatch::handler::logdata will be called with the contents
  // of the blob as it is encountered. blobs, puts, deletes, and merges will be
  // encountered in the same order in thich they were inserted. the blob will
  // not consume sequence number(s) and will not increase the count of the batch
  //
  // example application: add timestamps to the transaction log for use in
  // replication.
  void putlogdata(const slice& blob);

  // clear all updates buffered in this batch.
  void clear();

  // support for iterating over the contents of a batch.
  class handler {
   public:
    virtual ~handler();
    // default implementation will just call put without column family for
    // backwards compatibility. if the column family is not default,
    // the function is noop
    virtual status putcf(uint32_t column_family_id, const slice& key,
                         const slice& value) {
      if (column_family_id == 0) {
        // put() historically doesn't return status. we didn't want to be
        // backwards incompatible so we didn't change the return status
        // (this is a public api). we do an ordinary get and return status::ok()
        put(key, value);
        return status::ok();
      }
      return status::invalidargument(
          "non-default column family and putcf not implemented");
    }
    virtual void put(const slice& key, const slice& value);
    // merge and logdata are not pure virtual. otherwise, we would break
    // existing clients of handler on a source code level. the default
    // implementation of merge simply throws a runtime exception.
    virtual status mergecf(uint32_t column_family_id, const slice& key,
                           const slice& value) {
      if (column_family_id == 0) {
        merge(key, value);
        return status::ok();
      }
      return status::invalidargument(
          "non-default column family and mergecf not implemented");
    }
    virtual void merge(const slice& key, const slice& value);
    // the default implementation of logdata does nothing.
    virtual void logdata(const slice& blob);
    virtual status deletecf(uint32_t column_family_id, const slice& key) {
      if (column_family_id == 0) {
        delete(key);
        return status::ok();
      }
      return status::invalidargument(
          "non-default column family and deletecf not implemented");
    }
    virtual void delete(const slice& key);
    // continue is called by writebatch::iterate. if it returns false,
    // iteration is halted. otherwise, it continues iterating. the default
    // implementation always returns true.
    virtual bool continue();
  };
  status iterate(handler* handler) const;

  // retrieve the serialized version of this batch.
  const std::string& data() const { return rep_; }

  // retrieve data size of the batch.
  size_t getdatasize() const { return rep_.size(); }

  // returns the number of updates in the batch
  int count() const;

  // constructor with a serialized string object
  explicit writebatch(std::string rep): rep_(rep) {}

 private:
  friend class writebatchinternal;

 protected:
  std::string rep_;  // see comment in write_batch.cc for the format of rep_

  // intentionally copyable
};

}  // namespace rocksdb

#endif  // storage_rocksdb_include_write_batch_h_
