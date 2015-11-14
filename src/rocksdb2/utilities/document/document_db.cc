//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.

#ifndef rocksdb_lite

#include "rocksdb/utilities/document_db.h"

#include "rocksdb/cache.h"
#include "rocksdb/table.h"
#include "rocksdb/filter_policy.h"
#include "rocksdb/comparator.h"
#include "rocksdb/db.h"
#include "rocksdb/slice.h"
#include "rocksdb/utilities/json_document.h"
#include "util/coding.h"
#include "util/mutexlock.h"
#include "port/port.h"

namespace rocksdb {

// important note: secondary index column families should be very small and
// generally fit in memory. assume that accessing secondary index column
// families is much faster than accessing primary index (data heap) column
// family. accessing a key (i.e. checking for existance) from a column family in
// rocksdb is not much faster than accessing both key and value since they are
// kept together and loaded from storage together.

namespace {
// < 0   <=>  lhs < rhs
// == 0  <=>  lhs == rhs
// > 0   <=>  lhs == rhs
// todo(icanadi) move this to jsondocument?
int documentcompare(const jsondocument& lhs, const jsondocument& rhs) {
  assert(rhs.isobject() == false && rhs.isobject() == false &&
         lhs.type() == rhs.type());

  switch (lhs.type()) {
    case jsondocument::knull:
      return 0;
    case jsondocument::kbool:
      return static_cast<int>(lhs.getbool()) - static_cast<int>(rhs.getbool());
    case jsondocument::kdouble: {
      double res = lhs.getdouble() - rhs.getdouble();
      return res == 0.0 ? 0 : (res < 0.0 ? -1 : 1);
    }
    case jsondocument::kint64: {
      int64_t res = lhs.getint64() - rhs.getint64();
      return res == 0 ? 0 : (res < 0 ? -1 : 1);
    }
    case jsondocument::kstring:
      return slice(lhs.getstring()).compare(slice(rhs.getstring()));
    default:
      assert(false);
  }
  return 0;
}
}  // namespace

class filter {
 public:
  // returns nullptr on parse failure
  static filter* parsefilter(const jsondocument& filter);

  struct interval {
    const jsondocument* upper_bound;
    const jsondocument* lower_bound;
    bool upper_inclusive;
    bool lower_inclusive;
    interval()
        : upper_bound(nullptr),
          lower_bound(nullptr),
          upper_inclusive(false),
          lower_inclusive(false) {}
    interval(const jsondocument* ub, const jsondocument* lb, bool ui, bool li)
        : upper_bound(ub),
          lower_bound(lb),
          upper_inclusive(ui),
          lower_inclusive(li) {}
    void updateupperbound(const jsondocument* ub, bool inclusive);
    void updatelowerbound(const jsondocument* lb, bool inclusive);
  };

  bool satisfiesfilter(const jsondocument& document) const;
  const interval* getinterval(const std::string& field) const;

 private:
  explicit filter(const jsondocument& filter) : filter_(filter) {}

