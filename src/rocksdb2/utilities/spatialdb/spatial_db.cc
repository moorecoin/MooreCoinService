//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.

#ifndef rocksdb_lite

#include "rocksdb/utilities/spatial_db.h"

#define __stdc_format_macros
#include <inttypes.h>
#include <string>
#include <vector>
#include <algorithm>
#include <set>
#include <unordered_set>

#include "rocksdb/cache.h"
#include "rocksdb/options.h"
#include "rocksdb/memtablerep.h"
#include "rocksdb/slice_transform.h"
#include "rocksdb/table.h"
#include "rocksdb/db.h"
#include "rocksdb/utilities/stackable_db.h"
#include "util/coding.h"
#include "utilities/spatialdb/utils.h"

namespace rocksdb {
namespace spatial {

// column families are used to store element's data and spatial indexes. we use
// [default] column family to store the element data. this is the format of
// [default] column family:
// * id (fixed 64 big endian) -> blob (length prefixed slice) feature_set
// (serialized)
// we have one additional column family for each spatial index. the name of the
// column family is [spatial$<spatial_index_name>]. the format is:
// * quad_key (fixed 64 bit big endian) id (fixed 64 bit big endian) -> ""
// we store information about indexes in [metadata] column family. format is:
// * spatial$<spatial_index_name> -> bbox (4 double encodings) tile_bits
// (varint32)

namespace {
const std::string kmetadatacolumnfamilyname("metadata");
inline std::string getspatialindexcolumnfamilyname(
    const std::string& spatial_index_name) {
  return "spatial$" + spatial_index_name;
}
inline bool getspatialindexname(const std::string& column_family_name,
                                slice* dst) {
  *dst = slice(column_family_name);
  if (dst->starts_with("spatial$")) {
    dst->remove_prefix(8);  // strlen("spatial$")
    return true;
  }
  return false;
}

}  // namespace

variant::variant(const variant& v) : type_(v.type_) {
  switch (v.type_) {
    case knull:
      break;
    case kbool:
      data_.b = v.data_.b;
      break;
    case kint:
      data_.i = v.data_.i;
      break;
    case kdouble:
      data_.d = v.data_.d;
      break;
    case kstring:
      new (&data_.s) std::string(v.data_.s);
      break;
    default:
      assert(false);
  }
}

bool variant::operator==(const variant& rhs) {
  if (type_ != rhs.type_) {
    return false;
  }

  switch (type_) {
    case knull:
      return true;
    case kbool:
      return data_.b == rhs.data_.b;
    case kint:
      return data_.i == rhs.data_.i;
    case kdouble:
      return data_.d == rhs.data_.d;
    case kstring:
      return data_.s == rhs.data_.s;
    default:
      assert(false);
  }
  // it will never reach here, but otherwise the compiler complains
  return false;
}

bool variant::operator!=(const variant& rhs) { return !(*this == rhs); }

featureset* featureset::set(const std::string& key, const variant& value) {
  map_.insert({key, value});
  return this;
}

bool featureset::contains(const std::string& key) const {
  return map_.find(key) != map_.end();
}

const variant& featureset::get(const std::string& key) const {
  auto itr = map_.find(key);
  assert(itr != map_.end());
  return itr->second;
}

featureset::iterator featureset::find(const std::string& key) const {
  return iterator(map_.find(key));
}

void featureset::clear() { map_.clear(); }

void featureset::serialize(std::string* output) const {
  for (const auto& iter : map_) {
    putlengthprefixedslice(output, iter.first);
    output->push_back(static_cast<char>(iter.second.type()));
    switch (iter.second.type()) {
      case variant::knull:
        break;
      case variant::kbool:
        output->push_back(static_cast<char>(iter.second.get_bool()));
        break;
      case variant::kint:
        putvarint64(output, iter.second.get_int());
        break;
      case variant::kdouble: {
        putdouble(output, iter.second.get_double());
        break;
      }
      case variant::kstring:
        putlengthprefixedslice(output, iter.second.get_string());
        break;
      default:
        assert(false);
    }
  }
}

bool featureset::deserialize(const slice& input) {
  assert(map_.empty());
  slice s(input);
  while (s.size()) {
    slice key;
    if (!getlengthprefixedslice(&s, &key) || s.size() == 0) {
      return false;
    }
    char type = s[0];
    s.remove_prefix(1);
    switch (type) {
      case variant::knull: {
        map_.insert({key.tostring(), variant()});
        break;
      }
      case variant::kbool: {
        if (s.size() == 0) {
          return false;
        }
        map_.insert({key.tostring(), variant(static_cast<bool>(s[0]))});
        s.remove_prefix(1);
        break;
      }
      case variant::kint: {
        uint64_t v;
        if (!getvarint64(&s, &v)) {
          return false;
        }
        map_.insert({key.tostring(), variant(v)});
        break;
      }
      case variant::kdouble: {
        double d;
        if (!getdouble(&s, &d)) {
          return false;
        }
        map_.insert({key.tostring(), variant(d)});
        break;
      }
      case variant::kstring: {
        slice str;
        if (!getlengthprefixedslice(&s, &str)) {
          return false;
        }
        map_.insert({key.tostring(), str.tostring()});
        break;
      }
      default:
        return false;
    }
  }
  return true;
}

std::string featureset::debugstring() const {
  std::string out = "{";
  bool comma = false;
  for (const auto& iter : map_) {
    if (comma) {
      out.append(", ");
    } else {
      comma = true;
    }
    out.append("\"" + iter.first + "\": ");
    switch (iter.second.type()) {
      case variant::knull:
        out.append("null");
      case variant::kbool:
        if (iter.second.get_bool()) {
          out.append("true");
        } else {
          out.append("false");
        }
        break;
      case variant::kint: {
        char buf[32];
        snprintf(buf, sizeof(buf), "%" priu64, iter.second.get_int());
        out.append(buf);
        break;
      }
      case variant::kdouble: {
        char buf[32];
        snprintf(buf, sizeof(buf), "%lf", iter.second.get_double());
        out.append(buf);
        break;
      }
      case variant::kstring:
        out.append("\"" + iter.second.get_string() + "\"");
        break;
      default:
        assert(false);
    }
  }
  return out + "}";
}

class valuegetter {
 public:
  valuegetter() {}
  virtual ~valuegetter() {}

