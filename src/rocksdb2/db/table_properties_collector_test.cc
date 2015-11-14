//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.

#include <map>
#include <memory>
#include <string>

#include "db/db_impl.h"
#include "db/dbformat.h"
#include "db/table_properties_collector.h"
#include "rocksdb/table.h"
#include "table/block_based_table_factory.h"
#include "table/meta_blocks.h"
#include "table/plain_table_factory.h"
#include "table/table_builder.h"
#include "util/coding.h"
#include "util/testharness.h"
#include "util/testutil.h"

namespace rocksdb {

class tablepropertiestest {
};

// todo(kailiu) the following classes should be moved to some more general
// places, so that other tests can also make use of them.
// `fakewritablefile` and `fakerandomeaccessfile` bypass the real file system
// and therefore enable us to quickly setup the tests.
class fakewritablefile : public writablefile {
 public:
  ~fakewritablefile() { }

  const std::string& contents() const { return contents_; }

  virtual status close() { return status::ok(); }
  virtual status flush() { return status::ok(); }
  virtual status sync() { return status::ok(); }

  virtual status append(const slice& data) {
    contents_.append(data.data(), data.size());
    return status::ok();
  }

 private:
  std::string contents_;
};


class fakerandomeaccessfile : public randomaccessfile {
 public:
  explicit fakerandomeaccessfile(const slice& contents)
      : contents_(contents.data(), contents.size()) {
  }

  virtual ~fakerandomeaccessfile() { }

  uint64_t size() const { return contents_.size(); }

  virtual status read(uint64_t offset, size_t n, slice* result,
                       char* scratch) const {
    if (offset > contents_.size()) {
      return status::invalidargument("invalid read offset");
    }
    if (offset + n > contents_.size()) {
      n = contents_.size() - offset;
    }
    memcpy(scratch, &contents_[offset], n);
    *result = slice(scratch, n);
    return status::ok();
  }

 private:
  std::string contents_;
};


class dumblogger : public logger {
 public:
  virtual void logv(const char* format, va_list ap) { }
  virtual size_t getlogfilesize() const { return 0; }
};

// utilities test functions
namespace {
void makebuilder(const options& options,
                 const internalkeycomparator& internal_comparator,
                 std::unique_ptr<fakewritablefile>* writable,
                 std::unique_ptr<tablebuilder>* builder) {
  writable->reset(new fakewritablefile);
  builder->reset(options.table_factory->newtablebuilder(
      options, internal_comparator, writable->get(), options.compression));
}
}  // namespace

// collects keys that starts with "a" in a table.
class regularkeysstartwitha: public tablepropertiescollector {
 public:
   const char* name() const { return "regularkeysstartwitha"; }

   status finish(usercollectedproperties* properties) {
     std::string encoded;
     putvarint32(&encoded, count_);
     *properties = usercollectedproperties {
       { "tablepropertiestest", "rocksdb" },
       { "count", encoded }
     };
     return status::ok();
   }

   status add(const slice& user_key, const slice& value) {
     // simply asssume all user keys are not empty.
     if (user_key.data()[0] == 'a') {
       ++count_;
     }
     return status::ok();
   }

  virtual usercollectedproperties getreadableproperties() const {
    return usercollectedproperties{};
  }

