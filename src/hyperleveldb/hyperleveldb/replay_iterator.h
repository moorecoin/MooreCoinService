// copyright (c) 2013 the hyperleveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#ifndef storage_hyperleveldb_include_replay_iterator_h_
#define storage_hyperleveldb_include_replay_iterator_h_

#include "slice.h"
#include "status.h"

namespace hyperleveldb {

class replayiterator {
 public:
  replayiterator();

  // an iterator is either positioned at a deleted key, present key/value pair,
  // or not valid.  this method returns true iff the iterator is valid.
  virtual bool valid() = 0;

  // moves to the next entry in the source.  after this call, valid() is
  // true iff the iterator was not positioned at the last entry in the source.
  // requires: valid()
  virtual void next() = 0;

  // return true if the current entry points to a key-value pair.  if this
  // returns false, it means the current entry is a deleted entry.
  virtual bool hasvalue() = 0;

  // return the key for the current entry.  the underlying storage for
  // the returned slice is valid only until the next modification of
  // the iterator.
  // requires: valid()
  virtual slice key() const = 0;

  // return the value for the current entry.  the underlying storage for
  // the returned slice is valid only until the next modification of
  // the iterator.
  // requires: !atend() && !atstart()
  virtual slice value() const = 0;

  // if an error has occurred, return it.  else return an ok status.
  virtual status status() const = 0;

 protected:
  // must be released by giving it back to the db
  virtual ~replayiterator();

 private:
  // no copying allowed
  replayiterator(const replayiterator&);
  void operator=(const replayiterator&);
};

}  // namespace leveldb

#endif  // storage_leveldb_include_replay_iterator_h_