  virtual bool get(uint64_t id) = 0;
  virtual const slice value() const = 0;

  virtual status status() const = 0;
};

class valuegetterfromdb : public valuegetter {
 public:
  valuegetterfromdb(db* db, columnfamilyhandle* cf) : db_(db), cf_(cf) {}

  virtual bool get(uint64_t id) override {
    std::string encoded_id;
    putfixed64bigendian(&encoded_id, id);
    status_ = db_->get(readoptions(), cf_, encoded_id, &value_);
    if (status_.isnotfound()) {
      status_ = status::corruption("index inconsistency");
      return false;
    }

    return true;
  }

  virtual const slice value() const override { return value_; }

  virtual status status() const override { return status_; }

 private:
  std::string value_;
  db* db_;
  columnfamilyhandle* cf_;
  status status_;
};

class valuegetterfromiterator : public valuegetter {
 public:
  explicit valuegetterfromiterator(iterator* iterator) : iterator_(iterator) {}

  virtual bool get(uint64_t id) override {
    std::string encoded_id;
    putfixed64bigendian(&encoded_id, id);
    iterator_->seek(encoded_id);

    if (!iterator_->valid() || iterator_->key() != slice(encoded_id)) {
      status_ = status::corruption("index inconsistency");
      return false;
    }

    return true;
  }

  virtual const slice value() const override { return iterator_->value(); }

  virtual status status() const override { return status_; }

