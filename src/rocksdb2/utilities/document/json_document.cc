//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
#ifndef rocksdb_lite

#include "rocksdb/utilities/json_document.h"

#define __stdc_format_macros
#include <inttypes.h>
#include <cassert>
#include <string>
#include <map>
#include <vector>

#include "third-party/rapidjson/reader.h"
#include "util/coding.h"

namespace rocksdb {

jsondocument::jsondocument() : type_(knull) {}
jsondocument::jsondocument(bool b) : type_(kbool) { data_.b = b; }
jsondocument::jsondocument(double d) : type_(kdouble) { data_.d = d; }
jsondocument::jsondocument(int64_t i) : type_(kint64) { data_.i = i; }
jsondocument::jsondocument(const std::string& s) : type_(kstring) {
  new (&data_.s) std::string(s);
}
jsondocument::jsondocument(const char* s) : type_(kstring) {
  new (&data_.s) std::string(s);
}
jsondocument::jsondocument(type type) : type_(type) {
  // todo(icanadi) make all of this better by using templates
  switch (type) {
    case knull:
      break;
    case kobject:
      new (&data_.o) object;
      break;
    case kbool:
      data_.b = false;
      break;
    case kdouble:
      data_.d = 0.0;
      break;
    case karray:
      new (&data_.a) array;
      break;
    case kint64:
      data_.i = 0;
      break;
    case kstring:
      new (&data_.s) std::string();
      break;
    default:
      assert(false);
  }
}

jsondocument::jsondocument(const jsondocument& json_document)
    : jsondocument(json_document.type_) {
  switch (json_document.type_) {
    case knull:
      break;
    case karray:
      data_.a.reserve(json_document.data_.a.size());
      for (const auto& iter : json_document.data_.a) {
        // deep copy
        data_.a.push_back(new jsondocument(*iter));
      }
      break;
    case kbool:
      data_.b = json_document.data_.b;
      break;
    case kdouble:
      data_.d = json_document.data_.d;
      break;
    case kint64:
      data_.i = json_document.data_.i;
      break;
    case kobject: {
      for (const auto& iter : json_document.data_.o) {
        // deep copy
        data_.o.insert({iter.first, new jsondocument(*iter.second)});
      }
      break;
    }
    case kstring:
      data_.s = json_document.data_.s;
      break;
    default:
      assert(false);
  }
}

jsondocument::~jsondocument() {
  switch (type_) {
    case kobject:
      for (auto iter : data_.o) {
        delete iter.second;
      }
      (&data_.o)->~object();
      break;
    case karray:
      for (auto iter : data_.a) {
        delete iter;
      }
      (&data_.a)->~array();
      break;
    case kstring:
      using std::string;
      (&data_.s)->~string();
      break;
    default:
      // we're cool, no need for destructors for others
      break;
  }
}

jsondocument::type jsondocument::type() const { return type_; }

bool jsondocument::contains(const std::string& key) const {
  assert(type_ == kobject);
  auto iter = data_.o.find(key);
  return iter != data_.o.end();
}

const jsondocument* jsondocument::get(const std::string& key) const {
  assert(type_ == kobject);
  auto iter = data_.o.find(key);
  if (iter == data_.o.end()) {
    return nullptr;
  }
  return iter->second;
}

jsondocument& jsondocument::operator[](const std::string& key) {
  assert(type_ == kobject);
  auto iter = data_.o.find(key);
  assert(iter != data_.o.end());
  return *(iter->second);
}

const jsondocument& jsondocument::operator[](const std::string& key) const {
  assert(type_ == kobject);
  auto iter = data_.o.find(key);
  assert(iter != data_.o.end());
  return *(iter->second);
}

jsondocument* jsondocument::set(const std::string& key, const jsondocument& value) {
  assert(type_ == kobject);
  auto itr = data_.o.find(key);
  if (itr == data_.o.end()) {
    // insert
    data_.o.insert({key, new jsondocument(value)});
  } else {
    // overwrite
    delete itr->second;
    itr->second = new jsondocument(value);
  }
  return this;
}

size_t jsondocument::count() const {
  assert(type_ == karray || type_ == kobject);
  if (type_ == karray) {
    return data_.a.size();
  } else if (type_ == kobject) {
    return data_.o.size();
  }
  assert(false);
  return 0;
}

const jsondocument* jsondocument::getfromarray(size_t i) const {
  assert(type_ == karray);
  return data_.a[i];
}

jsondocument& jsondocument::operator[](size_t i) {
  assert(type_ == karray && i < data_.a.size());
  return *data_.a[i];
}

const jsondocument& jsondocument::operator[](size_t i) const {
  assert(type_ == karray && i < data_.a.size());
  return *data_.a[i];
}

jsondocument* jsondocument::setinarray(size_t i, const jsondocument& value) {
  assert(isarray() && i < data_.a.size());
  delete data_.a[i];
  data_.a[i] = new jsondocument(value);
  return this;
}

jsondocument* jsondocument::pushback(const jsondocument& value) {
  assert(isarray());
  data_.a.push_back(new jsondocument(value));
  return this;
}

bool jsondocument::isnull() const { return type() == knull; }
bool jsondocument::isarray() const { return type() == karray; }
bool jsondocument::isbool() const { return type() == kbool; }
bool jsondocument::isdouble() const { return type() == kdouble; }
bool jsondocument::isint64() const { return type() == kint64; }
bool jsondocument::isobject() const { return type() == kobject; }
bool jsondocument::isstring() const { return type() == kstring; }

bool jsondocument::getbool() const {
  assert(isbool());
  return data_.b;
}
double jsondocument::getdouble() const {
  assert(isdouble());
  return data_.d;
}
int64_t jsondocument::getint64() const {
  assert(isint64());
  return data_.i;
}
const std::string& jsondocument::getstring() const {
  assert(isstring());
  return data_.s;
}

bool jsondocument::operator==(const jsondocument& rhs) const {
  if (type_ != rhs.type_) {
    return false;
  }
  switch (type_) {
    case knull:
      return true;  // null == null
    case karray:
      if (data_.a.size() != rhs.data_.a.size()) {
        return false;
      }
      for (size_t i = 0; i < data_.a.size(); ++i) {
        if (!(*data_.a[i] == *rhs.data_.a[i])) {
          return false;
        }
      }
      return true;
    case kbool:
      return data_.b == rhs.data_.b;
    case kdouble:
      return data_.d == rhs.data_.d;
    case kint64:
      return data_.i == rhs.data_.i;
    case kobject:
      if (data_.o.size() != rhs.data_.o.size()) {
        return false;
      }
      for (const auto& iter : data_.o) {
        auto rhs_iter = rhs.data_.o.find(iter.first);
        if (rhs_iter == rhs.data_.o.end() ||
            !(*(rhs_iter->second) == *iter.second)) {
          return false;
        }
      }
      return true;
    case kstring:
      return data_.s == rhs.data_.s;
    default:
      assert(false);
  }
  // it can't come to here, but we don't want the compiler to complain
  return false;
}

std::string jsondocument::debugstring() const {
  std::string ret;
  switch (type_) {
    case knull:
      ret = "null";
      break;
    case karray:
      ret = "[";
      for (size_t i = 0; i < data_.a.size(); ++i) {
        if (i) {
          ret += ", ";
        }
        ret += data_.a[i]->debugstring();
      }
      ret += "]";
      break;
    case kbool:
      ret = data_.b ? "true" : "false";
      break;
    case kdouble: {
      char buf[100];
      snprintf(buf, sizeof(buf), "%lf", data_.d);
      ret = buf;
      break;
    }
    case kint64: {
      char buf[100];
      snprintf(buf, sizeof(buf), "%" prii64, data_.i);
      ret = buf;
      break;
    }
    case kobject: {
      bool first = true;
      ret = "{";
      for (const auto& iter : data_.o) {
        ret += first ? "" : ", ";
        first = false;
        ret += iter.first + ": ";
        ret += iter.second->debugstring();
      }
      ret += "}";
      break;
    }
    case kstring:
      ret = "\"" + data_.s + "\"";
      break;
    default:
      assert(false);
  }
  return ret;
}

jsondocument::itemsiteratorgenerator jsondocument::items() const {
  assert(type_ == kobject);
  return data_.o;
}

// parsing with rapidjson
// todo(icanadi) (perf) allocate objects with arena
jsondocument* jsondocument::parsejson(const char* json) {
  class jsondocumentbuilder {
   public:
    jsondocumentbuilder() {}

    void null() { stack_.push_back(new jsondocument()); }
    void bool(bool b) { stack_.push_back(new jsondocument(b)); }
    void int(int i) { int64(static_cast<int64_t>(i)); }
    void uint(unsigned i) { int64(static_cast<int64_t>(i)); }
    void int64(int64_t i) { stack_.push_back(new jsondocument(i)); }
    void uint64(uint64_t i) { int64(static_cast<int64_t>(i)); }
    void double(double d) { stack_.push_back(new jsondocument(d)); }
    void string(const char* str, size_t length, bool copy) {
      assert(copy);
      stack_.push_back(new jsondocument(std::string(str, length)));
    }
    void startobject() { stack_.push_back(new jsondocument(kobject)); }
    void endobject(size_t member_count) {
      assert(stack_.size() > 2 * member_count);
      auto object_base_iter = stack_.end() - member_count * 2 - 1;
      assert((*object_base_iter)->type_ == kobject);
      auto& object_map = (*object_base_iter)->data_.o;
      // iter will always be stack_.end() at some point (i.e. will not advance
      // past it) because of the way we calculate object_base_iter
      for (auto iter = object_base_iter + 1; iter != stack_.end(); iter += 2) {
        assert((*iter)->type_ == kstring);
        object_map.insert({(*iter)->data_.s, *(iter + 1)});
        delete *iter;
      }
      stack_.erase(object_base_iter + 1, stack_.end());
    }
    void startarray() { stack_.push_back(new jsondocument(karray)); }
    void endarray(size_t element_count) {
      assert(stack_.size() > element_count);
      auto array_base_iter = stack_.end() - element_count - 1;
      assert((*array_base_iter)->type_ == karray);
      (*array_base_iter)->data_.a.assign(array_base_iter + 1, stack_.end());
      stack_.erase(array_base_iter + 1, stack_.end());
    }

    jsondocument* getdocument() {
      if (stack_.size() != 1) {
        return nullptr;
      }
      return stack_.back();
    }

    void deletealldocumentsonstack() {
      for (auto document : stack_) {
        delete document;
      }
      stack_.clear();
    }

   private:
    std::vector<jsondocument*> stack_;
  };

  rapidjson::stringstream stream(json);
  rapidjson::reader reader;
  jsondocumentbuilder handler;
  bool ok = reader.parse<0>(stream, handler);
  if (!ok) {
    handler.deletealldocumentsonstack();
    return nullptr;
  }
  auto document = handler.getdocument();
  assert(document != nullptr);
  return document;
}

// serialization and deserialization
// format:
// ------
// document  ::= header(char) object
// object    ::= varint32(n) key_value*(n times)
// key_value ::= string element
// element   ::= 0x01                     (knull)
//            |  0x02 array               (karray)
//            |  0x03 byte                (kbool)
//            |  0x04 double              (kdouble)
//            |  0x05 int64               (kint64)
//            |  0x06 object              (kobject)
//            |  0x07 string              (kstring)
// array ::= varint32(n) element*(n times)
// todo(icanadi) evaluate string vs cstring format
// string ::= varint32(n) byte*(n times)
// double ::= 64-bit ieee 754 floating point (8 bytes)
// int64  ::= 8 bytes, 64-bit signed integer, little endian

namespace {
inline char getprefixfromtype(jsondocument::type type) {
  static char types[] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7};
  return types[type];
}

inline bool getnexttype(slice* input, jsondocument::type* type) {
  if (input->size() == 0) {
    return false;
  }
  static jsondocument::type prefixes[] = {
      jsondocument::knull,   jsondocument::karray, jsondocument::kbool,
      jsondocument::kdouble, jsondocument::kint64, jsondocument::kobject,
      jsondocument::kstring};
  size_t prefix = static_cast<size_t>((*input)[0]);
  if (prefix == 0 || prefix >= 0x8) {
    return false;
  }
  input->remove_prefix(1);
  *type = prefixes[static_cast<size_t>(prefix - 1)];
  return true;
}

// todo(icanadi): make sure this works on all platforms we support. some
// platforms may store double in different binary format (our specification says
// we need ieee 754)
inline void putdouble(std::string* dst, double d) {
  dst->append(reinterpret_cast<char*>(&d), sizeof(d));
}

bool decodedouble(slice* input, double* d) {
  if (input->size() < sizeof(double)) {
    return false;
  }
  memcpy(d, input->data(), sizeof(double));
  input->remove_prefix(sizeof(double));

  return true;
}
}  // namespace

