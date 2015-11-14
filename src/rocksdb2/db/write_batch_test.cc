//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "rocksdb/db.h"

#include <memory>
#include "db/memtable.h"
#include "db/column_family.h"
#include "db/write_batch_internal.h"
#include "rocksdb/env.h"
#include "rocksdb/memtablerep.h"
#include "rocksdb/utilities/write_batch_with_index.h"
#include "util/logging.h"
#include "util/testharness.h"

namespace rocksdb {

static std::string printcontents(writebatch* b) {
  internalkeycomparator cmp(bytewisecomparator());
  auto factory = std::make_shared<skiplistfactory>();
  options options;
  options.memtable_factory = factory;
  memtable* mem = new memtable(cmp, options);
  mem->ref();
  std::string state;
  columnfamilymemtablesdefault cf_mems_default(mem, &options);
  status s = writebatchinternal::insertinto(b, &cf_mems_default);
  int count = 0;
  iterator* iter = mem->newiterator(readoptions());
  for (iter->seektofirst(); iter->valid(); iter->next()) {
    parsedinternalkey ikey;
    memset((void *)&ikey, 0, sizeof(ikey));
    assert_true(parseinternalkey(iter->key(), &ikey));
    switch (ikey.type) {
      case ktypevalue:
        state.append("put(");
        state.append(ikey.user_key.tostring());
        state.append(", ");
        state.append(iter->value().tostring());
        state.append(")");
        count++;
        break;
      case ktypemerge:
        state.append("merge(");
        state.append(ikey.user_key.tostring());
        state.append(", ");
        state.append(iter->value().tostring());
        state.append(")");
        count++;
        break;
      case ktypedeletion:
        state.append("delete(");
        state.append(ikey.user_key.tostring());
        state.append(")");
        count++;
        break;
      default:
        assert(false);
        break;
    }
    state.append("@");
    state.append(numbertostring(ikey.sequence));
  }
  delete iter;
  if (!s.ok()) {
    state.append(s.tostring());
  } else if (count != writebatchinternal::count(b)) {
    state.append("countmismatch()");
  }
  delete mem->unref();
  return state;
}

class writebatchtest { };

test(writebatchtest, empty) {
  writebatch batch;
  assert_eq("", printcontents(&batch));
  assert_eq(0, writebatchinternal::count(&batch));
  assert_eq(0, batch.count());
}

test(writebatchtest, multiple) {
  writebatch batch;
  batch.put(slice("foo"), slice("bar"));
  batch.delete(slice("box"));
  batch.put(slice("baz"), slice("boo"));
  writebatchinternal::setsequence(&batch, 100);
  assert_eq(100u, writebatchinternal::sequence(&batch));
  assert_eq(3, writebatchinternal::count(&batch));
  assert_eq("put(baz, boo)@102"
            "delete(box)@101"
            "put(foo, bar)@100",
            printcontents(&batch));
  assert_eq(3, batch.count());
}

test(writebatchtest, corruption) {
  writebatch batch;
  batch.put(slice("foo"), slice("bar"));
  batch.delete(slice("box"));
  writebatchinternal::setsequence(&batch, 200);
  slice contents = writebatchinternal::contents(&batch);
  writebatchinternal::setcontents(&batch,
                                  slice(contents.data(),contents.size()-1));
  assert_eq("put(foo, bar)@200"
            "corruption: bad writebatch delete",
            printcontents(&batch));
}

test(writebatchtest, append) {
  writebatch b1, b2;
  writebatchinternal::setsequence(&b1, 200);
  writebatchinternal::setsequence(&b2, 300);
  writebatchinternal::append(&b1, &b2);
  assert_eq("",
            printcontents(&b1));
  assert_eq(0, b1.count());
  b2.put("a", "va");
  writebatchinternal::append(&b1, &b2);
  assert_eq("put(a, va)@200",
            printcontents(&b1));
  assert_eq(1, b1.count());
  b2.clear();
  b2.put("b", "vb");
  writebatchinternal::append(&b1, &b2);
  assert_eq("put(a, va)@200"
            "put(b, vb)@201",
            printcontents(&b1));
  assert_eq(2, b1.count());
  b2.delete("foo");
  writebatchinternal::append(&b1, &b2);
  assert_eq("put(a, va)@200"
            "put(b, vb)@202"
            "put(b, vb)@201"
            "delete(foo)@203",
            printcontents(&b1));
  assert_eq(4, b1.count());
}

namespace {
  struct testhandler : public writebatch::handler {
    std::string seen;
    virtual status putcf(uint32_t column_family_id, const slice& key,
                       const slice& value) {
      if (column_family_id == 0) {
        seen += "put(" + key.tostring() + ", " + value.tostring() + ")";
      } else {
        seen += "putcf(" + std::to_string(column_family_id) + ", " +
                key.tostring() + ", " + value.tostring() + ")";
      }
      return status::ok();
    }
    virtual status mergecf(uint32_t column_family_id, const slice& key,
                         const slice& value) {
      if (column_family_id == 0) {
        seen += "merge(" + key.tostring() + ", " + value.tostring() + ")";
      } else {
        seen += "mergecf(" + std::to_string(column_family_id) + ", " +
                key.tostring() + ", " + value.tostring() + ")";
      }
      return status::ok();
    }
    virtual void logdata(const slice& blob) {
      seen += "logdata(" + blob.tostring() + ")";
    }
    virtual status deletecf(uint32_t column_family_id, const slice& key) {
      if (column_family_id == 0) {
        seen += "delete(" + key.tostring() + ")";
      } else {
        seen += "deletecf(" + std::to_string(column_family_id) + ", " +
                key.tostring() + ")";
      }
      return status::ok();
    }
  };
}

test(writebatchtest, blob) {
  writebatch batch;
  batch.put(slice("k1"), slice("v1"));
  batch.put(slice("k2"), slice("v2"));
  batch.put(slice("k3"), slice("v3"));
  batch.putlogdata(slice("blob1"));
  batch.delete(slice("k2"));
  batch.putlogdata(slice("blob2"));
  batch.merge(slice("foo"), slice("bar"));
  assert_eq(5, batch.count());
  assert_eq("merge(foo, bar)@4"
            "put(k1, v1)@0"
            "delete(k2)@3"
            "put(k2, v2)@1"
            "put(k3, v3)@2",
            printcontents(&batch));

  testhandler handler;
  batch.iterate(&handler);
  assert_eq(
            "put(k1, v1)"
            "put(k2, v2)"
            "put(k3, v3)"
            "logdata(blob1)"
            "delete(k2)"
            "logdata(blob2)"
            "merge(foo, bar)",
            handler.seen);
}

test(writebatchtest, continue) {
  writebatch batch;

  struct handler : public testhandler {
    int num_seen = 0;
    virtual status putcf(uint32_t column_family_id, const slice& key,
                       const slice& value) {
      ++num_seen;
      return testhandler::putcf(column_family_id, key, value);
    }
    virtual status mergecf(uint32_t column_family_id, const slice& key,
                         const slice& value) {
      ++num_seen;
      return testhandler::mergecf(column_family_id, key, value);
    }
    virtual void logdata(const slice& blob) {
      ++num_seen;
      testhandler::logdata(blob);
    }
    virtual status deletecf(uint32_t column_family_id, const slice& key) {
      ++num_seen;
      return testhandler::deletecf(column_family_id, key);
    }
    virtual bool continue() override {
      return num_seen < 3;
    }
  } handler;

  batch.put(slice("k1"), slice("v1"));
  batch.putlogdata(slice("blob1"));
  batch.delete(slice("k1"));
  batch.putlogdata(slice("blob2"));
  batch.merge(slice("foo"), slice("bar"));
  batch.iterate(&handler);
  assert_eq(
            "put(k1, v1)"
            "logdata(blob1)"
            "delete(k1)",
            handler.seen);
}

test(writebatchtest, putgatherslices) {
  writebatch batch;
  batch.put(slice("foo"), slice("bar"));

  {
    // try a write where the key is one slice but the value is two
    slice key_slice("baz");
    slice value_slices[2] = { slice("header"), slice("payload") };
    batch.put(sliceparts(&key_slice, 1),
              sliceparts(value_slices, 2));
  }

  {
    // one where the key is composite but the value is a single slice
    slice key_slices[3] = { slice("key"), slice("part2"), slice("part3") };
    slice value_slice("value");
    batch.put(sliceparts(key_slices, 3),
              sliceparts(&value_slice, 1));
  }

  writebatchinternal::setsequence(&batch, 100);
  assert_eq("put(baz, headerpayload)@101"
            "put(foo, bar)@100"
            "put(keypart2part3, value)@102",
            printcontents(&batch));
  assert_eq(3, batch.count());
}

namespace {
class columnfamilyhandleimpldummy : public columnfamilyhandleimpl {
 public:
  explicit columnfamilyhandleimpldummy(int id)
      : columnfamilyhandleimpl(nullptr, nullptr, nullptr), id_(id) {}
  uint32_t getid() const override { return id_; }

