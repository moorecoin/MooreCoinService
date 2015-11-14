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

#include <google/protobuf/wire_format.h>
#include <google/protobuf/wire_format_lite_inl.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/unittest.pb.h>
#include <google/protobuf/unittest_mset.pb.h>
#include <google/protobuf/test_util.h>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/testing/googletest.h>
#include <gtest/gtest.h>
#include <google/protobuf/stubs/stl_util.h>

namespace google {
namespace protobuf {
namespace internal {
namespace {

test(wireformattest, enumsinsync) {
  // verify that wireformatlite::fieldtype and wireformatlite::cpptype match
  // fielddescriptor::type and fielddescriptor::cpptype.

  expect_eq(implicit_cast<int>(fielddescriptor::max_type),
            implicit_cast<int>(wireformatlite::max_field_type));
  expect_eq(implicit_cast<int>(fielddescriptor::max_cpptype),
            implicit_cast<int>(wireformatlite::max_cpptype));

  for (int i = 1; i <= wireformatlite::max_field_type; i++) {
    expect_eq(
      implicit_cast<int>(fielddescriptor::typetocpptype(
        static_cast<fielddescriptor::type>(i))),
      implicit_cast<int>(wireformatlite::fieldtypetocpptype(
        static_cast<wireformatlite::fieldtype>(i))));
  }
}

test(wireformattest, maxfieldnumber) {
  // make sure the max field number constant is accurate.
  expect_eq((1 << (32 - wireformatlite::ktagtypebits)) - 1,
            fielddescriptor::kmaxnumber);
}

test(wireformattest, parse) {
  unittest::testalltypes source, dest;
  string data;

  // serialize using the generated code.
  testutil::setallfields(&source);
  source.serializetostring(&data);

  // parse using wireformat.
  io::arrayinputstream raw_input(data.data(), data.size());
  io::codedinputstream input(&raw_input);
  wireformat::parseandmergepartial(&input, &dest);

  // check.
  testutil::expectallfieldsset(dest);
}

test(wireformattest, parseextensions) {
  unittest::testallextensions source, dest;
  string data;

  // serialize using the generated code.
  testutil::setallextensions(&source);
  source.serializetostring(&data);

  // parse using wireformat.
  io::arrayinputstream raw_input(data.data(), data.size());
  io::codedinputstream input(&raw_input);
  wireformat::parseandmergepartial(&input, &dest);

  // check.
  testutil::expectallextensionsset(dest);
}

test(wireformattest, parsepacked) {
  unittest::testpackedtypes source, dest;
  string data;

  // serialize using the generated code.
  testutil::setpackedfields(&source);
  source.serializetostring(&data);

  // parse using wireformat.
  io::arrayinputstream raw_input(data.data(), data.size());
  io::codedinputstream input(&raw_input);
  wireformat::parseandmergepartial(&input, &dest);

  // check.
  testutil::expectpackedfieldsset(dest);
}

test(wireformattest, parsepackedfromunpacked) {
  // serialize using the generated code.
  unittest::testunpackedtypes source;
  testutil::setunpackedfields(&source);
  string data = source.serializeasstring();

  // parse using wireformat.
  unittest::testpackedtypes dest;
  io::arrayinputstream raw_input(data.data(), data.size());
  io::codedinputstream input(&raw_input);
  wireformat::parseandmergepartial(&input, &dest);

  // check.
  testutil::expectpackedfieldsset(dest);
}

test(wireformattest, parseunpackedfrompacked) {
  // serialize using the generated code.
  unittest::testpackedtypes source;
  testutil::setpackedfields(&source);
  string data = source.serializeasstring();

  // parse using wireformat.
  unittest::testunpackedtypes dest;
  io::arrayinputstream raw_input(data.data(), data.size());
  io::codedinputstream input(&raw_input);
  wireformat::parseandmergepartial(&input, &dest);

  // check.
  testutil::expectunpackedfieldsset(dest);
}

test(wireformattest, parsepackedextensions) {
  unittest::testpackedextensions source, dest;
  string data;

  // serialize using the generated code.
  testutil::setpackedextensions(&source);
  source.serializetostring(&data);

  // parse using wireformat.
  io::arrayinputstream raw_input(data.data(), data.size());
  io::codedinputstream input(&raw_input);
  wireformat::parseandmergepartial(&input, &dest);

  // check.
  testutil::expectpackedextensionsset(dest);
}

test(wireformattest, bytesize) {
  unittest::testalltypes message;
  testutil::setallfields(&message);

  expect_eq(message.bytesize(), wireformat::bytesize(message));
  message.clear();
  expect_eq(0, message.bytesize());
  expect_eq(0, wireformat::bytesize(message));
}

test(wireformattest, bytesizeextensions) {
  unittest::testallextensions message;
  testutil::setallextensions(&message);

  expect_eq(message.bytesize(),
            wireformat::bytesize(message));
  message.clear();
  expect_eq(0, message.bytesize());
  expect_eq(0, wireformat::bytesize(message));
}

test(wireformattest, bytesizepacked) {
  unittest::testpackedtypes message;
  testutil::setpackedfields(&message);

  expect_eq(message.bytesize(), wireformat::bytesize(message));
  message.clear();
  expect_eq(0, message.bytesize());
  expect_eq(0, wireformat::bytesize(message));
}

test(wireformattest, bytesizepackedextensions) {
  unittest::testpackedextensions message;
  testutil::setpackedextensions(&message);

  expect_eq(message.bytesize(),
            wireformat::bytesize(message));
  message.clear();
  expect_eq(0, message.bytesize());
  expect_eq(0, wireformat::bytesize(message));
}

test(wireformattest, serialize) {
  unittest::testalltypes message;
  string generated_data;
  string dynamic_data;

  testutil::setallfields(&message);
  int size = message.bytesize();

  // serialize using the generated code.
  {
    io::stringoutputstream raw_output(&generated_data);
    io::codedoutputstream output(&raw_output);
    message.serializewithcachedsizes(&output);
    assert_false(output.haderror());
  }

  // serialize using wireformat.
  {
    io::stringoutputstream raw_output(&dynamic_data);
    io::codedoutputstream output(&raw_output);
    wireformat::serializewithcachedsizes(message, size, &output);
    assert_false(output.haderror());
  }

  // should be the same.
  // don't use expect_eq here because we're comparing raw binary data and
  // we really don't want it dumped to stdout on failure.
  expect_true(dynamic_data == generated_data);
}

test(wireformattest, serializeextensions) {
  unittest::testallextensions message;
  string generated_data;
  string dynamic_data;

  testutil::setallextensions(&message);
  int size = message.bytesize();

  // serialize using the generated code.
  {
    io::stringoutputstream raw_output(&generated_data);
    io::codedoutputstream output(&raw_output);
    message.serializewithcachedsizes(&output);
    assert_false(output.haderror());
  }

  // serialize using wireformat.
  {
    io::stringoutputstream raw_output(&dynamic_data);
    io::codedoutputstream output(&raw_output);
    wireformat::serializewithcachedsizes(message, size, &output);
    assert_false(output.haderror());
  }

  // should be the same.
  // don't use expect_eq here because we're comparing raw binary data and
  // we really don't want it dumped to stdout on failure.
  expect_true(dynamic_data == generated_data);
}

test(wireformattest, serializefieldsandextensions) {
  unittest::testfieldorderings message;
  string generated_data;
  string dynamic_data;

  testutil::setallfieldsandextensions(&message);
  int size = message.bytesize();

  // serialize using the generated code.
  {
    io::stringoutputstream raw_output(&generated_data);
    io::codedoutputstream output(&raw_output);
    message.serializewithcachedsizes(&output);
    assert_false(output.haderror());
  }

  // serialize using wireformat.
  {
    io::stringoutputstream raw_output(&dynamic_data);
    io::codedoutputstream output(&raw_output);
    wireformat::serializewithcachedsizes(message, size, &output);
    assert_false(output.haderror());
  }

  // should be the same.
  // don't use expect_eq here because we're comparing raw binary data and
  // we really don't want it dumped to stdout on failure.
  expect_true(dynamic_data == generated_data);

  // should output in canonical order.
  testutil::expectallfieldsandextensionsinorder(dynamic_data);
  testutil::expectallfieldsandextensionsinorder(generated_data);
}

test(wireformattest, parsemultipleextensionranges) {
  // make sure we can parse a message that contains multiple extensions ranges.
  unittest::testfieldorderings source;
  string data;

  testutil::setallfieldsandextensions(&source);
  source.serializetostring(&data);

  {
    unittest::testfieldorderings dest;
    expect_true(dest.parsefromstring(data));
    expect_eq(source.debugstring(), dest.debugstring());
  }

  // also test using reflection-based parsing.
  {
    unittest::testfieldorderings dest;
    io::arrayinputstream raw_input(data.data(), data.size());
    io::codedinputstream coded_input(&raw_input);
    expect_true(wireformat::parseandmergepartial(&coded_input, &dest));
    expect_eq(source.debugstring(), dest.debugstring());
  }
}

const int kunknowntypeid = 1550055;

test(wireformattest, serializemessageset) {
  // set up a testmessageset with two known messages and an unknown one.
  unittest::testmessageset message_set;
  message_set.mutableextension(
    unittest::testmessagesetextension1::message_set_extension)->set_i(123);
  message_set.mutableextension(
    unittest::testmessagesetextension2::message_set_extension)->set_str("foo");
  message_set.mutable_unknown_fields()->addlengthdelimited(
    kunknowntypeid, "bar");

  string data;
  assert_true(message_set.serializetostring(&data));

  // parse back using rawmessageset and check the contents.
  unittest::rawmessageset raw;
  assert_true(raw.parsefromstring(data));

  expect_eq(0, raw.unknown_fields().field_count());

  assert_eq(3, raw.item_size());
  expect_eq(
    unittest::testmessagesetextension1::descriptor()->extension(0)->number(),
    raw.item(0).type_id());
  expect_eq(
    unittest::testmessagesetextension2::descriptor()->extension(0)->number(),
    raw.item(1).type_id());
  expect_eq(kunknowntypeid, raw.item(2).type_id());

  unittest::testmessagesetextension1 message1;
  expect_true(message1.parsefromstring(raw.item(0).message()));
  expect_eq(123, message1.i());

  unittest::testmessagesetextension2 message2;
  expect_true(message2.parsefromstring(raw.item(1).message()));
  expect_eq("foo", message2.str());

  expect_eq("bar", raw.item(2).message());
}

test(wireformattest, serializemessagesetvariouswaysareequal) {
  // serialize a messageset to a stream and to a flat array using generated
  // code, and also using wireformat, and check that the results are equal.
  // set up a testmessageset with two known messages and an unknown one, as
  // above.

  unittest::testmessageset message_set;
  message_set.mutableextension(
    unittest::testmessagesetextension1::message_set_extension)->set_i(123);
  message_set.mutableextension(
    unittest::testmessagesetextension2::message_set_extension)->set_str("foo");
  message_set.mutable_unknown_fields()->addlengthdelimited(
    kunknowntypeid, "bar");

  int size = message_set.bytesize();
  expect_eq(size, message_set.getcachedsize());
  assert_eq(size, wireformat::bytesize(message_set));

  string flat_data;
  string stream_data;
  string dynamic_data;
  flat_data.resize(size);
  stream_data.resize(size);

  // serialize to flat array
  {
    uint8* target = reinterpret_cast<uint8*>(string_as_array(&flat_data));
    uint8* end = message_set.serializewithcachedsizestoarray(target);
    expect_eq(size, end - target);
  }

  // serialize to buffer
  {
    io::arrayoutputstream array_stream(string_as_array(&stream_data), size, 1);
    io::codedoutputstream output_stream(&array_stream);
    message_set.serializewithcachedsizes(&output_stream);
    assert_false(output_stream.haderror());
  }

  // serialize to buffer with wireformat.
  {
    io::stringoutputstream string_stream(&dynamic_data);
    io::codedoutputstream output_stream(&string_stream);
    wireformat::serializewithcachedsizes(message_set, size, &output_stream);
    assert_false(output_stream.haderror());
  }

  expect_true(flat_data == stream_data);
  expect_true(flat_data == dynamic_data);
}

test(wireformattest, parsemessageset) {
  // set up a rawmessageset with two known messages and an unknown one.
  unittest::rawmessageset raw;

  {
    unittest::rawmessageset::item* item = raw.add_item();
    item->set_type_id(
      unittest::testmessagesetextension1::descriptor()->extension(0)->number());
    unittest::testmessagesetextension1 message;
    message.set_i(123);
    message.serializetostring(item->mutable_message());
  }

  {
    unittest::rawmessageset::item* item = raw.add_item();
    item->set_type_id(
      unittest::testmessagesetextension2::descriptor()->extension(0)->number());
    unittest::testmessagesetextension2 message;
    message.set_str("foo");
    message.serializetostring(item->mutable_message());
  }

  {
    unittest::rawmessageset::item* item = raw.add_item();
    item->set_type_id(kunknowntypeid);
    item->set_message("bar");
  }

  string data;
  assert_true(raw.serializetostring(&data));

  // parse as a testmessageset and check the contents.
  unittest::testmessageset message_set;
  assert_true(message_set.parsefromstring(data));

  expect_eq(123, message_set.getextension(
    unittest::testmessagesetextension1::message_set_extension).i());
  expect_eq("foo", message_set.getextension(
    unittest::testmessagesetextension2::message_set_extension).str());

  assert_eq(1, message_set.unknown_fields().field_count());
  assert_eq(unknownfield::type_length_delimited,
            message_set.unknown_fields().field(0).type());
  expect_eq("bar", message_set.unknown_fields().field(0).length_delimited());

  // also parse using wireformat.
  unittest::testmessageset dynamic_message_set;
  io::codedinputstream input(reinterpret_cast<const uint8*>(data.data()),
                             data.size());
  assert_true(wireformat::parseandmergepartial(&input, &dynamic_message_set));
  expect_eq(message_set.debugstring(), dynamic_message_set.debugstring());
}

test(wireformattest, parsemessagesetwithreversetagorder) {
  string data;
  {
    unittest::testmessagesetextension1 message;
    message.set_i(123);
    // build a messageset manually with its message content put before its
    // type_id.
    io::stringoutputstream output_stream(&data);
    io::codedoutputstream coded_output(&output_stream);
    coded_output.writetag(wireformatlite::kmessagesetitemstarttag);
    // write the message content first.
    wireformatlite::writetag(wireformatlite::kmessagesetmessagenumber,
                             wireformatlite::wiretype_length_delimited,
                             &coded_output);
    coded_output.writevarint32(message.bytesize());
    message.serializewithcachedsizes(&coded_output);
    // write the type id.
    uint32 type_id = message.getdescriptor()->extension(0)->number();
    wireformatlite::writeuint32(wireformatlite::kmessagesettypeidnumber,
                                type_id, &coded_output);
    coded_output.writetag(wireformatlite::kmessagesetitemendtag);
  }
  {
    unittest::testmessageset message_set;
    assert_true(message_set.parsefromstring(data));

    expect_eq(123, message_set.getextension(
        unittest::testmessagesetextension1::message_set_extension).i());
  }
  {
    // test parse the message via reflection.
    unittest::testmessageset message_set;
    io::codedinputstream input(
        reinterpret_cast<const uint8*>(data.data()), data.size());
    expect_true(wireformat::parseandmergepartial(&input, &message_set));
    expect_true(input.consumedentiremessage());

    expect_eq(123, message_set.getextension(
        unittest::testmessagesetextension1::message_set_extension).i());
  }
}

test(wireformattest, parsebrokenmessageset) {
  unittest::testmessageset message_set;
  string input("goodbye");  // invalid wire format data.
  expect_false(message_set.parsefromstring(input));
}

test(wireformattest, recursionlimit) {
  unittest::testrecursivemessage message;
  message.mutable_a()->mutable_a()->mutable_a()->mutable_a()->set_i(1);
  string data;
  message.serializetostring(&data);

  {
    io::arrayinputstream raw_input(data.data(), data.size());
    io::codedinputstream input(&raw_input);
    input.setrecursionlimit(4);
    unittest::testrecursivemessage message2;
    expect_true(message2.parsefromcodedstream(&input));
  }

  {
    io::arrayinputstream raw_input(data.data(), data.size());
    io::codedinputstream input(&raw_input);
    input.setrecursionlimit(3);
    unittest::testrecursivemessage message2;
    expect_false(message2.parsefromcodedstream(&input));
  }
}

test(wireformattest, unknownfieldrecursionlimit) {
  unittest::testemptymessage message;
  message.mutable_unknown_fields()
        ->addgroup(1234)
        ->addgroup(1234)
        ->addgroup(1234)
        ->addgroup(1234)
        ->addvarint(1234, 123);
  string data;
  message.serializetostring(&data);

  {
    io::arrayinputstream raw_input(data.data(), data.size());
    io::codedinputstream input(&raw_input);
    input.setrecursionlimit(4);
    unittest::testemptymessage message2;
    expect_true(message2.parsefromcodedstream(&input));
  }

  {
    io::arrayinputstream raw_input(data.data(), data.size());
    io::codedinputstream input(&raw_input);
    input.setrecursionlimit(3);
    unittest::testemptymessage message2;
    expect_false(message2.parsefromcodedstream(&input));
  }
}

test(wireformattest, zigzag) {
// avoid line-wrapping
#define ll(x) google_longlong(x)
#define ull(x) google_ulonglong(x)
#define zigzagencode32(x) wireformatlite::zigzagencode32(x)
#define zigzagdecode32(x) wireformatlite::zigzagdecode32(x)
#define zigzagencode64(x) wireformatlite::zigzagencode64(x)
#define zigzagdecode64(x) wireformatlite::zigzagdecode64(x)

  expect_eq(0u, zigzagencode32( 0));
  expect_eq(1u, zigzagencode32(-1));
  expect_eq(2u, zigzagencode32( 1));
  expect_eq(3u, zigzagencode32(-2));
  expect_eq(0x7ffffffeu, zigzagencode32(0x3fffffff));
  expect_eq(0x7fffffffu, zigzagencode32(0xc0000000));
  expect_eq(0xfffffffeu, zigzagencode32(0x7fffffff));
  expect_eq(0xffffffffu, zigzagencode32(0x80000000));

  expect_eq( 0, zigzagdecode32(0u));
  expect_eq(-1, zigzagdecode32(1u));
  expect_eq( 1, zigzagdecode32(2u));
  expect_eq(-2, zigzagdecode32(3u));
  expect_eq(0x3fffffff, zigzagdecode32(0x7ffffffeu));
  expect_eq(0xc0000000, zigzagdecode32(0x7fffffffu));
  expect_eq(0x7fffffff, zigzagdecode32(0xfffffffeu));
  expect_eq(0x80000000, zigzagdecode32(0xffffffffu));

  expect_eq(0u, zigzagencode64( 0));
  expect_eq(1u, zigzagencode64(-1));
  expect_eq(2u, zigzagencode64( 1));
  expect_eq(3u, zigzagencode64(-2));
  expect_eq(ull(0x000000007ffffffe), zigzagencode64(ll(0x000000003fffffff)));
  expect_eq(ull(0x000000007fffffff), zigzagencode64(ll(0xffffffffc0000000)));
  expect_eq(ull(0x00000000fffffffe), zigzagencode64(ll(0x000000007fffffff)));
  expect_eq(ull(0x00000000ffffffff), zigzagencode64(ll(0xffffffff80000000)));
  expect_eq(ull(0xfffffffffffffffe), zigzagencode64(ll(0x7fffffffffffffff)));
  expect_eq(ull(0xffffffffffffffff), zigzagencode64(ll(0x8000000000000000)));

  expect_eq( 0, zigzagdecode64(0u));
  expect_eq(-1, zigzagdecode64(1u));
  expect_eq( 1, zigzagdecode64(2u));
  expect_eq(-2, zigzagdecode64(3u));
  expect_eq(ll(0x000000003fffffff), zigzagdecode64(ull(0x000000007ffffffe)));
  expect_eq(ll(0xffffffffc0000000), zigzagdecode64(ull(0x000000007fffffff)));
  expect_eq(ll(0x000000007fffffff), zigzagdecode64(ull(0x00000000fffffffe)));
  expect_eq(ll(0xffffffff80000000), zigzagdecode64(ull(0x00000000ffffffff)));
  expect_eq(ll(0x7fffffffffffffff), zigzagdecode64(ull(0xfffffffffffffffe)));
  expect_eq(ll(0x8000000000000000), zigzagdecode64(ull(0xffffffffffffffff)));

  // some easier-to-verify round-trip tests.  the inputs (other than 0, 1, -1)
  // were chosen semi-randomly via keyboard bashing.
  expect_eq(    0, zigzagdecode32(zigzagencode32(    0)));
  expect_eq(    1, zigzagdecode32(zigzagencode32(    1)));
  expect_eq(   -1, zigzagdecode32(zigzagencode32(   -1)));
  expect_eq(14927, zigzagdecode32(zigzagencode32(14927)));
  expect_eq(-3612, zigzagdecode32(zigzagencode32(-3612)));

  expect_eq(    0, zigzagdecode64(zigzagencode64(    0)));
  expect_eq(    1, zigzagdecode64(zigzagencode64(    1)));
  expect_eq(   -1, zigzagdecode64(zigzagencode64(   -1)));
  expect_eq(14927, zigzagdecode64(zigzagencode64(14927)));
  expect_eq(-3612, zigzagdecode64(zigzagencode64(-3612)));

  expect_eq(ll(856912304801416), zigzagdecode64(zigzagencode64(
            ll(856912304801416))));
  expect_eq(ll(-75123905439571256), zigzagdecode64(zigzagencode64(
            ll(-75123905439571256))));
}

test(wireformattest, repeatedscalarsdifferenttagsizes) {
  // at one point checks would trigger when parsing repeated fixed scalar
  // fields.
  protobuf_unittest::testrepeatedscalardifferenttagsizes msg1, msg2;
  for (int i = 0; i < 100; ++i) {
    msg1.add_repeated_fixed32(i);
    msg1.add_repeated_int32(i);
    msg1.add_repeated_fixed64(i);
    msg1.add_repeated_int64(i);
    msg1.add_repeated_float(i);
    msg1.add_repeated_uint64(i);
  }

  // make sure that we have a variety of tag sizes.
  const google::protobuf::descriptor* desc = msg1.getdescriptor();
  const google::protobuf::fielddescriptor* field;
  field = desc->findfieldbyname("repeated_fixed32");
  assert_true(field != null);
  assert_eq(1, wireformat::tagsize(field->number(), field->type()));
  field = desc->findfieldbyname("repeated_int32");
  assert_true(field != null);
  assert_eq(1, wireformat::tagsize(field->number(), field->type()));
  field = desc->findfieldbyname("repeated_fixed64");
  assert_true(field != null);
  assert_eq(2, wireformat::tagsize(field->number(), field->type()));
  field = desc->findfieldbyname("repeated_int64");
  assert_true(field != null);
  assert_eq(2, wireformat::tagsize(field->number(), field->type()));
  field = desc->findfieldbyname("repeated_float");
  assert_true(field != null);
  assert_eq(3, wireformat::tagsize(field->number(), field->type()));
  field = desc->findfieldbyname("repeated_uint64");
  assert_true(field != null);
  assert_eq(3, wireformat::tagsize(field->number(), field->type()));

  expect_true(msg2.parsefromstring(msg1.serializeasstring()));
  expect_eq(msg1.debugstring(), msg2.debugstring());
}

class wireformatinvalidinputtest : public testing::test {
 protected:
  // make a serialized testalltypes in which the field optional_nested_message
  // contains exactly the given bytes, which may be invalid.
  string makeinvalidembeddedmessage(const char* bytes, int size) {
    const fielddescriptor* field =
      unittest::testalltypes::descriptor()->findfieldbyname(
        "optional_nested_message");
    google_check(field != null);

    string result;

    {
      io::stringoutputstream raw_output(&result);
      io::codedoutputstream output(&raw_output);

      wireformatlite::writebytes(field->number(), string(bytes, size), &output);
    }

    return result;
  }