void jsondocument::serialize(std::string* dst) const {
  // first byte is reserved for header
  // currently, header is only version number. that will help us provide
  // backwards compatility. we might also store more information here if
  // necessary
  dst->push_back(kserializationformatversion);
  serializeinternal(dst, false);
}

void jsondocument::serializeinternal(std::string* dst, bool type_prefix) const {
  if (type_prefix) {
    dst->push_back(getprefixfromtype(type_));
  }
  switch (type_) {
    case knull:
      // just the prefix is all we need
      break;
    case karray:
      putvarint32(dst, static_cast<uint32_t>(data_.a.size()));
      for (const auto& element : data_.a) {
        element->serializeinternal(dst, true);
      }
      break;
    case kbool:
      dst->push_back(static_cast<char>(data_.b));
      break;
    case kdouble:
      putdouble(dst, data_.d);
      break;
    case kint64:
      putfixed64(dst, static_cast<uint64_t>(data_.i));
      break;
    case kobject: {
      putvarint32(dst, static_cast<uint32_t>(data_.o.size()));
      for (const auto& iter : data_.o) {
        putlengthprefixedslice(dst, slice(iter.first));
        iter.second->serializeinternal(dst, true);
      }
      break;
    }
    case kstring:
      putlengthprefixedslice(dst, slice(data_.s));
      break;
    default:
      assert(false);
  }
}