  // copied from the parameter
  const jsondocument filter_;
  // upper_bound and lower_bound point to jsondocuments in filter_, so no need
  // to free them
  // constant after construction
  std::unordered_map<std::string, interval> intervals_;
};

void filter::interval::updateupperbound(const jsondocument* ub,
                                        bool inclusive) {
  bool update = (upper_bound == nullptr);
  if (!update) {
    int cmp = documentcompare(*upper_bound, *ub);
    update = (cmp > 0) || (cmp == 0 && !inclusive);
  }
  if (update) {
    upper_bound = ub;
    upper_inclusive = inclusive;
  }
}

void filter::interval::updatelowerbound(const jsondocument* lb,
                                        bool inclusive) {
  bool update = (lower_bound == nullptr);
  if (!update) {
    int cmp = documentcompare(*lower_bound, *lb);
    update = (cmp < 0) || (cmp == 0 && !inclusive);
  }
  if (update) {
    lower_bound = lb;
    lower_inclusive = inclusive;
  }
}

filter* filter::parsefilter(const jsondocument& filter) {
  if (filter.isobject() == false) {
    return nullptr;
  }

  std::unique_ptr<filter> f(new filter(filter));

  for (const auto& items : f->filter_.items()) {
    if (items.first.size() && items.first[0] == '$') {
      // fields starting with '$' are commands
      continue;
    }
    assert(f->intervals_.find(items.first) == f->intervals_.end());
    if (items.second->isobject()) {
      if (items.second->count() == 0) {
        // uhm...?
        return nullptr;
      }
      interval interval;
      for (const auto& condition : items.second->items()) {
        if (condition.second->isobject() || condition.second->isarray()) {
          // comparison operators not defined on objects. invalid array
          return nullptr;
        }
        // comparison operators:
        if (condition.first == "$gt") {
          interval.updatelowerbound(condition.second, false);
        } else if (condition.first == "$gte") {
          interval.updatelowerbound(condition.second, true);
        } else if (condition.first == "$lt") {
          interval.updateupperbound(condition.second, false);
        } else if (condition.first == "$lte") {
          interval.updateupperbound(condition.second, true);
        } else {
          // todo(icanadi) more logical operators
          return nullptr;
        }
      }
      f->intervals_.insert({items.first, interval});
    } else {
      // equality
      f->intervals_.insert(
          {items.first, interval(items.second, items.second, true, true)});
    }
  }

  return f.release();
}

const filter::interval* filter::getinterval(const std::string& field) const {
  auto itr = intervals_.find(field);
  if (itr == intervals_.end()) {
    return nullptr;
  }
  // we can do that since intervals_ is constant after construction
  return &itr->second;
}

bool filter::satisfiesfilter(const jsondocument& document) const {
  for (const auto& interval : intervals_) {
    auto value = document.get(interval.first);
    if (value == nullptr) {
      // doesn't have the value, doesn't satisfy the filter
      // (we don't support null queries yet)
      return false;
    }
    if (interval.second.upper_bound != nullptr) {
      if (value->type() != interval.second.upper_bound->type()) {
        // no cross-type queries yet
        // todo(icanadi) do this at least for numbers!
        return false;
      }
      int cmp = documentcompare(*interval.second.upper_bound, *value);
      if (cmp < 0 || (cmp == 0 && interval.second.upper_inclusive == false)) {
        // bigger (or equal) than upper bound
        return false;
      }
    }
    if (interval.second.lower_bound != nullptr) {
      if (value->type() != interval.second.lower_bound->type()) {
        // no cross-type queries yet
        return false;
      }
      int cmp = documentcompare(*interval.second.lower_bound, *value);
      if (cmp > 0 || (cmp == 0 && interval.second.lower_inclusive == false)) {
        // smaller (or equal) than the lower bound
        return false;
      }
    }
  }
  return true;
}

class index {
 public:
  index() = default;
  virtual ~index() {}

  virtual const char* name() const = 0;

  // functions that are executed during write time
  // ---------------------------------------------
  // getindexkey() generates a key that will be used to index document and
  // returns the key though the second std::string* parameter
  virtual void getindexkey(const jsondocument& document,
                           std::string* key) const = 0;
  // keys generated with getindexkey() will be compared using this comparator.
  // it should be assumed that there will be a suffix added to the index key
  // according to indexkey implementation
  virtual const comparator* getcomparator() const = 0;

  // functions that are executed during query time
  // ---------------------------------------------
  enum direction {
    kforwards,
    kbackwards,
  };
  // returns true if this index can provide some optimization for satisfying
  // filter. false otherwise
  virtual bool usefulindex(const filter& filter) const = 0;
  // for every filter (assuming usefulindex()) there is a continuous interval of
  // keys in the index that satisfy the index conditions. that interval can be
  // three things:
  // * [a, b]
  // * [a, infinity>
  // * <-infinity, b]
  //
  // query engine that uses this index for optimization will access the interval
  // by first calling position() and then iterating in the direction (returned
  // by position()) while shouldcontinuelooking() is true.
  // * for [a, b] interval position() will seek() to a and return kforwards.
  // shouldcontinuelooking() will be true until the iterator value gets beyond b
  // -- then it will return false
  // * for [a, infinity> position() will seek() to a and return kforwards.
  // shouldcontinuelooking() will always return true
  // * for <-infinity, b] position() will seek() to b and return kbackwards.
  // shouldcontinuelooking() will always return true (given that iterator is
  // advanced by calling prev())
  virtual direction position(const filter& filter,
                             iterator* iterator) const = 0;
  virtual bool shouldcontinuelooking(const filter& filter,
                                     const slice& secondary_key,
                                     direction direction) const = 0;

  // static function that is executed when index is created
  // ---------------------------------------------
  // create index from user-supplied description. return nullptr on parse
  // failure.
  static index* createindexfromdescription(const jsondocument& description,
                                           const std::string& name);