 private:
  std::unique_ptr<iterator> iterator_;
  status status_;
};

class spatialindexcursor : public cursor {
 public:
  // tile_box is inclusive
  spatialindexcursor(iterator* spatial_iterator, valuegetter* value_getter,
                     const boundingbox<uint64_t>& tile_bbox, uint32_t tile_bits)
      : value_getter_(value_getter), valid_(true) {
    // calculate quad keys we'll need to query
    std::vector<uint64_t> quad_keys;
    quad_keys.reserve((tile_bbox.max_x - tile_bbox.min_x + 1) *
                      (tile_bbox.max_y - tile_bbox.min_y + 1));
    for (uint64_t x = tile_bbox.min_x; x <= tile_bbox.max_x; ++x) {
      for (uint64_t y = tile_bbox.min_y; y <= tile_bbox.max_y; ++y) {
        quad_keys.push_back(getquadkeyfromtile(x, y, tile_bits));
      }
    }
    std::sort(quad_keys.begin(), quad_keys.end());

    // load primary key ids for all quad keys
    for (auto quad_key : quad_keys) {
      std::string encoded_quad_key;
      putfixed64bigendian(&encoded_quad_key, quad_key);
      slice slice_quad_key(encoded_quad_key);

      // if checkquadkey is true, there is no need to reseek, since
      // spatial_iterator is already pointing at the correct quad key. this is
      // an optimization.
      if (!checkquadkey(spatial_iterator, slice_quad_key)) {
        spatial_iterator->seek(slice_quad_key);
      }

      while (checkquadkey(spatial_iterator, slice_quad_key)) {
        // extract id from spatial_iterator
        uint64_t id;
        bool ok = getfixed64bigendian(
            slice(spatial_iterator->key().data() + sizeof(uint64_t),
                  sizeof(uint64_t)),
            &id);
        if (!ok) {
          valid_ = false;
          status_ = status::corruption("spatial index corruption");
          break;
        }
        primary_key_ids_.insert(id);
        spatial_iterator->next();
      }
    }

    if (!spatial_iterator->status().ok()) {
      status_ = spatial_iterator->status();
      valid_ = false;
    }
    delete spatial_iterator;

    valid_ = valid_ && primary_key_ids_.size() > 0;

    if (valid_) {
      primary_keys_iterator_ = primary_key_ids_.begin();
      extractdata();
    }
  }

  virtual bool valid() const override { return valid_; }

  virtual void next() override {
    assert(valid_);

    ++primary_keys_iterator_;
    if (primary_keys_iterator_ == primary_key_ids_.end()) {
      valid_ = false;
      return;
    }

    extractdata();
  }

  virtual const slice blob() override { return current_blob_; }
  virtual const featureset& feature_set() override {
    return current_feature_set_;
  }

  virtual status status() const override {
    if (!status_.ok()) {
      return status_;
    }
    return value_getter_->status();
  }

 private:
  // * returns true if spatial iterator is on the current quad key and all is
  // well
  // * returns false if spatial iterator is not on current, or iterator is
  // invalid or corruption
  bool checkquadkey(iterator* spatial_iterator, const slice& quad_key) {
    if (!spatial_iterator->valid()) {
      return false;
    }
    if (spatial_iterator->key().size() != 2 * sizeof(uint64_t)) {
      status_ = status::corruption("invalid spatial index key");
      valid_ = false;
      return false;
    }
    slice spatial_iterator_quad_key(spatial_iterator->key().data(),
                                    sizeof(uint64_t));
    if (spatial_iterator_quad_key != quad_key) {
      // caller needs to reseek
      return false;
    }
    // if we come to here, we have found the quad key
    return true;
  }

  void extractdata() {
    assert(valid_);
    valid_ = value_getter_->get(*primary_keys_iterator_);

    if (valid_) {
      slice data = value_getter_->value();
      current_feature_set_.clear();
      if (!getlengthprefixedslice(&data, &current_blob_) ||
          !current_feature_set_.deserialize(data)) {
        status_ = status::corruption("primary key column family corruption");
        valid_ = false;
      }
    }

  }

  unique_ptr<valuegetter> value_getter_;
  bool valid_;
  status status_;

  featureset current_feature_set_;
  slice current_blob_;

  // this is loaded from spatial iterator.
  std::unordered_set<uint64_t> primary_key_ids_;
  std::unordered_set<uint64_t>::iterator primary_keys_iterator_;
};

class errorcursor : public cursor {
 public:
  explicit errorcursor(status s) : s_(s) { assert(!s.ok()); }
  virtual status status() const override { return s_; }
  virtual bool valid() const override { return false; }
  virtual void next() override { assert(false); }

  virtual const slice blob() override {
    assert(false);
    return slice();
  }
  virtual const featureset& feature_set() override {
    assert(false);
    // compiler complains otherwise
    return trash_;
  }

