//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "db/dbformat.h"
#include "util/logging.h"
#include "util/testharness.h"

namespace rocksdb {

static std::string ikey(const std::string& user_key,
                        uint64_t seq,
                        valuetype vt) {
  std::string encoded;
  appendinternalkey(&encoded, parsedinternalkey(user_key, seq, vt));
  return encoded;
}

static std::string shorten(const std::string& s, const std::string& l) {
  std::string result = s;
  internalkeycomparator(bytewisecomparator()).findshortestseparator(&result, l);
  return result;
}

static std::string shortsuccessor(const std::string& s) {
  std::string result = s;
  internalkeycomparator(bytewisecomparator()).findshortsuccessor(&result);
  return result;
}

static void testkey(const std::string& key,
                    uint64_t seq,
                    valuetype vt) {
  std::string encoded = ikey(key, seq, vt);

  slice in(encoded);
  parsedinternalkey decoded("", 0, ktypevalue);

  assert_true(parseinternalkey(in, &decoded));
  assert_eq(key, decoded.user_key.tostring());
  assert_eq(seq, decoded.sequence);
  assert_eq(vt, decoded.type);

  assert_true(!parseinternalkey(slice("bar"), &decoded));
}

class formattest { };

test(formattest, internalkey_encodedecode) {
  const char* keys[] = { "", "k", "hello", "longggggggggggggggggggggg" };
  const uint64_t seq[] = {
    1, 2, 3,
    (1ull << 8) - 1, 1ull << 8, (1ull << 8) + 1,
    (1ull << 16) - 1, 1ull << 16, (1ull << 16) + 1,
    (1ull << 32) - 1, 1ull << 32, (1ull << 32) + 1
  };
  for (unsigned int k = 0; k < sizeof(keys) / sizeof(keys[0]); k++) {
    for (unsigned int s = 0; s < sizeof(seq) / sizeof(seq[0]); s++) {
      testkey(keys[k], seq[s], ktypevalue);
      testkey("hello", 1, ktypedeletion);
    }
  }
}

test(formattest, internalkeyshortseparator) {
  // when user keys are same
  assert_eq(ikey("foo", 100, ktypevalue),
            shorten(ikey("foo", 100, ktypevalue),
                    ikey("foo", 99, ktypevalue)));
  assert_eq(ikey("foo", 100, ktypevalue),
            shorten(ikey("foo", 100, ktypevalue),
                    ikey("foo", 101, ktypevalue)));
  assert_eq(ikey("foo", 100, ktypevalue),
            shorten(ikey("foo", 100, ktypevalue),
                    ikey("foo", 100, ktypevalue)));
  assert_eq(ikey("foo", 100, ktypevalue),
            shorten(ikey("foo", 100, ktypevalue),
                    ikey("foo", 100, ktypedeletion)));

  // when user keys are misordered
  assert_eq(ikey("foo", 100, ktypevalue),
            shorten(ikey("foo", 100, ktypevalue),
                    ikey("bar", 99, ktypevalue)));

  // when user keys are different, but correctly ordered
  assert_eq(ikey("g", kmaxsequencenumber, kvaluetypeforseek),
            shorten(ikey("foo", 100, ktypevalue),
                    ikey("hello", 200, ktypevalue)));

  // when start user key is prefix of limit user key
  assert_eq(ikey("foo", 100, ktypevalue),
            shorten(ikey("foo", 100, ktypevalue),
                    ikey("foobar", 200, ktypevalue)));

  // when limit user key is prefix of start user key
  assert_eq(ikey("foobar", 100, ktypevalue),
            shorten(ikey("foobar", 100, ktypevalue),
                    ikey("foo", 200, ktypevalue)));
}

test(formattest, internalkeyshortestsuccessor) {
  assert_eq(ikey("g", kmaxsequencenumber, kvaluetypeforseek),
            shortsuccessor(ikey("foo", 100, ktypevalue)));
  assert_eq(ikey("\xff\xff", 100, ktypevalue),
            shortsuccessor(ikey("\xff\xff", 100, ktypevalue)));
}

test(formattest, iterkeyoperation) {
  iterkey k;
  const char p[] = "abcdefghijklmnopqrstuvwxyz";
  const char q[] = "0123456789";

  assert_eq(std::string(k.getkey().data(), k.getkey().size()),
            std::string(""));

  k.trimappend(0, p, 3);
  assert_eq(std::string(k.getkey().data(), k.getkey().size()),
            std::string("abc"));

  k.trimappend(1, p, 3);
  assert_eq(std::string(k.getkey().data(), k.getkey().size()),
            std::string("aabc"));

  k.trimappend(0, p, 26);
  assert_eq(std::string(k.getkey().data(), k.getkey().size()),
            std::string("abcdefghijklmnopqrstuvwxyz"));

  k.trimappend(26, q, 10);
  assert_eq(std::string(k.getkey().data(), k.getkey().size()),
            std::string("abcdefghijklmnopqrstuvwxyz0123456789"));

  k.trimappend(36, q, 1);
  assert_eq(std::string(k.getkey().data(), k.getkey().size()),
            std::string("abcdefghijklmnopqrstuvwxyz01234567890"));

  k.trimappend(26, q, 1);
  assert_eq(std::string(k.getkey().data(), k.getkey().size()),
            std::string("abcdefghijklmnopqrstuvwxyz0"));

  // size going up, memory allocation is triggered
  k.trimappend(27, p, 26);
  assert_eq(std::string(k.getkey().data(), k.getkey().size()),
            std::string("abcdefghijklmnopqrstuvwxyz0"
              "abcdefghijklmnopqrstuvwxyz"));
}

}  // namespace rocksdb

int main(int argc, char** argv) {
  return rocksdb::test::runalltests();
}