 private:
  // no copying allowed
  index(const index&);
  void operator=(const index&);
};

// encoding helper function
namespace {
std::string internalsecondaryindexname(const std::string& user_name) {
  return "index_" + user_name;
}

// don't change these, they are persisted in secondary indexes
enum jsonprimitivesencoding : char {
  knull = 0x1,
  kbool = 0x2,
  kdouble = 0x3,
  kint64 = 0x4,
  kstring = 0x5,
};

// encodes simple json members (meaning string, integer, etc)
// the end result of this will be lexicographically compared to each other
bool encodejsonprimitive(const jsondocument& json, std::string* dst) {
  // todo(icanadi) revise this at some point, have a custom comparator
  switch (json.type()) {
    case jsondocument::knull:
      dst->push_back(knull);
      break;
    case jsondocument::kbool:
      dst->push_back(kbool);
      dst->push_back(static_cast<char>(json.getbool()));
      break;
    case jsondocument::kdouble:
      dst->push_back(kdouble);
      putfixed64(dst, static_cast<uint64_t>(json.getdouble()));
      break;
    case jsondocument::kint64:
      dst->push_back(kint64);
      // todo(icanadi) oops, this will not work correctly for negative numbers
      putfixed64(dst, static_cast<uint64_t>(json.getint64()));
      break;
    case jsondocument::kstring:
      dst->push_back(kstring);
      dst->append(json.getstring());
      break;
    default:
      return false;
  }
  return true;
}

}  // namespace

// format of the secondary key is:
// <secondary_key><primary_key><offset_of_primary_key uint32_t>
class indexkey {
 public:
  indexkey() : ok_(false) {}
  explicit indexkey(const slice& slice) {
    if (slice.size() < sizeof(uint32_t)) {
      ok_ = false;
      return;
    }
    uint32_t primary_key_offset =
        decodefixed32(slice.data() + slice.size() - sizeof(uint32_t));
    if (primary_key_offset >= slice.size() - sizeof(uint32_t)) {
      ok_ = false;
      return;
    }
    parts_[0] = slice(slice.data(), primary_key_offset);
    parts_[1] = slice(slice.data() + primary_key_offset,
                      slice.size() - primary_key_offset - sizeof(uint32_t));
    ok_ = true;
  }
  indexkey(const slice& secondary_key, const slice& primary_key) : ok_(true) {
    parts_[0] = secondary_key;
    parts_[1] = primary_key;
  }

  sliceparts getsliceparts() {
    uint32_t primary_key_offset = static_cast<uint32_t>(parts_[0].size());
    encodefixed32(primary_key_offset_buf_, primary_key_offset);
    parts_[2] = slice(primary_key_offset_buf_, sizeof(uint32_t));
    return sliceparts(parts_, 3);
  }

  const slice& getprimarykey() const { return parts_[1]; }
  const slice& getsecondarykey() const { return parts_[0]; }

  bool ok() const { return ok_; }

 private:
  bool ok_;
  // 0 -- secondary key
  // 1 -- primary key
  // 2 -- primary key offset
  slice parts_[3];
  char primary_key_offset_buf_[sizeof(uint32_t)];
};

class simplesortedindex : public index {
 public:
  simplesortedindex(const std::string field, const std::string& name)
      : field_(field), name_(name) {}

  virtual const char* name() const override { return name_.c_str(); }

  virtual void getindexkey(const jsondocument& document, std::string* key) const
      override {
    auto value = document.get(field_);
    if (value == nullptr) {
      // null
      if (!encodejsonprimitive(jsondocument(jsondocument::knull), key)) {
        assert(false);
      }
    }
    if (!encodejsonprimitive(*value, key)) {
      assert(false);
    }
  }
  virtual const comparator* getcomparator() const override {
    return bytewisecomparator();
  }

