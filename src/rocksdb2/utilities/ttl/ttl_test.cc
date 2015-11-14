// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include <memory>
#include "rocksdb/compaction_filter.h"
#include "rocksdb/utilities/db_ttl.h"
#include "util/testharness.h"
#include "util/logging.h"
#include <map>
#include <unistd.h>

namespace rocksdb {

namespace {

typedef std::map<std::string, std::string> kvmap;

enum batchoperation {
  put = 0,
  delete = 1
};
}

class specialtimeenv : public envwrapper {
 public:
  explicit specialtimeenv(env* base) : envwrapper(base) {
    base->getcurrenttime(&current_time_);
  }

  void sleep(int64_t sleep_time) { current_time_ += sleep_time; }
  virtual status getcurrenttime(int64_t* current_time) {
    *current_time = current_time_;
    return status::ok();
  }

 private:
  int64_t current_time_;
};

class ttltest {
 public:
  ttltest() {
    env_.reset(new specialtimeenv(env::default()));
    dbname_ = test::tmpdir() + "/db_ttl";
    options_.create_if_missing = true;
    options_.env = env_.get();
    // ensure that compaction is kicked in to always strip timestamp from kvs
    options_.max_grandparent_overlap_factor = 0;
    // compaction should take place always from level0 for determinism
    options_.max_mem_compaction_level = 0;
    db_ttl_ = nullptr;
    destroydb(dbname_, options());
  }

  ~ttltest() {
    closettl();
    destroydb(dbname_, options());
  }

  // open database with ttl support when ttl not provided with db_ttl_ pointer
  void openttl() {
    assert_true(db_ttl_ ==
                nullptr);  //  db should be closed before opening again
    assert_ok(dbwithttl::open(options_, dbname_, &db_ttl_));
  }

  // open database with ttl support when ttl provided with db_ttl_ pointer
  void openttl(int32_t ttl) {
    assert_true(db_ttl_ == nullptr);
    assert_ok(dbwithttl::open(options_, dbname_, &db_ttl_, ttl));
  }

  // open with testfilter compaction filter
  void openttlwithtestcompaction(int32_t ttl) {
    options_.compaction_filter_factory =
      std::shared_ptr<compactionfilterfactory>(
          new testfilterfactory(ksamplesize_, knewvalue_));
    openttl(ttl);
  }

  // open database with ttl support in read_only mode
  void openreadonlyttl(int32_t ttl) {
    assert_true(db_ttl_ == nullptr);
    assert_ok(dbwithttl::open(options_, dbname_, &db_ttl_, ttl, true));
  }

  void closettl() {
    delete db_ttl_;
    db_ttl_ = nullptr;
  }

  // populates and returns a kv-map
  void makekvmap(int64_t num_entries) {
    kvmap_.clear();
    int digits = 1;
    for (int dummy = num_entries; dummy /= 10 ; ++digits);
    int digits_in_i = 1;
    for (int64_t i = 0; i < num_entries; i++) {
      std::string key = "key";
      std::string value = "value";
      if (i % 10 == 0) {
        digits_in_i++;
      }
      for(int j = digits_in_i; j < digits; j++) {
        key.append("0");
        value.append("0");
      }
      appendnumberto(&key, i);
      appendnumberto(&value, i);
      kvmap_[key] = value;
    }
    assert_eq((int)kvmap_.size(), num_entries);//check all insertions done
  }

  // makes a write-batch with key-vals from kvmap_ and 'write''s it
  void makeputwritebatch(const batchoperation* batch_ops, int num_ops) {
    assert_le(num_ops, (int)kvmap_.size());
    static writeoptions wopts;
    static flushoptions flush_opts;
    writebatch batch;
    kv_it_ = kvmap_.begin();
    for (int i = 0; i < num_ops && kv_it_ != kvmap_.end(); i++, kv_it_++) {
      switch (batch_ops[i]) {
        case put:
          batch.put(kv_it_->first, kv_it_->second);
          break;
        case delete:
          batch.delete(kv_it_->first);
          break;
        default:
          assert_true(false);
      }
    }
    db_ttl_->write(wopts, &batch);
    db_ttl_->flush(flush_opts);
  }