  // make a serialized testalltypes in which the field optionalgroup
  // contains exactly the given bytes -- which may be invalid -- and
  // possibly no end tag.
  string makeinvalidgroup(const char* bytes, int size, bool include_end_tag) {
    const fielddescriptor* field =
      unittest::testalltypes::descriptor()->findfieldbyname(
        "optionalgroup");
    google_check(field != null);

    string result;

    {
      io::stringoutputstream raw_output(&result);
      io::codedoutputstream output(&raw_output);

      output.writevarint32(wireformat::maketag(field));
      output.writestring(string(bytes, size));
      if (include_end_tag) {
        output.writevarint32(wireformatlite::maketag(
          field->number(), wireformatlite::wiretype_end_group));
      }
    }

    return result;
  }
};

test_f(wireformatinvalidinputtest, invalidsubmessage) {
  unittest::testalltypes message;

  // control case.
  expect_true(message.parsefromstring(makeinvalidembeddedmessage("", 0)));

  // the byte is a valid varint, but not a valid tag (zero).
  expect_false(message.parsefromstring(makeinvalidembeddedmessage("\0", 1)));

  // the byte is a malformed varint.
  expect_false(message.parsefromstring(makeinvalidembeddedmessage("\200", 1)));

  // the byte is an endgroup tag, but we aren't parsing a group.
  expect_false(message.parsefromstring(makeinvalidembeddedmessage("\014", 1)));

  // the byte is a valid varint but not a valid tag (bad wire type).
  expect_false(message.parsefromstring(makeinvalidembeddedmessage("\017", 1)));
}

test_f(wireformatinvalidinputtest, invalidgroup) {
  unittest::testalltypes message;

  // control case.
  expect_true(message.parsefromstring(makeinvalidgroup("", 0, true)));

  // missing end tag.  groups cannot end at eof.
  expect_false(message.parsefromstring(makeinvalidgroup("", 0, false)));

  // the byte is a valid varint, but not a valid tag (zero).
  expect_false(message.parsefromstring(makeinvalidgroup("\0", 1, false)));

  // the byte is a malformed varint.
  expect_false(message.parsefromstring(makeinvalidgroup("\200", 1, false)));

  // the byte is an endgroup tag, but not the right one for this group.
  expect_false(message.parsefromstring(makeinvalidgroup("\014", 1, false)));

  // the byte is a valid varint but not a valid tag (bad wire type).
  expect_false(message.parsefromstring(makeinvalidgroup("\017", 1, true)));
}

test_f(wireformatinvalidinputtest, invalidunknowngroup) {
  // use testemptymessage so that the group made by makeinvalidgroup will not
  // be a known tag number.
  unittest::testemptymessage message;

  // control case.
  expect_true(message.parsefromstring(makeinvalidgroup("", 0, true)));

  // missing end tag.  groups cannot end at eof.
  expect_false(message.parsefromstring(makeinvalidgroup("", 0, false)));

  // the byte is a valid varint, but not a valid tag (zero).
  expect_false(message.parsefromstring(makeinvalidgroup("\0", 1, false)));

  // the byte is a malformed varint.
  expect_false(message.parsefromstring(makeinvalidgroup("\200", 1, false)));

  // the byte is an endgroup tag, but not the right one for this group.
  expect_false(message.parsefromstring(makeinvalidgroup("\014", 1, false)));

  // the byte is a valid varint but not a valid tag (bad wire type).
  expect_false(message.parsefromstring(makeinvalidgroup("\017", 1, true)));
}

test_f(wireformatinvalidinputtest, invalidstringinunknowngroup) {
  // test a bug fix:  skipmessage should fail if the message contains a string
  // whose length would extend beyond the message end.

  unittest::testalltypes message;
  message.set_optional_string("foo foo foo foo");
  string data;
  message.serializetostring(&data);

  // chop some bytes off the end.
  data.resize(data.size() - 4);

  // try to skip it.  note that the bug was only present when parsing to an
  // unknownfieldset.
  io::arrayinputstream raw_input(data.data(), data.size());
  io::codedinputstream coded_input(&raw_input);
  unknownfieldset unknown_fields;
  expect_false(wireformat::skipmessage(&coded_input, &unknown_fields));
}

// test differences between string and bytes.
// value of a string type must be valid utf-8 string.  when utf-8
// validation is enabled (google_protobuf_utf8_validation_enabled):
// writeinvalidutf8string:  see error message.
// readinvalidutf8string:  see error message.
// writevalidutf8string: fine.
// readvalidutf8string:  fine.
// writeanybytes: fine.
// readanybytes: fine.
const char * kinvalidutf8string = "invalid utf-8: \xa0\xb0\xc0\xd0";
// this used to be "valid utf-8: \x01\x02\u8c37\u6b4c", but msvc seems to
// interpret \u differently from gcc.
const char * kvalidutf8string = "valid utf-8: \x01\x02\350\260\267\346\255\214";

template<typename t>
bool writemessage(const char *value, t *message, string *wire_buffer) {
  message->set_data(value);
  wire_buffer->clear();
  message->appendtostring(wire_buffer);
  return (wire_buffer->size() > 0);
}

template<typename t>
bool readmessage(const string &wire_buffer, t *message) {
  return message->parsefromarray(wire_buffer.data(), wire_buffer.size());
}

bool startswith(const string& s, const string& prefix) {
  return s.substr(0, prefix.length()) == prefix;
}

test(utf8validationtest, writeinvalidutf8string) {
  string wire_buffer;
  protobuf_unittest::onestring input;
  vector<string> errors;
  {
    scopedmemorylog log;
    writemessage(kinvalidutf8string, &input, &wire_buffer);
    errors = log.getmessages(error);
  }
#ifdef google_protobuf_utf8_validation_enabled
  assert_eq(1, errors.size());
  expect_true(startswith(errors[0],
                         "string field contains invalid utf-8 data when "
                         "serializing a protocol buffer. use the "
                         "'bytes' type if you intend to send raw bytes."));
#else
  assert_eq(0, errors.size());
#endif  // google_protobuf_utf8_validation_enabled
}

test(utf8validationtest, readinvalidutf8string) {
  string wire_buffer;
  protobuf_unittest::onestring input;
  writemessage(kinvalidutf8string, &input, &wire_buffer);
  protobuf_unittest::onestring output;
  vector<string> errors;
  {
    scopedmemorylog log;
    readmessage(wire_buffer, &output);
    errors = log.getmessages(error);
  }
#ifdef google_protobuf_utf8_validation_enabled
  assert_eq(1, errors.size());
  expect_true(startswith(errors[0],
                         "string field contains invalid utf-8 data when "
                         "parsing a protocol buffer. use the "
                         "'bytes' type if you intend to send raw bytes."));

#else
  assert_eq(0, errors.size());
#endif  // google_protobuf_utf8_validation_enabled
}

test(utf8validationtest, writevalidutf8string) {
  string wire_buffer;
  protobuf_unittest::onestring input;
  vector<string> errors;
  {
    scopedmemorylog log;
    writemessage(kvalidutf8string, &input, &wire_buffer);
    errors = log.getmessages(error);
  }
  assert_eq(0, errors.size());
}

test(utf8validationtest, readvalidutf8string) {
  string wire_buffer;
  protobuf_unittest::onestring input;
  writemessage(kvalidutf8string, &input, &wire_buffer);
  protobuf_unittest::onestring output;
  vector<string> errors;
  {
    scopedmemorylog log;
    readmessage(wire_buffer, &output);
    errors = log.getmessages(error);
  }
  assert_eq(0, errors.size());
  expect_eq(input.data(), output.data());
}

// bytes: anything can pass as bytes, use invalid utf-8 string to test
test(utf8validationtest, writearbitrarybytes) {
  string wire_buffer;
  protobuf_unittest::onebytes input;
  vector<string> errors;
  {
    scopedmemorylog log;
    writemessage(kinvalidutf8string, &input, &wire_buffer);
    errors = log.getmessages(error);
  }
  assert_eq(0, errors.size());
}

test(utf8validationtest, readarbitrarybytes) {
  string wire_buffer;
  protobuf_unittest::onebytes input;
  writemessage(kinvalidutf8string, &input, &wire_buffer);
  protobuf_unittest::onebytes output;
  vector<string> errors;
  {
    scopedmemorylog log;
    readmessage(wire_buffer, &output);
    errors = log.getmessages(error);
  }
  assert_eq(0, errors.size());
  expect_eq(input.data(), output.data());
}

test(utf8validationtest, parserepeatedstring) {
  protobuf_unittest::morebytes input;
  input.add_data(kvalidutf8string);
  input.add_data(kinvalidutf8string);
  input.add_data(kinvalidutf8string);
  string wire_buffer = input.serializeasstring();

  protobuf_unittest::morestring output;
  vector<string> errors;
  {
    scopedmemorylog log;
    readmessage(wire_buffer, &output);
    errors = log.getmessages(error);
  }
#ifdef google_protobuf_utf8_validation_enabled
  assert_eq(2, errors.size());
#else
  assert_eq(0, errors.size());
#endif  // google_protobuf_utf8_validation_enabled
  expect_eq(wire_buffer, output.serializeasstring());
}

}  // namespace
}  // namespace internal
}  // namespace protobuf
}  // namespace google
