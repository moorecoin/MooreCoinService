//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.

#include <vector>
#include <string>
#include <set>

#include "rocksdb/utilities/spatial_db.h"
#include "util/testharness.h"
#include "util/testutil.h"
#include "util/random.h"

namespace rocksdb {
namespace spatial {

class spatialdbtest {
 public:
  spatialdbtest() {
    dbname_ = test::tmpdir() + "/spatial_db_test";
    destroydb(dbname_, options());
  }

  void assertcursorresults(boundingbox<double> bbox, const std::string& index,
                           const std::vector<std::string>& blobs) {
    cursor* c = db_->query(readoptions(), bbox, index);
    assert_ok(c->status());
    std::multiset<std::string> b;
    for (auto x : blobs) {
      b.insert(x);
    }

    while (c->valid()) {
      auto itr = b.find(c->blob().tostring());
      assert_true(itr != b.end());
      b.erase(itr);
      c->next();
    }
    assert_eq(b.size(), 0u);
    assert_ok(c->status());
    delete c;
  }

  std::string dbname_;
  spatialdb* db_;
};

test(spatialdbtest, featuresetserializetest) {
  featureset fs;

  fs.set("a", std::string("b"));
  fs.set("x", static_cast<uint64_t>(3));
  fs.set("y", false);
  fs.set("n", variant());  // null
  fs.set("m", 3.25);

  assert_true(fs.find("w") == fs.end());
  assert_true(fs.find("x") != fs.end());
  assert_true((*fs.find("x")).second == variant(static_cast<uint64_t>(3)));
  assert_true((*fs.find("y")).second != variant(true));
  std::set<std::string> keys({"a", "x", "y", "n", "m"});
  for (const auto& x : fs) {
    assert_true(keys.find(x.first) != keys.end());
    keys.erase(x.first);
  }
  assert_eq(keys.size(), 0u);

  std::string serialized;
  fs.serialize(&serialized);

  featureset deserialized;
  assert_true(deserialized.deserialize(serialized));

  assert_true(deserialized.contains("a"));
  assert_eq(deserialized.get("a").type(), variant::kstring);
  assert_eq(deserialized.get("a").get_string(), "b");
  assert_true(deserialized.contains("x"));
  assert_eq(deserialized.get("x").type(), variant::kint);
  assert_eq(deserialized.get("x").get_int(), static_cast<uint64_t>(3));
  assert_true(deserialized.contains("y"));
  assert_eq(deserialized.get("y").type(), variant::kbool);
  assert_eq(deserialized.get("y").get_bool(), false);
  assert_true(deserialized.contains("n"));
  assert_eq(deserialized.get("n").type(), variant::knull);
  assert_true(deserialized.contains("m"));
  assert_eq(deserialized.get("m").type(), variant::kdouble);
  assert_eq(deserialized.get("m").get_double(), 3.25);

  // corrupted serialization
  serialized = serialized.substr(0, serialized.size() - 3);
  deserialized.clear();
  assert_true(!deserialized.deserialize(serialized));
}

test(spatialdbtest, testnextid) {
  assert_ok(spatialdb::create(
      spatialdboptions(), dbname_,
      {spatialindexoptions("simple", boundingbox<double>(0, 0, 100, 100), 2)}));

  assert_ok(spatialdb::open(spatialdboptions(), dbname_, &db_));
  assert_ok(db_->insert(writeoptions(), boundingbox<double>(5, 5, 10, 10),
                        "one", featureset(), {"simple"}));
  assert_ok(db_->insert(writeoptions(), boundingbox<double>(10, 10, 15, 15),
                        "two", featureset(), {"simple"}));
  delete db_;

  assert_ok(spatialdb::open(spatialdboptions(), dbname_, &db_));
  assert_ok(db_->insert(writeoptions(), boundingbox<double>(55, 55, 65, 65),
                        "three", featureset(), {"simple"}));
  delete db_;

  assert_ok(spatialdb::open(spatialdboptions(), dbname_, &db_));
  assertcursorresults(boundingbox<double>(0, 0, 100, 100), "simple",
                      {"one", "two", "three"});
  delete db_;
}

test(spatialdbtest, featuresettest) {
  assert_ok(spatialdb::create(
      spatialdboptions(), dbname_,
      {spatialindexoptions("simple", boundingbox<double>(0, 0, 100, 100), 2)}));
  assert_ok(spatialdb::open(spatialdboptions(), dbname_, &db_));

  featureset fs;
  fs.set("a", std::string("b"));
  fs.set("c", std::string("d"));

  assert_ok(db_->insert(writeoptions(), boundingbox<double>(5, 5, 10, 10),
                        "one", fs, {"simple"}));

  cursor* c =
      db_->query(readoptions(), boundingbox<double>(5, 5, 10, 10), "simple");

  assert_true(c->valid());
  assert_eq(c->blob().compare("one"), 0);
  featureset returned = c->feature_set();
  assert_true(returned.contains("a"));
  assert_true(!returned.contains("b"));
  assert_true(returned.contains("c"));
  assert_eq(returned.get("a").type(), variant::kstring);
  assert_eq(returned.get("a").get_string(), "b");
  assert_eq(returned.get("c").type(), variant::kstring);
  assert_eq(returned.get("c").get_string(), "d");

  c->next();
  assert_true(!c->valid());

  delete c;
  delete db_;
}

test(spatialdbtest, simpletest) {
  // iter 0 -- not read only
  // iter 1 -- read only
  for (int iter = 0; iter < 2; ++iter) {
    destroydb(dbname_, options());
    assert_ok(spatialdb::create(
        spatialdboptions(), dbname_,
        {spatialindexoptions("index", boundingbox<double>(0, 0, 128, 128),
                             3)}));
    assert_ok(spatialdb::open(spatialdboptions(), dbname_, &db_));

    assert_ok(db_->insert(writeoptions(), boundingbox<double>(33, 17, 63, 79),
                          "one", featureset(), {"index"}));
    assert_ok(db_->insert(writeoptions(), boundingbox<double>(65, 65, 111, 111),
                          "two", featureset(), {"index"}));
    assert_ok(db_->insert(writeoptions(), boundingbox<double>(1, 49, 127, 63),
                          "three", featureset(), {"index"}));
    assert_ok(db_->insert(writeoptions(), boundingbox<double>(20, 100, 21, 101),
                          "four", featureset(), {"index"}));
    assert_ok(db_->insert(writeoptions(), boundingbox<double>(81, 33, 127, 63),
                          "five", featureset(), {"index"}));
    assert_ok(db_->insert(writeoptions(), boundingbox<double>(1, 65, 47, 95),
                          "six", featureset(), {"index"}));

    if (iter == 1) {
      delete db_;
      assert_ok(spatialdb::open(spatialdboptions(), dbname_, &db_, true));
    }

    assertcursorresults(boundingbox<double>(33, 17, 47, 31), "index", {"one"});
    assertcursorresults(boundingbox<double>(17, 33, 79, 63), "index",
                        {"one", "three"});
    assertcursorresults(boundingbox<double>(17, 81, 63, 111), "index",
                        {"four", "six"});
    assertcursorresults(boundingbox<double>(85, 86, 85, 86), "index", {"two"});
    assertcursorresults(boundingbox<double>(33, 1, 127, 111), "index",
                        {"one", "two", "three", "five", "six"});
    // even though the bounding box doesn't intersect, we got "four" back
    // because
    // it's in the same tile
    assertcursorresults(boundingbox<double>(18, 98, 19, 99), "index", {"four"});
    assertcursorresults(boundingbox<double>(130, 130, 131, 131), "index", {});
    assertcursorresults(boundingbox<double>(81, 17, 127, 31), "index", {});
    assertcursorresults(boundingbox<double>(90, 50, 91, 51), "index",
                        {"three", "five"});

    delete db_;
  }
}

namespace {
std::string randomstr(random* rnd) {
  std::string r;
  for (int k = 0; k < 10; ++k) {
    r.push_back(rnd->uniform(26) + 'a');
  }
  return r;
}

boundingbox<int> randomboundingbox(int limit, random* rnd, int max_size) {
  boundingbox<int> r;
  r.min_x = rnd->uniform(limit - 1);
  r.min_y = rnd->uniform(limit - 1);
  r.max_x = r.min_x + rnd->uniform(std::min(limit - 1 - r.min_x, max_size)) + 1;
  r.max_y = r.min_y + rnd->uniform(std::min(limit - 1 - r.min_y, max_size)) + 1;
  return r;
}

boundingbox<double> scalebb(boundingbox<int> b, double step) {
  return boundingbox<double>(b.min_x * step + 1, b.min_y * step + 1,
                             (b.max_x + 1) * step - 1,
                             (b.max_y + 1) * step - 1);
}

}  // namespace

test(spatialdbtest, randomizedtest) {
  random rnd(301);
  std::vector<std::pair<std::string, boundingbox<int>>> elements;

  boundingbox<double> spatial_index_bounds(0, 0, (1ll << 32), (1ll << 32));
  assert_ok(spatialdb::create(
      spatialdboptions(), dbname_,
      {spatialindexoptions("index", spatial_index_bounds, 7)}));
  assert_ok(spatialdb::open(spatialdboptions(), dbname_, &db_));
  double step = (1ll << 32) / (1 << 7);

  for (int i = 0; i < 1000; ++i) {
    std::string blob = randomstr(&rnd);
    boundingbox<int> bbox = randomboundingbox(128, &rnd, 10);
    assert_ok(db_->insert(writeoptions(), scalebb(bbox, step), blob,
                          featureset(), {"index"}));
    elements.push_back(make_pair(blob, bbox));
  }

  db_->compact();

  for (int i = 0; i < 1000; ++i) {
    boundingbox<int> int_bbox = randomboundingbox(128, &rnd, 10);
    boundingbox<double> double_bbox = scalebb(int_bbox, step);
    std::vector<std::string> blobs;
    for (auto e : elements) {
      if (e.second.intersects(int_bbox)) {
        blobs.push_back(e.first);
      }
    }
    assertcursorresults(double_bbox, "index", blobs);
  }

  delete db_;
}

}  // namespace spatial
}  // namespace rocksdb

int main(int argc, char** argv) { return rocksdb::test::runalltests(); }
