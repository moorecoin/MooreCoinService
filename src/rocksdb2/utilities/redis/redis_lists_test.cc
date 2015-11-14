//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
/**
 * a test harness for the redis api built on rocksdb.
 *
 * usage: build with: "make redis_test" (in rocksdb directory).
 *        run unit tests with: "./redis_test"
 *        manual/interactive user testing: "./redis_test -m"
 *        manual user testing + restart database: "./redis_test -m -d"
 *
 * todo:  add large random test cases to verify efficiency and scalability
 *
 * @author deon nicholas (dnicholas@fb.com)
 */


#include <iostream>
#include <cctype>

#include "redis_lists.h"
#include "util/testharness.h"
#include "util/random.h"

using namespace rocksdb;
using namespace std;

namespace rocksdb {

class redisliststest {
 public:
  static const string kdefaultdbname;
  static options options;

  redisliststest() {
    options.create_if_missing = true;
  }
};

const string redisliststest::kdefaultdbname = "/tmp/redisdefaultdb/";
options redisliststest::options = options();

// operator== and operator<< are defined below for vectors (lists)
// needed for assert_eq

namespace {
void assertlisteq(const std::vector<std::string>& result,
                  const std::vector<std::string>& expected_result) {
  assert_eq(result.size(), expected_result.size());
  for (size_t i = 0; i < result.size(); ++i) {
    assert_eq(result[i], expected_result[i]);
  }
}
}  // namespace

// pushright, length, index, range
test(redisliststest, simpletest) {
  redislists redis(kdefaultdbname, options, true);   // destructive

  string tempv; // used below for all index(), popright(), popleft()

  // simple pushright (should return the new length each time)
  assert_eq(redis.pushright("k1", "v1"), 1);
  assert_eq(redis.pushright("k1", "v2"), 2);
  assert_eq(redis.pushright("k1", "v3"), 3);

  // check length and index() functions
  assert_eq(redis.length("k1"), 3);        // check length
  assert_true(redis.index("k1", 0, &tempv));
  assert_eq(tempv, "v1");   // check valid indices
  assert_true(redis.index("k1", 1, &tempv));
  assert_eq(tempv, "v2");
  assert_true(redis.index("k1", 2, &tempv));
  assert_eq(tempv, "v3");

  // check range function and vectors
  std::vector<std::string> result = redis.range("k1", 0, 2);   // get the list
  std::vector<std::string> expected_result(3);
  expected_result[0] = "v1";
  expected_result[1] = "v2";
  expected_result[2] = "v3";
  assertlisteq(result, expected_result);
}

// pushleft, length, index, range
test(redisliststest, simpletest2) {
  redislists redis(kdefaultdbname, options, true);   // destructive

  string tempv; // used below for all index(), popright(), popleft()

  // simple pushright
  assert_eq(redis.pushleft("k1", "v3"), 1);
  assert_eq(redis.pushleft("k1", "v2"), 2);
  assert_eq(redis.pushleft("k1", "v1"), 3);

  // check length and index() functions
  assert_eq(redis.length("k1"), 3);        // check length
  assert_true(redis.index("k1", 0, &tempv));
  assert_eq(tempv, "v1");   // check valid indices
  assert_true(redis.index("k1", 1, &tempv));
  assert_eq(tempv, "v2");
  assert_true(redis.index("k1", 2, &tempv));
  assert_eq(tempv, "v3");

  // check range function and vectors
  std::vector<std::string> result = redis.range("k1", 0, 2);   // get the list
  std::vector<std::string> expected_result(3);
  expected_result[0] = "v1";
  expected_result[1] = "v2";
  expected_result[2] = "v3";
  assertlisteq(result, expected_result);
}

// exhaustive test of the index() function
test(redisliststest, indextest) {
  redislists redis(kdefaultdbname, options, true);   // destructive

  string tempv; // used below for all index(), popright(), popleft()

  // empty index check (return empty and should not crash or edit tempv)
  tempv = "yo";
  assert_true(!redis.index("k1", 0, &tempv));
  assert_eq(tempv, "yo");
  assert_true(!redis.index("fda", 3, &tempv));
  assert_eq(tempv, "yo");
  assert_true(!redis.index("random", -12391, &tempv));
  assert_eq(tempv, "yo");

  // simple pushes (will yield: [v6, v4, v4, v1, v2, v3]
  redis.pushright("k1", "v1");
  redis.pushright("k1", "v2");
  redis.pushright("k1", "v3");
  redis.pushleft("k1", "v4");
  redis.pushleft("k1", "v4");
  redis.pushleft("k1", "v6");

  // simple, non-negative indices
  assert_true(redis.index("k1", 0, &tempv));
  assert_eq(tempv, "v6");
  assert_true(redis.index("k1", 1, &tempv));
  assert_eq(tempv, "v4");
  assert_true(redis.index("k1", 2, &tempv));
  assert_eq(tempv, "v4");
  assert_true(redis.index("k1", 3, &tempv));
  assert_eq(tempv, "v1");
  assert_true(redis.index("k1", 4, &tempv));
  assert_eq(tempv, "v2");
  assert_true(redis.index("k1", 5, &tempv));
  assert_eq(tempv, "v3");

  // negative indices
  assert_true(redis.index("k1", -6, &tempv));
  assert_eq(tempv, "v6");
  assert_true(redis.index("k1", -5, &tempv));
  assert_eq(tempv, "v4");
  assert_true(redis.index("k1", -4, &tempv));
  assert_eq(tempv, "v4");
  assert_true(redis.index("k1", -3, &tempv));
  assert_eq(tempv, "v1");
  assert_true(redis.index("k1", -2, &tempv));
  assert_eq(tempv, "v2");
  assert_true(redis.index("k1", -1, &tempv));
  assert_eq(tempv, "v3");

  // out of bounds (return empty, no crash)
  assert_true(!redis.index("k1", 6, &tempv));
  assert_true(!redis.index("k1", 123219, &tempv));
  assert_true(!redis.index("k1", -7, &tempv));
  assert_true(!redis.index("k1", -129, &tempv));
}


// exhaustive test of the range() function
test(redisliststest, rangetest) {
  redislists redis(kdefaultdbname, options, true);   // destructive

  string tempv; // used below for all index(), popright(), popleft()

  // simple pushes (will yield: [v6, v4, v4, v1, v2, v3])
  redis.pushright("k1", "v1");
  redis.pushright("k1", "v2");
  redis.pushright("k1", "v3");
  redis.pushleft("k1", "v4");
  redis.pushleft("k1", "v4");
  redis.pushleft("k1", "v6");

  // sanity check (check the length;  make sure it's 6)
  assert_eq(redis.length("k1"), 6);

  // simple range
  std::vector<std::string> res = redis.range("k1", 1, 4);
  assert_eq((int)res.size(), 4);
  assert_eq(res[0], "v4");
  assert_eq(res[1], "v4");
  assert_eq(res[2], "v1");
  assert_eq(res[3], "v2");

  // negative indices (i.e.: measured from the end)
  res = redis.range("k1", 2, -1);
  assert_eq((int)res.size(), 4);
  assert_eq(res[0], "v4");
  assert_eq(res[1], "v1");
  assert_eq(res[2], "v2");
  assert_eq(res[3], "v3");

  res = redis.range("k1", -6, -4);
  assert_eq((int)res.size(), 3);
  assert_eq(res[0], "v6");
  assert_eq(res[1], "v4");
  assert_eq(res[2], "v4");

  res = redis.range("k1", -1, 5);
  assert_eq((int)res.size(), 1);
  assert_eq(res[0], "v3");

  // partial / broken indices
  res = redis.range("k1", -3, 1000000);
  assert_eq((int)res.size(), 3);
  assert_eq(res[0], "v1");
  assert_eq(res[1], "v2");
  assert_eq(res[2], "v3");

  res = redis.range("k1", -1000000, 1);
  assert_eq((int)res.size(), 2);
  assert_eq(res[0], "v6");
  assert_eq(res[1], "v4");

  // invalid indices
  res = redis.range("k1", 7, 9);
  assert_eq((int)res.size(), 0);

  res = redis.range("k1", -8, -7);
  assert_eq((int)res.size(), 0);

  res = redis.range("k1", 3, 2);
  assert_eq((int)res.size(), 0);

  res = redis.range("k1", 5, -2);
  assert_eq((int)res.size(), 0);

  // range matches index
  res = redis.range("k1", -6, -4);
  assert_true(redis.index("k1", -6, &tempv));
  assert_eq(tempv, res[0]);
  assert_true(redis.index("k1", -5, &tempv));
  assert_eq(tempv, res[1]);
  assert_true(redis.index("k1", -4, &tempv));
  assert_eq(tempv, res[2]);

  // last check
  res = redis.range("k1", 0, -6);
  assert_eq((int)res.size(), 1);
  assert_eq(res[0], "v6");
}

// exhaustive test for insertbefore(), and insertafter()
test(redisliststest, inserttest) {
  redislists redis(kdefaultdbname, options, true);

  string tempv; // used below for all index(), popright(), popleft()

  // insert on empty list (return 0, and do not crash)
  assert_eq(redis.insertbefore("k1", "non-exist", "a"), 0);
  assert_eq(redis.insertafter("k1", "other-non-exist", "c"), 0);
  assert_eq(redis.length("k1"), 0);

  // push some preliminary stuff [g, f, e, d, c, b, a]
  redis.pushleft("k1", "a");
  redis.pushleft("k1", "b");
  redis.pushleft("k1", "c");
  redis.pushleft("k1", "d");
  redis.pushleft("k1", "e");
  redis.pushleft("k1", "f");
  redis.pushleft("k1", "g");
  assert_eq(redis.length("k1"), 7);

  // test insertbefore
  int newlength = redis.insertbefore("k1", "e", "hello");
  assert_eq(newlength, 8);
  assert_eq(redis.length("k1"), newlength);
  assert_true(redis.index("k1", 1, &tempv));
  assert_eq(tempv, "f");
  assert_true(redis.index("k1", 3, &tempv));
  assert_eq(tempv, "e");
  assert_true(redis.index("k1", 2, &tempv));
  assert_eq(tempv, "hello");

  // test insertafter
  newlength =  redis.insertafter("k1", "c", "bye");
  assert_eq(newlength, 9);
  assert_eq(redis.length("k1"), newlength);
  assert_true(redis.index("k1", 6, &tempv));
  assert_eq(tempv, "bye");

  // test bad value on insertbefore
  newlength = redis.insertbefore("k1", "yo", "x");
  assert_eq(newlength, 9);
  assert_eq(redis.length("k1"), newlength);

  // test bad value on insertafter
  newlength = redis.insertafter("k1", "xxxx", "y");
  assert_eq(newlength, 9);
  assert_eq(redis.length("k1"), newlength);

  // test insertbefore beginning
  newlength = redis.insertbefore("k1", "g", "begggggggggggggggg");
  assert_eq(newlength, 10);
  assert_eq(redis.length("k1"), newlength);

  // test insertafter end
  newlength = redis.insertafter("k1", "a", "enddd");
  assert_eq(newlength, 11);
  assert_eq(redis.length("k1"), newlength);

  // make sure nothing weird happened.
  assert_true(redis.index("k1", 0, &tempv));
  assert_eq(tempv, "begggggggggggggggg");
  assert_true(redis.index("k1", 1, &tempv));
  assert_eq(tempv, "g");
  assert_true(redis.index("k1", 2, &tempv));
  assert_eq(tempv, "f");
  assert_true(redis.index("k1", 3, &tempv));
  assert_eq(tempv, "hello");
  assert_true(redis.index("k1", 4, &tempv));
  assert_eq(tempv, "e");
  assert_true(redis.index("k1", 5, &tempv));
  assert_eq(tempv, "d");
  assert_true(redis.index("k1", 6, &tempv));
  assert_eq(tempv, "c");
  assert_true(redis.index("k1", 7, &tempv));
  assert_eq(tempv, "bye");
  assert_true(redis.index("k1", 8, &tempv));
  assert_eq(tempv, "b");
  assert_true(redis.index("k1", 9, &tempv));
  assert_eq(tempv, "a");
  assert_true(redis.index("k1", 10, &tempv));
  assert_eq(tempv, "enddd");
}

// exhaustive test of set function
test(redisliststest, settest) {
  redislists redis(kdefaultdbname, options, true);

  string tempv; // used below for all index(), popright(), popleft()

  // set on empty list (return false, and do not crash)
  assert_eq(redis.set("k1", 7, "a"), false);
  assert_eq(redis.set("k1", 0, "a"), false);
  assert_eq(redis.set("k1", -49, "cx"), false);
  assert_eq(redis.length("k1"), 0);

  // push some preliminary stuff [g, f, e, d, c, b, a]
  redis.pushleft("k1", "a");
  redis.pushleft("k1", "b");
  redis.pushleft("k1", "c");
  redis.pushleft("k1", "d");
  redis.pushleft("k1", "e");
  redis.pushleft("k1", "f");
  redis.pushleft("k1", "g");
  assert_eq(redis.length("k1"), 7);

  // test regular set
  assert_true(redis.set("k1", 0, "0"));
  assert_true(redis.set("k1", 3, "3"));
  assert_true(redis.set("k1", 6, "6"));
  assert_true(redis.set("k1", 2, "2"));
  assert_true(redis.set("k1", 5, "5"));
  assert_true(redis.set("k1", 1, "1"));
  assert_true(redis.set("k1", 4, "4"));

  assert_eq(redis.length("k1"), 7); // size should not change
  assert_true(redis.index("k1", 0, &tempv));
  assert_eq(tempv, "0");
  assert_true(redis.index("k1", 1, &tempv));
  assert_eq(tempv, "1");
  assert_true(redis.index("k1", 2, &tempv));
  assert_eq(tempv, "2");
  assert_true(redis.index("k1", 3, &tempv));
  assert_eq(tempv, "3");
  assert_true(redis.index("k1", 4, &tempv));
  assert_eq(tempv, "4");
  assert_true(redis.index("k1", 5, &tempv));
  assert_eq(tempv, "5");
  assert_true(redis.index("k1", 6, &tempv));
  assert_eq(tempv, "6");

  // set with negative indices
  assert_true(redis.set("k1", -7, "a"));
  assert_true(redis.set("k1", -4, "d"));
  assert_true(redis.set("k1", -1, "g"));
  assert_true(redis.set("k1", -5, "c"));
  assert_true(redis.set("k1", -2, "f"));
  assert_true(redis.set("k1", -6, "b"));
  assert_true(redis.set("k1", -3, "e"));

  assert_eq(redis.length("k1"), 7); // size should not change
  assert_true(redis.index("k1", 0, &tempv));
  assert_eq(tempv, "a");
  assert_true(redis.index("k1", 1, &tempv));
  assert_eq(tempv, "b");
  assert_true(redis.index("k1", 2, &tempv));
  assert_eq(tempv, "c");
  assert_true(redis.index("k1", 3, &tempv));
  assert_eq(tempv, "d");
  assert_true(redis.index("k1", 4, &tempv));
  assert_eq(tempv, "e");
  assert_true(redis.index("k1", 5, &tempv));
  assert_eq(tempv, "f");
  assert_true(redis.index("k1", 6, &tempv));
  assert_eq(tempv, "g");

  // bad indices (just out-of-bounds / off-by-one check)
  assert_eq(redis.set("k1", -8, "off-by-one in negative index"), false);
  assert_eq(redis.set("k1", 7, "off-by-one-error in positive index"), false);
  assert_eq(redis.set("k1", 43892, "big random index should fail"), false);
  assert_eq(redis.set("k1", -21391, "large negative index should fail"), false);

  // one last check (to make sure nothing weird happened)
  assert_eq(redis.length("k1"), 7); // size should not change
  assert_true(redis.index("k1", 0, &tempv));
  assert_eq(tempv, "a");
  assert_true(redis.index("k1", 1, &tempv));
  assert_eq(tempv, "b");
  assert_true(redis.index("k1", 2, &tempv));
  assert_eq(tempv, "c");
  assert_true(redis.index("k1", 3, &tempv));
  assert_eq(tempv, "d");
  assert_true(redis.index("k1", 4, &tempv));
  assert_eq(tempv, "e");
  assert_true(redis.index("k1", 5, &tempv));
  assert_eq(tempv, "f");
  assert_true(redis.index("k1", 6, &tempv));
  assert_eq(tempv, "g");
}

// testing insert, push, and set, in a mixed environment
test(redisliststest, insertpushsettest) {
  redislists redis(kdefaultdbname, options, true);   // destructive

  string tempv; // used below for all index(), popright(), popleft()

  // a series of pushes and insertions
  // will result in [newbegin, z, a, aftera, x, newend]
  // also, check the return value sometimes (should return length)
  int lengthcheck;
  lengthcheck = redis.pushleft("k1", "a");
  assert_eq(lengthcheck, 1);
  redis.pushleft("k1", "z");
  redis.pushright("k1", "x");
  lengthcheck = redis.insertafter("k1", "a", "aftera");
  assert_eq(lengthcheck , 4);
  redis.insertbefore("k1", "z", "newbegin");  // insertbefore beginning of list
  redis.insertafter("k1", "x", "newend");     // insertafter end of list

  // check
  std::vector<std::string> res = redis.range("k1", 0, -1); // get the list
  assert_eq((int)res.size(), 6);
  assert_eq(res[0], "newbegin");
  assert_eq(res[5], "newend");
  assert_eq(res[3], "aftera");

  // testing duplicate values/pivots (multiple occurrences of 'a')
  assert_true(redis.set("k1", 0, "a"));     // [a, z, a, aftera, x, newend]
  redis.insertafter("k1", "a", "happy");    // [a, happy, z, a, aftera, ...]
  assert_true(redis.index("k1", 1, &tempv));
  assert_eq(tempv, "happy");
  redis.insertbefore("k1", "a", "sad");     // [sad, a, happy, z, a, aftera, ...]
  assert_true(redis.index("k1", 0, &tempv));
  assert_eq(tempv, "sad");
  assert_true(redis.index("k1", 2, &tempv));
  assert_eq(tempv, "happy");
  assert_true(redis.index("k1", 5, &tempv));
  assert_eq(tempv, "aftera");
  redis.insertafter("k1", "a", "zz");         // [sad, a, zz, happy, z, a, aftera, ...]
  assert_true(redis.index("k1", 2, &tempv));
  assert_eq(tempv, "zz");
  assert_true(redis.index("k1", 6, &tempv));
  assert_eq(tempv, "aftera");
  assert_true(redis.set("k1", 1, "nota"));    // [sad, nota, zz, happy, z, a, ...]
  redis.insertbefore("k1", "a", "ba");        // [sad, nota, zz, happy, z, ba, a, ...]
  assert_true(redis.index("k1", 4, &tempv));
  assert_eq(tempv, "z");
  assert_true(redis.index("k1", 5, &tempv));
  assert_eq(tempv, "ba");
  assert_true(redis.index("k1", 6, &tempv));
  assert_eq(tempv, "a");

  // we currently have: [sad, nota, zz, happy, z, ba, a, aftera, x, newend]
  // redis.print("k1");   // manually check

  // test inserting before/after non-existent values
  lengthcheck = redis.length("k1"); // ensure that the length doesn't change
  assert_eq(lengthcheck, 10);
  assert_eq(redis.insertbefore("k1", "non-exist", "randval"), lengthcheck);
  assert_eq(redis.insertafter("k1", "nothing", "a"), lengthcheck);
  assert_eq(redis.insertafter("randkey", "randval", "ranvalue"), 0); // empty
  assert_eq(redis.length("k1"), lengthcheck); // the length should not change

  // simply test the set() function
  redis.set("k1", 5, "ba2");
  redis.insertbefore("k1", "ba2", "beforeba2");
  assert_true(redis.index("k1", 4, &tempv));
  assert_eq(tempv, "z");
  assert_true(redis.index("k1", 5, &tempv));
  assert_eq(tempv, "beforeba2");
  assert_true(redis.index("k1", 6, &tempv));
  assert_eq(tempv, "ba2");
  assert_true(redis.index("k1", 7, &tempv));
  assert_eq(tempv, "a");

  // we have: [sad, nota, zz, happy, z, beforeba2, ba2, a, aftera, x, newend]

  // set() with negative indices
  redis.set("k1", -1, "endprank");
  assert_true(!redis.index("k1", 11, &tempv));
  assert_true(redis.index("k1", 10, &tempv));
  assert_eq(tempv, "endprank"); // ensure set worked correctly
  redis.set("k1", -11, "t");
  assert_true(redis.index("k1", 0, &tempv));
  assert_eq(tempv, "t");

  // test out of bounds set
  assert_eq(redis.set("k1", -12, "ssd"), false);
  assert_eq(redis.set("k1", 11, "sasd"), false);
  assert_eq(redis.set("k1", 1200, "big"), false);
}

// testing trim, pop
test(redisliststest, trimpoptest) {
  redislists redis(kdefaultdbname, options, true);   // destructive

  string tempv; // used below for all index(), popright(), popleft()

  // a series of pushes and insertions
  // will result in [newbegin, z, a, aftera, x, newend]
  redis.pushleft("k1", "a");
  redis.pushleft("k1", "z");
  redis.pushright("k1", "x");
  redis.insertbefore("k1", "z", "newbegin");    // insertbefore start of list
  redis.insertafter("k1", "x", "newend");       // insertafter end of list
  redis.insertafter("k1", "a", "aftera");

  // simple popleft/right test
  assert_true(redis.popleft("k1", &tempv));
  assert_eq(tempv, "newbegin");
  assert_eq(redis.length("k1"), 5);
  assert_true(redis.index("k1", 0, &tempv));
  assert_eq(tempv, "z");
  assert_true(redis.popright("k1", &tempv));
  assert_eq(tempv, "newend");
  assert_eq(redis.length("k1"), 4);
  assert_true(redis.index("k1", -1, &tempv));
  assert_eq(tempv, "x");

  // now have: [z, a, aftera, x]

  // test trim
  assert_true(redis.trim("k1", 0, -1));       // [z, a, aftera, x] (do nothing)
  assert_eq(redis.length("k1"), 4);
  assert_true(redis.trim("k1", 0, 2));                     // [z, a, aftera]
  assert_eq(redis.length("k1"), 3);
  assert_true(redis.index("k1", -1, &tempv));
  assert_eq(tempv, "aftera");
  assert_true(redis.trim("k1", 1, 1));                     // [a]
  assert_eq(redis.length("k1"), 1);
  assert_true(redis.index("k1", 0, &tempv));
  assert_eq(tempv, "a");

  // test out of bounds (empty) trim
  assert_true(redis.trim("k1", 1, 0));
  assert_eq(redis.length("k1"), 0);

  // popping with empty list (return empty without error)
  assert_true(!redis.popleft("k1", &tempv));
  assert_true(!redis.popright("k1", &tempv));
  assert_true(redis.trim("k1", 0, 5));

  // exhaustive trim test (negative and invalid indices)
  // will start in [newbegin, z, a, aftera, x, newend]
  redis.pushleft("k1", "a");
  redis.pushleft("k1", "z");
  redis.pushright("k1", "x");
  redis.insertbefore("k1", "z", "newbegin");    // insertbefore start of list
  redis.insertafter("k1", "x", "newend");       // insertafter end of list
  redis.insertafter("k1", "a", "aftera");
  assert_true(redis.trim("k1", -6, -1));                     // should do nothing
  assert_eq(redis.length("k1"), 6);
  assert_true(redis.trim("k1", 1, -2));
  assert_true(redis.index("k1", 0, &tempv));
  assert_eq(tempv, "z");
  assert_true(redis.index("k1", 3, &tempv));
  assert_eq(tempv, "x");
  assert_eq(redis.length("k1"), 4);
  assert_true(redis.trim("k1", -3, -2));
  assert_eq(redis.length("k1"), 2);
}

// testing remove, removefirst, removelast
test(redisliststest, removetest) {
  redislists redis(kdefaultdbname, options, true);   // destructive

  string tempv; // used below for all index(), popright(), popleft()

  // a series of pushes and insertions
  // will result in [newbegin, z, a, aftera, x, newend, a, a]
  redis.pushleft("k1", "a");
  redis.pushleft("k1", "z");
  redis.pushright("k1", "x");
  redis.insertbefore("k1", "z", "newbegin");    // insertbefore start of list
  redis.insertafter("k1", "x", "newend");       // insertafter end of list
  redis.insertafter("k1", "a", "aftera");
  redis.pushright("k1", "a");
  redis.pushright("k1", "a");

  // verify
  assert_true(redis.index("k1", 0, &tempv));
  assert_eq(tempv, "newbegin");
  assert_true(redis.index("k1", -1, &tempv));
  assert_eq(tempv, "a");

  // check removefirst (remove the first two 'a')
  // results in [newbegin, z, aftera, x, newend, a]
  int numremoved = redis.remove("k1", 2, "a");
  assert_eq(numremoved, 2);
  assert_true(redis.index("k1", 0, &tempv));
  assert_eq(tempv, "newbegin");
  assert_true(redis.index("k1", 1, &tempv));
  assert_eq(tempv, "z");
  assert_true(redis.index("k1", 4, &tempv));
  assert_eq(tempv, "newend");
  assert_true(redis.index("k1", 5, &tempv));
  assert_eq(tempv, "a");
  assert_eq(redis.length("k1"), 6);

  // repopulate some stuff
  // results in: [x, x, x, x, x, newbegin, z, x, aftera, x, newend, a, x]
  redis.pushleft("k1", "x");
  redis.pushleft("k1", "x");
  redis.pushleft("k1", "x");
  redis.pushleft("k1", "x");
  redis.pushleft("k1", "x");
  redis.pushright("k1", "x");
  redis.insertafter("k1", "z", "x");

  // test removal from end
  numremoved = redis.remove("k1", -2, "x");
  assert_eq(numremoved, 2);
  assert_true(redis.index("k1", 8, &tempv));
  assert_eq(tempv, "aftera");
  assert_true(redis.index("k1", 9, &tempv));
  assert_eq(tempv, "newend");
  assert_true(redis.index("k1", 10, &tempv));
  assert_eq(tempv, "a");
  assert_true(!redis.index("k1", 11, &tempv));
  numremoved = redis.remove("k1", -2, "x");
  assert_eq(numremoved, 2);
  assert_true(redis.index("k1", 4, &tempv));
  assert_eq(tempv, "newbegin");
  assert_true(redis.index("k1", 6, &tempv));
  assert_eq(tempv, "aftera");

  // we now have: [x, x, x, x, newbegin, z, aftera, newend, a]
  assert_eq(redis.length("k1"), 9);
  assert_true(redis.index("k1", -1, &tempv));
  assert_eq(tempv, "a");
  assert_true(redis.index("k1", 0, &tempv));
  assert_eq(tempv, "x");

  // test over-shooting (removing more than there exists)
  numremoved = redis.remove("k1", -9000, "x");
  assert_eq(numremoved , 4);    // only really removed 4
  assert_eq(redis.length("k1"), 5);
  assert_true(redis.index("k1", 0, &tempv));
  assert_eq(tempv, "newbegin");
  numremoved = redis.remove("k1", 1, "x");
  assert_eq(numremoved, 0);

  // try removing all!
  numremoved = redis.remove("k1", 0, "newbegin");   // remove 0 will remove all!
  assert_eq(numremoved, 1);

  // removal from an empty-list
  assert_true(redis.trim("k1", 1, 0));
  numremoved = redis.remove("k1", 1, "z");
  assert_eq(numremoved, 0);
}


// test multiple keys and persistence
test(redisliststest, persistencemultikeytest) {

  string tempv; // used below for all index(), popright(), popleft()

  // block one: populate a single key in the database
  {
    redislists redis(kdefaultdbname, options, true);   // destructive

    // a series of pushes and insertions
    // will result in [newbegin, z, a, aftera, x, newend, a, a]
    redis.pushleft("k1", "a");
    redis.pushleft("k1", "z");
    redis.pushright("k1", "x");
    redis.insertbefore("k1", "z", "newbegin");    // insertbefore start of list
    redis.insertafter("k1", "x", "newend");       // insertafter end of list
    redis.insertafter("k1", "a", "aftera");
    redis.pushright("k1", "a");
    redis.pushright("k1", "a");

    assert_true(redis.index("k1", 3, &tempv));
    assert_eq(tempv, "aftera");
  }

  // block two: make sure changes were saved and add some other key
  {
    redislists redis(kdefaultdbname, options, false); // persistent, non-destructive

    // check
    assert_eq(redis.length("k1"), 8);
    assert_true(redis.index("k1", 3, &tempv));
    assert_eq(tempv, "aftera");

    redis.pushright("k2", "randomkey");
    redis.pushleft("k2", "sas");

    redis.popleft("k1", &tempv);
  }

  // block three: verify the changes from block 2
  {
    redislists redis(kdefaultdbname, options, false); // persistent, non-destructive

    // check
    assert_eq(redis.length("k1"), 7);
    assert_eq(redis.length("k2"), 2);
    assert_true(redis.index("k1", 0, &tempv));
    assert_eq(tempv, "z");
    assert_true(redis.index("k2", -2, &tempv));
    assert_eq(tempv, "sas");
  }
}

/// the manual redis test begins here
/// this will only occur if you run: ./redis_test -m

namespace {
void makeupper(std::string* const s) {
  int len = s->length();
  for(int i=0; i<len; ++i) {
    (*s)[i] = toupper((*s)[i]); // c-version defined in <ctype.h>
  }
}

/// allows the user to enter in redis commands into the command-line.
/// this is useful for manual / interacticve testing / debugging.
///  use destructive=true to clean the database before use.
///  use destructive=false to remember the previous state (i.e.: persistent)
/// should be called from main function.
int manual_redis_test(bool destructive){
  redislists redis(redisliststest::kdefaultdbname,
                   redisliststest::options,
                   destructive);

  // todo: right now, please use spaces to separate each word.
  //  in actual redis, you can use quotes to specify compound values
  //  example: rpush mylist "this is a compound value"

  std::string command;
  while(true) {
    cin >> command;
    makeupper(&command);

    if (command == "linsert") {
      std::string k, t, p, v;
      cin >> k >> t >> p >> v;
      makeupper(&t);
      if (t=="before") {
        std::cout << redis.insertbefore(k, p, v) << std::endl;
      } else if (t=="after") {
        std::cout << redis.insertafter(k, p, v) << std::endl;
      }
    } else if (command == "lpush") {
      std::string k, v;
      std::cin >> k >> v;
      redis.pushleft(k, v);
    } else if (command == "rpush") {
      std::string k, v;
      std::cin >> k >> v;
      redis.pushright(k, v);
    } else if (command == "lpop") {
      std::string k;
      std::cin >> k;
      string res;
      redis.popleft(k, &res);
      std::cout << res << std::endl;
    } else if (command == "rpop") {
      std::string k;
      std::cin >> k;
      string res;
      redis.popright(k, &res);
      std::cout << res << std::endl;
    } else if (command == "lrem") {
      std::string k;
      int amt;
      std::string v;

      std::cin >> k >> amt >> v;
      std::cout << redis.remove(k, amt, v) << std::endl;
    } else if (command == "llen") {
      std::string k;
      std::cin >> k;
      std::cout << redis.length(k) << std::endl;
    } else if (command == "lrange") {
      std::string k;
      int i, j;
      std::cin >> k >> i >> j;
      std::vector<std::string> res = redis.range(k, i, j);
      for (auto it = res.begin(); it != res.end(); ++it) {
        std::cout << " " << (*it);
      }
      std::cout << std::endl;
    } else if (command == "ltrim") {
      std::string k;
      int i, j;
      std::cin >> k >> i >> j;
      redis.trim(k, i, j);
    } else if (command == "lset") {
      std::string k;
      int idx;
      std::string v;
      cin >> k >> idx >> v;
      redis.set(k, idx, v);
    } else if (command == "lindex") {
      std::string k;
      int idx;
      std::cin >> k >> idx;
      string res;
      redis.index(k, idx, &res);
      std::cout << res << std::endl;
    } else if (command == "print") {      // added by deon
      std::string k;
      cin >> k;
      redis.print(k);
    } else if (command == "quit") {
      return 0;
    } else {
      std::cout << "unknown command: " << command << std::endl;
    }
  }
}
}  // namespace

} // namespace rocksdb


// usage: "./redis_test" for default (unit tests)
//        "./redis_test -m" for manual testing (redis command api)
//        "./redis_test -m -d" for destructive manual test (erase db before use)


namespace {
// check for "want" argument in the argument list
bool found_arg(int argc, char* argv[], const char* want){
  for(int i=1; i<argc; ++i){
    if (strcmp(argv[i], want) == 0) {
      return true;
    }
  }
  return false;
}
}  // namespace

// will run unit tests.
// however, if -m is specified, it will do user manual/interactive testing
// -m -d is manual and destructive (will clear the database before use)
int main(int argc, char* argv[]) {
  if (found_arg(argc, argv, "-m")) {
    bool destructive = found_arg(argc, argv, "-d");
    return rocksdb::manual_redis_test(destructive);
  } else {
    return rocksdb::test::runalltests();
  }
}

