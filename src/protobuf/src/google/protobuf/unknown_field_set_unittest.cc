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
// this test is testing a lot more than just the unknownfieldset class.  it
// tests handling of unknown fields throughout the system.

#include <google/protobuf/unknown_field_set.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/wire_format.h>
#include <google/protobuf/unittest.pb.h>
#include <google/protobuf/test_util.h>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/testing/googletest.h>
#include <gtest/gtest.h>
#include <google/protobuf/stubs/stl_util.h>

namespace google {
namespace protobuf {

using internal::wireformat;

class unknownfieldsettest : public testing::test {
 protected:
  virtual void setup() {
    descriptor_ = unittest::testalltypes::descriptor();
    testutil::setallfields(&all_fields_);
    all_fields_.serializetostring(&all_fields_data_);
    assert_true(empty_message_.parsefromstring(all_fields_data_));
    unknown_fields_ = empty_message_.mutable_unknown_fields();
  }

  const unknownfield* getfield(const string& name) {
    const fielddescriptor* field = descriptor_->findfieldbyname(name);
    if (field == null) return null;
    for (int i = 0; i < unknown_fields_->field_count(); i++) {
      if (unknown_fields_->field(i).number() == field->number()) {
        return &unknown_fields_->field(i);
      }
    }
    return null;
  }

  // constructs a protocol buffer which contains fields with all the same
  // numbers as all_fields_data_ except that each field is some other wire
  // type.
  string getbizarrodata() {
    unittest::testemptymessage bizarro_message;
    unknownfieldset* bizarro_unknown_fields =
      bizarro_message.mutable_unknown_fields();
    for (int i = 0; i < unknown_fields_->field_count(); i++) {
      const unknownfield& unknown_field = unknown_fields_->field(i);
      if (unknown_field.type() == unknownfield::type_varint) {
        bizarro_unknown_fields->addfixed32(unknown_field.number(), 1);
      } else {
        bizarro_unknown_fields->addvarint(unknown_field.number(), 1);
      }
    }

    string data;
    expect_true(bizarro_message.serializetostring(&data));
    return data;
  }

  const descriptor* descriptor_;
  unittest::testalltypes all_fields_;
  string all_fields_data_;