 private:
  uint32_t count_ = 0;
};

class regularkeysstartwithafactory : public tablepropertiescollectorfactory {
 public:
  virtual tablepropertiescollector* createtablepropertiescollector() {
    return new regularkeysstartwitha();
  }
  const char* name() const { return "regularkeysstartwitha"; }
};

extern uint64_t kblockbasedtablemagicnumber;
extern uint64_t kplaintablemagicnumber;
namespace {
void testcustomizedtablepropertiescollector(
    uint64_t magic_number, bool encode_as_internal, const options& options,
    const internalkeycomparator& internal_comparator) {
  // make sure the entries will be inserted with order.
  std::map<std::string, std::string> kvs = {
    {"about   ", "val5"},  // starts with 'a'
    {"abstract", "val2"},  // starts with 'a'
    {"around  ", "val7"},  // starts with 'a'
    {"beyond  ", "val3"},
    {"builder ", "val1"},
    {"cancel  ", "val4"},
    {"find    ", "val6"},
  };

  // -- step 1: build table
  std::unique_ptr<tablebuilder> builder;
  std::unique_ptr<fakewritablefile> writable;
  makebuilder(options, internal_comparator, &writable, &builder);

  for (const auto& kv : kvs) {
    if (encode_as_internal) {
      internalkey ikey(kv.first, 0, valuetype::ktypevalue);
      builder->add(ikey.encode(), kv.second);
    } else {
      builder->add(kv.first, kv.second);
    }
  }
  assert_ok(builder->finish());

  // -- step 2: read properties
  fakerandomeaccessfile readable(writable->contents());
  tableproperties* props;
  status s = readtableproperties(
      &readable,
      writable->contents().size(),
      magic_number,
      env::default(),
      nullptr,
      &props
  );
  std::unique_ptr<tableproperties> props_guard(props);
  assert_ok(s);

  auto user_collected = props->user_collected_properties;

  assert_eq("rocksdb", user_collected.at("tablepropertiestest"));

  uint32_t starts_with_a = 0;
  slice key(user_collected.at("count"));
  assert_true(getvarint32(&key, &starts_with_a));
  assert_eq(3u, starts_with_a);
}
}  // namespace

test(tablepropertiestest, customizedtablepropertiescollector) {
  // test properties collectors with internal keys or regular keys
  // for block based table
  for (bool encode_as_internal : { true, false }) {
    options options;
    std::shared_ptr<tablepropertiescollectorfactory> collector_factory(
        new regularkeysstartwithafactory());
    if (encode_as_internal) {
      options.table_properties_collector_factories.emplace_back(
          new userkeytablepropertiescollectorfactory(collector_factory));
    } else {
      options.table_properties_collector_factories.resize(1);
      options.table_properties_collector_factories[0] = collector_factory;
    }
    test::plaininternalkeycomparator ikc(options.comparator);
    testcustomizedtablepropertiescollector(kblockbasedtablemagicnumber,
                                           encode_as_internal, options, ikc);
  }

  // test plain table
  options options;
  options.table_properties_collector_factories.emplace_back(
      new regularkeysstartwithafactory());

  plaintableoptions plain_table_options;
  plain_table_options.user_key_len = 8;
  plain_table_options.bloom_bits_per_key = 8;
  plain_table_options.hash_table_ratio = 0;

  options.table_factory =
      std::make_shared<plaintablefactory>(plain_table_options);
  test::plaininternalkeycomparator ikc(options.comparator);
  testcustomizedtablepropertiescollector(kplaintablemagicnumber, true, options,
                                         ikc);
}

namespace {
void testinternalkeypropertiescollector(
    uint64_t magic_number,
    bool sanitized,
    std::shared_ptr<tablefactory> table_factory) {
  internalkey keys[] = {
    internalkey("a       ", 0, valuetype::ktypevalue),
    internalkey("b       ", 0, valuetype::ktypevalue),
    internalkey("c       ", 0, valuetype::ktypevalue),
    internalkey("w       ", 0, valuetype::ktypedeletion),
    internalkey("x       ", 0, valuetype::ktypedeletion),
    internalkey("y       ", 0, valuetype::ktypedeletion),
    internalkey("z       ", 0, valuetype::ktypedeletion),
  };

  std::unique_ptr<tablebuilder> builder;
  std::unique_ptr<fakewritablefile> writable;
  options options;
  test::plaininternalkeycomparator pikc(options.comparator);

  options.table_factory = table_factory;
  if (sanitized) {
    options.table_properties_collector_factories.emplace_back(
        new regularkeysstartwithafactory());
    // with sanitization, even regular properties collector will be able to
    // handle internal keys.
    auto comparator = options.comparator;
    // hack: set options.info_log to avoid writing log in
    // sanitizeoptions().
    options.info_log = std::make_shared<dumblogger>();
    options = sanitizeoptions("db",            // just a place holder
                              &pikc,
                              options);
    options.comparator = comparator;
  } else {
    options.table_properties_collector_factories = {
        std::make_shared<internalkeypropertiescollectorfactory>()};
  }

  for (int iter = 0; iter < 2; ++iter) {
    makebuilder(options, pikc, &writable, &builder);
    for (const auto& k : keys) {
      builder->add(k.encode(), "val");
    }

    assert_ok(builder->finish());

    fakerandomeaccessfile readable(writable->contents());
    tableproperties* props;
    status s =
        readtableproperties(&readable, writable->contents().size(),
                            magic_number, env::default(), nullptr, &props);
    assert_ok(s);

    std::unique_ptr<tableproperties> props_guard(props);
    auto user_collected = props->user_collected_properties;
    uint64_t deleted = getdeletedkeys(user_collected);
    assert_eq(4u, deleted);

    if (sanitized) {
      uint32_t starts_with_a = 0;
      slice key(user_collected.at("count"));
      assert_true(getvarint32(&key, &starts_with_a));
      assert_eq(1u, starts_with_a);
    }
  }
}
}  // namespace

test(tablepropertiestest, internalkeypropertiescollector) {
  testinternalkeypropertiescollector(
      kblockbasedtablemagicnumber,
      true /* sanitize */,
      std::make_shared<blockbasedtablefactory>()
  );
  testinternalkeypropertiescollector(
      kblockbasedtablemagicnumber,
      true /* not sanitize */,
      std::make_shared<blockbasedtablefactory>()
  );

  plaintableoptions plain_table_options;
  plain_table_options.user_key_len = 8;
  plain_table_options.bloom_bits_per_key = 8;
  plain_table_options.hash_table_ratio = 0;

  testinternalkeypropertiescollector(
      kplaintablemagicnumber, false /* not sanitize */,
      std::make_shared<plaintablefactory>(plain_table_options));
}

}  // namespace rocksdb

int main(int argc, char** argv) {
  return rocksdb::test::runalltests();
}
