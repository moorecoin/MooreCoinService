// copyright (c) 2013, facebook, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.
// copyright (c) 2013 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#ifndef storage_rocksdb_include_compaction_filter_h_
#define storage_rocksdb_include_compaction_filter_h_

#include <memory>
#include <string>
#include <vector>

namespace rocksdb {

class slice;
class slicetransform;

// context information of a compaction run
struct compactionfiltercontext {
  // does this compaction run include all data files
  bool is_full_compaction;
  // is this compaction requested by the client (true),
  // or is it occurring as an automatic compaction process
  bool is_manual_compaction;
};

// compactionfilter allows an application to modify/delete a key-value at
// the time of compaction.

class compactionfilter {
 public:
  // context information of a compaction run
  struct context {
    // does this compaction run include all data files
    bool is_full_compaction;
    // is this compaction requested by the client (true),
    // or is it occurring as an automatic compaction process
    bool is_manual_compaction;
  };

  virtual ~compactionfilter() {}

  // the compaction process invokes this
  // method for kv that is being compacted. a return value
  // of false indicates that the kv should be preserved in the
  // output of this compaction run and a return value of true
  // indicates that this key-value should be removed from the
  // output of the compaction.  the application can inspect
  // the existing value of the key and make decision based on it.
  //
  // when the value is to be preserved, the application has the option
  // to modify the existing_value and pass it back through new_value.
  // value_changed needs to be set to true in this case.
  //
  // if multithreaded compaction is being used *and* a single compactionfilter
  // instance was supplied via options::compaction_filter, this method may be
  // called from different threads concurrently.  the application must ensure
  // that the call is thread-safe.
  //
  // if the compactionfilter was created by a factory, then it will only ever
  // be used by a single thread that is doing the compaction run, and this
  // call does not need to be thread-safe.  however, multiple filters may be
  // in existence and operating concurrently.
  virtual bool filter(int level,
                      const slice& key,
                      const slice& existing_value,
                      std::string* new_value,
                      bool* value_changed) const = 0;

  // returns a name that identifies this compaction filter.
  // the name will be printed to log file on start up for diagnosis.
  virtual const char* name() const = 0;
};

// compactionfilterv2 that buffers kv pairs sharing the same prefix and let
// application layer to make individual decisions for all the kv pairs in the
// buffer.
class compactionfilterv2 {
 public:
  virtual ~compactionfilterv2() {}

  // the compaction process invokes this method for all the kv pairs
  // sharing the same prefix. it is a "roll-up" version of compactionfilter.
  //
  // each entry in the return vector indicates if the corresponding kv should
  // be preserved in the output of this compaction run. the application can
  // inspect the existing values of the keys and make decision based on it.
  //
  // when a value is to be preserved, the application has the option
  // to modify the entry in existing_values and pass it back through an entry
  // in new_values. a corresponding values_changed entry needs to be set to
  // true in this case. note that the new_values vector contains only changed
  // values, i.e. new_values.size() <= values_changed.size().
  //
  typedef std::vector<slice> slicevector;
  virtual std::vector<bool> filter(int level,
                                   const slicevector& keys,
                                   const slicevector& existing_values,
                                   std::vector<std::string>* new_values,
                                   std::vector<bool>* values_changed)
    const = 0;

  // returns a name that identifies this compaction filter.
  // the name will be printed to log file on start up for diagnosis.
  virtual const char* name() const = 0;
};

// each compaction will create a new compactionfilter allowing the
// application to know about different compactions
class compactionfilterfactory {
 public:
  virtual ~compactionfilterfactory() { }

  virtual std::unique_ptr<compactionfilter> createcompactionfilter(
      const compactionfilter::context& context) = 0;

  // returns a name that identifies this compaction filter factory.
  virtual const char* name() const = 0;
};

// default implementation of compactionfilterfactory which does not
// return any filter
class defaultcompactionfilterfactory : public compactionfilterfactory {
 public:
  virtual std::unique_ptr<compactionfilter> createcompactionfilter(
      const compactionfilter::context& context) override {
    return std::unique_ptr<compactionfilter>(nullptr);
  }

  virtual const char* name() const override {
    return "defaultcompactionfilterfactory";
  }
};

// each compaction will create a new compactionfilterv2
//
// compactionfilterfactoryv2 enables application to specify a prefix and use
// compactionfilterv2 to filter kv-pairs in batches. each batch contains all
// the kv-pairs sharing the same prefix.
//
// this is useful for applications that require grouping kv-pairs in
// compaction filter to make a purge/no-purge decision. for example, if the
// key prefix is user id and the rest of key represents the type of value.
// this batching filter will come in handy if the application's compaction
// filter requires knowledge of all types of values for any user id.
//
class compactionfilterfactoryv2 {
 public:
  // note: compactionfilterfactoryv2 will not delete prefix_extractor
  explicit compactionfilterfactoryv2(const slicetransform* prefix_extractor)
    : prefix_extractor_(prefix_extractor) { }

  virtual ~compactionfilterfactoryv2() { }

  virtual std::unique_ptr<compactionfilterv2> createcompactionfilterv2(
    const compactionfiltercontext& context) = 0;

  // returns a name that identifies this compaction filter factory.
  virtual const char* name() const = 0;

  const slicetransform* getprefixextractor() const {
    return prefix_extractor_;
  }

  void setprefixextractor(const slicetransform* prefix_extractor) {
    prefix_extractor_ = prefix_extractor;
  }

 private:
  // prefix extractor for compaction filter v2
  // keys sharing the same prefix will be buffered internally.
  // client can implement a filter callback function to operate on the buffer
  const slicetransform* prefix_extractor_;
};

// default implementation of compactionfilterfactoryv2 which does not
// return any filter
class defaultcompactionfilterfactoryv2 : public compactionfilterfactoryv2 {
 public:
  explicit defaultcompactionfilterfactoryv2()
      : compactionfilterfactoryv2(nullptr) { }

  virtual std::unique_ptr<compactionfilterv2>
  createcompactionfilterv2(
      const compactionfiltercontext& context) override {
    return std::unique_ptr<compactionfilterv2>(nullptr);
  }

  virtual const char* name() const override {
    return "defaultcompactionfilterfactoryv2";
  }
};

}  // namespace rocksdb

#endif  // storage_rocksdb_include_compaction_filter_h_
