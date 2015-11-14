//  copyright (c) 2014, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.

#ifndef rocksdb_lite
#include "table/cuckoo_table_builder.h"

#include <assert.h>
#include <algorithm>
#include <limits>
#include <string>
#include <vector>

#include "db/dbformat.h"
#include "rocksdb/env.h"
#include "rocksdb/table.h"
#include "table/block_builder.h"
#include "table/cuckoo_table_factory.h"
#include "table/format.h"
#include "table/meta_blocks.h"
#include "util/autovector.h"
#include "util/random.h"

namespace rocksdb {
const std::string cuckootablepropertynames::kemptykey =
      "rocksdb.cuckoo.bucket.empty.key";
const std::string cuckootablepropertynames::knumhashfunc =
      "rocksdb.cuckoo.hash.num";
const std::string cuckootablepropertynames::khashtablesize =
      "rocksdb.cuckoo.hash.size";
const std::string cuckootablepropertynames::kvaluelength =
      "rocksdb.cuckoo.value.length";
const std::string cuckootablepropertynames::kislastlevel =
      "rocksdb.cuckoo.file.islastlevel";
const std::string cuckootablepropertynames::kcuckooblocksize =
      "rocksdb.cuckoo.hash.cuckooblocksize";

// obtained by running echo rocksdb.table.cuckoo | sha1sum
extern const uint64_t kcuckootablemagicnumber = 0x926789d0c5f17873ull;

cuckootablebuilder::cuckootablebuilder(
    writablefile* file, double max_hash_table_ratio,
    uint32_t max_num_hash_table, uint32_t max_search_depth,
    const comparator* user_comparator, uint32_t cuckoo_block_size,
    uint64_t (*get_slice_hash)(const slice&, uint32_t, uint64_t))
    : num_hash_func_(2),
      file_(file),
      max_hash_table_ratio_(max_hash_table_ratio),
      max_num_hash_func_(max_num_hash_table),
      max_search_depth_(max_search_depth),
      cuckoo_block_size_(std::max(1u, cuckoo_block_size)),
      hash_table_size_(2),
      is_last_level_file_(false),
      has_seen_first_key_(false),
      ucomp_(user_comparator),
      get_slice_hash_(get_slice_hash),
      closed_(false) {
  properties_.num_entries = 0;
  // data is in a huge block.
  properties_.num_data_blocks = 1;
  properties_.index_size = 0;
  properties_.filter_size = 0;
}

void cuckootablebuilder::add(const slice& key, const slice& value) {
  if (properties_.num_entries >= kmaxvectoridx - 1) {
    status_ = status::notsupported("number of keys in a file must be < 2^32-1");
    return;
  }
  parsedinternalkey ikey;
  if (!parseinternalkey(key, &ikey)) {
    status_ = status::corruption("unable to parse key into inernal key.");
    return;
  }
  // determine if we can ignore the sequence number and value type from
  // internal keys by looking at sequence number from first key. we assume
  // that if first key has a zero sequence number, then all the remaining
  // keys will have zero seq. no.
  if (!has_seen_first_key_) {
    is_last_level_file_ = ikey.sequence == 0;
    has_seen_first_key_ = true;
    smallest_user_key_.assign(ikey.user_key.data(), ikey.user_key.size());
    largest_user_key_.assign(ikey.user_key.data(), ikey.user_key.size());
  }
  // even if one sequence number is non-zero, then it is not last level.
  assert(!is_last_level_file_ || ikey.sequence == 0);
  if (is_last_level_file_) {
    kvs_.emplace_back(std::make_pair(
          ikey.user_key.tostring(), value.tostring()));
  } else {
    kvs_.emplace_back(std::make_pair(key.tostring(), value.tostring()));
  }

  // in order to fill the empty buckets in the hash table, we identify a
  // key which is not used so far (unused_user_key). we determine this by
  // maintaining smallest and largest keys inserted so far in bytewise order
  // and use them to find a key outside this range in finish() operation.
  // note that this strategy is independent of user comparator used here.
  if (ikey.user_key.compare(smallest_user_key_) < 0) {
    smallest_user_key_.assign(ikey.user_key.data(), ikey.user_key.size());
  } else if (ikey.user_key.compare(largest_user_key_) > 0) {
    largest_user_key_.assign(ikey.user_key.data(), ikey.user_key.size());
  }
  if (hash_table_size_ < kvs_.size() / max_hash_table_ratio_) {
    hash_table_size_ *= 2;
  }
}

status cuckootablebuilder::makehashtable(std::vector<cuckoobucket>* buckets) {
  uint64_t hash_table_size_minus_one = hash_table_size_ - 1;
  buckets->resize(hash_table_size_minus_one + cuckoo_block_size_);
  uint64_t make_space_for_key_call_id = 0;
  for (uint32_t vector_idx = 0; vector_idx < kvs_.size(); vector_idx++) {
    uint64_t bucket_id;
    bool bucket_found = false;
    autovector<uint64_t> hash_vals;
    slice user_key = is_last_level_file_ ? kvs_[vector_idx].first :
      extractuserkey(kvs_[vector_idx].first);
    for (uint32_t hash_cnt = 0; hash_cnt < num_hash_func_ && !bucket_found;
        ++hash_cnt) {
      uint64_t hash_val = cuckoohash(user_key, hash_cnt,
          hash_table_size_minus_one, get_slice_hash_);
      // if there is a collision, check next cuckoo_block_size_ locations for
      // empty locations. while checking, if we reach end of the hash table,
      // stop searching and proceed for next hash function.
      for (uint32_t block_idx = 0; block_idx < cuckoo_block_size_;
          ++block_idx, ++hash_val) {
        if ((*buckets)[hash_val].vector_idx == kmaxvectoridx) {
          bucket_id = hash_val;
          bucket_found = true;
          break;
        } else {
          if (ucomp_->compare(user_key, is_last_level_file_
                ? slice(kvs_[(*buckets)[hash_val].vector_idx].first)
                : extractuserkey(
                  kvs_[(*buckets)[hash_val].vector_idx].first)) == 0) {
            return status::notsupported("same key is being inserted again.");
          }
          hash_vals.push_back(hash_val);
        }
      }
    }
    while (!bucket_found && !makespaceforkey(hash_vals,
          ++make_space_for_key_call_id, buckets, &bucket_id)) {
      // rehash by increashing number of hash tables.
      if (num_hash_func_ >= max_num_hash_func_) {
        return status::notsupported("too many collisions. unable to hash.");
      }
      // we don't really need to rehash the entire table because old hashes are
      // still valid and we only increased the number of hash functions.
      uint64_t hash_val = cuckoohash(user_key, num_hash_func_,
          hash_table_size_minus_one, get_slice_hash_);
      ++num_hash_func_;
      for (uint32_t block_idx = 0; block_idx < cuckoo_block_size_;
          ++block_idx, ++hash_val) {
        if ((*buckets)[hash_val].vector_idx == kmaxvectoridx) {
          bucket_found = true;
          bucket_id = hash_val;
          break;
        } else {
          hash_vals.push_back(hash_val);
        }
      }
    }
    (*buckets)[bucket_id].vector_idx = vector_idx;
  }
  return status::ok();
}

status cuckootablebuilder::finish() {
  assert(!closed_);
  closed_ = true;
  std::vector<cuckoobucket> buckets;
  status s;
  std::string unused_bucket;
  if (!kvs_.empty()) {
    s = makehashtable(&buckets);
    if (!s.ok()) {
      return s;
    }
    // determine unused_user_key to fill empty buckets.
    std::string unused_user_key = smallest_user_key_;
    int curr_pos = unused_user_key.size() - 1;
    while (curr_pos >= 0) {
      --unused_user_key[curr_pos];
      if (slice(unused_user_key).compare(smallest_user_key_) < 0) {
        break;
      }
      --curr_pos;
    }
    if (curr_pos < 0) {
      // try using the largest key to identify an unused key.
      unused_user_key = largest_user_key_;
      curr_pos = unused_user_key.size() - 1;
      while (curr_pos >= 0) {
        ++unused_user_key[curr_pos];
        if (slice(unused_user_key).compare(largest_user_key_) > 0) {
          break;
        }
        --curr_pos;
      }
    }
    if (curr_pos < 0) {
      return status::corruption("unable to find unused key");
    }
    if (is_last_level_file_) {
      unused_bucket = unused_user_key;
    } else {
      parsedinternalkey ikey(unused_user_key, 0, ktypevalue);
      appendinternalkey(&unused_bucket, ikey);
    }
  }
  properties_.num_entries = kvs_.size();
  properties_.fixed_key_len = unused_bucket.size();
  uint32_t value_length = kvs_.empty() ? 0 : kvs_[0].second.size();
  uint32_t bucket_size = value_length + properties_.fixed_key_len;
  properties_.user_collected_properties[
        cuckootablepropertynames::kvaluelength].assign(
        reinterpret_cast<const char*>(&value_length), sizeof(value_length));

  unused_bucket.resize(bucket_size, 'a');
  // write the table.
  uint32_t num_added = 0;
  for (auto& bucket : buckets) {
    if (bucket.vector_idx == kmaxvectoridx) {
      s = file_->append(slice(unused_bucket));
    } else {
      ++num_added;
      s = file_->append(kvs_[bucket.vector_idx].first);
      if (s.ok()) {
        s = file_->append(kvs_[bucket.vector_idx].second);
      }
    }
    if (!s.ok()) {
      return s;
    }
  }
  assert(num_added == numentries());
  properties_.raw_key_size = num_added * properties_.fixed_key_len;
  properties_.raw_value_size = num_added * value_length;

  uint64_t offset = buckets.size() * bucket_size;
  properties_.data_size = offset;
  unused_bucket.resize(properties_.fixed_key_len);
  properties_.user_collected_properties[
    cuckootablepropertynames::kemptykey] = unused_bucket;
  properties_.user_collected_properties[
    cuckootablepropertynames::knumhashfunc].assign(
        reinterpret_cast<char*>(&num_hash_func_), sizeof(num_hash_func_));

  uint64_t hash_table_size = buckets.size() - cuckoo_block_size_ + 1;
  properties_.user_collected_properties[
    cuckootablepropertynames::khashtablesize].assign(
        reinterpret_cast<const char*>(&hash_table_size),
        sizeof(hash_table_size));
  properties_.user_collected_properties[
    cuckootablepropertynames::kislastlevel].assign(
        reinterpret_cast<const char*>(&is_last_level_file_),
        sizeof(is_last_level_file_));
  properties_.user_collected_properties[
    cuckootablepropertynames::kcuckooblocksize].assign(
        reinterpret_cast<const char*>(&cuckoo_block_size_),
        sizeof(cuckoo_block_size_));

  // write meta blocks.
  metaindexbuilder meta_index_builder;
  propertyblockbuilder property_block_builder;

  property_block_builder.addtableproperty(properties_);
  property_block_builder.add(properties_.user_collected_properties);
  slice property_block = property_block_builder.finish();
  blockhandle property_block_handle;
  property_block_handle.set_offset(offset);
  property_block_handle.set_size(property_block.size());
  s = file_->append(property_block);
  offset += property_block.size();
  if (!s.ok()) {
    return s;
  }

  meta_index_builder.add(kpropertiesblock, property_block_handle);
  slice meta_index_block = meta_index_builder.finish();

  blockhandle meta_index_block_handle;
  meta_index_block_handle.set_offset(offset);
  meta_index_block_handle.set_size(meta_index_block.size());
  s = file_->append(meta_index_block);
  if (!s.ok()) {
    return s;
  }

  footer footer(kcuckootablemagicnumber);
  footer.set_metaindex_handle(meta_index_block_handle);
  footer.set_index_handle(blockhandle::nullblockhandle());
  std::string footer_encoding;
  footer.encodeto(&footer_encoding);
  s = file_->append(footer_encoding);
  return s;
}

void cuckootablebuilder::abandon() {
  assert(!closed_);
  closed_ = true;
}

uint64_t cuckootablebuilder::numentries() const {
  return kvs_.size();
}

uint64_t cuckootablebuilder::filesize() const {
  if (closed_) {
    return file_->getfilesize();
  } else if (properties_.num_entries == 0) {
    return 0;
  }

  // account for buckets being a power of two.
  // as elements are added, file size remains constant for a while and doubles
  // its size. since compaction algorithm stops adding elements only after it
  // exceeds the file limit, we account for the extra element being added here.
  uint64_t expected_hash_table_size = hash_table_size_;
  if (expected_hash_table_size < (kvs_.size() + 1) / max_hash_table_ratio_) {
    expected_hash_table_size *= 2;
  }
  return (kvs_[0].first.size() + kvs_[0].second.size()) *
    expected_hash_table_size;
}

// this method is invoked when there is no place to insert the target key.
// it searches for a set of elements that can be moved to accommodate target
// key. the search is a bfs graph traversal with first level (hash_vals)
// being all the buckets target key could go to.
// then, from each node (curr_node), we find all the buckets that curr_node
// could go to. they form the children of curr_node in the tree.
// we continue the traversal until we find an empty bucket, in which case, we
// move all elements along the path from first level to this empty bucket, to
// make space for target key which is inserted at first level (*bucket_id).
// if tree depth exceedes max depth, we return false indicating failure.
bool cuckootablebuilder::makespaceforkey(
    const autovector<uint64_t>& hash_vals,
    const uint64_t make_space_for_key_call_id,
    std::vector<cuckoobucket>* buckets,
    uint64_t* bucket_id) {
  struct cuckoonode {
    uint64_t bucket_id;
    uint32_t depth;
    uint32_t parent_pos;
    cuckoonode(uint64_t bucket_id, uint32_t depth, int parent_pos)
      : bucket_id(bucket_id), depth(depth), parent_pos(parent_pos) {}
  };
  // this is bfs search tree that is stored simply as a vector.
  // each node stores the index of parent node in the vector.
  std::vector<cuckoonode> tree;
  // we want to identify already visited buckets in the current method call so
  // that we don't add same buckets again for exploration in the tree.
  // we do this by maintaining a count of current method call in
  // make_space_for_key_call_id, which acts as a unique id for this invocation
  // of the method. we store this number into the nodes that we explore in
  // current method call.
  // it is unlikely for the increment operation to overflow because the maximum
  // no. of times this will be called is <= max_num_hash_func_ + kvs_.size().
  for (uint32_t hash_cnt = 0; hash_cnt < num_hash_func_; ++hash_cnt) {
    uint64_t bucket_id = hash_vals[hash_cnt];
    (*buckets)[bucket_id].make_space_for_key_call_id =
      make_space_for_key_call_id;
    tree.push_back(cuckoonode(bucket_id, 0, 0));
  }
  uint64_t hash_table_size_minus_one = hash_table_size_ - 1;
  bool null_found = false;
  uint32_t curr_pos = 0;
  while (!null_found && curr_pos < tree.size()) {
    cuckoonode& curr_node = tree[curr_pos];
    uint32_t curr_depth = curr_node.depth;
    if (curr_depth >= max_search_depth_) {
      break;
    }
    cuckoobucket& curr_bucket = (*buckets)[curr_node.bucket_id];
    for (uint32_t hash_cnt = 0;
        hash_cnt < num_hash_func_ && !null_found; ++hash_cnt) {
      uint64_t child_bucket_id = cuckoohash(
          (is_last_level_file_ ? kvs_[curr_bucket.vector_idx].first :
           extractuserkey(slice(kvs_[curr_bucket.vector_idx].first))),
          hash_cnt, hash_table_size_minus_one, get_slice_hash_);
      // iterate inside cuckoo block.
      for (uint32_t block_idx = 0; block_idx < cuckoo_block_size_;
          ++block_idx, ++child_bucket_id) {
        if ((*buckets)[child_bucket_id].make_space_for_key_call_id ==
            make_space_for_key_call_id) {
          continue;
        }
        (*buckets)[child_bucket_id].make_space_for_key_call_id =
          make_space_for_key_call_id;
        tree.push_back(cuckoonode(child_bucket_id, curr_depth + 1,
              curr_pos));
        if ((*buckets)[child_bucket_id].vector_idx == kmaxvectoridx) {
          null_found = true;
          break;
        }
      }
    }
    ++curr_pos;
  }

  if (null_found) {
    // there is an empty node in tree.back(). now, traverse the path from this
    // empty node to top of the tree and at every node in the path, replace
    // child with the parent. stop when first level is reached in the tree
    // (happens when 0 <= bucket_to_replace_pos < num_hash_func_) and return
    // this location in first level for target key to be inserted.
    uint32_t bucket_to_replace_pos = tree.size()-1;
    while (bucket_to_replace_pos >= num_hash_func_) {
      cuckoonode& curr_node = tree[bucket_to_replace_pos];
      (*buckets)[curr_node.bucket_id] =
        (*buckets)[tree[curr_node.parent_pos].bucket_id];
      bucket_to_replace_pos = curr_node.parent_pos;
    }
    *bucket_id = tree[bucket_to_replace_pos].bucket_id;
  }
  return null_found;
}

}  // namespace rocksdb
#endif  // rocksdb_lite