 private:
  status s_;
  featureset trash_;
};

class spatialdbimpl : public spatialdb {
 public:
  // * db -- base db that needs to be forwarded to stackabledb
  // * data_column_family -- column family used to store the data
  // * spatial_indexes -- a list of spatial indexes together with column
  // families that correspond to those spatial indexes
  // * next_id -- next id in auto-incrementing id. this is usually
  // `max_id_currenty_in_db + 1`
  spatialdbimpl(
      db* db, columnfamilyhandle* data_column_family,
      const std::vector<std::pair<spatialindexoptions, columnfamilyhandle*>>&
          spatial_indexes,
      uint64_t next_id, bool read_only)
      : spatialdb(db),
        data_column_family_(data_column_family),
        next_id_(next_id),
        read_only_(read_only) {
    for (const auto& index : spatial_indexes) {
      name_to_index_.insert(
          {index.first.name, indexcolumnfamily(index.first, index.second)});
    }
  }

  ~spatialdbimpl() {
    for (auto& iter : name_to_index_) {
      delete iter.second.column_family;
    }
    delete data_column_family_;
  }

  virtual status insert(
      const writeoptions& write_options, const boundingbox<double>& bbox,
      const slice& blob, const featureset& feature_set,
      const std::vector<std::string>& spatial_indexes) override {
    writebatch batch;

    if (spatial_indexes.size() == 0) {
      return status::invalidargument("spatial indexes can't be empty");
    }

    uint64_t id = next_id_.fetch_add(1);

    for (const auto& si : spatial_indexes) {
      auto itr = name_to_index_.find(si);
      if (itr == name_to_index_.end()) {
        return status::invalidargument("can't find index " + si);
      }
      const auto& spatial_index = itr->second.index;
      if (!spatial_index.bbox.intersects(bbox)) {
        continue;
      }
      boundingbox<uint64_t> tile_bbox = gettileboundingbox(spatial_index, bbox);

      for (uint64_t x = tile_bbox.min_x; x <= tile_bbox.max_x; ++x) {
        for (uint64_t y = tile_bbox.min_y; y <= tile_bbox.max_y; ++y) {
          // see above for format
          std::string key;
          putfixed64bigendian(
              &key, getquadkeyfromtile(x, y, spatial_index.tile_bits));
          putfixed64bigendian(&key, id);
          batch.put(itr->second.column_family, key, slice());
        }
      }
    }

    // see above for format
    std::string data_key;
    putfixed64bigendian(&data_key, id);
    std::string data_value;
    putlengthprefixedslice(&data_value, blob);
    feature_set.serialize(&data_value);
    batch.put(data_column_family_, data_key, data_value);

    return write(write_options, &batch);
  }

  virtual status compact() override {
    status s, t;
    for (auto& iter : name_to_index_) {
      t = flush(flushoptions(), iter.second.column_family);
      if (!t.ok()) {
        s = t;
      }
      t = compactrange(iter.second.column_family, nullptr, nullptr);
      if (!t.ok()) {
        s = t;
      }
    }
    t = flush(flushoptions(), data_column_family_);
    if (!t.ok()) {
      s = t;
    }
    t = compactrange(data_column_family_, nullptr, nullptr);
    if (!t.ok()) {
      s = t;
    }
    return s;
  }

  virtual cursor* query(const readoptions& read_options,
                        const boundingbox<double>& bbox,
                        const std::string& spatial_index) override {
    auto itr = name_to_index_.find(spatial_index);
    if (itr == name_to_index_.end()) {
      return new errorcursor(status::invalidargument(
          "spatial index " + spatial_index + " not found"));
    }
    const auto& si = itr->second.index;
    iterator* spatial_iterator;
    valuegetter* value_getter;

    if (read_only_) {
      spatial_iterator = newiterator(read_options, itr->second.column_family);
      value_getter = new valuegetterfromdb(this, data_column_family_);
    } else {
      std::vector<iterator*> iterators;
      status s = newiterators(read_options,
                              {data_column_family_, itr->second.column_family},
                              &iterators);
      if (!s.ok()) {
        return new errorcursor(s);
      }

      spatial_iterator = iterators[1];
      value_getter = new valuegetterfromiterator(iterators[0]);
    }
    return new spatialindexcursor(spatial_iterator, value_getter,
                                  gettileboundingbox(si, bbox), si.tile_bits);
  }

