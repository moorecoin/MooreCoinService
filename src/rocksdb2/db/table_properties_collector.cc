//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.

#include "db/table_properties_collector.h"

#include "db/dbformat.h"
#include "util/coding.h"

namespace rocksdb {

status internalkeypropertiescollector::add(
    const slice& key, const slice& value) {
  parsedinternalkey ikey;
  if (!parseinternalkey(key, &ikey)) {
    return status::invalidargument("invalid internal key");
  }

  if (ikey.type == valuetype::ktypedeletion) {
    ++deleted_keys_;
  }

  return status::ok();
}

status internalkeypropertiescollector::finish(
    usercollectedproperties* properties) {
  assert(properties);
  assert(properties->find(
        internalkeytablepropertiesnames::kdeletedkeys) == properties->end());
  std::string val;

  putvarint64(&val, deleted_keys_);
  properties->insert({ internalkeytablepropertiesnames::kdeletedkeys, val });

  return status::ok();
}

usercollectedproperties
internalkeypropertiescollector::getreadableproperties() const {
  return {
    { "kdeletedkeys", std::to_string(deleted_keys_) }
  };
}


status userkeytablepropertiescollector::add(
    const slice& key, const slice& value) {
  parsedinternalkey ikey;
  if (!parseinternalkey(key, &ikey)) {
    return status::invalidargument("invalid internal key");
  }

  return collector_->add(ikey.user_key, value);
}

status userkeytablepropertiescollector::finish(
    usercollectedproperties* properties) {
  return collector_->finish(properties);
}

usercollectedproperties
userkeytablepropertiescollector::getreadableproperties() const {
  return collector_->getreadableproperties();
}


const std::string internalkeytablepropertiesnames::kdeletedkeys
  = "rocksdb.deleted.keys";

uint64_t getdeletedkeys(
    const usercollectedproperties& props) {
  auto pos = props.find(internalkeytablepropertiesnames::kdeletedkeys);
  if (pos == props.end()) {
    return 0;
  }
  slice raw = pos->second;
  uint64_t val = 0;
  return getvarint64(&raw, &val) ? val : 0;
}

}  // namespace rocksdb
