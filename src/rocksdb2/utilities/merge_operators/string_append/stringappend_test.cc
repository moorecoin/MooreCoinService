/**
 * an persistent map : key -> (list of strings), using rocksdb merge.
 * this file is a test-harness / use-case for the stringappendoperator.
 *
 * @author deon nicholas (dnicholas@fb.com)
 * copyright 2013 facebook, inc.
*/

#include <iostream>
#include <map>

#include "rocksdb/db.h"
#include "rocksdb/merge_operator.h"
#include "rocksdb/utilities/db_ttl.h"
#include "utilities/merge_operators.h"
#include "utilities/merge_operators/string_append/stringappend.h"
#include "utilities/merge_operators/string_append/stringappend2.h"
#include "util/testharness.h"
#include "util/random.h"

using namespace rocksdb;

namespace rocksdb {

// path to the database on file system
const std::string kdbname = "/tmp/mergetestdb";

namespace {
// opendb opens a (possibly new) rocksdb database with a stringappendoperator
std::shared_ptr<db> opennormaldb(char delim_char) {
  db* db;
  options options;
  options.create_if_missing = true;
  options.merge_operator.reset(new stringappendoperator(delim_char));
  assert_ok(db::open(options, kdbname,  &db));
  return std::shared_ptr<db>(db);
}

// open a ttldb with a non-associative stringappendtestoperator
std::shared_ptr<db> openttldb(char delim_char) {
  dbwithttl* db;
  options options;
  options.create_if_missing = true;
  options.merge_operator.reset(new stringappendtestoperator(delim_char));
  assert_ok(dbwithttl::open(options, kdbname, &db, 123456));
  return std::shared_ptr<db>(db);
}
}  // namespace

/// stringlists represents a set of string-lists, each with a key-index.
/// supports append(list, string) and get(list)
class stringlists {
 public:

  //constructor: specifies the rocksdb db
  /* implicit */
  stringlists(std::shared_ptr<db> db)
      : db_(db),
        merge_option_(),
        get_option_() {
    assert(db);
  }

  // append string val onto the list defined by key; return true on success
  bool append(const std::string& key, const std::string& val){
    slice valslice(val.data(), val.size());
    auto s = db_->merge(merge_option_, key, valslice);

    if (s.ok()) {
      return true;
    } else {
      std::cerr << "error " << s.tostring() << std::endl;
      return false;
    }
  }

  // returns the list of strings associated with key (or "" if does not exist)
  bool get(const std::string& key, std::string* const result){
    assert(result != nullptr); // we should have a place to store the result
    auto s = db_->get(get_option_, key, result);

    if (s.ok()) {
      return true;
    }

    // either key does not exist, or there is some error.
    *result = "";       // always return empty string (just for convention)

    //notfound is okay; just return empty (similar to std::map)
    //but network or db errors, etc, should fail the test (or at least yell)
    if (!s.isnotfound()) {
      std::cerr << "error " << s.tostring() << std::endl;
    }

    // always return false if s.ok() was not true
    return false;
  }


 private:
  std::shared_ptr<db> db_;
  writeoptions merge_option_;
  readoptions get_option_;

};


// the class for unit-testing
class stringappendoperatortest {
 public:
  stringappendoperatortest() {
    destroydb(kdbname, options());    // start each test with a fresh db
  }

  typedef std::shared_ptr<db> (* openfuncptr)(char);

  // allows user to open databases with different configurations.
  // e.g.: can open a db or a ttldb, etc.
  static void setopendbfunction(openfuncptr func) {
    opendb = func;
  }