 private:
  columnfamilyhandle* data_column_family_;
  struct indexcolumnfamily {
    spatialindexoptions index;
    columnfamilyhandle* column_family;
    indexcolumnfamily(const spatialindexoptions& _index,
                      columnfamilyhandle* _cf)
        : index(_index), column_family(_cf) {}
  };
  // constant after construction!
  std::unordered_map<std::string, indexcolumnfamily> name_to_index_;

  std::atomic<uint64_t> next_id_;
  bool read_only_;
};

namespace {
dboptions getdboptions(const spatialdboptions& options) {
  dboptions db_options;
  db_options.max_background_compactions = 3 * options.num_threads / 4;
  db_options.max_background_flushes =
      options.num_threads - db_options.max_background_compactions;
  db_options.env->setbackgroundthreads(db_options.max_background_compactions,
                                       env::low);
  db_options.env->setbackgroundthreads(db_options.max_background_flushes,
                                       env::high);
  if (options.bulk_load) {
    db_options.disabledatasync = true;
  }
  return db_options;
}

columnfamilyoptions getcolumnfamilyoptions(const spatialdboptions& options,
                                           std::shared_ptr<cache> block_cache) {
  columnfamilyoptions column_family_options;
  column_family_options.write_buffer_size = 128 * 1024 * 1024;  // 128mb
  column_family_options.max_write_buffer_number = 4;
  column_family_options.level0_file_num_compaction_trigger = 2;
  column_family_options.level0_slowdown_writes_trigger = 16;
  column_family_options.level0_slowdown_writes_trigger = 32;
  // only compress levels >= 2
  column_family_options.compression_per_level.resize(
      column_family_options.num_levels);
  for (int i = 0; i < column_family_options.num_levels; ++i) {
    if (i < 2) {
      column_family_options.compression_per_level[i] = knocompression;
    } else {
      column_family_options.compression_per_level[i] = klz4compression;
    }
  }
  blockbasedtableoptions table_options;
  table_options.block_cache = block_cache;
  column_family_options.table_factory.reset(
      newblockbasedtablefactory(table_options));
  return column_family_options;
}

columnfamilyoptions optimizeoptionsfordatacolumnfamily(
    columnfamilyoptions options, std::shared_ptr<cache> block_cache) {
  options.prefix_extractor.reset(newnooptransform());
  blockbasedtableoptions block_based_options;
  block_based_options.index_type = blockbasedtableoptions::khashsearch;
  block_based_options.block_cache = block_cache;
  options.table_factory.reset(newblockbasedtablefactory(block_based_options));
  return options;
}

}  // namespace

class metadatastorage {
 public:
  metadatastorage(db* db, columnfamilyhandle* cf) : db_(db), cf_(cf) {}
  ~metadatastorage() {}

  // format: <min_x double> <min_y double> <max_x double> <max_y double>
  // <tile_bits varint32>
  status addindex(const spatialindexoptions& index) {
    std::string encoded_index;
    putdouble(&encoded_index, index.bbox.min_x);
    putdouble(&encoded_index, index.bbox.min_y);
    putdouble(&encoded_index, index.bbox.max_x);
    putdouble(&encoded_index, index.bbox.max_y);
    putvarint32(&encoded_index, index.tile_bits);
    return db_->put(writeoptions(), cf_,
                    getspatialindexcolumnfamilyname(index.name), encoded_index);
  }

  status getindex(const std::string& name, spatialindexoptions* dst) {
    std::string value;
    status s = db_->get(readoptions(), cf_,
                        getspatialindexcolumnfamilyname(name), &value);
    if (!s.ok()) {
      return s;
    }
    dst->name = name;
    slice encoded_index(value);
    bool ok = getdouble(&encoded_index, &(dst->bbox.min_x));
    ok = ok && getdouble(&encoded_index, &(dst->bbox.min_y));
    ok = ok && getdouble(&encoded_index, &(dst->bbox.max_x));
    ok = ok && getdouble(&encoded_index, &(dst->bbox.max_y));
    ok = ok && getvarint32(&encoded_index, &(dst->tile_bits));
    return ok ? status::ok() : status::corruption("index encoding corrupted");
  }

