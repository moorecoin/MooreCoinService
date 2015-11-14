// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#pragma once
#include "rocksdb/db.h"

namespace rocksdb {

// this class contains apis to stack rocksdb wrappers.eg. stack ttl over base d
class stackabledb : public db {
 public:
  // stackabledb is the owner of db now!
  explicit stackabledb(db* db) : db_(db) {}

  ~stackabledb() {
    delete db_;
  }

  virtual db* getbasedb() {
    return db_;
  }

  virtual status createcolumnfamily(const columnfamilyoptions& options,
                                    const std::string& column_family_name,
                                    columnfamilyhandle** handle) {
    return db_->createcolumnfamily(options, column_family_name, handle);
  }

  virtual status dropcolumnfamily(columnfamilyhandle* column_family) {
    return db_->dropcolumnfamily(column_family);
  }

  using db::put;
  virtual status put(const writeoptions& options,
                     columnfamilyhandle* column_family, const slice& key,
                     const slice& val) override {
    return db_->put(options, column_family, key, val);
  }

  using db::get;
  virtual status get(const readoptions& options,
                     columnfamilyhandle* column_family, const slice& key,
                     std::string* value) override {
    return db_->get(options, column_family, key, value);
  }

  using db::multiget;
  virtual std::vector<status> multiget(
      const readoptions& options,
      const std::vector<columnfamilyhandle*>& column_family,
      const std::vector<slice>& keys,
      std::vector<std::string>* values) override {
    return db_->multiget(options, column_family, keys, values);
  }

  using db::keymayexist;
  virtual bool keymayexist(const readoptions& options,
                           columnfamilyhandle* column_family, const slice& key,
                           std::string* value,
                           bool* value_found = nullptr) override {
    return db_->keymayexist(options, column_family, key, value, value_found);
  }

  using db::delete;
  virtual status delete(const writeoptions& wopts,
                        columnfamilyhandle* column_family,
                        const slice& key) override {
    return db_->delete(wopts, column_family, key);
  }

  using db::merge;
  virtual status merge(const writeoptions& options,
                       columnfamilyhandle* column_family, const slice& key,
                       const slice& value) override {
    return db_->merge(options, column_family, key, value);
  }


  virtual status write(const writeoptions& opts, writebatch* updates)
    override {
      return db_->write(opts, updates);
  }

  using db::newiterator;
  virtual iterator* newiterator(const readoptions& opts,
                                columnfamilyhandle* column_family) override {
    return db_->newiterator(opts, column_family);
  }

  virtual status newiterators(
      const readoptions& options,
      const std::vector<columnfamilyhandle*>& column_families,
      std::vector<iterator*>* iterators) {
    return db_->newiterators(options, column_families, iterators);
  }


  virtual const snapshot* getsnapshot() override {
    return db_->getsnapshot();
  }

  virtual void releasesnapshot(const snapshot* snapshot) override {
    return db_->releasesnapshot(snapshot);
  }

  using db::getproperty;
  virtual bool getproperty(columnfamilyhandle* column_family,
                           const slice& property, std::string* value) override {
    return db_->getproperty(column_family, property, value);
  }

  using db::getintproperty;
  virtual bool getintproperty(columnfamilyhandle* column_family,
                              const slice& property, uint64_t* value) override {
    return db_->getintproperty(column_family, property, value);
  }

  using db::getapproximatesizes;
  virtual void getapproximatesizes(columnfamilyhandle* column_family,
                                   const range* r, int n,
                                   uint64_t* sizes) override {
      return db_->getapproximatesizes(column_family, r, n, sizes);
  }

  using db::compactrange;
  virtual status compactrange(columnfamilyhandle* column_family,
                              const slice* begin, const slice* end,
                              bool reduce_level = false, int target_level = -1,
                              uint32_t target_path_id = 0) override {
    return db_->compactrange(column_family, begin, end, reduce_level,
                             target_level, target_path_id);
  }

  using db::numberlevels;
  virtual int numberlevels(columnfamilyhandle* column_family) override {
    return db_->numberlevels(column_family);
  }

  using db::maxmemcompactionlevel;
  virtual int maxmemcompactionlevel(columnfamilyhandle* column_family)
      override {
    return db_->maxmemcompactionlevel(column_family);
  }

  using db::level0stopwritetrigger;
  virtual int level0stopwritetrigger(columnfamilyhandle* column_family)
      override {
    return db_->level0stopwritetrigger(column_family);
  }

  virtual const std::string& getname() const override {
    return db_->getname();
  }

  virtual env* getenv() const override {
    return db_->getenv();
  }

  using db::getoptions;
  virtual const options& getoptions(columnfamilyhandle* column_family) const
      override {
    return db_->getoptions(column_family);
  }

  using db::flush;
  virtual status flush(const flushoptions& fopts,
                       columnfamilyhandle* column_family) override {
    return db_->flush(fopts, column_family);
  }

  virtual status disablefiledeletions() override {
    return db_->disablefiledeletions();
  }

  virtual status enablefiledeletions(bool force) override {
    return db_->enablefiledeletions(force);
  }

  virtual void getlivefilesmetadata(
      std::vector<livefilemetadata>* metadata) override {
    db_->getlivefilesmetadata(metadata);
  }

  virtual status getlivefiles(std::vector<std::string>& vec, uint64_t* mfs,
                              bool flush_memtable = true) override {
      return db_->getlivefiles(vec, mfs, flush_memtable);
  }

  virtual sequencenumber getlatestsequencenumber() const override {
    return db_->getlatestsequencenumber();
  }

  virtual status getsortedwalfiles(vectorlogptr& files) override {
    return db_->getsortedwalfiles(files);
  }

  virtual status deletefile(std::string name) override {
    return db_->deletefile(name);
  }

  virtual status getdbidentity(std::string& identity) {
    return db_->getdbidentity(identity);
  }

  using db::getpropertiesofalltables;
  virtual status getpropertiesofalltables(columnfamilyhandle* column_family,
                                          tablepropertiescollection* props) {
    return db_->getpropertiesofalltables(column_family, props);
  }

  virtual status getupdatessince(
      sequencenumber seq_number, unique_ptr<transactionlogiterator>* iter,
      const transactionlogiterator::readoptions& read_options) override {
    return db_->getupdatessince(seq_number, iter, read_options);
  }

  virtual columnfamilyhandle* defaultcolumnfamily() const override {
    return db_->defaultcolumnfamily();
  }

 protected:
  db* db_;
};

} //  namespace rocksdb