  // puts num_entries starting from start_pos_map from kvmap_ into the database
  void putvalues(int start_pos_map, int num_entries, bool flush = true,
                 columnfamilyhandle* cf = nullptr) {
    assert_true(db_ttl_);
    assert_le(start_pos_map + num_entries, (int)kvmap_.size());
    static writeoptions wopts;
    static flushoptions flush_opts;
    kv_it_ = kvmap_.begin();
    advance(kv_it_, start_pos_map);
    for (int i = 0; kv_it_ != kvmap_.end() && i < num_entries; i++, kv_it_++) {
      assert_ok(cf == nullptr
                    ? db_ttl_->put(wopts, kv_it_->first, kv_it_->second)
                    : db_ttl_->put(wopts, cf, kv_it_->first, kv_it_->second));
    }
    // put a mock kv at the end because compactionfilter doesn't delete last key
    assert_ok(cf == nullptr ? db_ttl_->put(wopts, "keymock", "valuemock")
                            : db_ttl_->put(wopts, cf, "keymock", "valuemock"));
    if (flush) {
      if (cf == nullptr) {
        db_ttl_->flush(flush_opts);
      } else {
        db_ttl_->flush(flush_opts, cf);
      }
    }
  }

  // runs a manual compaction
  void manualcompact(columnfamilyhandle* cf = nullptr) {
    if (cf == nullptr) {
      db_ttl_->compactrange(nullptr, nullptr);
    } else {
      db_ttl_->compactrange(cf, nullptr, nullptr);
    }
  }

  // checks the whole kvmap_ to return correct values using keymayexist
  void simplekeymayexistcheck() {
    static readoptions ropts;
    bool value_found;
    std::string val;
    for(auto &kv : kvmap_) {
      bool ret = db_ttl_->keymayexist(ropts, kv.first, &val, &value_found);
      if (ret == false || value_found == false) {
        fprintf(stderr, "keymayexist could not find key=%s in the database but"
                        " should have\n", kv.first.c_str());
        assert_true(false);
      } else if (val.compare(kv.second) != 0) {
        fprintf(stderr, " value for key=%s present in database is %s but"
                        " should be %s\n", kv.first.c_str(), val.c_str(),
                        kv.second.c_str());
        assert_true(false);
      }
    }
  }

  // sleeps for slp_tim then runs a manual compaction
  // checks span starting from st_pos from kvmap_ in the db and
  // gets should return true if check is true and false otherwise
  // also checks that value that we got is the same as inserted; and =knewvalue
  //   if test_compaction_change is true
  void sleepcompactcheck(int slp_tim, int st_pos, int span, bool check = true,
                         bool test_compaction_change = false,
                         columnfamilyhandle* cf = nullptr) {
    assert_true(db_ttl_);

    env_->sleep(slp_tim);
    manualcompact(cf);
    static readoptions ropts;
    kv_it_ = kvmap_.begin();
    advance(kv_it_, st_pos);
    std::string v;
    for (int i = 0; kv_it_ != kvmap_.end() && i < span; i++, kv_it_++) {
      status s = (cf == nullptr) ? db_ttl_->get(ropts, kv_it_->first, &v)
                                 : db_ttl_->get(ropts, cf, kv_it_->first, &v);
      if (s.ok() != check) {
        fprintf(stderr, "key=%s ", kv_it_->first.c_str());
        if (!s.ok()) {
          fprintf(stderr, "is absent from db but was expected to be present\n");
        } else {
          fprintf(stderr, "is present in db but was expected to be absent\n");
        }
        assert_true(false);
      } else if (s.ok()) {
          if (test_compaction_change && v.compare(knewvalue_) != 0) {
            fprintf(stderr, " value for key=%s present in database is %s but "
                            " should be %s\n", kv_it_->first.c_str(), v.c_str(),
                            knewvalue_.c_str());
            assert_true(false);
          } else if (!test_compaction_change && v.compare(kv_it_->second) !=0) {
            fprintf(stderr, " value for key=%s present in database is %s but "
                            " should be %s\n", kv_it_->first.c_str(), v.c_str(),
                            kv_it_->second.c_str());
            assert_true(false);
          }
      }
    }
  }