  virtual bool usefulindex(const filter& filter) const {
    return filter.getinterval(field_) != nullptr;
  }
  // requires: usefulindex(filter) == true
  virtual direction position(const filter& filter, iterator* iterator) const {
    auto interval = filter.getinterval(field_);
    assert(interval != nullptr);  // because index is useful
    direction direction;

    std::string op;
    const jsondocument* limit;
    if (interval->lower_bound != nullptr) {
      limit = interval->lower_bound;
      direction = kforwards;
    } else {
      limit = interval->upper_bound;
      direction = kbackwards;
    }

    std::string encoded_limit;
    if (!encodejsonprimitive(*limit, &encoded_limit)) {
      assert(false);
    }
    iterator->seek(slice(encoded_limit));

    return direction;
  }
  // requires: usefulindex(filter) == true
  virtual bool shouldcontinuelooking(const filter& filter,
                                     const slice& secondary_key,
                                     index::direction direction) const {
    auto interval = filter.getinterval(field_);
    assert(interval != nullptr);  // because index is useful

    if (direction == kforwards) {
      if (interval->upper_bound == nullptr) {
        // continue looking, no upper bound
        return true;
      }
      std::string encoded_upper_bound;
      if (!encodejsonprimitive(*interval->upper_bound, &encoded_upper_bound)) {
        // uhm...?
        // todo(icanadi) store encoded upper and lower bounds in filter*?
        assert(false);
      }
      // todo(icanadi) we need to somehow decode this and use documentcompare()
      int compare = secondary_key.compare(slice(encoded_upper_bound));
      // if (current key is bigger than upper bound) or (current key is equal to
      // upper bound, but inclusive is false) then stop looking. otherwise,
      // continue
      return (compare > 0 ||
              (compare == 0 && interval->upper_inclusive == false))
                 ? false
                 : true;
    } else {
      assert(direction == kbackwards);
      if (interval->lower_bound == nullptr) {
        // continue looking, no lower bound
        return true;
      }
      std::string encoded_lower_bound;
      if (!encodejsonprimitive(*interval->lower_bound, &encoded_lower_bound)) {
        // uhm...?
        // todo(icanadi) store encoded upper and lower bounds in filter*?
        assert(false);
      }
      // todo(icanadi) we need to somehow decode this and use documentcompare()
      int compare = secondary_key.compare(slice(encoded_lower_bound));
      // if (current key is smaller than lower bound) or (current key is equal
      // to lower bound, but inclusive is false) then stop looking. otherwise,
      // continue
      return (compare < 0 ||
              (compare == 0 && interval->lower_inclusive == false))
                 ? false
                 : true;
    }

    assert(false);
    // this is here just so compiler doesn't complain
    return false;
  }

 private:
  std::string field_;
  std::string name_;
};

index* index::createindexfromdescription(const jsondocument& description,
                                         const std::string& name) {
  if (!description.isobject() || description.count() != 1) {
    // not supported yet
    return nullptr;
  }
  const auto& field = *description.items().begin();
  if (field.second->isint64() == false || field.second->getint64() != 1) {
    // not supported yet
    return nullptr;
  }
  return new simplesortedindex(field.first, name);
}

class cursorwithfilterindexed : public cursor {
 public:
  cursorwithfilterindexed(iterator* primary_index_iter,
                          iterator* secondary_index_iter, const index* index,
                          const filter* filter)
      : primary_index_iter_(primary_index_iter),
        secondary_index_iter_(secondary_index_iter),
        index_(index),
        filter_(filter),
        valid_(true),
        current_json_document_(nullptr) {
    assert(filter_.get() != nullptr);
    direction_ = index->position(*filter_.get(), secondary_index_iter_.get());
    updateindexkey();
    advanceuntilsatisfies();
  }

  virtual bool valid() const override {
    return valid_ && secondary_index_iter_->valid();
  }
  virtual void next() {
    assert(valid());
    advance();
    advanceuntilsatisfies();
  }
  // temporary object. copy it if you want to use it
  virtual const jsondocument& document() const {
    assert(valid());
    return *current_json_document_;
  }
  virtual status status() const {
    if (!status_.ok()) {
      return status_;
    }
    if (!primary_index_iter_->status().ok()) {
      return primary_index_iter_->status();
    }
    return secondary_index_iter_->status();
  }

 private:
  void advance() {
    if (direction_ == index::kforwards) {
      secondary_index_iter_->next();
    } else {
      secondary_index_iter_->prev();
    }
    updateindexkey();
  }
  void advanceuntilsatisfies() {
    bool found = false;
    while (secondary_index_iter_->valid() &&
           index_->shouldcontinuelooking(
               *filter_.get(), index_key_.getsecondarykey(), direction_)) {
      if (!updatejsondocument()) {
        // corruption happened
        return;
      }
      if (filter_->satisfiesfilter(*current_json_document_)) {
        // we found satisfied!
        found = true;
        break;
      } else {
        // doesn't satisfy :(
        advance();
      }
    }
    if (!found) {
      valid_ = false;
    }
  }