 protected:
  static openfuncptr opendb;
};
stringappendoperatortest::openfuncptr stringappendoperatortest::opendb = nullptr;

// the test cases begin here

test(stringappendoperatortest, iteratortest) {
  auto db_ = opendb(',');
  stringlists slists(db_);

  slists.append("k1", "v1");
  slists.append("k1", "v2");
  slists.append("k1", "v3");

  slists.append("k2", "a1");
  slists.append("k2", "a2");
  slists.append("k2", "a3");

  std::string res;
  std::unique_ptr<rocksdb::iterator> it(db_->newiterator(readoptions()));
  std::string k1("k1");
  std::string k2("k2");
  bool first = true;
  for (it->seek(k1); it->valid(); it->next()) {
    res = it->value().tostring();
    if (first) {
      assert_eq(res, "v1,v2,v3");
      first = false;
    } else {
      assert_eq(res, "a1,a2,a3");
    }
  }
  slists.append("k2", "a4");
  slists.append("k1", "v4");

  // snapshot should still be the same. should ignore a4 and v4.
  first = true;
  for (it->seek(k1); it->valid(); it->next()) {
    res = it->value().tostring();
    if (first) {
      assert_eq(res, "v1,v2,v3");
      first = false;
    } else {
      assert_eq(res, "a1,a2,a3");
    }
  }


  // should release the snapshot and be aware of the new stuff now
  it.reset(db_->newiterator(readoptions()));
  first = true;
  for (it->seek(k1); it->valid(); it->next()) {
    res = it->value().tostring();
    if (first) {
      assert_eq(res, "v1,v2,v3,v4");
      first = false;
    } else {
      assert_eq(res, "a1,a2,a3,a4");
    }
  }

  // start from k2 this time.
  for (it->seek(k2); it->valid(); it->next()) {
    res = it->value().tostring();
    if (first) {
      assert_eq(res, "v1,v2,v3,v4");
      first = false;
    } else {
      assert_eq(res, "a1,a2,a3,a4");
    }
  }

  slists.append("k3", "g1");

  it.reset(db_->newiterator(readoptions()));
  first = true;
  std::string k3("k3");
  for(it->seek(k2); it->valid(); it->next()) {
    res = it->value().tostring();
    if (first) {
      assert_eq(res, "a1,a2,a3,a4");
      first = false;
    } else {
      assert_eq(res, "g1");
    }
  }
  for(it->seek(k3); it->valid(); it->next()) {
    res = it->value().tostring();
    if (first) {
      // should not be hit
      assert_eq(res, "a1,a2,a3,a4");
      first = false;
    } else {
      assert_eq(res, "g1");
    }
  }

}

test(stringappendoperatortest, simpletest) {
  auto db = opendb(',');
  stringlists slists(db);

  slists.append("k1", "v1");
  slists.append("k1", "v2");
  slists.append("k1", "v3");

  std::string res;
  bool status = slists.get("k1", &res);

  assert_true(status);
  assert_eq(res, "v1,v2,v3");
}

test(stringappendoperatortest, simpledelimitertest) {
  auto db = opendb('|');
  stringlists slists(db);

  slists.append("k1", "v1");
  slists.append("k1", "v2");
  slists.append("k1", "v3");

  std::string res;
  slists.get("k1", &res);
  assert_eq(res, "v1|v2|v3");
}

test(stringappendoperatortest, onevaluenodelimitertest) {
  auto db = opendb('!');
  stringlists slists(db);

  slists.append("random_key", "single_val");

  std::string res;
  slists.get("random_key", &res);
  assert_eq(res, "single_val");
}

test(stringappendoperatortest, variouskeys) {
  auto db = opendb('\n');
  stringlists slists(db);

  slists.append("c", "asdasd");
  slists.append("a", "x");
  slists.append("b", "y");
  slists.append("a", "t");
  slists.append("a", "r");
  slists.append("b", "2");
  slists.append("c", "asdasd");

  std::string a, b, c;
  bool sa, sb, sc;
  sa = slists.get("a", &a);
  sb = slists.get("b", &b);
  sc = slists.get("c", &c);

  assert_true(sa && sb && sc); // all three keys should have been found

  assert_eq(a, "x\nt\nr");
  assert_eq(b, "y\n2");
  assert_eq(c, "asdasd\nasdasd");
}

// generate semi random keys/words from a small distribution.
test(stringappendoperatortest, randommixgetappend) {
  auto db = opendb(' ');
  stringlists slists(db);

  // generate a list of random keys and values
  const int kwordcount = 15;
  std::string words[] = {"sdasd", "triejf", "fnjsdfn", "dfjisdfsf", "342839",
                         "dsuha", "mabuais", "sadajsid", "jf9834hf", "2d9j89",
                         "dj9823jd", "a", "dk02ed2dh", "$(jd4h984$(*", "mabz"};
  const int kkeycount = 6;
  std::string keys[] = {"dhaiusdhu", "denidw", "daisda", "keykey", "muki",
                        "shzassdianmd"};

  // will store a local copy of all data in order to verify correctness
  std::map<std::string, std::string> parallel_copy;

  // generate a bunch of random queries (append and get)!
  enum query_t  { append_op, get_op, num_ops };
  random randomgen(1337);       //deterministic seed; always get same results!

  const int knumqueries = 30;
  for (int q=0; q<knumqueries; ++q) {
    // generate a random query (append or get) and random parameters
    query_t query = (query_t)randomgen.uniform((int)num_ops);
    std::string key = keys[randomgen.uniform((int)kkeycount)];
    std::string word = words[randomgen.uniform((int)kwordcount)];

    // apply the query and any checks.
    if (query == append_op) {

      // apply the rocksdb test-harness append defined above
      slists.append(key, word);  //apply the rocksdb append

      // apply the similar "append" to the parallel copy
      if (parallel_copy[key].size() > 0) {
        parallel_copy[key] += " " + word;
      } else {
        parallel_copy[key] = word;
      }

    } else if (query == get_op) {
      // assumes that a non-existent key just returns <empty>
      std::string res;
      slists.get(key, &res);
      assert_eq(res, parallel_copy[key]);
    }

  }

}

test(stringappendoperatortest, bigrandommixgetappend) {
  auto db = opendb(' ');
  stringlists slists(db);

  // generate a list of random keys and values
  const int kwordcount = 15;
  std::string words[] = {"sdasd", "triejf", "fnjsdfn", "dfjisdfsf", "342839",
                         "dsuha", "mabuais", "sadajsid", "jf9834hf", "2d9j89",
                         "dj9823jd", "a", "dk02ed2dh", "$(jd4h984$(*", "mabz"};
  const int kkeycount = 6;
  std::string keys[] = {"dhaiusdhu", "denidw", "daisda", "keykey", "muki",
                        "shzassdianmd"};

  // will store a local copy of all data in order to verify correctness
  std::map<std::string, std::string> parallel_copy;

  // generate a bunch of random queries (append and get)!
  enum query_t  { append_op, get_op, num_ops };
  random randomgen(9138204);       // deterministic seed

  const int knumqueries = 1000;
  for (int q=0; q<knumqueries; ++q) {
    // generate a random query (append or get) and random parameters
    query_t query = (query_t)randomgen.uniform((int)num_ops);
    std::string key = keys[randomgen.uniform((int)kkeycount)];
    std::string word = words[randomgen.uniform((int)kwordcount)];

    //apply the query and any checks.
    if (query == append_op) {

      // apply the rocksdb test-harness append defined above
      slists.append(key, word);  //apply the rocksdb append

      // apply the similar "append" to the parallel copy
      if (parallel_copy[key].size() > 0) {
        parallel_copy[key] += " " + word;
      } else {
        parallel_copy[key] = word;
      }

    } else if (query == get_op) {
      // assumes that a non-existent key just returns <empty>
      std::string res;
      slists.get(key, &res);
      assert_eq(res, parallel_copy[key]);
    }

  }

}


test(stringappendoperatortest, persistentvariouskeys) {
  // perform the following operations in limited scope
  {
    auto db = opendb('\n');
    stringlists slists(db);

    slists.append("c", "asdasd");
    slists.append("a", "x");
    slists.append("b", "y");
    slists.append("a", "t");
    slists.append("a", "r");
    slists.append("b", "2");
    slists.append("c", "asdasd");

    std::string a, b, c;
    slists.get("a", &a);
    slists.get("b", &b);
    slists.get("c", &c);

    assert_eq(a, "x\nt\nr");
    assert_eq(b, "y\n2");
    assert_eq(c, "asdasd\nasdasd");
  }

  // reopen the database (the previous changes should persist / be remembered)
  {
    auto db = opendb('\n');
    stringlists slists(db);

    slists.append("c", "bbnagnagsx");
    slists.append("a", "sa");
    slists.append("b", "df");
    slists.append("a", "gh");
    slists.append("a", "jk");
    slists.append("b", "l;");
    slists.append("c", "rogosh");

    // the previous changes should be on disk (l0)
    // the most recent changes should be in memory (memtable)
    // hence, this will test both get() paths.
    std::string a, b, c;
    slists.get("a", &a);
    slists.get("b", &b);
    slists.get("c", &c);

    assert_eq(a, "x\nt\nr\nsa\ngh\njk");
    assert_eq(b, "y\n2\ndf\nl;");
    assert_eq(c, "asdasd\nasdasd\nbbnagnagsx\nrogosh");
  }

  // reopen the database (the previous changes should persist / be remembered)
  {
    auto db = opendb('\n');
    stringlists slists(db);

    // all changes should be on disk. this will test versionset get()
    std::string a, b, c;
    slists.get("a", &a);
    slists.get("b", &b);
    slists.get("c", &c);

    assert_eq(a, "x\nt\nr\nsa\ngh\njk");
    assert_eq(b, "y\n2\ndf\nl;");
    assert_eq(c, "asdasd\nasdasd\nbbnagnagsx\nrogosh");
  }
}

test(stringappendoperatortest, persistentflushandcompaction) {
  // perform the following operations in limited scope
  {
    auto db = opendb('\n');
    stringlists slists(db);
    std::string a, b, c;
    bool success;

    // append, flush, get
    slists.append("c", "asdasd");
    db->flush(rocksdb::flushoptions());
    success = slists.get("c", &c);
    assert_true(success);
    assert_eq(c, "asdasd");

    // append, flush, append, get
    slists.append("a", "x");
    slists.append("b", "y");
    db->flush(rocksdb::flushoptions());
    slists.append("a", "t");
    slists.append("a", "r");
    slists.append("b", "2");

    success = slists.get("a", &a);
    assert(success == true);
    assert_eq(a, "x\nt\nr");

    success = slists.get("b", &b);
    assert(success == true);
    assert_eq(b, "y\n2");

    // append, get
    success = slists.append("c", "asdasd");
    assert(success);
    success = slists.append("b", "monkey");
    assert(success);

    // i omit the "assert(success)" checks here.
    slists.get("a", &a);
    slists.get("b", &b);
    slists.get("c", &c);

    assert_eq(a, "x\nt\nr");
    assert_eq(b, "y\n2\nmonkey");
    assert_eq(c, "asdasd\nasdasd");
  }

  // reopen the database (the previous changes should persist / be remembered)
  {
    auto db = opendb('\n');
    stringlists slists(db);
    std::string a, b, c;

    // get (quick check for persistence of previous database)
    slists.get("a", &a);
    assert_eq(a, "x\nt\nr");

    //append, compact, get
    slists.append("c", "bbnagnagsx");
    slists.append("a", "sa");
    slists.append("b", "df");
    db->compactrange(nullptr, nullptr);
    slists.get("a", &a);
    slists.get("b", &b);
    slists.get("c", &c);
    assert_eq(a, "x\nt\nr\nsa");
    assert_eq(b, "y\n2\nmonkey\ndf");
    assert_eq(c, "asdasd\nasdasd\nbbnagnagsx");

    // append, get
    slists.append("a", "gh");
    slists.append("a", "jk");
    slists.append("b", "l;");
    slists.append("c", "rogosh");
    slists.get("a", &a);
    slists.get("b", &b);
    slists.get("c", &c);
    assert_eq(a, "x\nt\nr\nsa\ngh\njk");
    assert_eq(b, "y\n2\nmonkey\ndf\nl;");
    assert_eq(c, "asdasd\nasdasd\nbbnagnagsx\nrogosh");

    // compact, get
    db->compactrange(nullptr, nullptr);
    assert_eq(a, "x\nt\nr\nsa\ngh\njk");
    assert_eq(b, "y\n2\nmonkey\ndf\nl;");
    assert_eq(c, "asdasd\nasdasd\nbbnagnagsx\nrogosh");

    // append, flush, compact, get
    slists.append("b", "afcg");
    db->flush(rocksdb::flushoptions());
    db->compactrange(nullptr, nullptr);
    slists.get("b", &b);
    assert_eq(b, "y\n2\nmonkey\ndf\nl;\nafcg");
  }
}

test(stringappendoperatortest, simpletestnulldelimiter) {
  auto db = opendb('\0');
  stringlists slists(db);

  slists.append("k1", "v1");
  slists.append("k1", "v2");
  slists.append("k1", "v3");

  std::string res;
  bool status = slists.get("k1", &res);
  assert_true(status);

  // construct the desired string. default constructor doesn't like '\0' chars.
  std::string checker("v1,v2,v3");    // verify that the string is right size.
  checker[2] = '\0';                  // use null delimiter instead of comma.
  checker[5] = '\0';
  assert(checker.size() == 8);        // verify it is still the correct size

  // check that the rocksdb result string matches the desired string
  assert(res.size() == checker.size());
  assert_eq(res, checker);
}

} // namespace rocksdb

int main(int arc, char** argv) {
  // run with regular database
  {
    fprintf(stderr, "running tests with regular db and operator.\n");
    stringappendoperatortest::setopendbfunction(&opennormaldb);
    rocksdb::test::runalltests();
  }

  // run with ttl
  {
    fprintf(stderr, "running tests with ttl db and generic operator.\n");
    stringappendoperatortest::setopendbfunction(&openttldb);
    rocksdb::test::runalltests();
  }

  return 0;
}
