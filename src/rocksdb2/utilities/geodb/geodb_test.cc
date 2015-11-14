//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
//
#include "utilities/geodb/geodb_impl.h"

#include <cctype>
#include "util/testharness.h"

namespace rocksdb {

class geodbtest {
 public:
  static const std::string kdefaultdbname;
  static options options;
  db* db;
  geodb* geodb;

  geodbtest() {
    geodboptions geodb_options;
    assert_ok(destroydb(kdefaultdbname, options));
    options.create_if_missing = true;
    status status = db::open(options, kdefaultdbname, &db);
    geodb =  new geodbimpl(db, geodb_options);
  }

  ~geodbtest() {
    delete geodb;
  }

  geodb* getdb() {
    return geodb;
  }
};

const std::string geodbtest::kdefaultdbname = "/tmp/geodefault";
options geodbtest::options = options();

// insert, get and remove
test(geodbtest, simpletest) {
  geoposition pos1(100, 101);
  std::string id1("id1");
  std::string value1("value1");

  // insert first object into database
  geoobject obj1(pos1, id1, value1);
  status status = getdb()->insert(obj1);
  assert_true(status.ok());

  // insert second object into database
  geoposition pos2(200, 201);
  std::string id2("id2");
  std::string value2 = "value2";
  geoobject obj2(pos2, id2, value2);
  status = getdb()->insert(obj2);
  assert_true(status.ok());

  // retrieve first object using position
  std::string value;
  status = getdb()->getbyposition(pos1, slice(id1), &value);
  assert_true(status.ok());
  assert_eq(value, value1);

  // retrieve first object using id
  geoobject obj;
  status = getdb()->getbyid(slice(id1), &obj);
  assert_true(status.ok());
  assert_eq(obj.position.latitude, 100);
  assert_eq(obj.position.longitude, 101);
  assert_eq(obj.id.compare(id1), 0);
  assert_eq(obj.value, value1);

  // delete first object
  status = getdb()->remove(slice(id1));
  assert_true(status.ok());
  status = getdb()->getbyposition(pos1, slice(id1), &value);
  assert_true(status.isnotfound());
  status = getdb()->getbyid(id1, &obj);
  assert_true(status.isnotfound());

  // check that we can still find second object
  status = getdb()->getbyposition(pos2, id2, &value);
  assert_true(status.ok());
  assert_eq(value, value2);
  status = getdb()->getbyid(id2, &obj);
  assert_true(status.ok());
}

// search.
// verify distances via http://www.stevemorse.org/nearest/distance.php
test(geodbtest, search) {
  geoposition pos1(45, 45);
  std::string id1("mid1");
  std::string value1 = "midvalue1";

  // insert object at 45 degree latitude
  geoobject obj1(pos1, id1, value1);
  status status = getdb()->insert(obj1);
  assert_true(status.ok());

  // search all objects centered at 46 degree latitude with
  // a radius of 200 kilometers. we should find the one object that
  // we inserted earlier.
  std::vector<geoobject> values;
  status = getdb()->searchradial(geoposition(46, 46), 200000, &values);
  assert_true(status.ok());
  assert_eq(values.size(), 1u);

  // search all objects centered at 46 degree latitude with
  // a radius of 2 kilometers. there should be none.
  values.clear();
  status = getdb()->searchradial(geoposition(46, 46), 2, &values);
  assert_true(status.ok());
  assert_eq(values.size(), 0u);
}

}  // namespace rocksdb

int main(int argc, char* argv[]) {
  return rocksdb::test::runalltests();
}
