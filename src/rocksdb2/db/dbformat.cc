//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
#include "db/dbformat.h"

#include <stdio.h>
#include "port/port.h"
#include "util/coding.h"
#include "util/perf_context_imp.h"

namespace rocksdb {

uint64_t packsequenceandtype(uint64_t seq, valuetype t) {
  assert(seq <= kmaxsequencenumber);
  assert(t <= kvaluetypeforseek);
  return (seq << 8) | t;
}

void appendinternalkey(std::string* result, const parsedinternalkey& key) {
  result->append(key.user_key.data(), key.user_key.size());
  putfixed64(result, packsequenceandtype(key.sequence, key.type));
}

std::string parsedinternalkey::debugstring(bool hex) const {
  char buf[50];
  snprintf(buf, sizeof(buf), "' @ %llu : %d",
           (unsigned long long) sequence,
           int(type));
  std::string result = "'";
  result += user_key.tostring(hex);
  result += buf;
  return result;
}

std::string internalkey::debugstring(bool hex) const {
  std::string result;
  parsedinternalkey parsed;
  if (parseinternalkey(rep_, &parsed)) {
    result = parsed.debugstring(hex);
  } else {
    result = "(bad)";
    result.append(escapestring(rep_));
  }
  return result;
}

const char* internalkeycomparator::name() const {
  return name_.c_str();
}

int internalkeycomparator::compare(const slice& akey, const slice& bkey) const {
  // order by:
  //    increasing user key (according to user-supplied comparator)
  //    decreasing sequence number
  //    decreasing type (though sequence# should be enough to disambiguate)
  int r = user_comparator_->compare(extractuserkey(akey), extractuserkey(bkey));
  perf_counter_add(user_key_comparison_count, 1);
  if (r == 0) {
    const uint64_t anum = decodefixed64(akey.data() + akey.size() - 8);
    const uint64_t bnum = decodefixed64(bkey.data() + bkey.size() - 8);
    if (anum > bnum) {
      r = -1;
    } else if (anum < bnum) {
      r = +1;
    }
  }
  return r;
}

int internalkeycomparator::compare(const parsedinternalkey& a,
                                   const parsedinternalkey& b) const {
  // order by:
  //    increasing user key (according to user-supplied comparator)
  //    decreasing sequence number
  //    decreasing type (though sequence# should be enough to disambiguate)
  int r = user_comparator_->compare(a.user_key, b.user_key);
  perf_counter_add(user_key_comparison_count, 1);
  if (r == 0) {
    if (a.sequence > b.sequence) {
      r = -1;
    } else if (a.sequence < b.sequence) {
      r = +1;
    } else if (a.type > b.type) {
      r = -1;
    } else if (a.type < b.type) {
      r = +1;
    }
  }
  return r;
}

void internalkeycomparator::findshortestseparator(
      std::string* start,
      const slice& limit) const {
  // attempt to shorten the user portion of the key
  slice user_start = extractuserkey(*start);
  slice user_limit = extractuserkey(limit);
  std::string tmp(user_start.data(), user_start.size());
  user_comparator_->findshortestseparator(&tmp, user_limit);
  if (tmp.size() < user_start.size() &&
      user_comparator_->compare(user_start, tmp) < 0) {
    // user key has become shorter physically, but larger logically.
    // tack on the earliest possible number to the shortened user key.
    putfixed64(&tmp, packsequenceandtype(kmaxsequencenumber,kvaluetypeforseek));
    assert(this->compare(*start, tmp) < 0);
    assert(this->compare(tmp, limit) < 0);
    start->swap(tmp);
  }
}

void internalkeycomparator::findshortsuccessor(std::string* key) const {
  slice user_key = extractuserkey(*key);
  std::string tmp(user_key.data(), user_key.size());
  user_comparator_->findshortsuccessor(&tmp);
  if (tmp.size() < user_key.size() &&
      user_comparator_->compare(user_key, tmp) < 0) {
    // user key has become shorter physically, but larger logically.
    // tack on the earliest possible number to the shortened user key.
    putfixed64(&tmp, packsequenceandtype(kmaxsequencenumber,kvaluetypeforseek));
    assert(this->compare(*key, tmp) < 0);
    key->swap(tmp);
  }
}

lookupkey::lookupkey(const slice& user_key, sequencenumber s) {
  size_t usize = user_key.size();
  size_t needed = usize + 13;  // a conservative estimate
  char* dst;
  if (needed <= sizeof(space_)) {
    dst = space_;
  } else {
    dst = new char[needed];
  }
  start_ = dst;
  dst = encodevarint32(dst, usize + 8);
  kstart_ = dst;
  memcpy(dst, user_key.data(), usize);
  dst += usize;
  encodefixed64(dst, packsequenceandtype(s, kvaluetypeforseek));
  dst += 8;
  end_ = dst;
}

}  // namespace rocksdb
