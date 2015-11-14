//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.

#include <set>

#include "rocksdb/utilities/json_document.h"

#include "util/testutil.h"
#include "util/testharness.h"

namespace rocksdb {
namespace {
void assertfield(const jsondocument& json, const std::string& field) {
  assert_true(json.contains(field));
  assert_true(json[field].isnull());
}

void assertfield(const jsondocument& json, const std::string& field,
                 const std::string& expected) {
  assert_true(json.contains(field));
  assert_true(json[field].isstring());
  assert_eq(expected, json[field].getstring());
}

void assertfield(const jsondocument& json, const std::string& field,
                 int64_t expected) {
  assert_true(json.contains(field));
  assert_true(json[field].isint64());
  assert_eq(expected, json[field].getint64());
}

void assertfield(const jsondocument& json, const std::string& field,
                 bool expected) {
  assert_true(json.contains(field));
  assert_true(json[field].isbool());
  assert_eq(expected, json[field].getbool());
}

void assertfield(const jsondocument& json, const std::string& field,
                 double expected) {
  assert_true(json.contains(field));
  assert_true(json[field].isdouble());
  assert_eq(expected, json[field].getdouble());
}
}  // namespace

class jsondocumenttest {
 public:
  void assertsamplejson(const jsondocument& json) {
    assertfield(json, "title", std::string("json"));
    assertfield(json, "type", std::string("object"));
    // properties
    assert_true(json.contains("properties"));
    assert_true(json["properties"].contains("flags"));
    assert_true(json["properties"]["flags"].isarray());
    assert_eq(3u, json["properties"]["flags"].count());
    assert_true(json["properties"]["flags"][0].isint64());
    assert_eq(10, json["properties"]["flags"][0].getint64());
    assert_true(json["properties"]["flags"][1].isstring());
    assert_eq("parse", json["properties"]["flags"][1].getstring());
    assert_true(json["properties"]["flags"][2].isobject());
    assertfield(json["properties"]["flags"][2], "tag", std::string("no"));
    assertfield(json["properties"]["flags"][2], std::string("status"));
    assertfield(json["properties"], "age", 110.5e-4);
    assertfield(json["properties"], "depth", static_cast<int64_t>(-10));
    // test iteration
    std::set<std::string> expected({"flags", "age", "depth"});
    for (auto item : json["properties"].items()) {
      auto iter = expected.find(item.first);
      assert_true(iter != expected.end());
      expected.erase(iter);
    }
    assert_eq(0u, expected.size());
    assert_true(json.contains("latlong"));
    assert_true(json["latlong"].isarray());
    assert_eq(2u, json["latlong"].count());
    assert_true(json["latlong"][0].isdouble());
    assert_eq(53.25, json["latlong"][0].getdouble());
    assert_true(json["latlong"][1].isdouble());
    assert_eq(43.75, json["latlong"][1].getdouble());
    assertfield(json, "enabled", true);
  }

  const std::string ksamplejson =
      "{ \"title\" : \"json\", \"type\" : \"object\", \"properties\" : { "
      "\"flags\": [10, \"parse\", {\"tag\": \"no\", \"status\": null}], "
      "\"age\": 110.5e-4, \"depth\": -10 }, \"latlong\": [53.25, 43.75], "
      "\"enabled\": true }";

  const std::string ksamplejsondifferent =
      "{ \"title\" : \"json\", \"type\" : \"object\", \"properties\" : { "
      "\"flags\": [10, \"parse\", {\"tag\": \"no\", \"status\": 2}], "
      "\"age\": 110.5e-4, \"depth\": -10 }, \"latlong\": [53.25, 43.75], "
      "\"enabled\": true }";
};

test(jsondocumenttest, parsing) {
  jsondocument x(static_cast<int64_t>(5));
  assert_true(x.isint64());

  // make sure it's correctly parsed
  auto parsed_json = jsondocument::parsejson(ksamplejson.c_str());
  assert_true(parsed_json != nullptr);
  assertsamplejson(*parsed_json);

  // test deep copying
  jsondocument copied_json_document(*parsed_json);
  assertsamplejson(copied_json_document);
  assert_true(copied_json_document == *parsed_json);
  delete parsed_json;

  auto parsed_different_sample =
      jsondocument::parsejson(ksamplejsondifferent.c_str());
  assert_true(parsed_different_sample != nullptr);
  assert_true(!(*parsed_different_sample == copied_json_document));
  delete parsed_different_sample;

  // parse error
  const std::string kfaultyjson =
      ksamplejson.substr(0, ksamplejson.size() - 10);
  assert_true(jsondocument::parsejson(kfaultyjson.c_str()) == nullptr);
}

test(jsondocumenttest, serialization) {
  auto parsed_json = jsondocument::parsejson(ksamplejson.c_str());
  assert_true(parsed_json != nullptr);
  std::string serialized;
  parsed_json->serialize(&serialized);
  delete parsed_json;

  auto deserialized_json = jsondocument::deserialize(slice(serialized));
  assert_true(deserialized_json != nullptr);
  assertsamplejson(*deserialized_json);
  delete deserialized_json;

  // deserialization failure
  assert_true(jsondocument::deserialize(
                  slice(serialized.data(), serialized.size() - 10)) == nullptr);
}

test(jsondocumenttest, mutation) {
  auto sample_json = jsondocument::parsejson(ksamplejson.c_str());
  assert_true(sample_json != nullptr);
  auto different_json = jsondocument::parsejson(ksamplejsondifferent.c_str());
  assert_true(different_json != nullptr);

  (*different_json)["properties"]["flags"][2].set("status", jsondocument());

  assert_true(*different_json == *sample_json);

  delete different_json;
  delete sample_json;

  auto json1 = jsondocument::parsejson("{\"a\": [1, 2, 3]}");
  auto json2 = jsondocument::parsejson("{\"a\": [2, 2, 3, 4]}");
  assert_true(json1 != nullptr && json2 != nullptr);

  (*json1)["a"].setinarray(0, static_cast<int64_t>(2))->pushback(
      static_cast<int64_t>(4));
  assert_true(*json1 == *json2);

  delete json1;
  delete json2;
}

}  //  namespace rocksdb

int main(int argc, char** argv) { return rocksdb::test::runalltests(); }
