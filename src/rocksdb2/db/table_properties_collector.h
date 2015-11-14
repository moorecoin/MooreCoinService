//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// this file defines a collection of statistics collectors.
#pragma once

#include "rocksdb/table_properties.h"

#include <memory>
#include <string>
#include <vector>

namespace rocksdb {

struct internalkeytablepropertiesnames {
  static const std::string kdeletedkeys;
};

// collecting the statistics for internal keys. visible only by internal
// rocksdb modules.
class internalkeypropertiescollector : public tablepropertiescollector {
 public:
  virtual status add(const slice& key, const slice& value) override;

  virtual status finish(usercollectedproperties* properties) override;

  virtual const char* name() const override {
    return "internalkeypropertiescollector";
  }

  usercollectedproperties getreadableproperties() const override;

 private:
  uint64_t deleted_keys_ = 0;
};

class internalkeypropertiescollectorfactory
    : public tablepropertiescollectorfactory {
 public:
  virtual tablepropertiescollector* createtablepropertiescollector() {
    return new internalkeypropertiescollector();
  }

  virtual const char* name() const override {
    return "internalkeypropertiescollectorfactory";
  }
};

// when rocksdb creates a new table, it will encode all "user keys" into
// "internal keys", which contains meta information of a given entry.
//
// this class extracts user key from the encoded internal key when add() is
// invoked.
class userkeytablepropertiescollector : public tablepropertiescollector {
 public:
  // transfer of ownership
  explicit userkeytablepropertiescollector(tablepropertiescollector* collector)
      : collector_(collector) {}

  virtual ~userkeytablepropertiescollector() {}

  virtual status add(const slice& key, const slice& value) override;

  virtual status finish(usercollectedproperties* properties) override;

  virtual const char* name() const override { return collector_->name(); }

  usercollectedproperties getreadableproperties() const override;

 protected:
  std::unique_ptr<tablepropertiescollector> collector_;
};

class userkeytablepropertiescollectorfactory
    : public tablepropertiescollectorfactory {
 public:
  explicit userkeytablepropertiescollectorfactory(
      std::shared_ptr<tablepropertiescollectorfactory> user_collector_factory)
      : user_collector_factory_(user_collector_factory) {}
  virtual tablepropertiescollector* createtablepropertiescollector() {
    return new userkeytablepropertiescollector(
        user_collector_factory_->createtablepropertiescollector());
  }

  virtual const char* name() const override {
    return user_collector_factory_->name();
  }

 private:
  std::shared_ptr<tablepropertiescollectorfactory> user_collector_factory_;
};

}  // namespace rocksdb
