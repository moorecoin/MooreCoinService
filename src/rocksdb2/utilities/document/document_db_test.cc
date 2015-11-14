//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.

#include <algorithm>

#include "rocksdb/utilities/json_document.h"
#include "rocksdb/utilities/document_db.h"

#include "util/testharness.h"
#include "util/testutil.h"

namespace rocksdb {

class documentdbtest {
 public:
  documentdbtest() {
    dbname_ = test::tmpdir() + "/document_db_test";
    destroydb(dbname_, options());
  }
  ~documentdbtest() {
    delete db_;
    destroydb(dbname_, options());
  }

  void assertcursorids(cursor* cursor, std::vector<int64_t> expected) {
    std::vector<int64_t> got;
    while (cursor->valid()) {
      assert_true(cursor->valid());
      assert_true(cursor->document().contains("_id"));
      got.push_back(cursor->document()["_id"].getint64());
      cursor->next();
    }
    std::sort(expected.begin(), expected.end());
    std::sort(got.begin(), got.end());
    assert_true(got == expected);
  }

  // converts ' to ", so that we don't have to escape " all over the place
  std::string convertquotes(const std::string& input) {
    std::string output;
    for (auto x : input) {
      if (x == '\'') {
        output.push_back('\"');
      } else {
        output.push_back(x);
      }
    }
    return output;
  }

  void createindexes(std::vector<documentdb::indexdescriptor> indexes) {
    for (auto i : indexes) {
      assert_ok(db_->createindex(writeoptions(), i));
    }
  }

  jsondocument* parse(const std::string doc) {
    return jsondocument::parsejson(convertquotes(doc).c_str());
  }