  bool updatejsondocument() {
    assert(secondary_index_iter_->valid());
    primary_index_iter_->seek(index_key_.getprimarykey());
    if (!primary_index_iter_->valid()) {
      status_ = status::corruption(
          "inconsistency between primary and secondary index");
      valid_ = false;
      return false;
    }
    current_json_document_.reset(
        jsondocument::deserialize(primary_index_iter_->value()));
    if (current_json_document_.get() == nullptr) {
      status_ = status::corruption("json deserialization failed");
      valid_ = false;
      return false;
    }
    return true;
  }
  void updateindexkey() {
    if (secondary_index_iter_->valid()) {
      index_key_ = indexkey(secondary_index_iter_->key());
      if (!index_key_.ok()) {
        status_ = status::corruption("invalid index key");
        valid_ = false;
      }
    }
  }
  std::unique_ptr<iterator> primary_index_iter_;
  std::unique_ptr<iterator> secondary_index_iter_;
  // we don't own index_
  const index* index_;
  index::direction direction_;
  std::unique_ptr<const filter> filter_;
  bool valid_;
  indexkey index_key_;
  std::unique_ptr<jsondocument> current_json_document_;
  status status_;
};

class cursorfromiterator : public cursor {
 public:
  explicit cursorfromiterator(iterator* iter)
      : iter_(iter), current_json_document_(nullptr) {
    iter_->seektofirst();
    updatecurrentjson();
  }

  virtual bool valid() const override { return status_.ok() && iter_->valid(); }
  virtual void next() override {
    iter_->next();
    updatecurrentjson();
  }
  virtual const jsondocument& document() const override {
    assert(valid());
    return *current_json_document_;
  };
  virtual status status() const override {
    if (!status_.ok()) {
      return status_;
    }
    return iter_->status();
  }

  // not part of public cursor interface
  slice key() const { return iter_->key(); }

 private:
  void updatecurrentjson() {
    if (valid()) {
      current_json_document_.reset(jsondocument::deserialize(iter_->value()));
      if (current_json_document_.get() == nullptr) {
        status_ = status::corruption("json deserialization failed");
      }
    }
  }

  status status_;
  std::unique_ptr<iterator> iter_;
  std::unique_ptr<jsondocument> current_json_document_;
};

class cursorwithfilter : public cursor {
 public:
  cursorwithfilter(cursor* base_cursor, const filter* filter)
      : base_cursor_(base_cursor), filter_(filter) {
    assert(filter_.get() != nullptr);
    seektonextsatisfies();
  }
  virtual bool valid() const override { return base_cursor_->valid(); }
  virtual void next() override {
    assert(valid());
    base_cursor_->next();
    seektonextsatisfies();
  }
  virtual const jsondocument& document() const override {
    assert(valid());
    return base_cursor_->document();
  }
  virtual status status() const override { return base_cursor_->status(); }

 private:
  void seektonextsatisfies() {
    for (; base_cursor_->valid(); base_cursor_->next()) {
      if (filter_->satisfiesfilter(base_cursor_->document())) {
        break;
      }
    }
  }
  std::unique_ptr<cursor> base_cursor_;
  std::unique_ptr<const filter> filter_;
};

class cursorerror : public cursor {
 public:
  explicit cursorerror(status s) : s_(s) { assert(!s.ok()); }
  virtual status status() const override { return s_; }
  virtual bool valid() const override { return false; }
  virtual void next() override {}
  virtual const jsondocument& document() const override {
    assert(false);
    // compiler complains otherwise
    return trash_;
  }

 private:
  status s_;
  jsondocument trash_;
};

class documentdbimpl : public documentdb {
 public:
  documentdbimpl(
      db* db, columnfamilyhandle* primary_key_column_family,
      const std::vector<std::pair<index*, columnfamilyhandle*>>& indexes,
      const options& rocksdb_options)
      : documentdb(db),
        primary_key_column_family_(primary_key_column_family),
        rocksdb_options_(rocksdb_options) {
    for (const auto& index : indexes) {
      name_to_index_.insert(
          {index.first->name(), indexcolumnfamily(index.first, index.second)});
    }
  }

  ~documentdbimpl() {
    for (auto& iter : name_to_index_) {
      delete iter.second.index;
      delete iter.second.column_family;
    }
    delete primary_key_column_family_;
  }

