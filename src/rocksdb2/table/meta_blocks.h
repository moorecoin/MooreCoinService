//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
#pragma once

#include <map>
#include <memory>
#include <vector>
#include <string>

#include "db/builder.h"
#include "rocksdb/comparator.h"
#include "rocksdb/options.h"
#include "rocksdb/slice.h"
#include "rocksdb/table_properties.h"
#include "table/block_builder.h"
#include "table/format.h"

namespace rocksdb {

class blockbuilder;
class blockhandle;
class env;
class footer;
class logger;
class randomaccessfile;
struct tableproperties;

// an stl style comparator that does the bytewise comparator comparasion
// internally.
struct bytewiselessthan {
  bool operator()(const std::string& key1, const std::string& key2) const {
    // smaller entries will be placed in front.
    return comparator->compare(key1, key2) <= 0;
  }

  const comparator* comparator = bytewisecomparator();
};

// when writing to a block that requires entries to be sorted by
// `bytewisecomparator`, we can buffer the content to `bytewisesortedmap`
// before writng to store.
typedef std::map<std::string, std::string, bytewiselessthan> bytewisesortedmap;

class metaindexbuilder {
 public:
  metaindexbuilder(const metaindexbuilder&) = delete;
  metaindexbuilder& operator=(const metaindexbuilder&) = delete;

  metaindexbuilder();
  void add(const std::string& key, const blockhandle& handle);

  // write all the added key/value pairs to the block and return the contents
  // of the block.
  slice finish();

 private:
  // store the sorted key/handle of the metablocks.
  bytewisesortedmap meta_block_handles_;
  std::unique_ptr<blockbuilder> meta_index_block_;
};

class propertyblockbuilder {
 public:
  propertyblockbuilder(const propertyblockbuilder&) = delete;
  propertyblockbuilder& operator=(const propertyblockbuilder&) = delete;

  propertyblockbuilder();

  void addtableproperty(const tableproperties& props);
  void add(const std::string& key, uint64_t value);
  void add(const std::string& key, const std::string& value);
  void add(const usercollectedproperties& user_collected_properties);

  // write all the added entries to the block and return the block contents
  slice finish();

 private:
  std::unique_ptr<blockbuilder> properties_block_;
  bytewisesortedmap props_;
};

// were we encounter any error occurs during user-defined statistics collection,
// we'll write the warning message to info log.
void logpropertiescollectionerror(
    logger* info_log, const std::string& method, const std::string& name);

// utility functions help table builder to trigger batch events for user
// defined property collectors.
// return value indicates if there is any error occurred; if error occurred,
// the warning message will be logged.
// notifycollecttablecollectorsonadd() triggers the `add` event for all
// property collectors.
bool notifycollecttablecollectorsonadd(
    const slice& key, const slice& value,
    const std::vector<std::unique_ptr<tablepropertiescollector>>& collectors,
    logger* info_log);

// notifycollecttablecollectorsonadd() triggers the `finish` event for all
// property collectors. the collected properties will be added to `builder`.
bool notifycollecttablecollectorsonfinish(
    const std::vector<std::unique_ptr<tablepropertiescollector>>& collectors,
    logger* info_log, propertyblockbuilder* builder);

// read the properties from the table.
// @returns a status to indicate if the operation succeeded. on success,
//          *table_properties will point to a heap-allocated tableproperties
//          object, otherwise value of `table_properties` will not be modified.
status readproperties(const slice &handle_value, randomaccessfile *file,
                      const footer &footer, env *env, logger *logger,
                      tableproperties **table_properties);

// directly read the properties from the properties block of a plain table.
// @returns a status to indicate if the operation succeeded. on success,
//          *table_properties will point to a heap-allocated tableproperties
//          object, otherwise value of `table_properties` will not be modified.
status readtableproperties(randomaccessfile* file, uint64_t file_size,
                           uint64_t table_magic_number, env* env,
                           logger* info_log, tableproperties** properties);

// seek to the properties block.
// if it successfully seeks to the properties block, "is_found" will be
// set to true.
extern status seektopropertiesblock(iterator* meta_iter, bool* is_found);

// find the meta block from the meta index block.
status findmetablock(iterator* meta_index_iter,
                     const std::string& meta_block_name,
                     blockhandle* block_handle);

// find the meta block
status findmetablock(randomaccessfile* file, uint64_t file_size,
                     uint64_t table_magic_number, env* env,
                     const std::string& meta_block_name,
                     blockhandle* block_handle);

// read the specified meta block with name meta_block_name
// from `file` and initialize `contents` with contents of this block.
// return status::ok in case of success.
status readmetablock(randomaccessfile* file, uint64_t file_size,
                     uint64_t table_magic_number, env* env,
                     const std::string& meta_block_name,
                     blockcontents* contents);

}  // namespace rocksdb