const char jsondocument::kserializationformatversion = 1;

jsondocument* jsondocument::deserialize(const slice& src) {
  slice input(src);
  if (src.size() == 0) {
    return nullptr;
  }
  char header = input[0];
  if (header != kserializationformatversion) {
    // don't understand this header (possibly newer version format and we don't
    // support downgrade)
    return nullptr;
  }
  input.remove_prefix(1);
  auto root = new jsondocument(kobject);
  bool ok = root->deserializeinternal(&input);
  if (!ok || input.size() > 0) {
    // parsing failure :(
    delete root;
    return nullptr;
  }
  return root;
}

bool jsondocument::deserializeinternal(slice* input) {
  switch (type_) {
    case knull:
      break;
    case karray: {
      uint32_t size;
      if (!getvarint32(input, &size)) {
        return false;
      }
      data_.a.resize(size);
      for (size_t i = 0; i < size; ++i) {
        type type;
        if (!getnexttype(input, &type)) {
          return false;
        }
        data_.a[i] = new jsondocument(type);
        if (!data_.a[i]->deserializeinternal(input)) {
          return false;
        }
      }
      break;
    }
    case kbool:
      if (input->size() < 1) {
        return false;
      }
      data_.b = static_cast<bool>((*input)[0]);
      input->remove_prefix(1);
      break;
    case kdouble:
      if (!decodedouble(input, &data_.d)) {
        return false;
      }
      break;
    case kint64: {
      uint64_t tmp;
      if (!getfixed64(input, &tmp)) {
        return false;
      }
      data_.i = static_cast<int64_t>(tmp);
      break;
    }
    case kobject: {
      uint32_t num_elements;
      bool ok = getvarint32(input, &num_elements);
      for (uint32_t i = 0; ok && i < num_elements; ++i) {
        slice key;
        ok = getlengthprefixedslice(input, &key);
        type type;
        ok = ok && getnexttype(input, &type);
        if (ok) {
          std::unique_ptr<jsondocument> value(new jsondocument(type));
          ok = value->deserializeinternal(input);
          if (ok) {
            data_.o.insert({key.tostring(), value.get()});
            value.release();
          }
        }
      }
      if (!ok) {
        return false;
      }
      break;
    }
    case kstring: {
      slice key;
      if (!getlengthprefixedslice(input, &key)) {
        return false;
      }
      data_.s = key.tostring();
      break;
    }
    default:
      // this is an assert and not a return because deserializeinternal() will
      // always be called with a valid type_. in case there has been data
      // corruption, getnexttype() is the function that will detect that and
      // return corruption
      assert(false);
  }
  return true;
}

}  // namespace rocksdb
#endif  // rocksdb_lite
