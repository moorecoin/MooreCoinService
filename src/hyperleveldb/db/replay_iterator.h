// copyright (c) 2013 the hyperleveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#ifndef storage_hyperleveldb_db_rolling_iterator_h_
#define storage_hyperleveldb_db_rolling_iterator_h_

#include <stdint.h>
#include <list>
#include "../hyperleveldb/db.h"
#include "../hyperleveldb/replay_iterator.h"
#include "dbformat.h"
#include "memtable.h"

namespace hyperleveldb {

class dbimpl;
struct replaystate {
  replaystate(iterator* i, sequencenumber s, sequencenumber l);
  replaystate(memtable* m, sequencenumber s);
  memtable* mem_;
  iterator* iter_;
  sequencenumber seq_start_;
  sequencenumber seq_limit_;
};

class replayiteratorimpl : public replayiterator {
 public:
  // refs the memtable on its own; caller must hold mutex while creating this
  replayiteratorimpl(dbimpl* db, port::mutex* mutex, const comparator* cmp,
      iterator* iter, memtable* m, sequencenumber s);
  virtual bool valid();
  virtual void next();
  virtual bool hasvalue();
  virtual slice key() const;
  virtual slice value() const;
  virtual status status() const;

  // extra interface

  // we ref the memtable; caller holds mutex passed into ctor
  // requires: caller must hold mutex passed into ctor
  void enqueue(memtable* m, sequencenumber s);

  // requires: caller must hold mutex passed into ctor
  void cleanup(); // calls delete this;

 private:
  virtual ~replayiteratorimpl();
  bool parsekey(parsedinternalkey* ikey);
  bool parsekey(const slice& k, parsedinternalkey* ikey);
  void prime();

  dbimpl* const db_;
  port::mutex* mutex_;
  const comparator* const user_comparator_;
  sequencenumber const start_at_;
  bool valid_;
  status status_;

  bool has_current_user_key_;
  std::string current_user_key_;
  sequencenumber current_user_sequence_;

  replaystate rs_;
  std::list<replaystate> mems_;
};

}  // namespace leveldb

#endif  // storage_leveldb_db_rolling_iterator_h_
