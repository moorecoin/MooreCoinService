// protocol buffers - google's data interchange format
// copyright 2008 google inc.  all rights reserved.
// http://code.google.com/p/protobuf/
//
// redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * neither the name of google inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// this software is provided by the copyright holders and contributors
// "as is" and any express or implied warranties, including, but not
// limited to, the implied warranties of merchantability and fitness for
// a particular purpose are disclaimed. in no event shall the copyright
// owner or contributors be liable for any direct, indirect, incidental,
// special, exemplary, or consequential damages (including, but not
// limited to, procurement of substitute goods or services; loss of use,
// data, or profits; or business interruption) however caused and on any
// theory of liability, whether in contract, strict liability, or tort
// (including negligence or otherwise) arising in any way out of the use
// of this software, even if advised of the possibility of such damage.

// author: kenton@google.com (kenton varda)
//  based on original protocol buffers design by
//  sanjay ghemawat, jeff dean, and others.
//
// since the reflection interface for dynamicmessage is implemented by
// genericmessagereflection, the only thing we really have to test is
// that dynamicmessage correctly sets up the information that
// genericmessagereflection needs to use.  so, we focus on that in this
// test.  other tests, such as generic_message_reflection_unittest and
// reflection_ops_unittest, cover the rest of the functionality used by
// dynamicmessage.

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/test_util.h>
#include <google/protobuf/unittest.pb.h>

#include <google/protobuf/testing/googletest.h>
#include <gtest/gtest.h>

namespace google {
namespace protobuf {

class dynamicmessagetest : public testing::test {
 protected:
  descriptorpool pool_;
  dynamicmessagefactory factory_;
  const descriptor* descriptor_;
  const message* prototype_;
  const descriptor* extensions_descriptor_;
  const message* extensions_prototype_;
  const descriptor* packed_descriptor_;
  const message* packed_prototype_;

  dynamicmessagetest(): factory_(&pool_) {}

  virtual void setup() {
    // we want to make sure that dynamicmessage works (particularly with
    // extensions) even if we use descriptors that are *not* from compiled-in
    // types, so we make copies of the descriptors for unittest.proto and
    // unittest_import.proto.
    filedescriptorproto unittest_file;
    filedescriptorproto unittest_import_file;
    filedescriptorproto unittest_import_public_file;

    unittest::testalltypes::descriptor()->file()->copyto(&unittest_file);
    unittest_import::importmessage::descriptor()->file()->copyto(
      &unittest_import_file);
    unittest_import::publicimportmessage::descriptor()->file()->copyto(
      &unittest_import_public_file);

    assert_true(pool_.buildfile(unittest_import_public_file) != null);
    assert_true(pool_.buildfile(unittest_import_file) != null);
    assert_true(pool_.buildfile(unittest_file) != null);

    descriptor_ = pool_.findmessagetypebyname("protobuf_unittest.testalltypes");
    assert_true(descriptor_ != null);
    prototype_ = factory_.getprototype(descriptor_);

    extensions_descriptor_ =
      pool_.findmessagetypebyname("protobuf_unittest.testallextensions");
    assert_true(extensions_descriptor_ != null);
    extensions_prototype_ = factory_.getprototype(extensions_descriptor_);

    packed_descriptor_ =
      pool_.findmessagetypebyname("protobuf_unittest.testpackedtypes");
    assert_true(packed_descriptor_ != null);
    packed_prototype_ = factory_.getprototype(packed_descriptor_);
  }
};

test_f(dynamicmessagetest, descriptor) {
  // check that the descriptor on the dynamicmessage matches the descriptor
  // passed to getprototype().
  expect_eq(prototype_->getdescriptor(), descriptor_);
}

test_f(dynamicmessagetest, oneprototype) {
  // check that requesting the same prototype twice produces the same object.
  expect_eq(prototype_, factory_.getprototype(descriptor_));
}

test_f(dynamicmessagetest, defaults) {
  // check that all default values are set correctly in the initial message.
  testutil::reflectiontester reflection_tester(descriptor_);
  reflection_tester.expectclearviareflection(*prototype_);
}

test_f(dynamicmessagetest, independentoffsets) {
  // check that all fields have independent offsets by setting each
  // one to a unique value then checking that they all still have those
  // unique values (i.e. they don't stomp each other).
  scoped_ptr<message> message(prototype_->new());
  testutil::reflectiontester reflection_tester(descriptor_);

  reflection_tester.setallfieldsviareflection(message.get());
  reflection_tester.expectallfieldssetviareflection(*message);
}

test_f(dynamicmessagetest, extensions) {
  // check that extensions work.
  scoped_ptr<message> message(extensions_prototype_->new());
  testutil::reflectiontester reflection_tester(extensions_descriptor_);

  reflection_tester.setallfieldsviareflection(message.get());
  reflection_tester.expectallfieldssetviareflection(*message);
}

test_f(dynamicmessagetest, packedfields) {
  // check that packed fields work properly.
  scoped_ptr<message> message(packed_prototype_->new());
  testutil::reflectiontester reflection_tester(packed_descriptor_);

  reflection_tester.setpackedfieldsviareflection(message.get());
  reflection_tester.expectpackedfieldssetviareflection(*message);
}

test_f(dynamicmessagetest, spaceused) {
  // test that spaceused() works properly

  // since we share the implementation with generated messages, we don't need
  // to test very much here.  just make sure it appears to be working.

  scoped_ptr<message> message(prototype_->new());
  testutil::reflectiontester reflection_tester(descriptor_);

  int initial_space_used = message->spaceused();

  reflection_tester.setallfieldsviareflection(message.get());
  expect_lt(initial_space_used, message->spaceused());
}

}  // namespace protobuf
}  // namespace google
