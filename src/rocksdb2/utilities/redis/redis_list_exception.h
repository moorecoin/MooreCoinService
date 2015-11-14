/**
 * a simple structure for exceptions in redislists.
 *
 * @author deon nicholas (dnicholas@fb.com)
 * copyright 2013 facebook
 */

#ifndef rocksdb_lite
#pragma once
#include <exception>

namespace rocksdb {

class redislistexception: public std::exception {
 public:
  const char* what() const throw() {
    return "invalid operation or corrupt data in redis list.";
  }
};

} // namespace rocksdb
#endif
