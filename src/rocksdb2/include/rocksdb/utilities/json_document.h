//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
#pragma once
#ifndef rocksdb_lite

#include <string>
#include <map>
#include <unordered_map>
#include <vector>

#include "rocksdb/slice.h"

// we use jsondocument for documentdb api
// implementation inspired by folly::dynamic and rapidjson

namespace rocksdb {

// note: none of this is thread-safe
class jsondocument {
 public:
  // return nullptr on parse failure
  static jsondocument* parsejson(const char* json);

  enum type {
    knull,
    karray,
    kbool,
    kdouble,
    kint64,
    kobject,
    kstring,
  };

  jsondocument();  // null
  /* implicit */ jsondocument(bool b);
  /* implicit */ jsondocument(double d);
  /* implicit */ jsondocument(int64_t i);
  /* implicit */ jsondocument(const std::string& s);
  /* implicit */ jsondocument(const char* s);
  // constructs jsondocument of specific type with default value
  explicit jsondocument(type type);

  // copy constructor
  jsondocument(const jsondocument& json_document);

  ~jsondocument();

  type type() const;

  // requires: isobject()
  bool contains(const std::string& key) const;
  // returns nullptr if !contains()
  // don't delete the returned pointer
  // requires: isobject()
  const jsondocument* get(const std::string& key) const;
  // requires: isobject()
  jsondocument& operator[](const std::string& key);
  // requires: isobject()
  const jsondocument& operator[](const std::string& key) const;
  // returns `this`, so you can chain operations.
  // copies value
  // requires: isobject()
  jsondocument* set(const std::string& key, const jsondocument& value);

  // requires: isarray() == true || isobject() == true
  size_t count() const;

  // requires: isarray()
  const jsondocument* getfromarray(size_t i) const;
  // requires: isarray()
  jsondocument& operator[](size_t i);
  // requires: isarray()
  const jsondocument& operator[](size_t i) const;
  // returns `this`, so you can chain operations.
  // copies the value
  // requires: isarray() && i < count()
  jsondocument* setinarray(size_t i, const jsondocument& value);
  // requires: isarray()
  jsondocument* pushback(const jsondocument& value);

  bool isnull() const;
  bool isarray() const;
  bool isbool() const;
  bool isdouble() const;
  bool isint64() const;
  bool isobject() const;
  bool isstring() const;

  // requires: isbool() == true
  bool getbool() const;
  // requires: isdouble() == true
  double getdouble() const;
  // requires: isint64() == true
  int64_t getint64() const;
  // requires: isstring() == true
  const std::string& getstring() const;

  bool operator==(const jsondocument& rhs) const;

  std::string debugstring() const;

 private:
  class itemsiteratorgenerator;

 public:
  // requires: isobject()
  itemsiteratorgenerator items() const;

  // appends serialized object to dst
  void serialize(std::string* dst) const;
  // returns nullptr if slice doesn't represent valid serialized jsondocument
  static jsondocument* deserialize(const slice& src);

 private:
  void serializeinternal(std::string* dst, bool type_prefix) const;
  // returns false if slice doesn't represent valid serialized jsondocument.
  // otherwise, true
  bool deserializeinternal(slice* input);

  typedef std::vector<jsondocument*> array;
  typedef std::unordered_map<std::string, jsondocument*> object;

  // iteration on objects
  class const_item_iterator {
   public:
    typedef object::const_iterator it;
    typedef object::value_type value_type;
    /* implicit */ const_item_iterator(it it) : it_(it) {}
    it& operator++() { return ++it_; }
    bool operator!=(const const_item_iterator& other) {
      return it_ != other.it_;
    }
    value_type operator*() { return *it_; }

   private:
    it it_;
  };
  class itemsiteratorgenerator {
   public:
    /* implicit */ itemsiteratorgenerator(const object& object)
        : object_(object) {}
    const_item_iterator begin() { return object_.begin(); }
    const_item_iterator end() { return object_.end(); }

   private:
    const object& object_;
  };

  union data {
    data() : n(nullptr) {}
    ~data() {}

    void* n;
    array a;
    bool b;
    double d;
    int64_t i;
    std::string s;
    object o;
  } data_;
  const type type_;

  // our serialization format's first byte specifies the encoding version. that
  // way, we can easily change our format while providing backwards
  // compatibility. this constant specifies the current version of the
  // serialization format
  static const char kserializationformatversion;
};

}  // namespace rocksdb

#endif  // rocksdb_lite