  // similar as sleepcompactcheck but uses ttliterator to read from db
  void sleepcompactcheckiter(int slp, int st_pos, int span, bool check=true) {
    assert_true(db_ttl_);
    env_->sleep(slp);
    manualcompact();
    static readoptions ropts;
    iterator *dbiter = db_ttl_->newiterator(ropts);
    kv_it_ = kvmap_.begin();
    advance(kv_it_, st_pos);

    dbiter->seek(kv_it_->first);
    if (!check) {
      if (dbiter->valid()) {
        assert_ne(dbiter->value().compare(kv_it_->second), 0);
      }
    } else {  // dbiter should have found out kvmap_[st_pos]
      for (int i = st_pos;
           kv_it_ != kvmap_.end() && i < st_pos + span;
           i++, kv_it_++)  {
        assert_true(dbiter->valid());
        assert_eq(dbiter->value().compare(kv_it_->second), 0);
        dbiter->next();
      }
    }
    delete dbiter;
  }

  class testfilter : public compactionfilter {
   public:
    testfilter(const int64_t ksamplesize, const std::string knewvalue)
      : ksamplesize_(ksamplesize),
        knewvalue_(knewvalue) {
    }

    // works on keys of the form "key<number>"
    // drops key if number at the end of key is in [0, ksamplesize_/3),
    // keeps key if it is in [ksamplesize_/3, 2*ksamplesize_/3),
    // change value if it is in [2*ksamplesize_/3, ksamplesize_)
    // eg. ksamplesize_=6. drop:key0-1...keep:key2-3...change:key4-5...
    virtual bool filter(int level, const slice& key,
                        const slice& value, std::string* new_value,
                        bool* value_changed) const override {
      assert(new_value != nullptr);

      std::string search_str = "0123456789";
      std::string key_string = key.tostring();
      size_t pos = key_string.find_first_of(search_str);
      int num_key_end;
      if (pos != std::string::npos) {
        num_key_end = stoi(key_string.substr(pos, key.size() - pos));
      } else {
        return false; // keep keys not matching the format "key<number>"
      }

      int partition = ksamplesize_ / 3;
      if (num_key_end < partition) {
        return true;
      } else if (num_key_end < partition * 2) {
        return false;
      } else {
        *new_value = knewvalue_;
        *value_changed = true;
        return false;
      }
    }

    virtual const char* name() const override {
      return "testfilter";
    }

   private:
    const int64_t ksamplesize_;
    const std::string knewvalue_;
  };

  class testfilterfactory : public compactionfilterfactory {
    public:
      testfilterfactory(const int64_t ksamplesize, const std::string knewvalue)
        : ksamplesize_(ksamplesize),
          knewvalue_(knewvalue) {
      }

      virtual std::unique_ptr<compactionfilter> createcompactionfilter(
          const compactionfilter::context& context) override {
        return std::unique_ptr<compactionfilter>(
            new testfilter(ksamplesize_, knewvalue_));
      }

      virtual const char* name() const override {
        return "testfilterfactory";
      }

    private:
      const int64_t ksamplesize_;
      const std::string knewvalue_;
  };


  // choose carefully so that put, gets & compaction complete in 1 second buffer
  const int64_t ksamplesize_ = 100;
  std::string dbname_;
  dbwithttl* db_ttl_;
  unique_ptr<specialtimeenv> env_;

