//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.

#include "rocksdb/table_properties.h"
#include "rocksdb/iterator.h"
#include "rocksdb/env.h"

namespace rocksdb {

namespace {
  void appendproperty(
      std::string& props,
      const std::string& key,
      const std::string& value,
      const std::string& prop_delim,
      const std::string& kv_delim) {
    props.append(key);
    props.append(kv_delim);
    props.append(value);
    props.append(prop_delim);
  }

  template <class tvalue>
  void appendproperty(
      std::string& props,
      const std::string& key,
      const tvalue& value,
      const std::string& prop_delim,
      const std::string& kv_delim) {
    appendproperty(
        props, key, std::to_string(value), prop_delim, kv_delim
    );
  }
}

std::string tableproperties::tostring(
    const std::string& prop_delim,
    const std::string& kv_delim) const {
  std::string result;
  result.reserve(1024);

  // basic info
  appendproperty(result, "# data blocks", num_data_blocks, prop_delim,
                 kv_delim);
  appendproperty(result, "# entries", num_entries, prop_delim, kv_delim);

  appendproperty(result, "raw key size", raw_key_size, prop_delim, kv_delim);
  appendproperty(result, "raw average key size",
                 num_entries != 0 ? 1.0 * raw_key_size / num_entries : 0.0,
                 prop_delim, kv_delim);
  appendproperty(result, "raw value size", raw_value_size, prop_delim,
                 kv_delim);
  appendproperty(result, "raw average value size",
                 num_entries != 0 ? 1.0 * raw_value_size / num_entries : 0.0,
                 prop_delim, kv_delim);

  appendproperty(result, "data block size", data_size, prop_delim, kv_delim);
  appendproperty(result, "index block size", index_size, prop_delim, kv_delim);
  appendproperty(result, "filter block size", filter_size, prop_delim,
                 kv_delim);
  appendproperty(result, "(estimated) table size",
                 data_size + index_size + filter_size, prop_delim, kv_delim);

  appendproperty(
      result, "filter policy name",
      filter_policy_name.empty() ? std::string("n/a") : filter_policy_name,
      prop_delim, kv_delim);

  return result;
}

const std::string tablepropertiesnames::kdatasize  =
    "rocksdb.data.size";
const std::string tablepropertiesnames::kindexsize =
    "rocksdb.index.size";
const std::string tablepropertiesnames::kfiltersize =
    "rocksdb.filter.size";
const std::string tablepropertiesnames::krawkeysize =
    "rocksdb.raw.key.size";
const std::string tablepropertiesnames::krawvaluesize =
    "rocksdb.raw.value.size";
const std::string tablepropertiesnames::knumdatablocks =
    "rocksdb.num.data.blocks";
const std::string tablepropertiesnames::knumentries =
    "rocksdb.num.entries";
const std::string tablepropertiesnames::kfilterpolicy =
    "rocksdb.filter.policy";
const std::string tablepropertiesnames::kformatversion =
    "rocksdb.format.version";
const std::string tablepropertiesnames::kfixedkeylen =
    "rocksdb.fixed.key.length";

extern const std::string kpropertiesblock = "rocksdb.properties";
// old property block name for backward compatibility
extern const std::string kpropertiesblockoldname = "rocksdb.stats";

// seek to the properties block.
// return true if it successfully seeks to the properties block.
status seektopropertiesblock(iterator* meta_iter, bool* is_found) {
  *is_found = true;
  meta_iter->seek(kpropertiesblock);
  if (meta_iter->status().ok() &&
      (!meta_iter->valid() || meta_iter->key() != kpropertiesblock)) {
    meta_iter->seek(kpropertiesblockoldname);
    if (meta_iter->status().ok() &&
        (!meta_iter->valid() || meta_iter->key() != kpropertiesblockoldname)) {
      *is_found = false;
    }
  }
  return meta_iter->status();
}

}  // namespace rocksdb
