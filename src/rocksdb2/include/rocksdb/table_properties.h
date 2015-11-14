// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
#pragma once

#include <string>
#include <map>
#include "rocksdb/status.h"

namespace rocksdb {

// -- table properties
// other than basic table properties, each table may also have the user
// collected properties.
// the value of the user-collected properties are encoded as raw bytes --
// users have to interprete these values by themselves.
// note: to do prefix seek/scan in `usercollectedproperties`, you can do
// something similar to:
//
// usercollectedproperties props = ...;
// for (auto pos = props.lower_bound(prefix);
//      pos != props.end() && pos->first.compare(0, prefix.size(), prefix) == 0;
//      ++pos) {
//   ...
// }
typedef std::map<const std::string, std::string> usercollectedproperties;

// tableproperties contains a bunch of read-only properties of its associated
// table.
struct tableproperties {
 public:
  // the total size of all data blocks.
  uint64_t data_size = 0;
  // the size of index block.
  uint64_t index_size = 0;
  // the size of filter block.
  uint64_t filter_size = 0;
  // total raw key size
  uint64_t raw_key_size = 0;
  // total raw value size
  uint64_t raw_value_size = 0;
  // the number of blocks in this table
  uint64_t num_data_blocks = 0;
  // the number of entries in this table
  uint64_t num_entries = 0;
  // format version, reserved for backward compatibility
  uint64_t format_version = 0;
  // if 0, key is variable length. otherwise number of bytes for each key.
  uint64_t fixed_key_len = 0;

  // the name of the filter policy used in this table.
  // if no filter policy is used, `filter_policy_name` will be an empty string.
  std::string filter_policy_name;

  // user collected properties
  usercollectedproperties user_collected_properties;

  // convert this object to a human readable form
  //   @prop_delim: delimiter for each property.
  std::string tostring(const std::string& prop_delim = "; ",
                       const std::string& kv_delim = "=") const;
};

// table properties' human-readable names in the property block.
struct tablepropertiesnames {
  static const std::string kdatasize;
  static const std::string kindexsize;
  static const std::string kfiltersize;
  static const std::string krawkeysize;
  static const std::string krawvaluesize;
  static const std::string knumdatablocks;
  static const std::string knumentries;
  static const std::string kformatversion;
  static const std::string kfixedkeylen;
  static const std::string kfilterpolicy;
};

extern const std::string kpropertiesblock;

// `tablepropertiescollector` provides the mechanism for users to collect
// their own interested properties. this class is essentially a collection
// of callback functions that will be invoked during table building.
// it is construced with tablepropertiescollectorfactory. the methods don't
// need to be thread-safe, as we will create exactly one
// tablepropertiescollector object per table and then call it sequentially
class tablepropertiescollector {
 public:
  virtual ~tablepropertiescollector() {}

  // add() will be called when a new key/value pair is inserted into the table.
  // @params key    the original key that is inserted into the table.
  // @params value  the original value that is inserted into the table.
  virtual status add(const slice& key, const slice& value) = 0;

  // finish() will be called when a table has already been built and is ready
  // for writing the properties block.
  // @params properties  user will add their collected statistics to
  // `properties`.
  virtual status finish(usercollectedproperties* properties) = 0;

  // return the human-readable properties, where the key is property name and
  // the value is the human-readable form of value.
  virtual usercollectedproperties getreadableproperties() const = 0;

  // the name of the properties collector can be used for debugging purpose.
  virtual const char* name() const = 0;
};

// constructs tablepropertiescollector. internals create a new
// tablepropertiescollector for each new table
class tablepropertiescollectorfactory {
 public:
  virtual ~tablepropertiescollectorfactory() {}
  // has to be thread-safe
  virtual tablepropertiescollector* createtablepropertiescollector() = 0;

  // the name of the properties collector can be used for debugging purpose.
  virtual const char* name() const = 0;
};

// extra properties
// below is a list of non-basic properties that are collected by database
// itself. especially some properties regarding to the internal keys (which
// is unknown to `table`).
extern uint64_t getdeletedkeys(const usercollectedproperties& props);

}  // namespace rocksdb
