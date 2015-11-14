//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//

#ifndef rocksdb_lite
#pragma once
#include <string>
#include <vector>

#include "rocksdb/utilities/stackable_db.h"
#include "rocksdb/status.h"

namespace rocksdb {

//
// configurable options needed for setting up a geo database
//
struct geodboptions {
  // backup info and error messages will be written to info_log
  // if non-nullptr.
  // default: nullptr
  logger* info_log;

  explicit geodboptions(logger* _info_log = nullptr):info_log(_info_log) { }
};

//
// a position in the earth's geoid
//
class geoposition {
 public:
  double latitude;
  double longitude;

  explicit geoposition(double la = 0, double lo = 0) :
    latitude(la), longitude(lo) {
  }
};

//
// description of an object on the geoid. it is located by a gps location,
// and is identified by the id. the value associated with this object is
// an opaque string 'value'. different objects identified by unique id's
// can have the same gps-location associated with them.
//
class geoobject {
 public:
  geoposition position;
  std::string id;
  std::string value;

  geoobject() {}

  geoobject(const geoposition& pos, const std::string& i,
            const std::string& val) :
    position(pos), id(i), value(val) {
  }
};

//
// stack your db with geodb to be able to get geo-spatial support
//
class geodb : public stackabledb {
 public:
  // geodboptions have to be the same as the ones used in a previous
  // incarnation of the db
  //
  // geodb owns the pointer `db* db` now. you should not delete it or
  // use it after the invocation of geodb
  // geodb(db* db, const geodboptions& options) : stackabledb(db) {}
  geodb(db* db, const geodboptions& options) : stackabledb(db) {}
  virtual ~geodb() {}

  // insert a new object into the location database. the object is
  // uniquely identified by the id. if an object with the same id already
  // exists in the db, then the old one is overwritten by the new
  // object being inserted here.
  virtual status insert(const geoobject& object) = 0;

  // retrieve the value of the object located at the specified gps
  // location and is identified by the 'id'.
  virtual status getbyposition(const geoposition& pos,
                               const slice& id, std::string* value) = 0;

  // retrieve the value of the object identified by the 'id'. this method
  // could be potentially slower than getbyposition
  virtual status getbyid(const slice& id, geoobject*  object) = 0;

  // delete the specified object
  virtual status remove(const slice& id) = 0;

  // returns a list of all items within a circular radius from the
  // specified gps location. if 'number_of_values' is specified,
  // then this call returns at most that many number of objects.
  // the radius is specified in 'meters'.
  virtual status searchradial(const geoposition& pos,
                              double radius,
                              std::vector<geoobject>* values,
                              int number_of_values = int_max) = 0;
};

}  // namespace rocksdb
#endif  // rocksdb_lite