 private:
  db* db_;
  columnfamilyhandle* cf_;
};

status spatialdb::create(
    const spatialdboptions& options, const std::string& name,
    const std::vector<spatialindexoptions>& spatial_indexes) {
  dboptions db_options = getdboptions(options);
  db_options.create_if_missing = true;
  db_options.create_missing_column_families = true;
  db_options.error_if_exists = true;

  auto block_cache = newlrucache(options.cache_size);
  columnfamilyoptions column_family_options =
      getcolumnfamilyoptions(options, block_cache);

  std::vector<columnfamilydescriptor> column_families;
  column_families.push_back(columnfamilydescriptor(
      kdefaultcolumnfamilyname,
      optimizeoptionsfordatacolumnfamily(column_family_options, block_cache)));
  column_families.push_back(
      columnfamilydescriptor(kmetadatacolumnfamilyname, column_family_options));

  for (const auto& index : spatial_indexes) {
    column_families.emplace_back(getspatialindexcolumnfamilyname(index.name),
                                 column_family_options);
  }

  std::vector<columnfamilyhandle*> handles;
  db* base_db;
  status s = db::open(db_options, name, column_families, &handles, &base_db);
  if (!s.ok()) {
    return s;
  }
  metadatastorage metadata(base_db, handles[1]);
  for (const auto& index : spatial_indexes) {
    s = metadata.addindex(index);
    if (!s.ok()) {
      break;
    }
  }

  for (auto h : handles) {
    delete h;
  }
  delete base_db;

  return s;
}

status spatialdb::open(const spatialdboptions& options, const std::string& name,
                       spatialdb** db, bool read_only) {
  dboptions db_options = getdboptions(options);
  auto block_cache = newlrucache(options.cache_size);
  columnfamilyoptions column_family_options =
      getcolumnfamilyoptions(options, block_cache);

  status s;
  std::vector<std::string> existing_column_families;
  std::vector<std::string> spatial_indexes;
  s = db::listcolumnfamilies(db_options, name, &existing_column_families);
  if (!s.ok()) {
    return s;
  }
  for (const auto& cf_name : existing_column_families) {
    slice spatial_index;
    if (getspatialindexname(cf_name, &spatial_index)) {
      spatial_indexes.emplace_back(spatial_index.data(), spatial_index.size());
    }
  }

  std::vector<columnfamilydescriptor> column_families;
  column_families.push_back(columnfamilydescriptor(
      kdefaultcolumnfamilyname,
      optimizeoptionsfordatacolumnfamily(column_family_options, block_cache)));
  column_families.push_back(
      columnfamilydescriptor(kmetadatacolumnfamilyname, column_family_options));

  for (const auto& index : spatial_indexes) {
    column_families.emplace_back(getspatialindexcolumnfamilyname(index),
                                 column_family_options);
  }
  std::vector<columnfamilyhandle*> handles;
  db* base_db;
  if (read_only) {
    s = db::openforreadonly(db_options, name, column_families, &handles,
                            &base_db);
  } else {
    s = db::open(db_options, name, column_families, &handles, &base_db);
  }
  if (!s.ok()) {
    return s;
  }

  metadatastorage metadata(base_db, handles[1]);

  std::vector<std::pair<spatialindexoptions, columnfamilyhandle*>> index_cf;
  assert(handles.size() == spatial_indexes.size() + 2);
  for (size_t i = 0; i < spatial_indexes.size(); ++i) {
    spatialindexoptions index_options;
    s = metadata.getindex(spatial_indexes[i], &index_options);
    if (!s.ok()) {
      break;
    }
    index_cf.emplace_back(index_options, handles[i + 2]);
  }
  uint64_t next_id = 1;
  if (s.ok()) {
    // find next_id
    iterator* iter = base_db->newiterator(readoptions(), handles[0]);
    iter->seektolast();
    if (iter->valid()) {
      uint64_t last_id = 0;
      if (!getfixed64bigendian(iter->key(), &last_id)) {
        s = status::corruption("invalid key in data column family");
      } else {
        next_id = last_id + 1;
      }
    }
    delete iter;
  }
  if (!s.ok()) {
    for (auto h : handles) {
      delete h;
    }
    delete base_db;
    return s;
  }

  // i don't need metadata column family any more, so delete it
  delete handles[1];
  *db = new spatialdbimpl(base_db, handles[0], index_cf, next_id, read_only);
  return status::ok();
}

}  // namespace spatial
}  // namespace rocksdb
#endif  // rocksdb_lite