 private:
  uint32_t id_;
};
}  // namespace anonymous

test(writebatchtest, columnfamiliesbatchtest) {
  writebatch batch;
  columnfamilyhandleimpldummy zero(0), two(2), three(3), eight(8);
  batch.put(&zero, slice("foo"), slice("bar"));
  batch.put(&two, slice("twofoo"), slice("bar2"));
  batch.put(&eight, slice("eightfoo"), slice("bar8"));
  batch.delete(&eight, slice("eightfoo"));
  batch.merge(&three, slice("threethree"), slice("3three"));
  batch.put(&zero, slice("foo"), slice("bar"));
  batch.merge(slice("omom"), slice("nom"));

  testhandler handler;
  batch.iterate(&handler);
  assert_eq(
      "put(foo, bar)"
      "putcf(2, twofoo, bar2)"
      "putcf(8, eightfoo, bar8)"
      "deletecf(8, eightfoo)"
      "mergecf(3, threethree, 3three)"
      "put(foo, bar)"
      "merge(omom, nom)",
      handler.seen);
}

test(writebatchtest, columnfamiliesbatchwithindextest) {
  writebatchwithindex batch(bytewisecomparator(), 20);
  columnfamilyhandleimpldummy zero(0), two(2), three(3), eight(8);
  batch.put(&zero, slice("foo"), slice("bar"));
  batch.put(&two, slice("twofoo"), slice("bar2"));
  batch.put(&eight, slice("eightfoo"), slice("bar8"));
  batch.delete(&eight, slice("eightfoo"));
  batch.merge(&three, slice("threethree"), slice("3three"));
  batch.put(&zero, slice("foo"), slice("bar"));
  batch.merge(slice("omom"), slice("nom"));

  std::unique_ptr<wbwiiterator> iter;

  iter.reset(batch.newiterator(&eight));
  iter->seek("eightfoo");
  assert_ok(iter->status());
  assert_true(iter->valid());
  assert_eq(writetype::kputrecord, iter->entry().type);
  assert_eq("eightfoo", iter->entry().key.tostring());
  assert_eq("bar8", iter->entry().value.tostring());

  iter->next();
  assert_ok(iter->status());
  assert_true(iter->valid());
  assert_eq(writetype::kdeleterecord, iter->entry().type);
  assert_eq("eightfoo", iter->entry().key.tostring());

  iter->next();
  assert_ok(iter->status());
  assert_true(!iter->valid());

  iter.reset(batch.newiterator());
  iter->seek("gggg");
  assert_ok(iter->status());
  assert_true(iter->valid());
  assert_eq(writetype::kmergerecord, iter->entry().type);
  assert_eq("omom", iter->entry().key.tostring());
  assert_eq("nom", iter->entry().value.tostring());

  iter->next();
  assert_ok(iter->status());
  assert_true(!iter->valid());

  iter.reset(batch.newiterator(&zero));
  iter->seek("foo");
  assert_ok(iter->status());
  assert_true(iter->valid());
  assert_eq(writetype::kputrecord, iter->entry().type);
  assert_eq("foo", iter->entry().key.tostring());
  assert_eq("bar", iter->entry().value.tostring());

  iter->next();
  assert_ok(iter->status());
  assert_true(iter->valid());
  assert_eq(writetype::kputrecord, iter->entry().type);
  assert_eq("foo", iter->entry().key.tostring());
  assert_eq("bar", iter->entry().value.tostring());

  iter->next();
  assert_ok(iter->status());
  assert_true(iter->valid());
  assert_eq(writetype::kmergerecord, iter->entry().type);
  assert_eq("omom", iter->entry().key.tostring());
  assert_eq("nom", iter->entry().value.tostring());

  iter->next();
  assert_ok(iter->status());
  assert_true(!iter->valid());

  testhandler handler;
  batch.getwritebatch()->iterate(&handler);
  assert_eq(
      "put(foo, bar)"
      "putcf(2, twofoo, bar2)"
      "putcf(8, eightfoo, bar8)"
      "deletecf(8, eightfoo)"
      "mergecf(3, threethree, 3three)"
      "put(foo, bar)"
      "merge(omom, nom)",
      handler.seen);
}

}  // namespace rocksdb

int main(int argc, char** argv) {
  return rocksdb::test::runalltests();
}
