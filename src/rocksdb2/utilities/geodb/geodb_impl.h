//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//

#ifndef rocksdb_lite

#pragma once
#include <algorithm>
#include <cmath>
#include <string>
#include <sstream>
#include <stdexcept>
#include <vector>

#include "rocksdb/utilities/geo_db.h"
#include "rocksdb/utilities/stackable_db.h"
#include "rocksdb/env.h"
#include "rocksdb/status.h"

namespace rocksdb {

// a specific implementation of geodb

class geodbimpl : public geodb {
 public:
  geodbimpl(db* db, const geodboptions& options);
  ~geodbimpl();

  // associate the gps location with the identified by 'id'. the value
  // is a blob that is associated with this object.
  virtual status insert(const geoobject& object);

  // retrieve the value of the object located at the specified gps
  // location and is identified by the 'id'.
  virtual status getbyposition(const geoposition& pos,
                               const slice& id,
                               std::string* value);

  // retrieve the value of the object identified by the 'id'. this method
  // could be potentially slower than getbyposition
  virtual status getbyid(const slice& id, geoobject* object);

  // delete the specified object
  virtual status remove(const slice& id);

  // returns a list of all items within a circular radius from the
  // specified gps location
  virtual status searchradial(const geoposition& pos,
                              double radius,
                              std::vector<geoobject>* values,
                              int number_of_values);

 private:
  db* db_;
  const geodboptions options_;
  const writeoptions woptions_;
  const readoptions roptions_;

  // the value of pi
  static constexpr double pi = 3.141592653589793;

  // convert degrees to radians
  static double radians(double x);

  // convert radians to degrees
  static double degrees(double x);

  // a pixel class that captures x and y coordinates
  class pixel {
   public:
    unsigned int x;
    unsigned int y;
    pixel(unsigned int a, unsigned int b) :
     x(a), y(b) {
    }
  };

  // a tile in the geoid
  class tile {
   public:
    unsigned int x;
    unsigned int y;
    tile(unsigned int a, unsigned int b) :
     x(a), y(b) {
    }
  };

  // convert a gps location to quad coordinate
  static std::string positiontoquad(const geoposition& pos, int levelofdetail);

  // arbitrary constant use for wgs84 via
  // http://en.wikipedia.org/wiki/world_geodetic_system
  // http://mathforum.org/library/drmath/view/51832.html
  // http://msdn.microsoft.com/en-us/library/bb259689.aspx
  // http://www.tuicool.com/articles/nbre73
  //
  const int detail = 23;
  static constexpr double earthradius = 6378137;
  static constexpr double minlatitude = -85.05112878;
  static constexpr double maxlatitude = 85.05112878;
  static constexpr double minlongitude = -180;
  static constexpr double maxlongitude = 180;

  // clips a number to the specified minimum and maximum values.
  static double clip(double n, double minvalue, double maxvalue) {
    return fmin(fmax(n, minvalue), maxvalue);
  }

  // determines the map width and height (in pixels) at a specified level
  // of detail, from 1 (lowest detail) to 23 (highest detail).
  // returns the map width and height in pixels.
  static unsigned int mapsize(int levelofdetail) {
    return (unsigned int)(256 << levelofdetail);
  }

  // determines the ground resolution (in meters per pixel) at a specified
  // latitude and level of detail.
  // latitude (in degrees) at which to measure the ground resolution.
  // level of detail, from 1 (lowest detail) to 23 (highest detail).
  // returns the ground resolution, in meters per pixel.
  static double groundresolution(double latitude, int levelofdetail);

  // converts a point from latitude/longitude wgs-84 coordinates (in degrees)
  // into pixel xy coordinates at a specified level of detail.
  static pixel positiontopixel(const geoposition& pos, int levelofdetail);

  static geoposition pixeltoposition(const pixel& pixel, int levelofdetail);

  // converts a pixel to a tile
  static tile pixeltotile(const pixel& pixel);

  static pixel tiletopixel(const tile& tile);

  // convert a tile to a quadkey
  static std::string tiletoquadkey(const tile& tile, int levelofdetail);

  // convert a quadkey to a tile and its level of detail
  static void quadkeytotile(std::string quadkey, tile* tile,
                            int *levelofdetail);

  // return the distance between two positions on the earth
  static double distance(double lat1, double lon1,
                         double lat2, double lon2);
  static geoposition displacelatlon(double lat, double lon,
                                    double deltay, double deltax);

  //
  // returns the top left position after applying the delta to
  // the specified position
  //
  static geoposition boundingtopleft(const geoposition& in, double radius) {
    return displacelatlon(in.latitude, in.longitude, -radius, -radius);
  }

  //
  // returns the bottom right position after applying the delta to
  // the specified position
  static geoposition boundingbottomright(const geoposition& in,
                                         double radius) {
    return displacelatlon(in.latitude, in.longitude, radius, radius);
  }

  //
  // get all quadkeys within a radius of a specified position
  //
  status searchquadids(const geoposition& position,
                       double radius,
                       std::vector<std::string>* quadkeys);

  // splits a string into its components
  static void stringsplit(std::vector<std::string>* tokens,
                          const std::string &text,
                          char sep);

  //
  // create keys for accessing rocksdb table(s)
  //
  static std::string makekey1(const geoposition& pos,
                              slice id,
                              std::string quadkey);
  static std::string makekey2(slice id);
  static std::string makekey1prefix(std::string quadkey,
                                    slice id);
  static std::string makequadkeyprefix(std::string quadkey);
};

}  // namespace rocksdb

#endif  // rocksdb_lite