  virtual status createindex(const writeoptions& write_options,
                             const indexdescriptor& index) {
    auto index_obj =
        index::createindexfromdescription(*index.description, index.name);
    if (index_obj == nullptr) {
      return status::invalidargument("failed parsing index description");
    }

    columnfamilyhandle* cf_handle;
    status s =
        createcolumnfamily(columnfamilyoptions(rocksdb_options_),
                           internalsecondaryindexname(index.name), &cf_handle);
    if (!s.ok()) {
      return s;
    }

    mutexlock l(&write_mutex_);

    std::unique_ptr<cursorfromiterator> cursor(new cursorfromiterator(
        documentdb::newiterator(readoptions(), primary_key_column_family_)));

    writebatch batch;
    for (; cursor->valid(); cursor->next()) {
      std::string secondary_index_key;
      index_obj->getindexkey(cursor->document(), &secondary_index_key);
      indexkey index_key(slice(secondary_index_key), cursor->key());
      batch.put(cf_handle, index_key.getsliceparts(), sliceparts());
    }

    if (!cursor->status().ok()) {
      delete index_obj;
      return cursor->status();
    }

    {
      mutexlock l_nti(&name_to_index_mutex_);
      name_to_index_.insert(
          {index.name, indexcolumnfamily(index_obj, cf_handle)});
    }

    return documentdb::write(write_options, &batch);
  }

  virtual status dropindex(const std::string& name) {
    mutexlock l(&write_mutex_);

    auto index_iter = name_to_index_.find(name);
    if (index_iter == name_to_index_.end()) {
      return status::invalidargument("no such index");
    }

    status s = dropcolumnfamily(index_iter->second.column_family);
    if (!s.ok()) {
      return s;
    }

    delete index_iter->second.index;
    delete index_iter->second.column_family;

    // remove from name_to_index_
    {
      mutexlock l_nti(&name_to_index_mutex_);
      name_to_index_.erase(index_iter);
    }

    return status::ok();
  }

  virtual status insert(const writeoptions& options,
                        const jsondocument& document) {
    writebatch batch;

    if (!document.isobject()) {
      return status::invalidargument("document not an object");
    }
    auto primary_key = document.get(kprimarykey);
    if (primary_key == nullptr || primary_key->isnull() ||
        (!primary_key->isstring() && !primary_key->isint64())) {
      return status::invalidargument(
          "no primary key or primary key format error");
    }
    std::string encoded_document;
    document.serialize(&encoded_document);
    std::string primary_key_encoded;
    if (!encodejsonprimitive(*primary_key, &primary_key_encoded)) {
      // previous call should be guaranteed to pass because of all primary_key
      // conditions checked before
      assert(false);
    }
    slice primary_key_slice(primary_key_encoded);

    // lock now, since we're starting db operations
    mutexlock l(&write_mutex_);
    // check if there is already a document with the same primary key
    std::string value;
    status s = documentdb::get(readoptions(), primary_key_column_family_,
                               primary_key_slice, &value);
    if (!s.isnotfound()) {
      return s.ok() ? status::invalidargument("duplicate primary key!") : s;
    }

    batch.put(primary_key_column_family_, primary_key_slice, encoded_document);

    for (const auto& iter : name_to_index_) {
      std::string secondary_index_key;
      iter.second.index->getindexkey(document, &secondary_index_key);
      indexkey index_key(slice(secondary_index_key), primary_key_slice);
      batch.put(iter.second.column_family, index_key.getsliceparts(),
                sliceparts());
    }

    return documentdb::write(options, &batch);
  }

  virtual status remove(const readoptions& read_options,
                        const writeoptions& write_options,
                        const jsondocument& query) override {
    mutexlock l(&write_mutex_);
    std::unique_ptr<cursor> cursor(
        constructfiltercursor(read_options, nullptr, query));

    writebatch batch;
    for (; cursor->status().ok() && cursor->valid(); cursor->next()) {
      const auto& document = cursor->document();
      if (!document.isobject()) {
        return status::corruption("document corruption");
      }
      auto primary_key = document.get(kprimarykey);
      if (primary_key == nullptr || primary_key->isnull() ||
          (!primary_key->isstring() && !primary_key->isint64())) {
        return status::corruption("document corruption");
      }

      // todo(icanadi) instead of doing this, just get primary key encoding from
      // cursor, as it already has this information
      std::string primary_key_encoded;
      if (!encodejsonprimitive(*primary_key, &primary_key_encoded)) {
        // previous call should be guaranteed to pass because of all primary_key
        // conditions checked before
        assert(false);
      }
      slice primary_key_slice(primary_key_encoded);
      batch.delete(primary_key_column_family_, primary_key_slice);

      for (const auto& iter : name_to_index_) {
        std::string secondary_index_key;
        iter.second.index->getindexkey(document, &secondary_index_key);
        indexkey index_key(slice(secondary_index_key), primary_key_slice);
        batch.delete(iter.second.column_family, index_key.getsliceparts());
      }
    }

    if (!cursor->status().ok()) {
      return cursor->status();
    }

    return documentdb::write(write_options, &batch);
  }

