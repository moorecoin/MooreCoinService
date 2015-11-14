//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.

#pragma once
#ifndef rocksdb_lite

#include <string>
#include <vector>

#include "rocksdb/db.h"
#include "rocksdb/slice.h"
#include "rocksdb/utilities/stackable_db.h"

namespace rocksdb {
namespace spatial {

// note: spatialdb is experimental and we might change its api without warning.
// please talk to us before developing against spatialdb api.
//
// spatialdb is a support for spatial indexes built on top of rocksdb.
// when creating a new spatialdb, clients specifies a list of spatial indexes to
// build on their data. each spatial index is defined by the area and
// granularity. if you're storing map data, different spatial index
// granularities can be used for different zoom levels.
//
// each element inserted into spatialdb has:
// * a bounding box, which determines how will the element be indexed
// * string blob, which will usually be wkb representation of the polygon
// (http://en.wikipedia.org/wiki/well-known_text)
// * feature set, which is a map of key-value pairs, where value can be null,
// int, double, bool, string
// * a list of indexes to insert the element in
//
// each query is executed on a single spatial index. query guarantees that it
// will return all elements intersecting the specified bounding box, but it
// might also return some extra non-intersecting elements.

// variant is a class that can be many things: null, bool, int, double or string
// it is used to store different value types in featureset (see below)
struct variant {
  // don't change the values here, they are persisted on disk
  enum type {
    knull = 0x0,
    kbool = 0x1,
    kint = 0x2,
    kdouble = 0x3,
    kstring = 0x4,
  };

  variant() : type_(knull) {}
  /* implicit */ variant(bool b) : type_(kbool) { data_.b = b; }
  /* implicit */ variant(uint64_t i) : type_(kint) { data_.i = i; }
  /* implicit */ variant(double d) : type_(kdouble) { data_.d = d; }
  /* implicit */ variant(const std::string& s) : type_(kstring) {
    new (&data_.s) std::string(s);
  }

  variant(const variant& v);

  ~variant() {
    if (type_ == kstring) {
      using std::string;
      (&data_.s)->~string();
    }
  }

  type type() const { return type_; }
  bool get_bool() const { return data_.b; }
  uint64_t get_int() const { return data_.i; }
  double get_double() const { return data_.d; }
  const std::string& get_string() const { return data_.s; }

  bool operator==(const variant& other);
  bool operator!=(const variant& other);

 private:
  type type_;
  union data {
    data() {}
    ~data() {}
    bool b;
    uint64_t i;
    double d;
    std::string s;
  } data_;
};

// featureset is a map of key-value pairs. one feature set is associated with
// each element in spatialdb. it can be used to add rich data about the element.
class featureset {
 private:
  typedef std::unordered_map<std::string, variant> map;

 public:
  class iterator {
   public:
    /* implicit */ iterator(const map::const_iterator itr) : itr_(itr) {}
    iterator& operator++() {
      ++itr_;
      return *this;
    }
    bool operator!=(const iterator& other) { return itr_ != other.itr_; }
    bool operator==(const iterator& other) { return itr_ == other.itr_; }
    map::value_type operator*() { return *itr_; }

   private:
    map::const_iterator itr_;
  };
  featureset() = default;

  featureset* set(const std::string& key, const variant& value);
  bool contains(const std::string& key) const;
  // requires: contains(key)
  const variant& get(const std::string& key) const;
  iterator find(const std::string& key) const;

  iterator begin() const { return map_.begin(); }
  iterator end() const { return map_.end(); }

  void clear();
  size_t size() const { return map_.size(); }

  void serialize(std::string* output) const;
  // required: empty featureset
  bool deserialize(const slice& input);

  std::string debugstring() const;

 private:
  map map_;
};

// boundingbox is a helper structure for defining rectangles representing
// bounding boxes of spatial elements.
template <typename t>
struct boundingbox {
  t min_x, min_y, max_x, max_y;
  boundingbox() = default;
  boundingbox(t _min_x, t _min_y, t _max_x, t _max_y)
      : min_x(_min_x), min_y(_min_y), max_x(_max_x), max_y(_max_y) {}

  bool intersects(const boundingbox<t>& a) const {
    return !(min_x > a.max_x || min_y > a.max_y || a.min_x > max_x ||
             a.min_y > max_y);
  }
};

struct spatialdboptions {
  uint64_t cache_size = 1 * 1024 * 1024 * 1024ll;  // 1gb
  int num_threads = 16;
  bool bulk_load = true;
};

// cursor is used to return data from the query to the client. to get all the
// data from the query, just call next() while valid() is true
class cursor {
 public:
  cursor() = default;
  virtual ~cursor() {}

  virtual bool valid() const = 0;
  // requires: valid()
  virtual void next() = 0;

  // lifetime of the underlying storage until the next call to next()
  // requires: valid()
  virtual const slice blob() = 0;
  // lifetime of the underlying storage until the next call to next()
  // requires: valid()
  virtual const featureset& feature_set() = 0;

  virtual status status() const = 0;

 private:
  // no copying allowed
  cursor(const cursor&);
  void operator=(const cursor&);
};

// spatialindexoptions defines a spatial index that will be built on the data
struct spatialindexoptions {
  // spatial indexes are referenced by names
  std::string name;
  // an area that is indexed. if the element is not intersecting with spatial
  // index's bbox, it will not be inserted into the index
  boundingbox<double> bbox;
  // tile_bits control the granularity of the spatial index. each dimension of
  // the bbox will be split into (1 << tile_bits) tiles, so there will be a
  // total of (1 << tile_bits)^2 tiles. it is recommended to configure a size of
  // each  tile to be approximately the size of the query on that spatial index
  uint32_t tile_bits;
  spatialindexoptions() {}
  spatialindexoptions(const std::string& _name,
                      const boundingbox<double>& _bbox, uint32_t _tile_bits)
      : name(_name), bbox(_bbox), tile_bits(_tile_bits) {}
};

class spatialdb : public stackabledb {
 public:
  // creates the spatialdb with specified list of indexes.
  // required: db doesn't exist
  static status create(const spatialdboptions& options, const std::string& name,
                       const std::vector<spatialindexoptions>& spatial_indexes);

  // open the existing spatialdb.  the resulting db object will be returned
  // through db parameter.
  // required: db was created using spatialdb::create
  static status open(const spatialdboptions& options, const std::string& name,
                     spatialdb** db, bool read_only = false);

  explicit spatialdb(db* db) : stackabledb(db) {}

  // insert the element into the db. element will be inserted into specified
  // spatial_indexes, based on specified bbox.
  // requires: spatial_indexes.size() > 0
  virtual status insert(const writeoptions& write_options,
                        const boundingbox<double>& bbox, const slice& blob,
                        const featureset& feature_set,
                        const std::vector<std::string>& spatial_indexes) = 0;

  // calling compact() after inserting a bunch of elements should speed up
  // reading. this is especially useful if you use spatialdboptions::bulk_load
  virtual status compact() = 0;

  // query the specified spatial_index. query will return all elements that
  // intersect bbox, but it may also return some extra elements.
  virtual cursor* query(const readoptions& read_options,
                        const boundingbox<double>& bbox,
                        const std::string& spatial_index) = 0;
};

}  // namespace spatial
}  // namespace rocksdb
#endif  // rocksdb_lite