 private:
  options options_;
  kvmap kvmap_;
  kvmap::iterator kv_it_;
  const std::string knewvalue_ = "new_value";
  unique_ptr<compactionfilter> test_comp_filter_;
}; // class ttltest

// if ttl is non positive or not provided, the behaviour is ttl = infinity
// this test opens the db 3 times with such default behavior and inserts a
// bunch of kvs each time. all kvs should accumulate in the db till the end
// partitions the sample-size provided into 3 sets over boundary1 and boundary2
test(ttltest, noeffect) {
  makekvmap(ksamplesize_);
  int boundary1 = ksamplesize_ / 3;
  int boundary2 = 2 * boundary1;

  openttl();
  putvalues(0, boundary1);                       //t=0: set1 never deleted
  sleepcompactcheck(1, 0, boundary1);            //t=1: set1 still there
  closettl();

  openttl(0);
  putvalues(boundary1, boundary2 - boundary1);   //t=1: set2 never deleted
  sleepcompactcheck(1, 0, boundary2);            //t=2: sets1 & 2 still there
  closettl();

  openttl(-1);
  putvalues(boundary2, ksamplesize_ - boundary2); //t=3: set3 never deleted
  sleepcompactcheck(1, 0, ksamplesize_, true);    //t=4: sets 1,2,3 still there
  closettl();
}

// puts a set of values and checks its presence using get during ttl
test(ttltest, presentduringttl) {
  makekvmap(ksamplesize_);

  openttl(2);                                 // t=0:open the db with ttl = 2
  putvalues(0, ksamplesize_);                  // t=0:insert set1. delete at t=2
  sleepcompactcheck(1, 0, ksamplesize_, true); // t=1:set1 should still be there
  closettl();
}

// puts a set of values and checks its absence using get after ttl
test(ttltest, absentafterttl) {
  makekvmap(ksamplesize_);

  openttl(1);                                  // t=0:open the db with ttl = 2
  putvalues(0, ksamplesize_);                  // t=0:insert set1. delete at t=2
  sleepcompactcheck(2, 0, ksamplesize_, false); // t=2:set1 should not be there
  closettl();
}

// resets the timestamp of a set of kvs by updating them and checks that they
// are not deleted according to the old timestamp
test(ttltest, resettimestamp) {
  makekvmap(ksamplesize_);

  openttl(3);
  putvalues(0, ksamplesize_);            // t=0: insert set1. delete at t=3
  env_->sleep(2);                        // t=2
  putvalues(0, ksamplesize_);            // t=2: insert set1. delete at t=5
  sleepcompactcheck(2, 0, ksamplesize_); // t=4: set1 should still be there
  closettl();
}

// similar to presentduringttl but uses iterator
test(ttltest, iterpresentduringttl) {
  makekvmap(ksamplesize_);

  openttl(2);
  putvalues(0, ksamplesize_);                 // t=0: insert. delete at t=2
  sleepcompactcheckiter(1, 0, ksamplesize_);  // t=1: set should be there
  closettl();
}

// similar to absentafterttl but uses iterator
test(ttltest, iterabsentafterttl) {
  makekvmap(ksamplesize_);

  openttl(1);
  putvalues(0, ksamplesize_);                      // t=0: insert. delete at t=1
  sleepcompactcheckiter(2, 0, ksamplesize_, false); // t=2: should not be there
  closettl();
}

// checks presence while opening the same db more than once with the same ttl
// note: the second open will open the same db
test(ttltest, multiopensamepresent) {
  makekvmap(ksamplesize_);

  openttl(2);
  putvalues(0, ksamplesize_);                   // t=0: insert. delete at t=2
  closettl();

  openttl(2);                                  // t=0. delete at t=2
  sleepcompactcheck(1, 0, ksamplesize_);        // t=1: set should be there
  closettl();
}

// checks absence while opening the same db more than once with the same ttl
// note: the second open will open the same db
test(ttltest, multiopensameabsent) {
  makekvmap(ksamplesize_);

  openttl(1);
  putvalues(0, ksamplesize_);                   // t=0: insert. delete at t=1
  closettl();

  openttl(1);                                  // t=0.delete at t=1
  sleepcompactcheck(2, 0, ksamplesize_, false); // t=2: set should not be there
  closettl();
}

// checks presence while opening the same db more than once with bigger ttl
test(ttltest, multiopendifferent) {
  makekvmap(ksamplesize_);

  openttl(1);
  putvalues(0, ksamplesize_);            // t=0: insert. delete at t=1
  closettl();

  openttl(3);                           // t=0: set deleted at t=3
  sleepcompactcheck(2, 0, ksamplesize_); // t=2: set should be there
  closettl();
}

// checks presence during ttl in read_only mode
test(ttltest, readonlypresentforever) {
  makekvmap(ksamplesize_);

  openttl(1);                                 // t=0:open the db normally
  putvalues(0, ksamplesize_);                  // t=0:insert set1. delete at t=1
  closettl();

  openreadonlyttl(1);
  sleepcompactcheck(2, 0, ksamplesize_);       // t=2:set1 should still be there
  closettl();
}

// checks whether writebatch works well with ttl
// puts all kvs in kvmap_ in a batch and writes first, then deletes first half
test(ttltest, writebatchtest) {
  makekvmap(ksamplesize_);
  batchoperation batch_ops[ksamplesize_];
  for (int i = 0; i < ksamplesize_; i++) {
    batch_ops[i] = put;
  }

  openttl(2);
  makeputwritebatch(batch_ops, ksamplesize_);
  for (int i = 0; i < ksamplesize_ / 2; i++) {
    batch_ops[i] = delete;
  }
  makeputwritebatch(batch_ops, ksamplesize_ / 2);
  sleepcompactcheck(0, 0, ksamplesize_ / 2, false);
  sleepcompactcheck(0, ksamplesize_ / 2, ksamplesize_ - ksamplesize_ / 2);
  closettl();
}

// checks user's compaction filter for correctness with ttl logic
test(ttltest, compactionfilter) {
  makekvmap(ksamplesize_);

  openttlwithtestcompaction(1);
  putvalues(0, ksamplesize_);                  // t=0:insert set1. delete at t=1
  // t=2: ttl logic takes precedence over testfilter:-set1 should not be there
  sleepcompactcheck(2, 0, ksamplesize_, false);
  closettl();

  openttlwithtestcompaction(3);
  putvalues(0, ksamplesize_);                   // t=0:insert set1.
  int partition = ksamplesize_ / 3;
  sleepcompactcheck(1, 0, partition, false);   // part dropped
  sleepcompactcheck(0, partition, partition);  // part kept
  sleepcompactcheck(0, 2 * partition, partition, true, true); // part changed
  closettl();
}

// insert some key-values which keymayexist should be able to get and check that
// values returned are fine
test(ttltest, keymayexist) {
  makekvmap(ksamplesize_);

  openttl();
  putvalues(0, ksamplesize_, false);

  simplekeymayexistcheck();

  closettl();
}

test(ttltest, columnfamiliestest) {
  db* db;
  options options;
  options.create_if_missing = true;
  options.env = env_.get();

  db::open(options, dbname_, &db);
  columnfamilyhandle* handle;
  assert_ok(db->createcolumnfamily(columnfamilyoptions(options),
                                   "ttl_column_family", &handle));

  delete handle;
  delete db;

  std::vector<columnfamilydescriptor> column_families;
  column_families.push_back(columnfamilydescriptor(
      kdefaultcolumnfamilyname, columnfamilyoptions(options)));
  column_families.push_back(columnfamilydescriptor(
      "ttl_column_family", columnfamilyoptions(options)));

  std::vector<columnfamilyhandle*> handles;

  assert_ok(dbwithttl::open(dboptions(options), dbname_, column_families,
                            &handles, &db_ttl_, {3, 5}, false));
  assert_eq(handles.size(), 2u);
  columnfamilyhandle* new_handle;
  assert_ok(db_ttl_->createcolumnfamilywithttl(options, "ttl_column_family_2",
                                               &new_handle, 2));
  handles.push_back(new_handle);

  makekvmap(ksamplesize_);
  putvalues(0, ksamplesize_, false, handles[0]);
  putvalues(0, ksamplesize_, false, handles[1]);
  putvalues(0, ksamplesize_, false, handles[2]);

  // everything should be there after 1 second
  sleepcompactcheck(1, 0, ksamplesize_, true, false, handles[0]);
  sleepcompactcheck(0, 0, ksamplesize_, true, false, handles[1]);
  sleepcompactcheck(0, 0, ksamplesize_, true, false, handles[2]);

  // only column family 1 should be alive after 4 seconds
  sleepcompactcheck(3, 0, ksamplesize_, false, false, handles[0]);
  sleepcompactcheck(0, 0, ksamplesize_, true, false, handles[1]);
  sleepcompactcheck(0, 0, ksamplesize_, false, false, handles[2]);

  // nothing should be there after 6 seconds
  sleepcompactcheck(2, 0, ksamplesize_, false, false, handles[0]);
  sleepcompactcheck(0, 0, ksamplesize_, false, false, handles[1]);
  sleepcompactcheck(0, 0, ksamplesize_, false, false, handles[2]);

  for (auto h : handles) {
    delete h;
  }
  delete db_ttl_;
  db_ttl_ = nullptr;
}

} //  namespace rocksdb

// a black-box test for the ttl wrapper around rocksdb
int main(int argc, char** argv) {
  return rocksdb::test::runalltests();
}
