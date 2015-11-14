// copyright 2013 facebook
/**
 * a (persistent) redis api built using the rocksdb backend.
 * implements redis lists as described on: http://redis.io/commands#list
 *
 * @throws all functions may throw a redislistexception on error/corruption.
 *
 * @notes internally, the set of lists is stored in a rocksdb database,
 *        mapping keys to values. each "value" is the list itself, storing
 *        some kind of internal representation of the data. all the
 *        representation details are handled by the redislistiterator class.
 *        the present file should be oblivious to the representation details,
 *        handling only the client (redis) api, and the calls to rocksdb.
 *
 * @todo  presently, all operations take at least o(nv) time where
 *        n is the number of elements in the list, and v is the average
 *        number of bytes per value in the list. so maybe, with merge operator
 *        we can improve this to an optimal o(v) amortized time, since we
 *        wouldn't have to read and re-write the entire list.
 *
 * @author deon nicholas (dnicholas@fb.com)
 */

#ifndef rocksdb_lite
#include "redis_lists.h"

#include <iostream>
#include <memory>
#include <cmath>

#include "rocksdb/slice.h"
#include "util/coding.h"

namespace rocksdb
{

/// constructors

redislists::redislists(const std::string& db_path,
                       options options, bool destructive)
    : put_option_(),
      get_option_() {

  // store the name of the database
  db_name_ = db_path;

  // if destructive, destroy the db before re-opening it.
  if (destructive) {
    destroydb(db_name_, options());
  }

  // now open and deal with the db
  db* db;
  status s = db::open(options, db_name_, &db);
  if (!s.ok()) {
    std::cerr << "error " << s.tostring() << std::endl;
    assert(false);
  }

  db_ = std::unique_ptr<db>(db);
}


/// accessors

// number of elements in the list associated with key
//   : throws redislistexception
int redislists::length(const std::string& key) {
  // extract the string data representing the list.
  std::string data;
  db_->get(get_option_, key, &data);

  // return the length
  redislistiterator it(data);
  return it.length();
}

// get the element at the specified index in the (list: key)
// returns <empty> ("") on out-of-bounds
//   : throws redislistexception
bool redislists::index(const std::string& key, int32_t index,
                       std::string* result) {
  // extract the string data representing the list.
  std::string data;
  db_->get(get_option_, key, &data);

  // handle redis negative indices (from the end); fast iff length() takes o(1)
  if (index < 0) {
    index = length(key) - (-index);  //replace (-i) with (n-i).
  }

  // iterate through the list until the desired index is found.
  int curindex = 0;
  redislistiterator it(data);
  while(curindex < index && !it.done()) {
    ++curindex;
    it.skip();
  }

  // if we actually found the index
  if (curindex == index && !it.done()) {
    slice elem;
    it.getcurrent(&elem);
    if (result != null) {
      *result = elem.tostring();
    }

    return true;
  } else {
    return false;
  }
}

// return a truncated version of the list.
// first, negative values for first/last are interpreted as "end of list".
// so, if first == -1, then it is re-set to index: (length(key) - 1)
// then, return exactly those indices i such that first <= i <= last.
//   : throws redislistexception
std::vector<std::string> redislists::range(const std::string& key,
                                           int32_t first, int32_t last) {
  // extract the string data representing the list.
  std::string data;
  db_->get(get_option_, key, &data);

  // handle negative bounds (-1 means last element, etc.)
  int listlen = length(key);
  if (first < 0) {
    first = listlen - (-first);           // replace (-x) with (n-x)
  }
  if (last < 0) {
    last = listlen - (-last);
  }

  // verify bounds (and truncate the range so that it is valid)
  first = std::max(first, 0);
  last = std::min(last, listlen-1);
  int len = std::max(last-first+1, 0);

  // initialize the resulting list
  std::vector<std::string> result(len);

  // traverse the list and update the vector
  int curidx = 0;
  slice elem;
  for (redislistiterator it(data); !it.done() && curidx<=last; it.skip()) {
    if (first <= curidx && curidx <= last) {
      it.getcurrent(&elem);
      result[curidx-first].assign(elem.data(),elem.size());
    }

    ++curidx;
  }

  // return the result. might be empty
  return result;
}

// print the (list: key) out to stdout. for debugging mostly. public for now.
void redislists::print(const std::string& key) {
  // extract the string data representing the list.
  std::string data;
  db_->get(get_option_, key, &data);

  // iterate through the list and print the items
  slice elem;
  for (redislistiterator it(data); !it.done(); it.skip()) {
    it.getcurrent(&elem);
    std::cout << "item " << elem.tostring() << std::endl;
  }

  //now print the byte data
  redislistiterator it(data);
  std::cout << "==printing data==" << std::endl;
  std::cout << data.size() << std::endl;
  std::cout << it.size() << " " << it.length() << std::endl;
  slice result = it.writeresult();
  std::cout << result.data() << std::endl;
  if (true) {
    std::cout << "size: " << result.size() << std::endl;
    const char* val = result.data();
    for(int i=0; i<(int)result.size(); ++i) {
      std::cout << (int)val[i] << " " << (val[i]>=32?val[i]:' ') << std::endl;
    }
    std::cout << std::endl;
  }
}

/// insert/update functions
/// note: the "real" insert function is private. see below.

// insertbefore and insertafter are simply wrappers around the insert function.
int redislists::insertbefore(const std::string& key, const std::string& pivot,
                             const std::string& value) {
  return insert(key, pivot, value, false);
}

int redislists::insertafter(const std::string& key, const std::string& pivot,
                            const std::string& value) {
  return insert(key, pivot, value, true);
}

// prepend value onto beginning of (list: key)
//   : throws redislistexception
int redislists::pushleft(const std::string& key, const std::string& value) {
  // get the original list data
  std::string data;
  db_->get(get_option_, key, &data);

  // construct the result
  redislistiterator it(data);
  it.reserve(it.size() + it.sizeof(value));
  it.insertelement(value);

  // push the data back to the db and return the length
  db_->put(put_option_, key, it.writeresult());
  return it.length();
}

// append value onto end of (list: key)
// todo: make this o(1) time. might require mergeoperator.
//   : throws redislistexception
int redislists::pushright(const std::string& key, const std::string& value) {
  // get the original list data
  std::string data;
  db_->get(get_option_, key, &data);

  // create an iterator to the data and seek to the end.
  redislistiterator it(data);
  it.reserve(it.size() + it.sizeof(value));
  while (!it.done()) {
    it.push();    // write each element as we go
  }

  // insert the new element at the current position (the end)
  it.insertelement(value);

  // push it back to the db, and return length
  db_->put(put_option_, key, it.writeresult());
  return it.length();
}

// set (list: key)[idx] = val. return true on success, false on fail.
//   : throws redislistexception
bool redislists::set(const std::string& key, int32_t index,
                     const std::string& value) {
  // get the original list data
  std::string data;
  db_->get(get_option_, key, &data);

  // handle negative index for redis (meaning -index from end of list)
  if (index < 0) {
    index = length(key) - (-index);
  }

  // iterate through the list until we find the element we want
  int curindex = 0;
  redislistiterator it(data);
  it.reserve(it.size() + it.sizeof(value));  // over-estimate is fine
  while(curindex < index && !it.done()) {
    it.push();
    ++curindex;
  }

  // if not found, return false (this occurs when index was invalid)
  if (it.done() || curindex != index) {
    return false;
  }

  // write the new element value, and drop the previous element value
  it.insertelement(value);
  it.skip();

  // write the data to the database
  // check status, since it needs to return true/false guarantee
  status s = db_->put(put_option_, key, it.writeresult());

  // success
  return s.ok();
}

/// delete / remove / pop functions

// trim (list: key) so that it will only contain the indices from start..stop
//  invalid indices will not generate an error, just empty,
//  or the portion of the list that fits in this interval
//   : throws redislistexception
bool redislists::trim(const std::string& key, int32_t start, int32_t stop) {
  // get the original list data
  std::string data;
  db_->get(get_option_, key, &data);

  // handle negative indices in redis
  int listlen = length(key);
  if (start < 0) {
    start = listlen - (-start);
  }
  if (stop < 0) {
    stop = listlen - (-stop);
  }

  // truncate bounds to only fit in the list
  start = std::max(start, 0);
  stop = std::min(stop, listlen-1);

  // construct an iterator for the list. drop all undesired elements.
  int curindex = 0;
  redislistiterator it(data);
  it.reserve(it.size());          // over-estimate
  while(!it.done()) {
    // if not within the range, just skip the item (drop it).
    // otherwise, continue as usual.
    if (start <= curindex && curindex <= stop) {
      it.push();
    } else {
      it.skip();
    }

    // increment the current index
    ++curindex;
  }

  // write the (possibly empty) result to the database
  status s = db_->put(put_option_, key, it.writeresult());

  // return true as long as the write succeeded
  return s.ok();
}

// return and remove the first element in the list (or "" if empty)
//   : throws redislistexception
bool redislists::popleft(const std::string& key, std::string* result) {
  // get the original list data
  std::string data;
  db_->get(get_option_, key, &data);

  // point to first element in the list (if it exists), and get its value/size
  redislistiterator it(data);
  if (it.length() > 0) {            // proceed only if list is non-empty
    slice elem;
    it.getcurrent(&elem);           // store the value of the first element
    it.reserve(it.size() - it.sizeof(elem));
    it.skip();                      // drop the first item and move to next

    // update the db
    db_->put(put_option_, key, it.writeresult());

    // return the value
    if (result != null) {
      *result = elem.tostring();
    }
    return true;
  } else {
    return false;
  }
}

// remove and return the last element in the list (or "" if empty)
// todo: make this o(1). might require mergeoperator.
//   : throws redislistexception
bool redislists::popright(const std::string& key, std::string* result) {
  // extract the original list data
  std::string data;
  db_->get(get_option_, key, &data);

  // construct an iterator to the data and move to last element
  redislistiterator it(data);
  it.reserve(it.size());
  int len = it.length();
  int curindex = 0;
  while(curindex < (len-1) && !it.done()) {
    it.push();
    ++curindex;
  }

  // extract and drop/skip the last element
  if (curindex == len-1) {
    assert(!it.done());         // sanity check. should not have ended here.

    // extract and pop the element
    slice elem;
    it.getcurrent(&elem);       // save value of element.
    it.skip();                  // skip the element

    // write the result to the database
    db_->put(put_option_, key, it.writeresult());

    // return the value
    if (result != null) {
      *result = elem.tostring();
    }
    return true;
  } else {
    // must have been an empty list
    assert(it.done() && len==0 && curindex == 0);
    return false;
  }
}

// remove the (first or last) "num" occurrences of value in (list: key)
//   : throws redislistexception
int redislists::remove(const std::string& key, int32_t num,
                       const std::string& value) {
  // negative num ==> removelast; positive num ==> remove first
  if (num < 0) {
    return removelast(key, -num, value);
  } else if (num > 0) {
    return removefirst(key, num, value);
  } else {
    return removefirst(key, length(key), value);
  }
}

// remove the first "num" occurrences of value in (list: key).
//   : throws redislistexception
int redislists::removefirst(const std::string& key, int32_t num,
                            const std::string& value) {
  // ensure that the number is positive
  assert(num >= 0);

  // extract the original list data
  std::string data;
  db_->get(get_option_, key, &data);

  // traverse the list, appending all but the desired occurrences of value
  int numskipped = 0;         // keep track of the number of times value is seen
  slice elem;
  redislistiterator it(data);
  it.reserve(it.size());
  while (!it.done()) {
    it.getcurrent(&elem);

    if (elem == value && numskipped < num) {
      // drop this item if desired
      it.skip();
      ++numskipped;
    } else {
      // otherwise keep the item and proceed as normal
      it.push();
    }
  }

  // put the result back to the database
  db_->put(put_option_, key, it.writeresult());

  // return the number of elements removed
  return numskipped;
}


// remove the last "num" occurrences of value in (list: key).
// todo: i traverse the list 2x. make faster. might require mergeoperator.
//   : throws redislistexception
int redislists::removelast(const std::string& key, int32_t num,
                           const std::string& value) {
  // ensure that the number is positive
  assert(num >= 0);

  // extract the original list data
  std::string data;
  db_->get(get_option_, key, &data);

  // temporary variable to hold the "current element" in the blocks below
  slice elem;

  // count the total number of occurrences of value
  int totaloccs = 0;
  for (redislistiterator it(data); !it.done(); it.skip()) {
    it.getcurrent(&elem);
    if (elem == value) {
      ++totaloccs;
    }
  }

  // construct an iterator to the data. reserve enough space for the result.
  redislistiterator it(data);
  int bytesremoved = std::min(num,totaloccs)*it.sizeof(value);
  it.reserve(it.size() - bytesremoved);

  // traverse the list, appending all but the desired occurrences of value.
  // note: "drop the last k occurrences" is equivalent to
  //  "keep only the first n-k occurrences", where n is total occurrences.
  int numkept = 0;          // keep track of the number of times value is kept
  while(!it.done()) {
    it.getcurrent(&elem);

    // if we are within the deletion range and equal to value, drop it.
    // otherwise, append/keep/push it.
    if (elem == value) {
      if (numkept < totaloccs - num) {
        it.push();
        ++numkept;
      } else {
        it.skip();
      }
    } else {
      // always append the others
      it.push();
    }
  }

  // put the result back to the database
  db_->put(put_option_, key, it.writeresult());

  // return the number of elements removed
  return totaloccs - numkept;
}

/// private functions

// insert element value into (list: key), right before/after
//  the first occurrence of pivot
//   : throws redislistexception
int redislists::insert(const std::string& key, const std::string& pivot,
                       const std::string& value, bool insert_after) {
  // get the original list data
  std::string data;
  db_->get(get_option_, key, &data);

  // construct an iterator to the data and reserve enough space for result.
  redislistiterator it(data);
  it.reserve(it.size() + it.sizeof(value));

  // iterate through the list until we find the element we want
  slice elem;
  bool found = false;
  while(!it.done() && !found) {
    it.getcurrent(&elem);

    // when we find the element, insert the element and mark found
    if (elem == pivot) {                // found it!
      found = true;
      if (insert_after == true) {       // skip one more, if inserting after it
        it.push();
      }
      it.insertelement(value);
    } else {
      it.push();
    }

  }

  // put the data (string) into the database
  if (found) {
    db_->put(put_option_, key, it.writeresult());
  }

  // returns the new (possibly unchanged) length of the list
  return it.length();
}

}  // namespace rocksdb
#endif  // rocksdb_lite