  std::string dbname_;
  documentdb* db_;
};

test(documentdbtest, simplequerytest) {
  documentdboptions options;
  documentdb::indexdescriptor index;
  index.description = parse("{'name': 1}");
  index.name = "name_index";

  assert_ok(documentdb::open(options, dbname_, {}, &db_));
  createindexes({index});
  delete db_;
  // now there is index present
  assert_ok(documentdb::open(options, dbname_, {index}, &db_));
  delete index.description;

  std::vector<std::string> json_objects = {
      "{'_id': 1, 'name': 'one'}",   "{'_id': 2, 'name': 'two'}",
      "{'_id': 3, 'name': 'three'}", "{'_id': 4, 'name': 'four'}"};

  for (auto& json : json_objects) {
    std::unique_ptr<jsondocument> document(parse(json));
    assert_true(document.get() != nullptr);
    assert_ok(db_->insert(writeoptions(), *document));
  }

  // inserting a document with existing primary key should return failure
  {
    std::unique_ptr<jsondocument> document(parse(json_objects[0]));
    assert_true(document.get() != nullptr);
    status s = db_->insert(writeoptions(), *document);
    assert_true(s.isinvalidargument());
  }

  // find equal to "two"
  {
    std::unique_ptr<jsondocument> query(
        parse("[{'$filter': {'name': 'two', '$index': 'name_index'}}]"));
    std::unique_ptr<cursor> cursor(db_->query(readoptions(), *query));
    assertcursorids(cursor.get(), {2});
  }

  // find less than "three"
  {
    std::unique_ptr<jsondocument> query(parse(
        "[{'$filter': {'name': {'$lt': 'three'}, '$index': "
        "'name_index'}}]"));
    std::unique_ptr<cursor> cursor(db_->query(readoptions(), *query));

    assertcursorids(cursor.get(), {1, 4});
  }

  // find less than "three" without index
  {
    std::unique_ptr<jsondocument> query(
        parse("[{'$filter': {'name': {'$lt': 'three'} }}]"));
    std::unique_ptr<cursor> cursor(db_->query(readoptions(), *query));
    assertcursorids(cursor.get(), {1, 4});
  }

  // remove less or equal to "three"
  {
    std::unique_ptr<jsondocument> query(
        parse("{'name': {'$lte': 'three'}, '$index': 'name_index'}"));
    assert_ok(db_->remove(readoptions(), writeoptions(), *query));
  }

  // find all -- only "two" left, everything else should be deleted
  {
    std::unique_ptr<jsondocument> query(parse("[]"));
    std::unique_ptr<cursor> cursor(db_->query(readoptions(), *query));
    assertcursorids(cursor.get(), {2});
  }
}

test(documentdbtest, complexquerytest) {
  documentdboptions options;
  documentdb::indexdescriptor priority_index;
  priority_index.description = parse("{'priority': 1}");
  priority_index.name = "priority";
  documentdb::indexdescriptor job_name_index;
  job_name_index.description = parse("{'job_name': 1}");
  job_name_index.name = "job_name";
  documentdb::indexdescriptor progress_index;
  progress_index.description = parse("{'progress': 1}");
  progress_index.name = "progress";

  assert_ok(documentdb::open(options, dbname_, {}, &db_));
  createindexes({priority_index, progress_index});
  delete priority_index.description;
  delete progress_index.description;

  std::vector<std::string> json_objects = {
      "{'_id': 1, 'job_name': 'play', 'priority': 10, 'progress': 14.2}",
      "{'_id': 2, 'job_name': 'white', 'priority': 2, 'progress': 45.1}",
      "{'_id': 3, 'job_name': 'straw', 'priority': 5, 'progress': 83.2}",
      "{'_id': 4, 'job_name': 'temporary', 'priority': 3, 'progress': 14.9}",
      "{'_id': 5, 'job_name': 'white', 'priority': 4, 'progress': 44.2}",
      "{'_id': 6, 'job_name': 'tea', 'priority': 1, 'progress': 12.4}",
      "{'_id': 7, 'job_name': 'delete', 'priority': 2, 'progress': 77.54}",
      "{'_id': 8, 'job_name': 'rock', 'priority': 3, 'progress': 93.24}",
      "{'_id': 9, 'job_name': 'steady', 'priority': 3, 'progress': 9.1}",
      "{'_id': 10, 'job_name': 'white', 'priority': 1, 'progress': 61.4}",
      "{'_id': 11, 'job_name': 'who', 'priority': 4, 'progress': 39.41}", };

  // add index on the fly!
  createindexes({job_name_index});
  delete job_name_index.description;

  for (auto& json : json_objects) {
    std::unique_ptr<jsondocument> document(parse(json));
    assert_true(document != nullptr);
    assert_ok(db_->insert(writeoptions(), *document));
  }

  // 2 < priority < 4 and progress > 10.0, index priority
  {
    std::unique_ptr<jsondocument> query(parse(
        "[{'$filter': {'priority': {'$lt': 4, '$gt': 2}, 'progress': {'$gt': "
        "10.0}, '$index': 'priority'}}]"));
    std::unique_ptr<cursor> cursor(db_->query(readoptions(), *query));
    assertcursorids(cursor.get(), {4, 8});
  }

  // 2 < priority < 4 and progress > 10.0, index progress
  {
    std::unique_ptr<jsondocument> query(parse(
        "[{'$filter': {'priority': {'$lt': 4, '$gt': 2}, 'progress': {'$gt': "
        "10.0}, '$index': 'progress'}}]"));
    std::unique_ptr<cursor> cursor(db_->query(readoptions(), *query));
    assertcursorids(cursor.get(), {4, 8});
  }

  // job_name == 'white' and priority >= 2, index job_name
  {
    std::unique_ptr<jsondocument> query(parse(
        "[{'$filter': {'job_name': 'white', 'priority': {'$gte': "
        "2}, '$index': 'job_name'}}]"));
    std::unique_ptr<cursor> cursor(db_->query(readoptions(), *query));
    assertcursorids(cursor.get(), {2, 5});
  }

  // 35.0 <= progress < 65.5, index progress
  {
    std::unique_ptr<jsondocument> query(parse(
        "[{'$filter': {'progress': {'$gt': 5.0, '$gte': 35.0, '$lt': 65.5}, "
        "'$index': 'progress'}}]"));
    std::unique_ptr<cursor> cursor(db_->query(readoptions(), *query));
    assertcursorids(cursor.get(), {2, 5, 10, 11});
  }

  // 2 < priority <= 4, index priority
  {
    std::unique_ptr<jsondocument> query(parse(
        "[{'$filter': {'priority': {'$gt': 2, '$lt': 8, '$lte': 4}, "
        "'$index': 'priority'}}]"));
    std::unique_ptr<cursor> cursor(db_->query(readoptions(), *query));
    assertcursorids(cursor.get(), {4, 5, 8, 9, 11});
  }

  // delete all whose progress is bigger than 50%
  {
    std::unique_ptr<jsondocument> query(
        parse("{'progress': {'$gt': 50.0}, '$index': 'progress'}"));
    assert_ok(db_->remove(readoptions(), writeoptions(), *query));
  }

  // 2 < priority < 6, index priority
  {
    std::unique_ptr<jsondocument> query(parse(
        "[{'$filter': {'priority': {'$gt': 2, '$lt': 6}, "
        "'$index': 'priority'}}]"));
    std::unique_ptr<cursor> cursor(db_->query(readoptions(), *query));
    assertcursorids(cursor.get(), {4, 5, 9, 11});
  }

  // update set priority to 10 where job_name is 'white'
  {
    std::unique_ptr<jsondocument> query(parse("{'job_name': 'white'}"));
    std::unique_ptr<jsondocument> update(parse("{'$set': {'priority': 10}}"));
    assert_ok(db_->update(readoptions(), writeoptions(), *query, *update));
  }

  // 4 < priority
  {
    std::unique_ptr<jsondocument> query(
        parse("[{'$filter': {'priority': {'$gt': 4}, '$index': 'priority'}}]"));
    std::unique_ptr<cursor> cursor(db_->query(readoptions(), *query));
    assert_ok(cursor->status());
    assertcursorids(cursor.get(), {1, 2, 5});
  }

  status s = db_->dropindex("doesnt-exist");
  assert_true(!s.ok());
  assert_ok(db_->dropindex("priority"));
}

}  //  namespace rocksdb

int main(int argc, char** argv) { return rocksdb::test::runalltests(); }
