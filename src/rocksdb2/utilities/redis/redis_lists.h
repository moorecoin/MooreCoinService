/**
 * a (persistent) redis api built using the rocksdb backend.
 * implements redis lists as described on: http://redis.io/commands#list
 *
 * @throws all functions may throw a redislistexception
 *
 * @author deon nicholas (dnicholas@fb.com)
 * copyright 2013 facebook
 */

#ifndef rocksdb_lite
#pragma once

#include <string>
#include "rocksdb/db.h"
#include "redis_list_iterator.h"
#include "redis_list_exception.h"

namespace rocksdb {

/// the redis functionality (see http://redis.io/commands#list)
/// all functions may throw a redislistexception
class redislists {
 public: // constructors / destructors
  /// construct a new redislists database, with name/path of db.
  /// will clear the database on open iff destructive is true (default false).
  /// otherwise, it will restore saved changes.
  /// may throw redislistexception
  redislists(const std::string& db_path,
             options options, bool destructive = false);

 public:  // accessors
  /// the number of items in (list: key)
  int length(const std::string& key);

  /// search the list for the (index)'th item (0-based) in (list:key)
  /// a negative index indicates: "from end-of-list"
  /// if index is within range: return true, and return the value in *result.
  /// if (index < -length or index>=length), then index is out of range:
  ///   return false (and *result is left unchanged)
  /// may throw redislistexception
  bool index(const std::string& key, int32_t index,
             std::string* result);

  /// return (list: key)[first..last] (inclusive)
  /// may throw redislistexception
  std::vector<std::string> range(const std::string& key,
                                 int32_t first, int32_t last);

  /// prints the entire (list: key), for debugging.
  void print(const std::string& key);

 public: // insert/update
  /// insert value before/after pivot in (list: key). return the length.
  /// may throw redislistexception
  int insertbefore(const std::string& key, const std::string& pivot,
                   const std::string& value);
  int insertafter(const std::string& key, const std::string& pivot,
                  const std::string& value);

  /// push / insert value at beginning/end of the list. return the length.
  /// may throw redislistexception
  int pushleft(const std::string& key, const std::string& value);
  int pushright(const std::string& key, const std::string& value);

  /// set (list: key)[idx] = val. return true on success, false on fail
  /// may throw redislistexception
  bool set(const std::string& key, int32_t index, const std::string& value);

 public: // delete / remove / pop / trim
  /// trim (list: key) so that it will only contain the indices from start..stop
  /// returns true on success
  /// may throw redislistexception
  bool trim(const std::string& key, int32_t start, int32_t stop);

  /// if list is empty, return false and leave *result unchanged.
  /// else, remove the first/last elem, store it in *result, and return true
  bool popleft(const std::string& key, std::string* result);  // first
  bool popright(const std::string& key, std::string* result); // last

  /// remove the first (or last) num occurrences of value from the list (key)
  /// return the number of elements removed.
  /// may throw redislistexception
  int remove(const std::string& key, int32_t num,
             const std::string& value);
  int removefirst(const std::string& key, int32_t num,
                  const std::string& value);
  int removelast(const std::string& key, int32_t num,
                 const std::string& value);

 private: // private functions
  /// calls insertbefore or insertafter
  int insert(const std::string& key, const std::string& pivot,
             const std::string& value, bool insert_after);
 private:
  std::string db_name_;       // the actual database name/path
  writeoptions put_option_;
  readoptions get_option_;

  /// the backend rocksdb database.
  /// map : key --> list
  ///       where a list is a sequence of elements
  ///       and an element is a 4-byte integer (n), followed by n bytes of data
  std::unique_ptr<db> db_;
};

} // namespace rocksdb
#endif  // rocksdb_lite
