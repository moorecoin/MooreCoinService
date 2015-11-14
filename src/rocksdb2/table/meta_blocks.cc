//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
#include "table/meta_blocks.h"

#include <map>
#include <string>

#include "rocksdb/table.h"
#include "rocksdb/table_properties.h"
#include "table/block.h"
#include "table/format.h"
#include "util/coding.h"

namespace rocksdb {

metaindexbuilder::metaindexbuilder()
    : meta_index_block_(new blockbuilder(1 /* restart interval */)) {}

void metaindexbuilder::add(const std::string& key,
                           const blockhandle& handle) {
  std::string handle_encoding;
  handle.encodeto(&handle_encoding);
  meta_block_handles_.insert({key, handle_encoding});
}

slice metaindexbuilder::finish() {
  for (const auto& metablock : meta_block_handles_) {
    meta_index_block_->add(metablock.first, metablock.second);
  }
  return meta_index_block_->finish();
}

propertyblockbuilder::propertyblockbuilder()
    : properties_block_(new blockbuilder(1 /* restart interval */)) {}

void propertyblockbuilder::add(const std::string& name,
                               const std::string& val) {
  props_.insert({name, val});
}

void propertyblockbuilder::add(const std::string& name, uint64_t val) {
  assert(props_.find(name) == props_.end());

  std::string dst;
  putvarint64(&dst, val);

  add(name, dst);
}

void propertyblockbuilder::add(
    const usercollectedproperties& user_collected_properties) {
  for (const auto& prop : user_collected_properties) {
    add(prop.first, prop.second);
  }
}

void propertyblockbuilder::addtableproperty(const tableproperties& props) {
  add(tablepropertiesnames::krawkeysize, props.raw_key_size);
  add(tablepropertiesnames::krawvaluesize, props.raw_value_size);
  add(tablepropertiesnames::kdatasize, props.data_size);
  add(tablepropertiesnames::kindexsize, props.index_size);
  add(tablepropertiesnames::knumentries, props.num_entries);
  add(tablepropertiesnames::knumdatablocks, props.num_data_blocks);
  add(tablepropertiesnames::kfiltersize, props.filter_size);
  add(tablepropertiesnames::kformatversion, props.format_version);
  add(tablepropertiesnames::kfixedkeylen, props.fixed_key_len);

  if (!props.filter_policy_name.empty()) {
    add(tablepropertiesnames::kfilterpolicy,
        props.filter_policy_name);
  }
}

slice propertyblockbuilder::finish() {
  for (const auto& prop : props_) {
    properties_block_->add(prop.first, prop.second);
  }

  return properties_block_->finish();
}

void logpropertiescollectionerror(
    logger* info_log, const std::string& method, const std::string& name) {
  assert(method == "add" || method == "finish");

  std::string msg =
    "[warning] encountered error when calling tablepropertiescollector::" +
    method + "() with collector name: " + name;
  log(info_log, "%s", msg.c_str());
}

bool notifycollecttablecollectorsonadd(
    const slice& key, const slice& value,
    const std::vector<std::unique_ptr<tablepropertiescollector>>& collectors,
    logger* info_log) {
  bool all_succeeded = true;
  for (auto& collector : collectors) {
    status s = collector->add(key, value);
    all_succeeded = all_succeeded && s.ok();
    if (!s.ok()) {
      logpropertiescollectionerror(info_log, "add" /* method */,
                                   collector->name());
    }
  }
  return all_succeeded;
}

bool notifycollecttablecollectorsonfinish(
    const std::vector<std::unique_ptr<tablepropertiescollector>>& collectors,
    logger* info_log, propertyblockbuilder* builder) {
  bool all_succeeded = true;
  for (auto& collector : collectors) {
    usercollectedproperties user_collected_properties;
    status s = collector->finish(&user_collected_properties);

    all_succeeded = all_succeeded && s.ok();
    if (!s.ok()) {
      logpropertiescollectionerror(info_log, "finish" /* method */,
                                   collector->name());
    } else {
      builder->add(user_collected_properties);
    }
  }

  return all_succeeded;
}

status readproperties(const slice &handle_value, randomaccessfile *file,
                      const footer &footer, env *env, logger *logger,
                      tableproperties **table_properties) {
  assert(table_properties);

  slice v = handle_value;
  blockhandle handle;
  if (!handle.decodefrom(&v).ok()) {
    return status::invalidargument("failed to decode properties block handle");
  }

  blockcontents block_contents;
  readoptions read_options;
  read_options.verify_checksums = false;
  status s = readblockcontents(file, footer, read_options, handle,
                               &block_contents, env, false);

  if (!s.ok()) {
    return s;
  }

  block properties_block(block_contents);
  std::unique_ptr<iterator> iter(
      properties_block.newiterator(bytewisecomparator()));

  auto new_table_properties = new tableproperties();
  // all pre-defined properties of type uint64_t
  std::unordered_map<std::string, uint64_t*> predefined_uint64_properties = {
      {tablepropertiesnames::kdatasize, &new_table_properties->data_size},
      {tablepropertiesnames::kindexsize, &new_table_properties->index_size},
      {tablepropertiesnames::kfiltersize, &new_table_properties->filter_size},
      {tablepropertiesnames::krawkeysize, &new_table_properties->raw_key_size},
      {tablepropertiesnames::krawvaluesize,
       &new_table_properties->raw_value_size},
      {tablepropertiesnames::knumdatablocks,
       &new_table_properties->num_data_blocks},
      {tablepropertiesnames::knumentries, &new_table_properties->num_entries},
      {tablepropertiesnames::kformatversion,
       &new_table_properties->format_version},
      {tablepropertiesnames::kfixedkeylen,
       &new_table_properties->fixed_key_len}, };

  std::string last_key;
  for (iter->seektofirst(); iter->valid(); iter->next()) {
    s = iter->status();
    if (!s.ok()) {
      break;
    }

    auto key = iter->key().tostring();
    // properties block is strictly sorted with no duplicate key.
    assert(last_key.empty() ||
           bytewisecomparator()->compare(key, last_key) > 0);
    last_key = key;

    auto raw_val = iter->value();
    auto pos = predefined_uint64_properties.find(key);

    if (pos != predefined_uint64_properties.end()) {
      // handle predefined rocksdb properties
      uint64_t val;
      if (!getvarint64(&raw_val, &val)) {
        // skip malformed value
        auto error_msg =
          "[warning] detect malformed value in properties meta-block:"
          "\tkey: " + key + "\tval: " + raw_val.tostring();
        log(logger, "%s", error_msg.c_str());
        continue;
      }
      *(pos->second) = val;
    } else if (key == tablepropertiesnames::kfilterpolicy) {
      new_table_properties->filter_policy_name = raw_val.tostring();
    } else {
      // handle user-collected properties
      new_table_properties->user_collected_properties.insert(
          {key, raw_val.tostring()});
    }
  }
  if (s.ok()) {
    *table_properties = new_table_properties;
  } else {
    delete new_table_properties;
  }

  return s;
}

status readtableproperties(randomaccessfile* file, uint64_t file_size,
                           uint64_t table_magic_number, env* env,
                           logger* info_log, tableproperties** properties) {
  // -- read metaindex block
  footer footer(table_magic_number);
  auto s = readfooterfromfile(file, file_size, &footer);
  if (!s.ok()) {
    return s;
  }

  auto metaindex_handle = footer.metaindex_handle();
  blockcontents metaindex_contents;
  readoptions read_options;
  read_options.verify_checksums = false;
  s = readblockcontents(file, footer, read_options, metaindex_handle,
                        &metaindex_contents, env, false);
  if (!s.ok()) {
    return s;
  }
  block metaindex_block(metaindex_contents);
  std::unique_ptr<iterator> meta_iter(
      metaindex_block.newiterator(bytewisecomparator()));

  // -- read property block
  bool found_properties_block = true;
  s = seektopropertiesblock(meta_iter.get(), &found_properties_block);
  if (!s.ok()) {
    return s;
  }

  tableproperties table_properties;
  if (found_properties_block == true) {
    s = readproperties(meta_iter->value(), file, footer, env, info_log,
                       properties);
  } else {
    s = status::notfound();
  }

  return s;
}

status findmetablock(iterator* meta_index_iter,
                     const std::string& meta_block_name,
                     blockhandle* block_handle) {
  meta_index_iter->seek(meta_block_name);
  if (meta_index_iter->status().ok() && meta_index_iter->valid() &&
      meta_index_iter->key() == meta_block_name) {
    slice v = meta_index_iter->value();
    return block_handle->decodefrom(&v);
  } else {
    return status::corruption("cannot find the meta block", meta_block_name);
  }
}

status findmetablock(randomaccessfile* file, uint64_t file_size,
                     uint64_t table_magic_number, env* env,
                     const std::string& meta_block_name,
                     blockhandle* block_handle) {
  footer footer(table_magic_number);
  auto s = readfooterfromfile(file, file_size, &footer);
  if (!s.ok()) {
    return s;
  }

  auto metaindex_handle = footer.metaindex_handle();
  blockcontents metaindex_contents;
  readoptions read_options;
  read_options.verify_checksums = false;
  s = readblockcontents(file, footer, read_options, metaindex_handle,
                        &metaindex_contents, env, false);
  if (!s.ok()) {
    return s;
  }
  block metaindex_block(metaindex_contents);

  std::unique_ptr<iterator> meta_iter;
  meta_iter.reset(metaindex_block.newiterator(bytewisecomparator()));

  return findmetablock(meta_iter.get(), meta_block_name, block_handle);
}

status readmetablock(randomaccessfile* file, uint64_t file_size,
                     uint64_t table_magic_number, env* env,
                     const std::string& meta_block_name,
                     blockcontents* contents) {
  footer footer(table_magic_number);
  auto s = readfooterfromfile(file, file_size, &footer);
  if (!s.ok()) {
    return s;
  }

  // reading metaindex block
  auto metaindex_handle = footer.metaindex_handle();
  blockcontents metaindex_contents;
  readoptions read_options;
  read_options.verify_checksums = false;
  s = readblockcontents(file, footer, read_options, metaindex_handle,
                        &metaindex_contents, env, false);
  if (!s.ok()) {
    return s;
  }

  // finding metablock
  block metaindex_block(metaindex_contents);

  std::unique_ptr<iterator> meta_iter;
  meta_iter.reset(metaindex_block.newiterator(bytewisecomparator()));

  blockhandle block_handle;
  s = findmetablock(meta_iter.get(), meta_block_name, &block_handle);

  if (!s.ok()) {
    return s;
  }

  // reading metablock
  s = readblockcontents(file, footer, read_options, block_handle, contents, env,
                        false);

  return s;
}

}  // namespace rocksdb