  // an empty message that has been parsed from all_fields_data_.  so, it has
  // unknown fields of every type.
  unittest::testemptymessage empty_message_;
  unknownfieldset* unknown_fields_;
};

namespace {

test_f(unknownfieldsettest, allfieldspresent) {
  // all fields of testalltypes should be present, in numeric order (because
  // that's the order we parsed them in).  fields that are not valid field
  // numbers of testalltypes should not be present.

  int pos = 0;

  for (int i = 0; i < 1000; i++) {
    const fielddescriptor* field = descriptor_->findfieldbynumber(i);
    if (field != null) {
      assert_lt(pos, unknown_fields_->field_count());
      expect_eq(i, unknown_fields_->field(pos++).number());
      if (field->is_repeated()) {
        // should have a second instance.
        assert_lt(pos, unknown_fields_->field_count());
        expect_eq(i, unknown_fields_->field(pos++).number());
      }
    }
  }
  expect_eq(unknown_fields_->field_count(), pos);
}

test_f(unknownfieldsettest, varint) {
  const unknownfield* field = getfield("optional_int32");
  assert_true(field != null);

  assert_eq(unknownfield::type_varint, field->type());
  expect_eq(all_fields_.optional_int32(), field->varint());
}

test_f(unknownfieldsettest, fixed32) {
  const unknownfield* field = getfield("optional_fixed32");
  assert_true(field != null);

  assert_eq(unknownfield::type_fixed32, field->type());
  expect_eq(all_fields_.optional_fixed32(), field->fixed32());
}

test_f(unknownfieldsettest, fixed64) {
  const unknownfield* field = getfield("optional_fixed64");
  assert_true(field != null);

  assert_eq(unknownfield::type_fixed64, field->type());
  expect_eq(all_fields_.optional_fixed64(), field->fixed64());
}

test_f(unknownfieldsettest, lengthdelimited) {
  const unknownfield* field = getfield("optional_string");
  assert_true(field != null);

  assert_eq(unknownfield::type_length_delimited, field->type());
  expect_eq(all_fields_.optional_string(), field->length_delimited());
}

test_f(unknownfieldsettest, group) {
  const unknownfield* field = getfield("optionalgroup");
  assert_true(field != null);

  assert_eq(unknownfield::type_group, field->type());
  assert_eq(1, field->group().field_count());

  const unknownfield& nested_field = field->group().field(0);
  const fielddescriptor* nested_field_descriptor =
    unittest::testalltypes::optionalgroup::descriptor()->findfieldbyname("a");
  assert_true(nested_field_descriptor != null);

  expect_eq(nested_field_descriptor->number(), nested_field.number());
  assert_eq(unknownfield::type_varint, nested_field.type());
  expect_eq(all_fields_.optionalgroup().a(), nested_field.varint());
}

test_f(unknownfieldsettest, serializefastandslowareequivalent) {
  int size = wireformat::computeunknownfieldssize(
      empty_message_.unknown_fields());
  string slow_buffer;
  string fast_buffer;
  slow_buffer.resize(size);
  fast_buffer.resize(size);

  uint8* target = reinterpret_cast<uint8*>(string_as_array(&fast_buffer));
  uint8* result = wireformat::serializeunknownfieldstoarray(
          empty_message_.unknown_fields(), target);
  expect_eq(size, result - target);

  {
    io::arrayoutputstream raw_stream(string_as_array(&slow_buffer), size, 1);
    io::codedoutputstream output_stream(&raw_stream);
    wireformat::serializeunknownfields(empty_message_.unknown_fields(),
                                       &output_stream);
    assert_false(output_stream.haderror());
  }
  expect_true(fast_buffer == slow_buffer);
}

test_f(unknownfieldsettest, serialize) {
  // check that serializing the unknownfieldset produces the original data
  // again.

  string data;
  empty_message_.serializetostring(&data);

  // don't use expect_eq because we don't want to dump raw binary data to
  // stdout.
  expect_true(data == all_fields_data_);
}

test_f(unknownfieldsettest, parseviareflection) {
  // make sure fields are properly parsed to the unknownfieldset when parsing
  // via reflection.

  unittest::testemptymessage message;
  io::arrayinputstream raw_input(all_fields_data_.data(),
                                 all_fields_data_.size());
  io::codedinputstream input(&raw_input);
  assert_true(wireformat::parseandmergepartial(&input, &message));

  expect_eq(message.debugstring(), empty_message_.debugstring());
}

test_f(unknownfieldsettest, serializeviareflection) {
  // make sure fields are properly written from the unknownfieldset when
  // serializing via reflection.

  string data;

  {
    io::stringoutputstream raw_output(&data);
    io::codedoutputstream output(&raw_output);
    int size = wireformat::bytesize(empty_message_);
    wireformat::serializewithcachedsizes(empty_message_, size, &output);
    assert_false(output.haderror());
  }

  // don't use expect_eq because we don't want to dump raw binary data to
  // stdout.
  expect_true(data == all_fields_data_);
}

test_f(unknownfieldsettest, copyfrom) {
  unittest::testemptymessage message;

  message.copyfrom(empty_message_);

  expect_eq(empty_message_.debugstring(), message.debugstring());
}

test_f(unknownfieldsettest, swap) {
  unittest::testemptymessage other_message;
  assert_true(other_message.parsefromstring(getbizarrodata()));

  expect_gt(empty_message_.unknown_fields().field_count(), 0);
  expect_gt(other_message.unknown_fields().field_count(), 0);
  const string debug_string = empty_message_.debugstring();
  const string other_debug_string = other_message.debugstring();
  expect_ne(debug_string, other_debug_string);

  empty_message_.swap(&other_message);
  expect_eq(debug_string, other_message.debugstring());
  expect_eq(other_debug_string, empty_message_.debugstring());
}

test_f(unknownfieldsettest, swapwithself) {
  const string debug_string = empty_message_.debugstring();
  expect_gt(empty_message_.unknown_fields().field_count(), 0);

  empty_message_.swap(&empty_message_);
  expect_gt(empty_message_.unknown_fields().field_count(), 0);
  expect_eq(debug_string, empty_message_.debugstring());
}

test_f(unknownfieldsettest, mergefrom) {
  unittest::testemptymessage source, destination;

  destination.mutable_unknown_fields()->addvarint(1, 1);
  destination.mutable_unknown_fields()->addvarint(3, 2);
  source.mutable_unknown_fields()->addvarint(2, 3);
  source.mutable_unknown_fields()->addvarint(3, 4);

  destination.mergefrom(source);

  expect_eq(
    // note:  the ordering of fields here depends on the ordering of adds
    //   and merging, above.
    "1: 1\n"
    "3: 2\n"
    "2: 3\n"
    "3: 4\n",
    destination.debugstring());
}


test_f(unknownfieldsettest, clear) {
  // clear the set.
  empty_message_.clear();
  expect_eq(0, unknown_fields_->field_count());
}

test_f(unknownfieldsettest, clearandfreememory) {
  expect_gt(unknown_fields_->field_count(), 0);
  unknown_fields_->clearandfreememory();
  expect_eq(0, unknown_fields_->field_count());
  unknown_fields_->addvarint(123456, 654321);
  expect_eq(1, unknown_fields_->field_count());
}

test_f(unknownfieldsettest, parseknownandunknown) {
  // test mixing known and unknown fields when parsing.

  unittest::testemptymessage source;
  source.mutable_unknown_fields()->addvarint(123456, 654321);
  string data;
  assert_true(source.serializetostring(&data));

  unittest::testalltypes destination;
  assert_true(destination.parsefromstring(all_fields_data_ + data));

  testutil::expectallfieldsset(destination);
  assert_eq(1, destination.unknown_fields().field_count());
  assert_eq(unknownfield::type_varint,
            destination.unknown_fields().field(0).type());
  expect_eq(654321, destination.unknown_fields().field(0).varint());
}

test_f(unknownfieldsettest, wrongtypetreatedasunknown) {
  // test that fields of the wrong wire type are treated like unknown fields
  // when parsing.

  unittest::testalltypes all_types_message;
  unittest::testemptymessage empty_message;
  string bizarro_data = getbizarrodata();
  assert_true(all_types_message.parsefromstring(bizarro_data));
  assert_true(empty_message.parsefromstring(bizarro_data));

  // all fields should have been interpreted as unknown, so the debug strings
  // should be the same.
  expect_eq(empty_message.debugstring(), all_types_message.debugstring());
}

test_f(unknownfieldsettest, wrongtypetreatedasunknownviareflection) {
  // same as wrongtypetreatedasunknown but via the reflection interface.

  unittest::testalltypes all_types_message;
  unittest::testemptymessage empty_message;
  string bizarro_data = getbizarrodata();
  io::arrayinputstream raw_input(bizarro_data.data(), bizarro_data.size());
  io::codedinputstream input(&raw_input);
  assert_true(wireformat::parseandmergepartial(&input, &all_types_message));
  assert_true(empty_message.parsefromstring(bizarro_data));

  expect_eq(empty_message.debugstring(), all_types_message.debugstring());
}

test_f(unknownfieldsettest, unknownextensions) {
  // make sure fields are properly parsed to the unknownfieldset even when
  // they are declared as extension numbers.

  unittest::testemptymessagewithextensions message;
  assert_true(message.parsefromstring(all_fields_data_));

  expect_eq(message.debugstring(), empty_message_.debugstring());
}

test_f(unknownfieldsettest, unknownextensionsreflection) {
  // same as unknownextensions except parsing via reflection.

  unittest::testemptymessagewithextensions message;
  io::arrayinputstream raw_input(all_fields_data_.data(),
                                 all_fields_data_.size());
  io::codedinputstream input(&raw_input);
  assert_true(wireformat::parseandmergepartial(&input, &message));

  expect_eq(message.debugstring(), empty_message_.debugstring());
}

test_f(unknownfieldsettest, wrongextensiontypetreatedasunknown) {
  // test that fields of the wrong wire type are treated like unknown fields
  // when parsing extensions.

  unittest::testallextensions all_extensions_message;
  unittest::testemptymessage empty_message;
  string bizarro_data = getbizarrodata();
  assert_true(all_extensions_message.parsefromstring(bizarro_data));
  assert_true(empty_message.parsefromstring(bizarro_data));

  // all fields should have been interpreted as unknown, so the debug strings
  // should be the same.
  expect_eq(empty_message.debugstring(), all_extensions_message.debugstring());
}

test_f(unknownfieldsettest, unknownenumvalue) {
  using unittest::testalltypes;
  using unittest::testallextensions;
  using unittest::testemptymessage;

  const fielddescriptor* singular_field =
    testalltypes::descriptor()->findfieldbyname("optional_nested_enum");
  const fielddescriptor* repeated_field =
    testalltypes::descriptor()->findfieldbyname("repeated_nested_enum");
  assert_true(singular_field != null);
  assert_true(repeated_field != null);

  string data;

  {
    testemptymessage empty_message;
    unknownfieldset* unknown_fields = empty_message.mutable_unknown_fields();
    unknown_fields->addvarint(singular_field->number(), testalltypes::bar);
    unknown_fields->addvarint(singular_field->number(), 5);  // not valid
    unknown_fields->addvarint(repeated_field->number(), testalltypes::foo);
    unknown_fields->addvarint(repeated_field->number(), 4);  // not valid
    unknown_fields->addvarint(repeated_field->number(), testalltypes::baz);
    unknown_fields->addvarint(repeated_field->number(), 6);  // not valid
    empty_message.serializetostring(&data);
  }

  {
    testalltypes message;
    assert_true(message.parsefromstring(data));
    expect_eq(testalltypes::bar, message.optional_nested_enum());
    assert_eq(2, message.repeated_nested_enum_size());
    expect_eq(testalltypes::foo, message.repeated_nested_enum(0));
    expect_eq(testalltypes::baz, message.repeated_nested_enum(1));

    const unknownfieldset& unknown_fields = message.unknown_fields();
    assert_eq(3, unknown_fields.field_count());

    expect_eq(singular_field->number(), unknown_fields.field(0).number());
    assert_eq(unknownfield::type_varint, unknown_fields.field(0).type());
    expect_eq(5, unknown_fields.field(0).varint());

    expect_eq(repeated_field->number(), unknown_fields.field(1).number());
    assert_eq(unknownfield::type_varint, unknown_fields.field(1).type());
    expect_eq(4, unknown_fields.field(1).varint());

    expect_eq(repeated_field->number(), unknown_fields.field(2).number());
    assert_eq(unknownfield::type_varint, unknown_fields.field(2).type());
    expect_eq(6, unknown_fields.field(2).varint());
  }

  {
    using unittest::optional_nested_enum_extension;
    using unittest::repeated_nested_enum_extension;

    testallextensions message;
    assert_true(message.parsefromstring(data));
    expect_eq(testalltypes::bar,
              message.getextension(optional_nested_enum_extension));
    assert_eq(2, message.extensionsize(repeated_nested_enum_extension));
    expect_eq(testalltypes::foo,
              message.getextension(repeated_nested_enum_extension, 0));
    expect_eq(testalltypes::baz,
              message.getextension(repeated_nested_enum_extension, 1));

    const unknownfieldset& unknown_fields = message.unknown_fields();
    assert_eq(3, unknown_fields.field_count());

    expect_eq(singular_field->number(), unknown_fields.field(0).number());
    assert_eq(unknownfield::type_varint, unknown_fields.field(0).type());
    expect_eq(5, unknown_fields.field(0).varint());

    expect_eq(repeated_field->number(), unknown_fields.field(1).number());
    assert_eq(unknownfield::type_varint, unknown_fields.field(1).type());
    expect_eq(4, unknown_fields.field(1).varint());

    expect_eq(repeated_field->number(), unknown_fields.field(2).number());
    assert_eq(unknownfield::type_varint, unknown_fields.field(2).type());
    expect_eq(6, unknown_fields.field(2).varint());
  }
}

test_f(unknownfieldsettest, spaceused) {
  unittest::testemptymessage empty_message;

  // make sure an unknown field set has zero space used until a field is
  // actually added.
  int base_size = empty_message.spaceused();
  unknownfieldset* unknown_fields = empty_message.mutable_unknown_fields();
  expect_eq(base_size, empty_message.spaceused());

  // make sure each thing we add to the set increases the spaceused().
  unknown_fields->addvarint(1, 0);
  expect_lt(base_size, empty_message.spaceused());
  base_size = empty_message.spaceused();

  string* str = unknown_fields->addlengthdelimited(1);
  expect_lt(base_size, empty_message.spaceused());
  base_size = empty_message.spaceused();

  str->assign(sizeof(string) + 1, 'x');
  expect_lt(base_size, empty_message.spaceused());
  base_size = empty_message.spaceused();

  unknownfieldset* group = unknown_fields->addgroup(1);
  expect_lt(base_size, empty_message.spaceused());
  base_size = empty_message.spaceused();

  group->addvarint(1, 0);
  expect_lt(base_size, empty_message.spaceused());
}


test_f(unknownfieldsettest, empty) {
  unknownfieldset unknown_fields;
  expect_true(unknown_fields.empty());
  unknown_fields.addvarint(6, 123);
  expect_false(unknown_fields.empty());
  unknown_fields.clear();
  expect_true(unknown_fields.empty());
}

test_f(unknownfieldsettest, deletesubrange) {
  // exhaustively test the deletion of every possible subrange in arrays of all
  // sizes from 0 through 9.
  for (int size = 0; size < 10; ++size) {
    for (int num = 0; num <= size; ++num) {
      for (int start = 0; start < size - num; ++start) {
        // create a set with "size" fields.
        unknownfieldset unknown;
        for (int i = 0; i < size; ++i) {
          unknown.addfixed32(i, i);
        }
        // delete the specified subrange.
        unknown.deletesubrange(start, num);
        // make sure the resulting field values are still correct.
        expect_eq(size - num, unknown.field_count());
        for (int i = 0; i < unknown.field_count(); ++i) {
          if (i < start) {
            expect_eq(i, unknown.field(i).fixed32());
          } else {
            expect_eq(i + num, unknown.field(i).fixed32());
          }
        }
      }
    }
  }
}

void checkdeletebynumber(const vector<int>& field_numbers, int deleted_number,
                        const vector<int>& expected_field_nubmers) {
  unknownfieldset unknown_fields;
  for (int i = 0; i < field_numbers.size(); ++i) {
    unknown_fields.addfixed32(field_numbers[i], i);
  }
  unknown_fields.deletebynumber(deleted_number);
  assert_eq(expected_field_nubmers.size(), unknown_fields.field_count());
  for (int i = 0; i < expected_field_nubmers.size(); ++i) {
    expect_eq(expected_field_nubmers[i],
              unknown_fields.field(i).number());
  }
}

#define make_vector(x) vector<int>(x, x + google_arraysize(x))
test_f(unknownfieldsettest, deletebynumber) {
  checkdeletebynumber(vector<int>(), 1, vector<int>());
  static const int ktestfieldnumbers1[] = {1, 2, 3};
  static const int kfieldnumbertodelete1 = 1;
  static const int kexpectedfieldnumbers1[] = {2, 3};
  checkdeletebynumber(make_vector(ktestfieldnumbers1), kfieldnumbertodelete1,
                      make_vector(kexpectedfieldnumbers1));
  static const int ktestfieldnumbers2[] = {1, 2, 3};
  static const int kfieldnumbertodelete2 = 2;
  static const int kexpectedfieldnumbers2[] = {1, 3};
  checkdeletebynumber(make_vector(ktestfieldnumbers2), kfieldnumbertodelete2,
                      make_vector(kexpectedfieldnumbers2));
  static const int ktestfieldnumbers3[] = {1, 2, 3};
  static const int kfieldnumbertodelete3 = 3;
  static const int kexpectedfieldnumbers3[] = {1, 2};
  checkdeletebynumber(make_vector(ktestfieldnumbers3), kfieldnumbertodelete3,
                      make_vector(kexpectedfieldnumbers3));
  static const int ktestfieldnumbers4[] = {1, 2, 1, 4, 1};
  static const int kfieldnumbertodelete4 = 1;
  static const int kexpectedfieldnumbers4[] = {2, 4};
  checkdeletebynumber(make_vector(ktestfieldnumbers4), kfieldnumbertodelete4,
                      make_vector(kexpectedfieldnumbers4));
  static const int ktestfieldnumbers5[] = {1, 2, 3, 4, 5};
  static const int kfieldnumbertodelete5 = 6;
  static const int kexpectedfieldnumbers5[] = {1, 2, 3, 4, 5};
  checkdeletebynumber(make_vector(ktestfieldnumbers5), kfieldnumbertodelete5,
                      make_vector(kexpectedfieldnumbers5));
}
#undef make_vector
}  // namespace

}  // namespace protobuf
}  // namespace google