  virtual status update(const readoptions& read_options,
                        const writeoptions& write_options,
                        const jsondocument& filter,
                        const jsondocument& updates) {
    mutexlock l(&write_mutex_);
    std::unique_ptr<cursor> cursor(
        constructfiltercursor(read_options, nullptr, filter));

    writebatch batch;
    for (; cursor->status().ok() && cursor->valid(); cursor->next()) {
      const auto& old_document = cursor->document();
      jsondocument new_document(old_document);
      if (!new_document.isobject()) {
        return status::corruption("document corruption");
      }
      // todo(icanadi) make this nicer, something like class filter
      for (const auto& update : updates.items()) {
        if (update.first == "$set") {
          for (const auto& itr : update.second->items()) {
            if (itr.first == kprimarykey) {
              return status::notsupported("please don't change primary key");
            }
            new_document.set(itr.first, *itr.second);
          }
        } else {
          // todo(icanadi) more commands
          return status::invalidargument("can't understand update command");
        }
      }

      // todo(icanadi) reuse some of this code
      auto primary_key = new_document.get(kprimarykey);
      if (primary_key == nullptr || primary_key->isnull() ||
          (!primary_key->isstring() && !primary_key->isint64())) {
        // this will happen when document on storage doesn't have primary key,
        // since we don't support any update operations on primary key. that's
        // why this is corruption error
        return status::corruption("corrupted document -- primary key missing");
      }
      std::string encoded_document;
      new_document.serialize(&encoded_document);
      std::string primary_key_encoded;
      if (!encodejsonprimitive(*primary_key, &primary_key_encoded)) {
        // previous call should be guaranteed to pass because of all primary_key
        // conditions checked before
        assert(false);
      }
      slice primary_key_slice(primary_key_encoded);
      batch.put(primary_key_column_family_, primary_key_slice,
                encoded_document);

      for (const auto& iter : name_to_index_) {
        std::string old_key, new_key;
        iter.second.index->getindexkey(old_document, &old_key);
        iter.second.index->getindexkey(new_document, &new_key);
        if (old_key == new_key) {
          // don't need to update this secondary index
          continue;
        }

        indexkey old_index_key(slice(old_key), primary_key_slice);
        indexkey new_index_key(slice(new_key), primary_key_slice);

        batch.delete(iter.second.column_family, old_index_key.getsliceparts());
        batch.put(iter.second.column_family, new_index_key.getsliceparts(),
                  sliceparts());
      }
    }

    if (!cursor->status().ok()) {
      return cursor->status();
    }

    return documentdb::write(write_options, &batch);
  }

  virtual cursor* query(const readoptions& read_options,
                        const jsondocument& query) override {
    cursor* cursor = nullptr;

    if (!query.isarray()) {
      return new cursorerror(
          status::invalidargument("query has to be an array"));
    }

    // todo(icanadi) support index "_id"
    for (size_t i = 0; i < query.count(); ++i) {
      const auto& command_doc = query[i];
      if (command_doc.count() != 1) {
        // there can be only one key-value pair in each of array elements.
        // key is the command and value are the params
        delete cursor;
        return new cursorerror(status::invalidargument("invalid query"));
      }
      const auto& command = *command_doc.items().begin();

      if (command.first == "$filter") {
        cursor = constructfiltercursor(read_options, cursor, *command.second);
      } else {
        // only filter is supported for now
        delete cursor;
        return new cursorerror(status::invalidargument("invalid query"));
      }
    }

    if (cursor == nullptr) {
      cursor = new cursorfromiterator(
          documentdb::newiterator(read_options, primary_key_column_family_));
    }

    return cursor;
  }

  // rocksdb functions
  virtual status get(const readoptions& options,
                     columnfamilyhandle* column_family, const slice& key,
                     std::string* value) override {
    return status::notsupported("");
  }
  virtual status get(const readoptions& options, const slice& key,
                     std::string* value) override {
    return status::notsupported("");
  }
  virtual status write(const writeoptions& options,
                       writebatch* updates) override {
    return status::notsupported("");
  }
  virtual iterator* newiterator(const readoptions& options,
                                columnfamilyhandle* column_family) override {
    return nullptr;
  }
  virtual iterator* newiterator(const readoptions& options) override {
    return nullptr;
  }

