//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.

#pragma once
#ifndef rocksdb_lite

#include <string>
#include <vector>

#include "rocksdb/utilities/stackable_db.h"
#include "rocksdb/utilities/json_document.h"
#include "rocksdb/db.h"

namespace rocksdb {

// important: documentdb is a work in progress. it is unstable and we might
// change the api without warning. talk to rocksdb team before using this in
// production ;)

// documentdb is a layer on top of rocksdb that provides a very simple json api.
// when creating a db, you specify a list of indexes you want to keep on your
// data. you can insert a json document to the db, which is automatically
// indexed. every document added to the db needs to have "_id" field which is
// automatically indexed and is an unique primary key. all other indexes are
// non-unique.

// note: field names in the json are not allowed to start with '$' or
// contain '.'. we don't currently enforce that rule, but will start behaving
// badly.

// cursor is what you get as a result of executing query. to get all
// results from a query, call next() on a cursor while  valid() returns true
class cursor {
 public:
  cursor() = default;
  virtual ~cursor() {}

  virtual bool valid() const = 0;
  virtual void next() = 0;
  // lifecycle of the returned jsondocument is until the next next() call
  virtual const jsondocument& document() const = 0;
  virtual status status() const = 0;

 private:
  // no copying allowed
  cursor(const cursor&);
  void operator=(const cursor&);
};

struct documentdboptions {
  int background_threads = 4;
  uint64_t memtable_size = 128 * 1024 * 1024;    // 128 mb
  uint64_t cache_size = 1 * 1024 * 1024 * 1024;  // 1 gb
};

// todo(icanadi) add `jsondocument* info` parameter to all calls that can be
// used by the caller to get more information about the call execution (number
// of dropped records, number of updated records, etc.)
class documentdb : public stackabledb {
 public:
  struct indexdescriptor {
    // currently, you can only define an index on a single field. to specify an
    // index on a field x, set index description to json "{x: 1}"
    // currently the value needs to be 1, which means ascending.
    // in the future, we plan to also support indexes on multiple keys, where
    // you could mix ascending sorting (1) with descending sorting indexes (-1)
    jsondocument* description;
    std::string name;
  };

  // open documentdb with specified indexes. the list of indexes has to be
  // complete, i.e. include all indexes present in the db, except the primary
  // key index.
  // otherwise, open() will return an error
  static status open(const documentdboptions& options, const std::string& name,
                     const std::vector<indexdescriptor>& indexes,
                     documentdb** db, bool read_only = false);

  explicit documentdb(db* db) : stackabledb(db) {}

  // create a new index. it will stop all writes for the duration of the call.
  // all current documents in the db are scanned and corresponding index entries
  // are created
  virtual status createindex(const writeoptions& write_options,
                             const indexdescriptor& index) = 0;

  // drop an index. client is responsible to make sure that index is not being
  // used by currently executing queries
  virtual status dropindex(const std::string& name) = 0;

  // insert a document to the db. the document needs to have a primary key "_id"
  // which can either be a string or an integer. otherwise the write will fail
  // with invalidargument.
  virtual status insert(const writeoptions& options,
                        const jsondocument& document) = 0;

  // deletes all documents matching a filter atomically
  virtual status remove(const readoptions& read_options,
                        const writeoptions& write_options,
                        const jsondocument& query) = 0;

  // does this sequence of operations:
  // 1. find all documents matching a filter
  // 2. for all documents, atomically:
  // 2.1. apply the update operators
  // 2.2. update the secondary indexes
  //
  // currently only $set update operator is supported.
  // syntax is: {$set: {key1: value1, key2: value2, etc...}}
  // this operator will change a document's key1 field to value1, key2 to
  // value2, etc. new values will be set even if a document didn't have an entry
  // for the specified key.
  //
  // you can not change a primary key of a document.
  //
  // update example: update({id: {$gt: 5}, $index: id}, {$set: {enabled: true}})
  virtual status update(const readoptions& read_options,
                        const writeoptions& write_options,
                        const jsondocument& filter,
                        const jsondocument& updates) = 0;

  // query has to be an array in which every element is an operator. currently
  // only $filter operator is supported. syntax of $filter operator is:
  // {$filter: {key1: condition1, key2: condition2, etc.}} where conditions can
  // be either:
  // 1) a single value in which case the condition is equality condition, or
  // 2) a defined operators, like {$gt: 4}, which will match all documents that
  // have key greater than 4.
  //
  // supported operators are:
  // 1) $gt -- greater than
  // 2) $gte -- greater than or equal
  // 3) $lt -- less than
  // 4) $lte -- less than or equal
  // if you want the filter to use an index, you need to specify it like this:
  // {$filter: {...(conditions)..., $index: index_name}}
  //
  // example query:
  // * [{$filter: {name: john, age: {$gte: 18}, $index: age}}]
  // will return all johns whose age is greater or equal to 18 and it will use
  // index "age" to satisfy the query.
  virtual cursor* query(const readoptions& read_options,
                        const jsondocument& query) = 0;
};

}  // namespace rocksdb
#endif  // rocksdb_lite
