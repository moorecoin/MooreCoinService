//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
#ifndef rocksdb_lite

#include "utilities/geodb/geodb_impl.h"

#define __stdc_format_macros

#include <vector>
#include <map>
#include <string>
#include <limits>
#include "db/filename.h"
#include "util/coding.h"

//
// there are two types of keys. the first type of key-values
// maps a geo location to the set of object ids and their values.
// table 1
//   key     : p + : + $quadkey + : + $id +
//             : + $latitude + : + $longitude
//   value  :  value of the object
// this table can be used to find all objects that reside near
// a specified geolocation.
//
// table 2
//   key  : 'k' + : + $id
//   value:  $quadkey

namespace rocksdb {

geodbimpl::geodbimpl(db* db, const geodboptions& options) :
  geodb(db, options), db_(db), options_(options) {
}

geodbimpl::~geodbimpl() {
}

status geodbimpl::insert(const geoobject& obj) {
  writebatch batch;

  // it is possible that this id is already associated with
  // with a different position. we first have to remove that
  // association before we can insert the new one.

  // remove existing object, if it exists
  geoobject old;
  status status = getbyid(obj.id, &old);
  if (status.ok()) {
    assert(obj.id.compare(old.id) == 0);
    std::string quadkey = positiontoquad(old.position, detail);
    std::string key1 = makekey1(old.position, old.id, quadkey);
    std::string key2 = makekey2(old.id);
    batch.delete(slice(key1));
    batch.delete(slice(key2));
  } else if (status.isnotfound()) {
    // what if another thread is trying to insert the same id concurrently?
  } else {
    return status;
  }

  // insert new object
  std::string quadkey = positiontoquad(obj.position, detail);
  std::string key1 = makekey1(obj.position, obj.id, quadkey);
  std::string key2 = makekey2(obj.id);
  batch.put(slice(key1), slice(obj.value));
  batch.put(slice(key2), slice(quadkey));
  return db_->write(woptions_, &batch);
}

status geodbimpl::getbyposition(const geoposition& pos,
                                const slice& id,
                                std::string* value) {
  std::string quadkey = positiontoquad(pos, detail);
  std::string key1 = makekey1(pos, id, quadkey);
  return db_->get(roptions_, slice(key1), value);
}

status geodbimpl::getbyid(const slice& id, geoobject* object) {
  status status;
  slice quadkey;

  // create an iterator so that we can get a consistent picture
  // of the database.
  iterator* iter = db_->newiterator(roptions_);

  // create key for table2
  std::string kt = makekey2(id);
  slice key2(kt);

  iter->seek(key2);
  if (iter->valid() && iter->status().ok()) {
    if (iter->key().compare(key2) == 0) {
      quadkey = iter->value();
    }
  }
  if (quadkey.size() == 0) {
    delete iter;
    return status::notfound(key2);
  }

  //
  // seek to the quadkey + id prefix
  //
  std::string prefix = makekey1prefix(quadkey.tostring(), id);
  iter->seek(slice(prefix));
  assert(iter->valid());
  if (!iter->valid() || !iter->status().ok()) {
    delete iter;
    return status::notfound();
  }

  // split the key into p + quadkey + id + lat + lon
  std::vector<std::string> parts;
  slice key = iter->key();
  stringsplit(&parts, key.tostring(), ':');
  assert(parts.size() == 5);
  assert(parts[0] == "p");
  assert(parts[1] == quadkey);
  assert(parts[2] == id);

  // fill up output parameters
  object->position.latitude = atof(parts[3].c_str());
  object->position.longitude = atof(parts[4].c_str());
  object->id = id.tostring();  // this is redundant
  object->value = iter->value().tostring();
  delete iter;
  return status::ok();
}


status geodbimpl::remove(const slice& id) {
  // read the object from the database
  geoobject obj;
  status status = getbyid(id, &obj);
  if (!status.ok()) {
    return status;
  }

  // remove the object by atomically deleting it from both tables
  std::string quadkey = positiontoquad(obj.position, detail);
  std::string key1 = makekey1(obj.position, obj.id, quadkey);
  std::string key2 = makekey2(obj.id);
  writebatch batch;
  batch.delete(slice(key1));
  batch.delete(slice(key2));
  return db_->write(woptions_, &batch);
}

status geodbimpl::searchradial(const geoposition& pos,
  double radius,
  std::vector<geoobject>* values,
  int number_of_values) {
  // gather all bounding quadkeys
  std::vector<std::string> qids;
  status s = searchquadids(pos, radius, &qids);
  if (!s.ok()) {
    return s;
  }

  // create an iterator
  iterator* iter = db_->newiterator(readoptions());

  // process each prospective quadkey
  for (std::string qid : qids) {
    // the user is interested in only these many objects.
    if (number_of_values == 0) {
      break;
    }

    // convert quadkey to db key prefix
    std::string dbkey = makequadkeyprefix(qid);

    for (iter->seek(dbkey);
         number_of_values > 0 && iter->valid() && iter->status().ok();
         iter->next()) {
      // split the key into p + quadkey + id + lat + lon
      std::vector<std::string> parts;
      slice key = iter->key();
      stringsplit(&parts, key.tostring(), ':');
      assert(parts.size() == 5);
      assert(parts[0] == "p");
      std::string* quadkey = &parts[1];

      // if the key we are looking for is a prefix of the key
      // we found from the database, then this is one of the keys
      // we are looking for.
      auto res = std::mismatch(qid.begin(), qid.end(), quadkey->begin());
      if (res.first == qid.end()) {
        geoposition pos(atof(parts[3].c_str()), atof(parts[4].c_str()));
        geoobject obj(pos, parts[4], iter->value().tostring());
        values->push_back(obj);
        number_of_values--;
      } else {
        break;
      }
    }
  }
  delete iter;
  return status::ok();
}

std::string geodbimpl::makekey1(const geoposition& pos, slice id,
                                std::string quadkey) {
  std::string lat = std::to_string(pos.latitude);
  std::string lon = std::to_string(pos.longitude);
  std::string key = "p:";
  key.reserve(5 + quadkey.size() + id.size() + lat.size() + lon.size());
  key.append(quadkey);
  key.append(":");
  key.append(id.tostring());
  key.append(":");
  key.append(lat);
  key.append(":");
  key.append(lon);
  return key;
}

std::string geodbimpl::makekey2(slice id) {
  std::string key = "k:";
  key.append(id.tostring());
  return key;
}

std::string geodbimpl::makekey1prefix(std::string quadkey,
                                      slice id) {
  std::string key = "p:";
  key.reserve(3 + quadkey.size() + id.size());
  key.append(quadkey);
  key.append(":");
  key.append(id.tostring());
  return key;
}

std::string geodbimpl::makequadkeyprefix(std::string quadkey) {
  std::string key = "p:";
  key.append(quadkey);
  return key;
}

void geodbimpl::stringsplit(std::vector<std::string>* tokens,
                            const std::string &text, char sep) {
  std::size_t start = 0, end = 0;
  while ((end = text.find(sep, start)) != std::string::npos) {
    tokens->push_back(text.substr(start, end - start));
    start = end + 1;
  }
  tokens->push_back(text.substr(start));
}

// convert degrees to radians
double geodbimpl::radians(double x) {
  return (x * pi) / 180;
}

// convert radians to degrees
double geodbimpl::degrees(double x) {
  return (x * 180) / pi;
}

// convert a gps location to quad coordinate
std::string geodbimpl::positiontoquad(const geoposition& pos,
                                      int levelofdetail) {
  pixel p = positiontopixel(pos, levelofdetail);
  tile tile = pixeltotile(p);
  return tiletoquadkey(tile, levelofdetail);
}

geoposition geodbimpl::displacelatlon(double lat, double lon,
                                      double deltay, double deltax) {
  double dlat = deltay / earthradius;
  double dlon = deltax / (earthradius * cos(radians(lat)));
  return geoposition(lat + degrees(dlat),
                     lon + degrees(dlon));
}

//
// return the distance between two positions on the earth
//
double geodbimpl::distance(double lat1, double lon1,
                           double lat2, double lon2) {
  double lon = radians(lon2 - lon1);
  double lat = radians(lat2 - lat1);

  double a = (sin(lat / 2) * sin(lat / 2)) +
              cos(radians(lat1)) * cos(radians(lat2)) *
              (sin(lon / 2) * sin(lon / 2));
  double angle = 2 * atan2(sqrt(a), sqrt(1 - a));
  return angle * earthradius;
}

//
// returns all the quadkeys inside the search range
//
status geodbimpl::searchquadids(const geoposition& position,
                                double radius,
                                std::vector<std::string>* quadkeys) {
  // get the outline of the search square
  geoposition topleftpos = boundingtopleft(position, radius);
  geoposition bottomrightpos = boundingbottomright(position, radius);

  pixel topleft =  positiontopixel(topleftpos, detail);
  pixel bottomright =  positiontopixel(bottomrightpos, detail);

  // how many level of details to look for
  int numberoftilesatmaxdepth = floor((bottomright.x - topleft.x) / 256);
  int zoomlevelstorise = floor(::log(numberoftilesatmaxdepth) / ::log(2));
  zoomlevelstorise++;
  int levels = std::max(0, detail - zoomlevelstorise);

  quadkeys->push_back(positiontoquad(geoposition(topleftpos.latitude,
                                                 topleftpos.longitude),
                                     levels));
  quadkeys->push_back(positiontoquad(geoposition(topleftpos.latitude,
                                                 bottomrightpos.longitude),
                                     levels));
  quadkeys->push_back(positiontoquad(geoposition(bottomrightpos.latitude,
                                                 topleftpos.longitude),
                                     levels));
  quadkeys->push_back(positiontoquad(geoposition(bottomrightpos.latitude,
                                                 bottomrightpos.longitude),
                                     levels));
  return status::ok();
}

// determines the ground resolution (in meters per pixel) at a specified
// latitude and level of detail.
// latitude (in degrees) at which to measure the ground resolution.
// level of detail, from 1 (lowest detail) to 23 (highest detail).
// returns the ground resolution, in meters per pixel.
double geodbimpl::groundresolution(double latitude, int levelofdetail) {
  latitude = clip(latitude, minlatitude, maxlatitude);
  return cos(latitude * pi / 180) * 2 * pi * earthradius /
         mapsize(levelofdetail);
}

// converts a point from latitude/longitude wgs-84 coordinates (in degrees)
// into pixel xy coordinates at a specified level of detail.
geodbimpl::pixel geodbimpl::positiontopixel(const geoposition& pos,
                                            int levelofdetail) {
  double latitude = clip(pos.latitude, minlatitude, maxlatitude);
  double x = (pos.longitude + 180) / 360;
  double sinlatitude = sin(latitude * pi / 180);
  double y = 0.5 - ::log((1 + sinlatitude) / (1 - sinlatitude)) / (4 * pi);
  double mapsize = mapsize(levelofdetail);
  double x = floor(clip(x * mapsize + 0.5, 0, mapsize - 1));
  double y = floor(clip(y * mapsize + 0.5, 0, mapsize - 1));
  return pixel((unsigned int)x, (unsigned int)y);
}

geoposition geodbimpl::pixeltoposition(const pixel& pixel, int levelofdetail) {
  double mapsize = mapsize(levelofdetail);
  double x = (clip(pixel.x, 0, mapsize - 1) / mapsize) - 0.5;
  double y = 0.5 - (clip(pixel.y, 0, mapsize - 1) / mapsize);
  double latitude = 90 - 360 * atan(exp(-y * 2 * pi)) / pi;
  double longitude = 360 * x;
  return geoposition(latitude, longitude);
}

// converts a pixel to a tile
geodbimpl::tile geodbimpl::pixeltotile(const pixel& pixel) {
  unsigned int tilex = floor(pixel.x / 256);
  unsigned int tiley = floor(pixel.y / 256);
  return tile(tilex, tiley);
}

geodbimpl::pixel geodbimpl::tiletopixel(const tile& tile) {
  unsigned int pixelx = tile.x * 256;
  unsigned int pixely = tile.y * 256;
  return pixel(pixelx, pixely);
}

// convert a tile to a quadkey
std::string geodbimpl::tiletoquadkey(const tile& tile, int levelofdetail) {
  std::stringstream quadkey;
  for (int i = levelofdetail; i > 0; i--) {
    char digit = '0';
    int mask = 1 << (i - 1);
    if ((tile.x & mask) != 0) {
      digit++;
    }
    if ((tile.y & mask) != 0) {
      digit++;
      digit++;
    }
    quadkey << digit;
  }
  return quadkey.str();
}

//
// convert a quadkey to a tile and its level of detail
//
void geodbimpl::quadkeytotile(std::string quadkey, tile* tile,
                                     int *levelofdetail) {
  tile->x = tile->y = 0;
  *levelofdetail = quadkey.size();
  const char* key = reinterpret_cast<const char *>(quadkey.c_str());
  for (int i = *levelofdetail; i > 0; i--) {
    int mask = 1 << (i - 1);
    switch (key[*levelofdetail - i]) {
      case '0':
        break;

      case '1':
        tile->x |= mask;
        break;

      case '2':
        tile->y |= mask;
        break;

      case '3':
        tile->x |= mask;
        tile->y |= mask;
        break;

      default:
        std::stringstream msg;
        msg << quadkey;
        msg << " invalid quadkey.";
        throw std::runtime_error(msg.str());
    }
  }
}
}  // namespace rocksdb

#endif  // rocksdb_lite