 private:
  cursor* constructfiltercursor(readoptions read_options, cursor* cursor,
                                const jsondocument& query) {
    std::unique_ptr<const filter> filter(filter::parsefilter(query));
    if (filter.get() == nullptr) {
      return new cursorerror(status::invalidargument("invalid query"));
    }

    indexcolumnfamily tmp_storage(nullptr, nullptr);

    if (cursor == nullptr) {
      auto index_name = query.get("$index");
      indexcolumnfamily* index_column_family = nullptr;
      if (index_name != nullptr && index_name->isstring()) {
        {
          mutexlock l(&name_to_index_mutex_);
          auto index_iter = name_to_index_.find(index_name->getstring());
          if (index_iter != name_to_index_.end()) {
            tmp_storage = index_iter->second;
            index_column_family = &tmp_storage;
          } else {
            return new cursorerror(
                status::invalidargument("index does not exist"));
          }
        }
      }

      if (index_column_family != nullptr &&
          index_column_family->index->usefulindex(*filter.get())) {
        std::vector<iterator*> iterators;
        status s = documentdb::newiterators(
            read_options,
            {primary_key_column_family_, index_column_family->column_family},
            &iterators);
        if (!s.ok()) {
          delete cursor;
          return new cursorerror(s);
        }
        assert(iterators.size() == 2);
        return new cursorwithfilterindexed(iterators[0], iterators[1],
                                           index_column_family->index,
                                           filter.release());
      } else {
        return new cursorwithfilter(
            new cursorfromiterator(documentdb::newiterator(
                read_options, primary_key_column_family_)),
            filter.release());
      }
    } else {
      return new cursorwithfilter(cursor, filter.release());
    }
    assert(false);
    return nullptr;
  }

  // currently, we lock and serialize all writes to rocksdb. reads are not
  // locked and always get consistent view of the database. we should optimize
  // locking in the future
  port::mutex write_mutex_;
  port::mutex name_to_index_mutex_;
  const char* kprimarykey = "_id";
  struct indexcolumnfamily {
    indexcolumnfamily(index* _index, columnfamilyhandle* _column_family)
        : index(_index), column_family(_column_family) {}
    index* index;
    columnfamilyhandle* column_family;
  };


  // name_to_index_ protected:
  // 1) when writing -- 1. lock write_mutex_, 2. lock name_to_index_mutex_
  // 2) when reading -- lock name_to_index_mutex_ or write_mutex_
  std::unordered_map<std::string, indexcolumnfamily> name_to_index_;
  columnfamilyhandle* primary_key_column_family_;
  options rocksdb_options_;
};

namespace {
options getrocksdboptionsfromoptions(const documentdboptions& options) {
  options rocksdb_options;
  rocksdb_options.max_background_compactions = options.background_threads - 1;
  rocksdb_options.max_background_flushes = 1;
  rocksdb_options.write_buffer_size = options.memtable_size;
  rocksdb_options.max_write_buffer_number = 6;
  blockbasedtableoptions table_options;
  table_options.block_cache = newlrucache(options.cache_size);
  rocksdb_options.table_factory.reset(newblockbasedtablefactory(table_options));
  return rocksdb_options;
}
}  // namespace

status documentdb::open(const documentdboptions& options,
                        const std::string& name,
                        const std::vector<documentdb::indexdescriptor>& indexes,
                        documentdb** db, bool read_only) {
  options rocksdb_options = getrocksdboptionsfromoptions(options);
  rocksdb_options.create_if_missing = true;

  std::vector<columnfamilydescriptor> column_families;
  column_families.push_back(columnfamilydescriptor(
      kdefaultcolumnfamilyname, columnfamilyoptions(rocksdb_options)));
  for (const auto& index : indexes) {
    column_families.emplace_back(internalsecondaryindexname(index.name),
                                 columnfamilyoptions(rocksdb_options));
  }
  std::vector<columnfamilyhandle*> handles;
  db* base_db;
  status s;
  if (read_only) {
    s = db::openforreadonly(dboptions(rocksdb_options), name, column_families,
                            &handles, &base_db);
  } else {
    s = db::open(dboptions(rocksdb_options), name, column_families, &handles,
                 &base_db);
  }
  if (!s.ok()) {
    return s;
  }

  std::vector<std::pair<index*, columnfamilyhandle*>> index_cf(indexes.size());
  assert(handles.size() == indexes.size() + 1);
  for (size_t i = 0; i < indexes.size(); ++i) {
    auto index = index::createindexfromdescription(*indexes[i].description,
                                                   indexes[i].name);
    index_cf[i] = {index, handles[i + 1]};
  }
  *db = new documentdbimpl(base_db, handles[0], index_cf, rocksdb_options);
  return status::ok();
}

}  // namespace rocksdb
#endif  // rocksdb_lite
