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
// this file makes extensive use of rfc 3092.  :)

#include <vector>

#include <google/protobuf/compiler/importer.h>
#include <google/protobuf/unittest.pb.h>
#include <google/protobuf/unittest_custom_options.pb.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor_database.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/stubs/strutil.h>
#include <google/protobuf/stubs/substitute.h>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/testing/googletest.h>
#include <gtest/gtest.h>

namespace google {
namespace protobuf {

// can't use an anonymous namespace here due to brokenness of tru64 compiler.
namespace descriptor_unittest {

// some helpers to make assembling descriptors faster.
descriptorproto* addmessage(filedescriptorproto* file, const string& name) {
  descriptorproto* result = file->add_message_type();
  result->set_name(name);
  return result;
}

descriptorproto* addnestedmessage(descriptorproto* parent, const string& name) {
  descriptorproto* result = parent->add_nested_type();
  result->set_name(name);
  return result;
}

enumdescriptorproto* addenum(filedescriptorproto* file, const string& name) {
  enumdescriptorproto* result = file->add_enum_type();
  result->set_name(name);
  return result;
}

enumdescriptorproto* addnestedenum(descriptorproto* parent,
                                   const string& name) {
  enumdescriptorproto* result = parent->add_enum_type();
  result->set_name(name);
  return result;
}

servicedescriptorproto* addservice(filedescriptorproto* file,
                                   const string& name) {
  servicedescriptorproto* result = file->add_service();
  result->set_name(name);
  return result;
}

fielddescriptorproto* addfield(descriptorproto* parent,
                               const string& name, int number,
                               fielddescriptorproto::label label,
                               fielddescriptorproto::type type) {
  fielddescriptorproto* result = parent->add_field();
  result->set_name(name);
  result->set_number(number);
  result->set_label(label);
  result->set_type(type);
  return result;
}

fielddescriptorproto* addextension(filedescriptorproto* file,
                                   const string& extendee,
                                   const string& name, int number,
                                   fielddescriptorproto::label label,
                                   fielddescriptorproto::type type) {
  fielddescriptorproto* result = file->add_extension();
  result->set_name(name);
  result->set_number(number);
  result->set_label(label);
  result->set_type(type);
  result->set_extendee(extendee);
  return result;
}

fielddescriptorproto* addnestedextension(descriptorproto* parent,
                                         const string& extendee,
                                         const string& name, int number,
                                         fielddescriptorproto::label label,
                                         fielddescriptorproto::type type) {
  fielddescriptorproto* result = parent->add_extension();
  result->set_name(name);
  result->set_number(number);
  result->set_label(label);
  result->set_type(type);
  result->set_extendee(extendee);
  return result;
}

descriptorproto::extensionrange* addextensionrange(descriptorproto* parent,
                                                   int start, int end) {
  descriptorproto::extensionrange* result = parent->add_extension_range();
  result->set_start(start);
  result->set_end(end);
  return result;
}

enumvaluedescriptorproto* addenumvalue(enumdescriptorproto* enum_proto,
                                       const string& name, int number) {
  enumvaluedescriptorproto* result = enum_proto->add_value();
  result->set_name(name);
  result->set_number(number);
  return result;
}

methoddescriptorproto* addmethod(servicedescriptorproto* service,
                                 const string& name,
                                 const string& input_type,
                                 const string& output_type) {
  methoddescriptorproto* result = service->add_method();
  result->set_name(name);
  result->set_input_type(input_type);
  result->set_output_type(output_type);
  return result;
}

// empty enums technically aren't allowed.  we need to insert a dummy value
// into them.
void addemptyenum(filedescriptorproto* file, const string& name) {
  addenumvalue(addenum(file, name), name + "_dummy", 1);
}

// ===================================================================

// test simple files.
class filedescriptortest : public testing::test {
 protected:
  virtual void setup() {
    // build descriptors for the following definitions:
    //
    //   // in "foo.proto"
    //   message foomessage { extensions 1; }
    //   enum fooenum {foo_enum_value = 1;}
    //   service fooservice {}
    //   extend foomessage { optional int32 foo_extension = 1; }
    //
    //   // in "bar.proto"
    //   package bar_package;
    //   message barmessage { extensions 1; }
    //   enum barenum {bar_enum_value = 1;}
    //   service barservice {}
    //   extend barmessage { optional int32 bar_extension = 1; }
    //
    // also, we have an empty file "baz.proto".  this file's purpose is to
    // make sure that even though it has the same package as foo.proto,
    // searching it for members of foo.proto won't work.

    filedescriptorproto foo_file;
    foo_file.set_name("foo.proto");
    addextensionrange(addmessage(&foo_file, "foomessage"), 1, 2);
    addenumvalue(addenum(&foo_file, "fooenum"), "foo_enum_value", 1);
    addservice(&foo_file, "fooservice");
    addextension(&foo_file, "foomessage", "foo_extension", 1,
                 fielddescriptorproto::label_optional,
                 fielddescriptorproto::type_int32);

    filedescriptorproto bar_file;
    bar_file.set_name("bar.proto");
    bar_file.set_package("bar_package");
    bar_file.add_dependency("foo.proto");
    addextensionrange(addmessage(&bar_file, "barmessage"), 1, 2);
    addenumvalue(addenum(&bar_file, "barenum"), "bar_enum_value", 1);
    addservice(&bar_file, "barservice");
    addextension(&bar_file, "bar_package.barmessage", "bar_extension", 1,
                 fielddescriptorproto::label_optional,
                 fielddescriptorproto::type_int32);

    filedescriptorproto baz_file;
    baz_file.set_name("baz.proto");

    // build the descriptors and get the pointers.
    foo_file_ = pool_.buildfile(foo_file);
    assert_true(foo_file_ != null);

    bar_file_ = pool_.buildfile(bar_file);
    assert_true(bar_file_ != null);

    baz_file_ = pool_.buildfile(baz_file);
    assert_true(baz_file_ != null);

    assert_eq(1, foo_file_->message_type_count());
    foo_message_ = foo_file_->message_type(0);
    assert_eq(1, foo_file_->enum_type_count());
    foo_enum_ = foo_file_->enum_type(0);
    assert_eq(1, foo_enum_->value_count());
    foo_enum_value_ = foo_enum_->value(0);
    assert_eq(1, foo_file_->service_count());
    foo_service_ = foo_file_->service(0);
    assert_eq(1, foo_file_->extension_count());
    foo_extension_ = foo_file_->extension(0);

    assert_eq(1, bar_file_->message_type_count());
    bar_message_ = bar_file_->message_type(0);
    assert_eq(1, bar_file_->enum_type_count());
    bar_enum_ = bar_file_->enum_type(0);
    assert_eq(1, bar_enum_->value_count());
    bar_enum_value_ = bar_enum_->value(0);
    assert_eq(1, bar_file_->service_count());
    bar_service_ = bar_file_->service(0);
    assert_eq(1, bar_file_->extension_count());
    bar_extension_ = bar_file_->extension(0);
  }

  descriptorpool pool_;

  const filedescriptor* foo_file_;
  const filedescriptor* bar_file_;
  const filedescriptor* baz_file_;

  const descriptor*          foo_message_;
  const enumdescriptor*      foo_enum_;
  const enumvaluedescriptor* foo_enum_value_;
  const servicedescriptor*   foo_service_;
  const fielddescriptor*     foo_extension_;

  const descriptor*          bar_message_;
  const enumdescriptor*      bar_enum_;
  const enumvaluedescriptor* bar_enum_value_;
  const servicedescriptor*   bar_service_;
  const fielddescriptor*     bar_extension_;
};

test_f(filedescriptortest, name) {
  expect_eq("foo.proto", foo_file_->name());
  expect_eq("bar.proto", bar_file_->name());
  expect_eq("baz.proto", baz_file_->name());
}

test_f(filedescriptortest, package) {
  expect_eq("", foo_file_->package());
  expect_eq("bar_package", bar_file_->package());
}

test_f(filedescriptortest, dependencies) {
  expect_eq(0, foo_file_->dependency_count());
  expect_eq(1, bar_file_->dependency_count());
  expect_eq(foo_file_, bar_file_->dependency(0));
}

test_f(filedescriptortest, findmessagetypebyname) {
  expect_eq(foo_message_, foo_file_->findmessagetypebyname("foomessage"));
  expect_eq(bar_message_, bar_file_->findmessagetypebyname("barmessage"));

  expect_true(foo_file_->findmessagetypebyname("barmessage") == null);
  expect_true(bar_file_->findmessagetypebyname("foomessage") == null);
  expect_true(baz_file_->findmessagetypebyname("foomessage") == null);

  expect_true(foo_file_->findmessagetypebyname("nosuchmessage") == null);
  expect_true(foo_file_->findmessagetypebyname("fooenum") == null);
}

test_f(filedescriptortest, findenumtypebyname) {
  expect_eq(foo_enum_, foo_file_->findenumtypebyname("fooenum"));
  expect_eq(bar_enum_, bar_file_->findenumtypebyname("barenum"));

  expect_true(foo_file_->findenumtypebyname("barenum") == null);
  expect_true(bar_file_->findenumtypebyname("fooenum") == null);
  expect_true(baz_file_->findenumtypebyname("fooenum") == null);

  expect_true(foo_file_->findenumtypebyname("nosuchenum") == null);
  expect_true(foo_file_->findenumtypebyname("foomessage") == null);
}

test_f(filedescriptortest, findenumvaluebyname) {
  expect_eq(foo_enum_value_, foo_file_->findenumvaluebyname("foo_enum_value"));
  expect_eq(bar_enum_value_, bar_file_->findenumvaluebyname("bar_enum_value"));

  expect_true(foo_file_->findenumvaluebyname("bar_enum_value") == null);
  expect_true(bar_file_->findenumvaluebyname("foo_enum_value") == null);
  expect_true(baz_file_->findenumvaluebyname("foo_enum_value") == null);

  expect_true(foo_file_->findenumvaluebyname("no_such_value") == null);
  expect_true(foo_file_->findenumvaluebyname("foomessage") == null);
}

test_f(filedescriptortest, findservicebyname) {
  expect_eq(foo_service_, foo_file_->findservicebyname("fooservice"));
  expect_eq(bar_service_, bar_file_->findservicebyname("barservice"));

  expect_true(foo_file_->findservicebyname("barservice") == null);
  expect_true(bar_file_->findservicebyname("fooservice") == null);
  expect_true(baz_file_->findservicebyname("fooservice") == null);

  expect_true(foo_file_->findservicebyname("nosuchservice") == null);
  expect_true(foo_file_->findservicebyname("foomessage") == null);
}

test_f(filedescriptortest, findextensionbyname) {
  expect_eq(foo_extension_, foo_file_->findextensionbyname("foo_extension"));
  expect_eq(bar_extension_, bar_file_->findextensionbyname("bar_extension"));

  expect_true(foo_file_->findextensionbyname("bar_extension") == null);
  expect_true(bar_file_->findextensionbyname("foo_extension") == null);
  expect_true(baz_file_->findextensionbyname("foo_extension") == null);

  expect_true(foo_file_->findextensionbyname("no_such_extension") == null);
  expect_true(foo_file_->findextensionbyname("foomessage") == null);
}

test_f(filedescriptortest, findextensionbynumber) {
  expect_eq(foo_extension_, pool_.findextensionbynumber(foo_message_, 1));
  expect_eq(bar_extension_, pool_.findextensionbynumber(bar_message_, 1));

  expect_true(pool_.findextensionbynumber(foo_message_, 2) == null);
}

test_f(filedescriptortest, buildagain) {
  // test that if te call buildfile again on the same input we get the same
  // filedescriptor back.
  filedescriptorproto file;
  foo_file_->copyto(&file);
  expect_eq(foo_file_, pool_.buildfile(file));

  // but if we change the file then it won't work.
  file.set_package("some.other.package");
  expect_true(pool_.buildfile(file) == null);
}

// ===================================================================

// test simple flat messages and fields.
class descriptortest : public testing::test {
 protected:
  virtual void setup() {
    // build descriptors for the following definitions:
    //
    //   // in "foo.proto"
    //   message testforeign {}
    //   enum testenum {}
    //
    //   message testmessage {
    //     required string      foo = 1;
    //     optional testenum    bar = 6;
    //     repeated testforeign baz = 500000000;
    //     optional group       qux = 15 {}
    //   }
    //
    //   // in "bar.proto"
    //   package corge.grault;
    //   message testmessage2 {
    //     required string foo = 1;
    //     required string bar = 2;
    //     required string quux = 6;
    //   }
    //
    // we cheat and use testforeign as the type for qux rather than create
    // an actual nested type.
    //
    // since all primitive types (including string) use the same building
    // code, there's no need to test each one individually.
    //
    // testmessage2 is primarily here to test findfieldbyname and friends.
    // all messages created from the same descriptorpool share the same lookup
    // table, so we need to insure that they don't interfere.

    filedescriptorproto foo_file;
    foo_file.set_name("foo.proto");
    addmessage(&foo_file, "testforeign");
    addemptyenum(&foo_file, "testenum");

    descriptorproto* message = addmessage(&foo_file, "testmessage");
    addfield(message, "foo", 1,
             fielddescriptorproto::label_required,
             fielddescriptorproto::type_string);
    addfield(message, "bar", 6,
             fielddescriptorproto::label_optional,
             fielddescriptorproto::type_enum)
      ->set_type_name("testenum");
    addfield(message, "baz", 500000000,
             fielddescriptorproto::label_repeated,
             fielddescriptorproto::type_message)
      ->set_type_name("testforeign");
    addfield(message, "qux", 15,
             fielddescriptorproto::label_optional,
             fielddescriptorproto::type_group)
      ->set_type_name("testforeign");

    filedescriptorproto bar_file;
    bar_file.set_name("bar.proto");
    bar_file.set_package("corge.grault");

    descriptorproto* message2 = addmessage(&bar_file, "testmessage2");
    addfield(message2, "foo", 1,
             fielddescriptorproto::label_required,
             fielddescriptorproto::type_string);
    addfield(message2, "bar", 2,
             fielddescriptorproto::label_required,
             fielddescriptorproto::type_string);
    addfield(message2, "quux", 6,
             fielddescriptorproto::label_required,
             fielddescriptorproto::type_string);

    // build the descriptors and get the pointers.
    foo_file_ = pool_.buildfile(foo_file);
    assert_true(foo_file_ != null);

    bar_file_ = pool_.buildfile(bar_file);
    assert_true(bar_file_ != null);

    assert_eq(1, foo_file_->enum_type_count());
    enum_ = foo_file_->enum_type(0);

    assert_eq(2, foo_file_->message_type_count());
    foreign_ = foo_file_->message_type(0);
    message_ = foo_file_->message_type(1);

    assert_eq(4, message_->field_count());
    foo_ = message_->field(0);
    bar_ = message_->field(1);
    baz_ = message_->field(2);
    qux_ = message_->field(3);

    assert_eq(1, bar_file_->message_type_count());
    message2_ = bar_file_->message_type(0);

    assert_eq(3, message2_->field_count());
    foo2_  = message2_->field(0);
    bar2_  = message2_->field(1);
    quux2_ = message2_->field(2);
  }

  descriptorpool pool_;

  const filedescriptor* foo_file_;
  const filedescriptor* bar_file_;

  const descriptor* message_;
  const descriptor* message2_;
  const descriptor* foreign_;
  const enumdescriptor* enum_;

  const fielddescriptor* foo_;
  const fielddescriptor* bar_;
  const fielddescriptor* baz_;
  const fielddescriptor* qux_;

  const fielddescriptor* foo2_;
  const fielddescriptor* bar2_;
  const fielddescriptor* quux2_;
};

test_f(descriptortest, name) {
  expect_eq("testmessage", message_->name());
  expect_eq("testmessage", message_->full_name());
  expect_eq(foo_file_, message_->file());

  expect_eq("testmessage2", message2_->name());
  expect_eq("corge.grault.testmessage2", message2_->full_name());
  expect_eq(bar_file_, message2_->file());
}

test_f(descriptortest, containingtype) {
  expect_true(message_->containing_type() == null);
  expect_true(message2_->containing_type() == null);
}

test_f(descriptortest, fieldsbyindex) {
  assert_eq(4, message_->field_count());
  expect_eq(foo_, message_->field(0));
  expect_eq(bar_, message_->field(1));
  expect_eq(baz_, message_->field(2));
  expect_eq(qux_, message_->field(3));
}

test_f(descriptortest, findfieldbyname) {
  // all messages in the same descriptorpool share a single lookup table for
  // fields.  so, in addition to testing that findfieldbyname finds the fields
  // of the message, we need to test that it does *not* find the fields of
  // *other* messages.

  expect_eq(foo_, message_->findfieldbyname("foo"));
  expect_eq(bar_, message_->findfieldbyname("bar"));
  expect_eq(baz_, message_->findfieldbyname("baz"));
  expect_eq(qux_, message_->findfieldbyname("qux"));
  expect_true(message_->findfieldbyname("no_such_field") == null);
  expect_true(message_->findfieldbyname("quux") == null);

  expect_eq(foo2_ , message2_->findfieldbyname("foo" ));
  expect_eq(bar2_ , message2_->findfieldbyname("bar" ));
  expect_eq(quux2_, message2_->findfieldbyname("quux"));
  expect_true(message2_->findfieldbyname("baz") == null);
  expect_true(message2_->findfieldbyname("qux") == null);
}

test_f(descriptortest, findfieldbynumber) {
  expect_eq(foo_, message_->findfieldbynumber(1));
  expect_eq(bar_, message_->findfieldbynumber(6));
  expect_eq(baz_, message_->findfieldbynumber(500000000));
  expect_eq(qux_, message_->findfieldbynumber(15));
  expect_true(message_->findfieldbynumber(837592) == null);
  expect_true(message_->findfieldbynumber(2) == null);

  expect_eq(foo2_ , message2_->findfieldbynumber(1));
  expect_eq(bar2_ , message2_->findfieldbynumber(2));
  expect_eq(quux2_, message2_->findfieldbynumber(6));
  expect_true(message2_->findfieldbynumber(15) == null);
  expect_true(message2_->findfieldbynumber(500000000) == null);
}

test_f(descriptortest, fieldname) {
  expect_eq("foo", foo_->name());
  expect_eq("bar", bar_->name());
  expect_eq("baz", baz_->name());
  expect_eq("qux", qux_->name());
}

test_f(descriptortest, fieldfullname) {
  expect_eq("testmessage.foo", foo_->full_name());
  expect_eq("testmessage.bar", bar_->full_name());
  expect_eq("testmessage.baz", baz_->full_name());
  expect_eq("testmessage.qux", qux_->full_name());

  expect_eq("corge.grault.testmessage2.foo", foo2_->full_name());
  expect_eq("corge.grault.testmessage2.bar", bar2_->full_name());
  expect_eq("corge.grault.testmessage2.quux", quux2_->full_name());
}

test_f(descriptortest, fieldfile) {
  expect_eq(foo_file_, foo_->file());
  expect_eq(foo_file_, bar_->file());
  expect_eq(foo_file_, baz_->file());
  expect_eq(foo_file_, qux_->file());

  expect_eq(bar_file_, foo2_->file());
  expect_eq(bar_file_, bar2_->file());
  expect_eq(bar_file_, quux2_->file());
}

test_f(descriptortest, fieldindex) {
  expect_eq(0, foo_->index());
  expect_eq(1, bar_->index());
  expect_eq(2, baz_->index());
  expect_eq(3, qux_->index());
}

test_f(descriptortest, fieldnumber) {
  expect_eq(        1, foo_->number());
  expect_eq(        6, bar_->number());
  expect_eq(500000000, baz_->number());
  expect_eq(       15, qux_->number());
}

test_f(descriptortest, fieldtype) {
  expect_eq(fielddescriptor::type_string , foo_->type());
  expect_eq(fielddescriptor::type_enum   , bar_->type());
  expect_eq(fielddescriptor::type_message, baz_->type());
  expect_eq(fielddescriptor::type_group  , qux_->type());
}

test_f(descriptortest, fieldlabel) {
  expect_eq(fielddescriptor::label_required, foo_->label());
  expect_eq(fielddescriptor::label_optional, bar_->label());
  expect_eq(fielddescriptor::label_repeated, baz_->label());
  expect_eq(fielddescriptor::label_optional, qux_->label());

  expect_true (foo_->is_required());
  expect_false(foo_->is_optional());
  expect_false(foo_->is_repeated());

  expect_false(bar_->is_required());
  expect_true (bar_->is_optional());
  expect_false(bar_->is_repeated());

  expect_false(baz_->is_required());
  expect_false(baz_->is_optional());
  expect_true (baz_->is_repeated());
}

test_f(descriptortest, fieldhasdefault) {
  expect_false(foo_->has_default_value());
  expect_false(bar_->has_default_value());
  expect_false(baz_->has_default_value());
  expect_false(qux_->has_default_value());
}

test_f(descriptortest, fieldcontainingtype) {
  expect_eq(message_, foo_->containing_type());
  expect_eq(message_, bar_->containing_type());
  expect_eq(message_, baz_->containing_type());
  expect_eq(message_, qux_->containing_type());

  expect_eq(message2_, foo2_ ->containing_type());
  expect_eq(message2_, bar2_ ->containing_type());
  expect_eq(message2_, quux2_->containing_type());
}

test_f(descriptortest, fieldmessagetype) {
  expect_true(foo_->message_type() == null);
  expect_true(bar_->message_type() == null);

  expect_eq(foreign_, baz_->message_type());
  expect_eq(foreign_, qux_->message_type());
}

test_f(descriptortest, fieldenumtype) {
  expect_true(foo_->enum_type() == null);
  expect_true(baz_->enum_type() == null);
  expect_true(qux_->enum_type() == null);

  expect_eq(enum_, bar_->enum_type());
}

// ===================================================================

class stylizedfieldnamestest : public testing::test {
 protected:
  void setup() {
    filedescriptorproto file;
    file.set_name("foo.proto");

    addextensionrange(addmessage(&file, "extendablemessage"), 1, 1000);

    descriptorproto* message = addmessage(&file, "testmessage");
    addfield(message, "foo_foo", 1,
             fielddescriptorproto::label_optional,
             fielddescriptorproto::type_int32);
    addfield(message, "foobar", 2,
             fielddescriptorproto::label_optional,
             fielddescriptorproto::type_int32);
    addfield(message, "foobaz", 3,
             fielddescriptorproto::label_optional,
             fielddescriptorproto::type_int32);
    addfield(message, "foofoo", 4,  // camel-case conflict with foo_foo.
             fielddescriptorproto::label_optional,
             fielddescriptorproto::type_int32);
    addfield(message, "foobar", 5,  // lower-case conflict with foobar.
             fielddescriptorproto::label_optional,
             fielddescriptorproto::type_int32);

    addnestedextension(message, "extendablemessage", "bar_foo", 1,
                       fielddescriptorproto::label_optional,
                       fielddescriptorproto::type_int32);
    addnestedextension(message, "extendablemessage", "barbar", 2,
                       fielddescriptorproto::label_optional,
                       fielddescriptorproto::type_int32);
    addnestedextension(message, "extendablemessage", "barbaz", 3,
                       fielddescriptorproto::label_optional,
                       fielddescriptorproto::type_int32);
    addnestedextension(message, "extendablemessage", "barfoo", 4,  // conflict
                       fielddescriptorproto::label_optional,
                       fielddescriptorproto::type_int32);
    addnestedextension(message, "extendablemessage", "barbar", 5,  // conflict
                       fielddescriptorproto::label_optional,
                       fielddescriptorproto::type_int32);

    addextension(&file, "extendablemessage", "baz_foo", 11,
                 fielddescriptorproto::label_optional,
                 fielddescriptorproto::type_int32);
    addextension(&file, "extendablemessage", "bazbar", 12,
                 fielddescriptorproto::label_optional,
                 fielddescriptorproto::type_int32);
    addextension(&file, "extendablemessage", "bazbaz", 13,
                 fielddescriptorproto::label_optional,
                 fielddescriptorproto::type_int32);
    addextension(&file, "extendablemessage", "bazfoo", 14,  // conflict
                 fielddescriptorproto::label_optional,
                 fielddescriptorproto::type_int32);
    addextension(&file, "extendablemessage", "bazbar", 15,  // conflict
                 fielddescriptorproto::label_optional,
                 fielddescriptorproto::type_int32);

    file_ = pool_.buildfile(file);
    assert_true(file_ != null);
    assert_eq(2, file_->message_type_count());
    message_ = file_->message_type(1);
    assert_eq("testmessage", message_->name());
    assert_eq(5, message_->field_count());
    assert_eq(5, message_->extension_count());
    assert_eq(5, file_->extension_count());
  }

  descriptorpool pool_;
  const filedescriptor* file_;
  const descriptor* message_;
};

test_f(stylizedfieldnamestest, lowercasename) {
  expect_eq("foo_foo", message_->field(0)->lowercase_name());
  expect_eq("foobar" , message_->field(1)->lowercase_name());
  expect_eq("foobaz" , message_->field(2)->lowercase_name());
  expect_eq("foofoo" , message_->field(3)->lowercase_name());
  expect_eq("foobar" , message_->field(4)->lowercase_name());

  expect_eq("bar_foo", message_->extension(0)->lowercase_name());
  expect_eq("barbar" , message_->extension(1)->lowercase_name());
  expect_eq("barbaz" , message_->extension(2)->lowercase_name());
  expect_eq("barfoo" , message_->extension(3)->lowercase_name());
  expect_eq("barbar" , message_->extension(4)->lowercase_name());

  expect_eq("baz_foo", file_->extension(0)->lowercase_name());
  expect_eq("bazbar" , file_->extension(1)->lowercase_name());
  expect_eq("bazbaz" , file_->extension(2)->lowercase_name());
  expect_eq("bazfoo" , file_->extension(3)->lowercase_name());
  expect_eq("bazbar" , file_->extension(4)->lowercase_name());
}

test_f(stylizedfieldnamestest, camelcasename) {
  expect_eq("foofoo", message_->field(0)->camelcase_name());
  expect_eq("foobar", message_->field(1)->camelcase_name());
  expect_eq("foobaz", message_->field(2)->camelcase_name());
  expect_eq("foofoo", message_->field(3)->camelcase_name());
  expect_eq("foobar", message_->field(4)->camelcase_name());

  expect_eq("barfoo", message_->extension(0)->camelcase_name());
  expect_eq("barbar", message_->extension(1)->camelcase_name());
  expect_eq("barbaz", message_->extension(2)->camelcase_name());
  expect_eq("barfoo", message_->extension(3)->camelcase_name());
  expect_eq("barbar", message_->extension(4)->camelcase_name());

  expect_eq("bazfoo", file_->extension(0)->camelcase_name());
  expect_eq("bazbar", file_->extension(1)->camelcase_name());
  expect_eq("bazbaz", file_->extension(2)->camelcase_name());
  expect_eq("bazfoo", file_->extension(3)->camelcase_name());
  expect_eq("bazbar", file_->extension(4)->camelcase_name());
}

test_f(stylizedfieldnamestest, findbylowercasename) {
  expect_eq(message_->field(0),
            message_->findfieldbylowercasename("foo_foo"));
  expect_eq(message_->field(1),
            message_->findfieldbylowercasename("foobar"));
  expect_eq(message_->field(2),
            message_->findfieldbylowercasename("foobaz"));
  expect_true(message_->findfieldbylowercasename("foobar") == null);
  expect_true(message_->findfieldbylowercasename("foobaz") == null);
  expect_true(message_->findfieldbylowercasename("bar_foo") == null);
  expect_true(message_->findfieldbylowercasename("nosuchfield") == null);

  expect_eq(message_->extension(0),
            message_->findextensionbylowercasename("bar_foo"));
  expect_eq(message_->extension(1),
            message_->findextensionbylowercasename("barbar"));
  expect_eq(message_->extension(2),
            message_->findextensionbylowercasename("barbaz"));
  expect_true(message_->findextensionbylowercasename("barbar") == null);
  expect_true(message_->findextensionbylowercasename("barbaz") == null);
  expect_true(message_->findextensionbylowercasename("foo_foo") == null);
  expect_true(message_->findextensionbylowercasename("nosuchfield") == null);

  expect_eq(file_->extension(0),
            file_->findextensionbylowercasename("baz_foo"));
  expect_eq(file_->extension(1),
            file_->findextensionbylowercasename("bazbar"));
  expect_eq(file_->extension(2),
            file_->findextensionbylowercasename("bazbaz"));
  expect_true(file_->findextensionbylowercasename("bazbar") == null);
  expect_true(file_->findextensionbylowercasename("bazbaz") == null);
  expect_true(file_->findextensionbylowercasename("nosuchfield") == null);
}

test_f(stylizedfieldnamestest, findbycamelcasename) {
  expect_eq(message_->field(0),
            message_->findfieldbycamelcasename("foofoo"));
  expect_eq(message_->field(1),
            message_->findfieldbycamelcasename("foobar"));
  expect_eq(message_->field(2),
            message_->findfieldbycamelcasename("foobaz"));
  expect_true(message_->findfieldbycamelcasename("foo_foo") == null);
  expect_true(message_->findfieldbycamelcasename("foobar") == null);
  expect_true(message_->findfieldbycamelcasename("barfoo") == null);
  expect_true(message_->findfieldbycamelcasename("nosuchfield") == null);

  expect_eq(message_->extension(0),
            message_->findextensionbycamelcasename("barfoo"));
  expect_eq(message_->extension(1),
            message_->findextensionbycamelcasename("barbar"));
  expect_eq(message_->extension(2),
            message_->findextensionbycamelcasename("barbaz"));
  expect_true(message_->findextensionbycamelcasename("bar_foo") == null);
  expect_true(message_->findextensionbycamelcasename("barbar") == null);
  expect_true(message_->findextensionbycamelcasename("foofoo") == null);
  expect_true(message_->findextensionbycamelcasename("nosuchfield") == null);

  expect_eq(file_->extension(0),
            file_->findextensionbycamelcasename("bazfoo"));
  expect_eq(file_->extension(1),
            file_->findextensionbycamelcasename("bazbar"));
  expect_eq(file_->extension(2),
            file_->findextensionbycamelcasename("bazbaz"));
  expect_true(file_->findextensionbycamelcasename("baz_foo") == null);
  expect_true(file_->findextensionbycamelcasename("bazbar") == null);
  expect_true(file_->findextensionbycamelcasename("nosuchfield") == null);
}

// ===================================================================

// test enum descriptors.
class enumdescriptortest : public testing::test {
 protected:
  virtual void setup() {
    // build descriptors for the following definitions:
    //
    //   // in "foo.proto"
    //   enum testenum {
    //     foo = 1;
    //     bar = 2;
    //   }
    //
    //   // in "bar.proto"
    //   package corge.grault;
    //   enum testenum2 {
    //     foo = 1;
    //     baz = 3;
    //   }
    //
    // testenum2 is primarily here to test findvaluebyname and friends.
    // all enums created from the same descriptorpool share the same lookup
    // table, so we need to insure that they don't interfere.

    // testenum
    filedescriptorproto foo_file;
    foo_file.set_name("foo.proto");

    enumdescriptorproto* enum_proto = addenum(&foo_file, "testenum");
    addenumvalue(enum_proto, "foo", 1);
    addenumvalue(enum_proto, "bar", 2);

    // testenum2
    filedescriptorproto bar_file;
    bar_file.set_name("bar.proto");
    bar_file.set_package("corge.grault");

    enumdescriptorproto* enum2_proto = addenum(&bar_file, "testenum2");
    addenumvalue(enum2_proto, "foo", 1);
    addenumvalue(enum2_proto, "baz", 3);

    // build the descriptors and get the pointers.
    foo_file_ = pool_.buildfile(foo_file);
    assert_true(foo_file_ != null);

    bar_file_ = pool_.buildfile(bar_file);
    assert_true(bar_file_ != null);

    assert_eq(1, foo_file_->enum_type_count());
    enum_ = foo_file_->enum_type(0);

    assert_eq(2, enum_->value_count());
    foo_ = enum_->value(0);
    bar_ = enum_->value(1);

    assert_eq(1, bar_file_->enum_type_count());
    enum2_ = bar_file_->enum_type(0);

    assert_eq(2, enum2_->value_count());
    foo2_ = enum2_->value(0);
    baz2_ = enum2_->value(1);
  }

  descriptorpool pool_;

  const filedescriptor* foo_file_;
  const filedescriptor* bar_file_;

  const enumdescriptor* enum_;
  const enumdescriptor* enum2_;

  const enumvaluedescriptor* foo_;
  const enumvaluedescriptor* bar_;

  const enumvaluedescriptor* foo2_;
  const enumvaluedescriptor* baz2_;
};

test_f(enumdescriptortest, name) {
  expect_eq("testenum", enum_->name());
  expect_eq("testenum", enum_->full_name());
  expect_eq(foo_file_, enum_->file());

  expect_eq("testenum2", enum2_->name());
  expect_eq("corge.grault.testenum2", enum2_->full_name());
  expect_eq(bar_file_, enum2_->file());
}

test_f(enumdescriptortest, containingtype) {
  expect_true(enum_->containing_type() == null);
  expect_true(enum2_->containing_type() == null);
}

test_f(enumdescriptortest, valuesbyindex) {
  assert_eq(2, enum_->value_count());
  expect_eq(foo_, enum_->value(0));
  expect_eq(bar_, enum_->value(1));
}

test_f(enumdescriptortest, findvaluebyname) {
  expect_eq(foo_ , enum_ ->findvaluebyname("foo"));
  expect_eq(bar_ , enum_ ->findvaluebyname("bar"));
  expect_eq(foo2_, enum2_->findvaluebyname("foo"));
  expect_eq(baz2_, enum2_->findvaluebyname("baz"));

  expect_true(enum_ ->findvaluebyname("no_such_value") == null);
  expect_true(enum_ ->findvaluebyname("baz"          ) == null);
  expect_true(enum2_->findvaluebyname("bar"          ) == null);
}

test_f(enumdescriptortest, findvaluebynumber) {
  expect_eq(foo_ , enum_ ->findvaluebynumber(1));
  expect_eq(bar_ , enum_ ->findvaluebynumber(2));
  expect_eq(foo2_, enum2_->findvaluebynumber(1));
  expect_eq(baz2_, enum2_->findvaluebynumber(3));

  expect_true(enum_ ->findvaluebynumber(416) == null);
  expect_true(enum_ ->findvaluebynumber(3) == null);
  expect_true(enum2_->findvaluebynumber(2) == null);
}

test_f(enumdescriptortest, valuename) {
  expect_eq("foo", foo_->name());
  expect_eq("bar", bar_->name());
}

test_f(enumdescriptortest, valuefullname) {
  expect_eq("foo", foo_->full_name());
  expect_eq("bar", bar_->full_name());
  expect_eq("corge.grault.foo", foo2_->full_name());
  expect_eq("corge.grault.baz", baz2_->full_name());
}

test_f(enumdescriptortest, valueindex) {
  expect_eq(0, foo_->index());
  expect_eq(1, bar_->index());
}

test_f(enumdescriptortest, valuenumber) {
  expect_eq(1, foo_->number());
  expect_eq(2, bar_->number());
}

test_f(enumdescriptortest, valuetype) {
  expect_eq(enum_ , foo_ ->type());
  expect_eq(enum_ , bar_ ->type());
  expect_eq(enum2_, foo2_->type());
  expect_eq(enum2_, baz2_->type());
}

// ===================================================================

// test service descriptors.
class servicedescriptortest : public testing::test {
 protected:
  virtual void setup() {
    // build descriptors for the following messages and service:
    //    // in "foo.proto"
    //    message foorequest  {}
    //    message fooresponse {}
    //    message barrequest  {}
    //    message barresponse {}
    //    message bazrequest  {}
    //    message bazresponse {}
    //
    //    service testservice {
    //      rpc foo(foorequest) returns (fooresponse);
    //      rpc bar(barrequest) returns (barresponse);
    //    }
    //
    //    // in "bar.proto"
    //    package corge.grault
    //    service testservice2 {
    //      rpc foo(foorequest) returns (fooresponse);
    //      rpc baz(bazrequest) returns (bazresponse);
    //    }

    filedescriptorproto foo_file;
    foo_file.set_name("foo.proto");

    addmessage(&foo_file, "foorequest");
    addmessage(&foo_file, "fooresponse");
    addmessage(&foo_file, "barrequest");
    addmessage(&foo_file, "barresponse");
    addmessage(&foo_file, "bazrequest");
    addmessage(&foo_file, "bazresponse");

    servicedescriptorproto* service = addservice(&foo_file, "testservice");
    addmethod(service, "foo", "foorequest", "fooresponse");
    addmethod(service, "bar", "barrequest", "barresponse");

    filedescriptorproto bar_file;
    bar_file.set_name("bar.proto");
    bar_file.set_package("corge.grault");
    bar_file.add_dependency("foo.proto");

    servicedescriptorproto* service2 = addservice(&bar_file, "testservice2");
    addmethod(service2, "foo", "foorequest", "fooresponse");
    addmethod(service2, "baz", "bazrequest", "bazresponse");

    // build the descriptors and get the pointers.
    foo_file_ = pool_.buildfile(foo_file);
    assert_true(foo_file_ != null);

    bar_file_ = pool_.buildfile(bar_file);
    assert_true(bar_file_ != null);

    assert_eq(6, foo_file_->message_type_count());
    foo_request_  = foo_file_->message_type(0);
    foo_response_ = foo_file_->message_type(1);
    bar_request_  = foo_file_->message_type(2);
    bar_response_ = foo_file_->message_type(3);
    baz_request_  = foo_file_->message_type(4);
    baz_response_ = foo_file_->message_type(5);

    assert_eq(1, foo_file_->service_count());
    service_ = foo_file_->service(0);

    assert_eq(2, service_->method_count());
    foo_ = service_->method(0);
    bar_ = service_->method(1);

    assert_eq(1, bar_file_->service_count());
    service2_ = bar_file_->service(0);

    assert_eq(2, service2_->method_count());
    foo2_ = service2_->method(0);
    baz2_ = service2_->method(1);
  }

  descriptorpool pool_;

  const filedescriptor* foo_file_;
  const filedescriptor* bar_file_;

  const descriptor* foo_request_;
  const descriptor* foo_response_;
  const descriptor* bar_request_;
  const descriptor* bar_response_;
  const descriptor* baz_request_;
  const descriptor* baz_response_;

  const servicedescriptor* service_;
  const servicedescriptor* service2_;

  const methoddescriptor* foo_;
  const methoddescriptor* bar_;

  const methoddescriptor* foo2_;
  const methoddescriptor* baz2_;
};

test_f(servicedescriptortest, name) {
  expect_eq("testservice", service_->name());
  expect_eq("testservice", service_->full_name());
  expect_eq(foo_file_, service_->file());

  expect_eq("testservice2", service2_->name());
  expect_eq("corge.grault.testservice2", service2_->full_name());
  expect_eq(bar_file_, service2_->file());
}

test_f(servicedescriptortest, methodsbyindex) {
  assert_eq(2, service_->method_count());
  expect_eq(foo_, service_->method(0));
  expect_eq(bar_, service_->method(1));
}

test_f(servicedescriptortest, findmethodbyname) {
  expect_eq(foo_ , service_ ->findmethodbyname("foo"));
  expect_eq(bar_ , service_ ->findmethodbyname("bar"));
  expect_eq(foo2_, service2_->findmethodbyname("foo"));
  expect_eq(baz2_, service2_->findmethodbyname("baz"));

  expect_true(service_ ->findmethodbyname("nosuchmethod") == null);
  expect_true(service_ ->findmethodbyname("baz"         ) == null);
  expect_true(service2_->findmethodbyname("bar"         ) == null);
}

test_f(servicedescriptortest, methodname) {
  expect_eq("foo", foo_->name());
  expect_eq("bar", bar_->name());
}

test_f(servicedescriptortest, methodfullname) {
  expect_eq("testservice.foo", foo_->full_name());
  expect_eq("testservice.bar", bar_->full_name());
  expect_eq("corge.grault.testservice2.foo", foo2_->full_name());
  expect_eq("corge.grault.testservice2.baz", baz2_->full_name());
}

test_f(servicedescriptortest, methodindex) {
  expect_eq(0, foo_->index());
  expect_eq(1, bar_->index());
}

test_f(servicedescriptortest, methodparent) {
  expect_eq(service_, foo_->service());
  expect_eq(service_, bar_->service());
}

test_f(servicedescriptortest, methodinputtype) {
  expect_eq(foo_request_, foo_->input_type());
  expect_eq(bar_request_, bar_->input_type());
}

test_f(servicedescriptortest, methodoutputtype) {
  expect_eq(foo_response_, foo_->output_type());
  expect_eq(bar_response_, bar_->output_type());
}

// ===================================================================

// test nested types.
class nesteddescriptortest : public testing::test {
 protected:
  virtual void setup() {
    // build descriptors for the following definitions:
    //
    //   // in "foo.proto"
    //   message testmessage {
    //     message foo {}
    //     message bar {}
    //     enum baz { a = 1; }
    //     enum qux { b = 1; }
    //   }
    //
    //   // in "bar.proto"
    //   package corge.grault;
    //   message testmessage2 {
    //     message foo {}
    //     message baz {}
    //     enum qux  { a = 1; }
    //     enum quux { c = 1; }
    //   }
    //
    // testmessage2 is primarily here to test findnestedtypebyname and friends.
    // all messages created from the same descriptorpool share the same lookup
    // table, so we need to insure that they don't interfere.
    //
    // we add enum values to the enums in order to test searching for enum
    // values across a message's scope.

    filedescriptorproto foo_file;
    foo_file.set_name("foo.proto");

    descriptorproto* message = addmessage(&foo_file, "testmessage");
    addnestedmessage(message, "foo");
    addnestedmessage(message, "bar");
    enumdescriptorproto* baz = addnestedenum(message, "baz");
    addenumvalue(baz, "a", 1);
    enumdescriptorproto* qux = addnestedenum(message, "qux");
    addenumvalue(qux, "b", 1);

    filedescriptorproto bar_file;
    bar_file.set_name("bar.proto");
    bar_file.set_package("corge.grault");

    descriptorproto* message2 = addmessage(&bar_file, "testmessage2");
    addnestedmessage(message2, "foo");
    addnestedmessage(message2, "baz");
    enumdescriptorproto* qux2 = addnestedenum(message2, "qux");
    addenumvalue(qux2, "a", 1);
    enumdescriptorproto* quux2 = addnestedenum(message2, "quux");
    addenumvalue(quux2, "c", 1);

    // build the descriptors and get the pointers.
    foo_file_ = pool_.buildfile(foo_file);
    assert_true(foo_file_ != null);

    bar_file_ = pool_.buildfile(bar_file);
    assert_true(bar_file_ != null);

    assert_eq(1, foo_file_->message_type_count());
    message_ = foo_file_->message_type(0);

    assert_eq(2, message_->nested_type_count());
    foo_ = message_->nested_type(0);
    bar_ = message_->nested_type(1);

    assert_eq(2, message_->enum_type_count());
    baz_ = message_->enum_type(0);
    qux_ = message_->enum_type(1);

    assert_eq(1, baz_->value_count());
    a_ = baz_->value(0);
    assert_eq(1, qux_->value_count());
    b_ = qux_->value(0);

    assert_eq(1, bar_file_->message_type_count());
    message2_ = bar_file_->message_type(0);

    assert_eq(2, message2_->nested_type_count());
    foo2_ = message2_->nested_type(0);
    baz2_ = message2_->nested_type(1);

    assert_eq(2, message2_->enum_type_count());
    qux2_ = message2_->enum_type(0);
    quux2_ = message2_->enum_type(1);

    assert_eq(1, qux2_->value_count());
    a2_ = qux2_->value(0);
    assert_eq(1, quux2_->value_count());
    c2_ = quux2_->value(0);
  }

  descriptorpool pool_;

  const filedescriptor* foo_file_;
  const filedescriptor* bar_file_;

  const descriptor* message_;
  const descriptor* message2_;

  const descriptor* foo_;
  const descriptor* bar_;
  const enumdescriptor* baz_;
  const enumdescriptor* qux_;
  const enumvaluedescriptor* a_;
  const enumvaluedescriptor* b_;

  const descriptor* foo2_;
  const descriptor* baz2_;
  const enumdescriptor* qux2_;
  const enumdescriptor* quux2_;
  const enumvaluedescriptor* a2_;
  const enumvaluedescriptor* c2_;
};

test_f(nesteddescriptortest, messagename) {
  expect_eq("foo", foo_ ->name());
  expect_eq("bar", bar_ ->name());
  expect_eq("foo", foo2_->name());
  expect_eq("baz", baz2_->name());

  expect_eq("testmessage.foo", foo_->full_name());
  expect_eq("testmessage.bar", bar_->full_name());
  expect_eq("corge.grault.testmessage2.foo", foo2_->full_name());
  expect_eq("corge.grault.testmessage2.baz", baz2_->full_name());
}

test_f(nesteddescriptortest, messagecontainingtype) {
  expect_eq(message_ , foo_ ->containing_type());
  expect_eq(message_ , bar_ ->containing_type());
  expect_eq(message2_, foo2_->containing_type());
  expect_eq(message2_, baz2_->containing_type());
}

test_f(nesteddescriptortest, nestedmessagesbyindex) {
  assert_eq(2, message_->nested_type_count());
  expect_eq(foo_, message_->nested_type(0));
  expect_eq(bar_, message_->nested_type(1));
}

test_f(nesteddescriptortest, findfieldbynamedoesntfindnestedtypes) {
  expect_true(message_->findfieldbyname("foo") == null);
  expect_true(message_->findfieldbyname("qux") == null);
  expect_true(message_->findextensionbyname("foo") == null);
  expect_true(message_->findextensionbyname("qux") == null);
}

test_f(nesteddescriptortest, findnestedtypebyname) {
  expect_eq(foo_ , message_ ->findnestedtypebyname("foo"));
  expect_eq(bar_ , message_ ->findnestedtypebyname("bar"));
  expect_eq(foo2_, message2_->findnestedtypebyname("foo"));
  expect_eq(baz2_, message2_->findnestedtypebyname("baz"));

  expect_true(message_ ->findnestedtypebyname("nosuchtype") == null);
  expect_true(message_ ->findnestedtypebyname("baz"       ) == null);
  expect_true(message2_->findnestedtypebyname("bar"       ) == null);

  expect_true(message_->findnestedtypebyname("qux") == null);
}

test_f(nesteddescriptortest, enumname) {
  expect_eq("baz" , baz_ ->name());
  expect_eq("qux" , qux_ ->name());
  expect_eq("qux" , qux2_->name());
  expect_eq("quux", quux2_->name());

  expect_eq("testmessage.baz", baz_->full_name());
  expect_eq("testmessage.qux", qux_->full_name());
  expect_eq("corge.grault.testmessage2.qux" , qux2_ ->full_name());
  expect_eq("corge.grault.testmessage2.quux", quux2_->full_name());
}

test_f(nesteddescriptortest, enumcontainingtype) {
  expect_eq(message_ , baz_  ->containing_type());
  expect_eq(message_ , qux_  ->containing_type());
  expect_eq(message2_, qux2_ ->containing_type());
  expect_eq(message2_, quux2_->containing_type());
}

test_f(nesteddescriptortest, nestedenumsbyindex) {
  assert_eq(2, message_->nested_type_count());
  expect_eq(foo_, message_->nested_type(0));
  expect_eq(bar_, message_->nested_type(1));
}

test_f(nesteddescriptortest, findenumtypebyname) {
  expect_eq(baz_  , message_ ->findenumtypebyname("baz" ));
  expect_eq(qux_  , message_ ->findenumtypebyname("qux" ));
  expect_eq(qux2_ , message2_->findenumtypebyname("qux" ));
  expect_eq(quux2_, message2_->findenumtypebyname("quux"));

  expect_true(message_ ->findenumtypebyname("nosuchtype") == null);
  expect_true(message_ ->findenumtypebyname("quux"      ) == null);
  expect_true(message2_->findenumtypebyname("baz"       ) == null);

  expect_true(message_->findenumtypebyname("foo") == null);
}

test_f(nesteddescriptortest, findenumvaluebyname) {
  expect_eq(a_ , message_ ->findenumvaluebyname("a"));
  expect_eq(b_ , message_ ->findenumvaluebyname("b"));
  expect_eq(a2_, message2_->findenumvaluebyname("a"));
  expect_eq(c2_, message2_->findenumvaluebyname("c"));

  expect_true(message_ ->findenumvaluebyname("no_such_value") == null);
  expect_true(message_ ->findenumvaluebyname("c"            ) == null);
  expect_true(message2_->findenumvaluebyname("b"            ) == null);

  expect_true(message_->findenumvaluebyname("foo") == null);
}

// ===================================================================

// test extensions.
class extensiondescriptortest : public testing::test {
 protected:
  virtual void setup() {
    // build descriptors for the following definitions:
    //
    //   enum baz {}
    //   message qux {}
    //
    //   message foo {
    //     extensions 10 to 19;
    //     extensions 30 to 39;
    //   }
    //   extends foo with optional int32 foo_int32 = 10;
    //   extends foo with repeated testenum foo_enum = 19;
    //   message bar {
    //     extends foo with optional qux foo_message = 30;
    //     // (using qux as the group type)
    //     extends foo with repeated group foo_group = 39;
    //   }

    filedescriptorproto foo_file;
    foo_file.set_name("foo.proto");

    addemptyenum(&foo_file, "baz");
    addmessage(&foo_file, "qux");

    descriptorproto* foo = addmessage(&foo_file, "foo");
    addextensionrange(foo, 10, 20);
    addextensionrange(foo, 30, 40);

    addextension(&foo_file, "foo", "foo_int32", 10,
                 fielddescriptorproto::label_optional,
                 fielddescriptorproto::type_int32);
    addextension(&foo_file, "foo", "foo_enum", 19,
                 fielddescriptorproto::label_repeated,
                 fielddescriptorproto::type_enum)
      ->set_type_name("baz");

    descriptorproto* bar = addmessage(&foo_file, "bar");
    addnestedextension(bar, "foo", "foo_message", 30,
                       fielddescriptorproto::label_optional,
                       fielddescriptorproto::type_message)
      ->set_type_name("qux");
    addnestedextension(bar, "foo", "foo_group", 39,
                       fielddescriptorproto::label_repeated,
                       fielddescriptorproto::type_group)
      ->set_type_name("qux");

    // build the descriptors and get the pointers.
    foo_file_ = pool_.buildfile(foo_file);
    assert_true(foo_file_ != null);

    assert_eq(1, foo_file_->enum_type_count());
    baz_ = foo_file_->enum_type(0);

    assert_eq(3, foo_file_->message_type_count());
    qux_ = foo_file_->message_type(0);
    foo_ = foo_file_->message_type(1);
    bar_ = foo_file_->message_type(2);
  }

  descriptorpool pool_;

  const filedescriptor* foo_file_;

  const descriptor* foo_;
  const descriptor* bar_;
  const enumdescriptor* baz_;
  const descriptor* qux_;
};

test_f(extensiondescriptortest, extensionranges) {
  expect_eq(0, bar_->extension_range_count());
  assert_eq(2, foo_->extension_range_count());

  expect_eq(10, foo_->extension_range(0)->start);
  expect_eq(30, foo_->extension_range(1)->start);

  expect_eq(20, foo_->extension_range(0)->end);
  expect_eq(40, foo_->extension_range(1)->end);
};

test_f(extensiondescriptortest, extensions) {
  expect_eq(0, foo_->extension_count());
  assert_eq(2, foo_file_->extension_count());
  assert_eq(2, bar_->extension_count());

  expect_true(foo_file_->extension(0)->is_extension());
  expect_true(foo_file_->extension(1)->is_extension());
  expect_true(bar_->extension(0)->is_extension());
  expect_true(bar_->extension(1)->is_extension());

  expect_eq("foo_int32"  , foo_file_->extension(0)->name());
  expect_eq("foo_enum"   , foo_file_->extension(1)->name());
  expect_eq("foo_message", bar_->extension(0)->name());
  expect_eq("foo_group"  , bar_->extension(1)->name());

  expect_eq(10, foo_file_->extension(0)->number());
  expect_eq(19, foo_file_->extension(1)->number());
  expect_eq(30, bar_->extension(0)->number());
  expect_eq(39, bar_->extension(1)->number());

  expect_eq(fielddescriptor::type_int32  , foo_file_->extension(0)->type());
  expect_eq(fielddescriptor::type_enum   , foo_file_->extension(1)->type());
  expect_eq(fielddescriptor::type_message, bar_->extension(0)->type());
  expect_eq(fielddescriptor::type_group  , bar_->extension(1)->type());

  expect_eq(baz_, foo_file_->extension(1)->enum_type());
  expect_eq(qux_, bar_->extension(0)->message_type());
  expect_eq(qux_, bar_->extension(1)->message_type());

  expect_eq(fielddescriptor::label_optional, foo_file_->extension(0)->label());
  expect_eq(fielddescriptor::label_repeated, foo_file_->extension(1)->label());
  expect_eq(fielddescriptor::label_optional, bar_->extension(0)->label());
  expect_eq(fielddescriptor::label_repeated, bar_->extension(1)->label());

  expect_eq(foo_, foo_file_->extension(0)->containing_type());
  expect_eq(foo_, foo_file_->extension(1)->containing_type());
  expect_eq(foo_, bar_->extension(0)->containing_type());
  expect_eq(foo_, bar_->extension(1)->containing_type());

  expect_true(foo_file_->extension(0)->extension_scope() == null);
  expect_true(foo_file_->extension(1)->extension_scope() == null);
  expect_eq(bar_, bar_->extension(0)->extension_scope());
  expect_eq(bar_, bar_->extension(1)->extension_scope());
};

test_f(extensiondescriptortest, isextensionnumber) {
  expect_false(foo_->isextensionnumber( 9));
  expect_true (foo_->isextensionnumber(10));
  expect_true (foo_->isextensionnumber(19));
  expect_false(foo_->isextensionnumber(20));
  expect_false(foo_->isextensionnumber(29));
  expect_true (foo_->isextensionnumber(30));
  expect_true (foo_->isextensionnumber(39));
  expect_false(foo_->isextensionnumber(40));
}

test_f(extensiondescriptortest, findextensionbyname) {
  // note that filedescriptor::findextensionbyname() is tested by
  // filedescriptortest.
  assert_eq(2, bar_->extension_count());

  expect_eq(bar_->extension(0), bar_->findextensionbyname("foo_message"));
  expect_eq(bar_->extension(1), bar_->findextensionbyname("foo_group"  ));

  expect_true(bar_->findextensionbyname("no_such_extension") == null);
  expect_true(foo_->findextensionbyname("foo_int32") == null);
  expect_true(foo_->findextensionbyname("foo_message") == null);
}

test_f(extensiondescriptortest, findallextensions) {
  vector<const fielddescriptor*> extensions;
  pool_.findallextensions(foo_, &extensions);
  assert_eq(4, extensions.size());
  expect_eq(10, extensions[0]->number());
  expect_eq(19, extensions[1]->number());
  expect_eq(30, extensions[2]->number());
  expect_eq(39, extensions[3]->number());
}

// ===================================================================

class misctest : public testing::test {
 protected:
  // function which makes a field descriptor of the given type.
  const fielddescriptor* getfielddescriptoroftype(fielddescriptor::type type) {
    filedescriptorproto file_proto;
    file_proto.set_name("foo.proto");
    addemptyenum(&file_proto, "dummyenum");

    descriptorproto* message = addmessage(&file_proto, "testmessage");
    fielddescriptorproto* field =
      addfield(message, "foo", 1, fielddescriptorproto::label_optional,
               static_cast<fielddescriptorproto::type>(static_cast<int>(type)));

    if (type == fielddescriptor::type_message ||
        type == fielddescriptor::type_group) {
      field->set_type_name("testmessage");
    } else if (type == fielddescriptor::type_enum) {
      field->set_type_name("dummyenum");
    }

    // build the descriptors and get the pointers.
    pool_.reset(new descriptorpool());
    const filedescriptor* file = pool_->buildfile(file_proto);

    if (file != null &&
        file->message_type_count() == 1 &&
        file->message_type(0)->field_count() == 1) {
      return file->message_type(0)->field(0);
    } else {
      return null;
    }
  }

  const char* gettypenameforfieldtype(fielddescriptor::type type) {
    const fielddescriptor* field = getfielddescriptoroftype(type);
    return field != null ? field->type_name() : "";
  }

  fielddescriptor::cpptype getcpptypeforfieldtype(fielddescriptor::type type) {
    const fielddescriptor* field = getfielddescriptoroftype(type);
    return field != null ? field->cpp_type() :
        static_cast<fielddescriptor::cpptype>(0);
  }

  const char* getcpptypenameforfieldtype(fielddescriptor::type type) {
    const fielddescriptor* field = getfielddescriptoroftype(type);
    return field != null ? field->cpp_type_name() : "";
  }

  scoped_ptr<descriptorpool> pool_;
};

test_f(misctest, typenames) {
  // test that correct type names are returned.

  typedef fielddescriptor fd;  // avoid ugly line wrapping

  expect_streq("double"  , gettypenameforfieldtype(fd::type_double  ));
  expect_streq("float"   , gettypenameforfieldtype(fd::type_float   ));
  expect_streq("int64"   , gettypenameforfieldtype(fd::type_int64   ));
  expect_streq("uint64"  , gettypenameforfieldtype(fd::type_uint64  ));
  expect_streq("int32"   , gettypenameforfieldtype(fd::type_int32   ));
  expect_streq("fixed64" , gettypenameforfieldtype(fd::type_fixed64 ));
  expect_streq("fixed32" , gettypenameforfieldtype(fd::type_fixed32 ));
  expect_streq("bool"    , gettypenameforfieldtype(fd::type_bool    ));
  expect_streq("string"  , gettypenameforfieldtype(fd::type_string  ));
  expect_streq("group"   , gettypenameforfieldtype(fd::type_group   ));
  expect_streq("message" , gettypenameforfieldtype(fd::type_message ));
  expect_streq("bytes"   , gettypenameforfieldtype(fd::type_bytes   ));
  expect_streq("uint32"  , gettypenameforfieldtype(fd::type_uint32  ));
  expect_streq("enum"    , gettypenameforfieldtype(fd::type_enum    ));
  expect_streq("sfixed32", gettypenameforfieldtype(fd::type_sfixed32));
  expect_streq("sfixed64", gettypenameforfieldtype(fd::type_sfixed64));
  expect_streq("sint32"  , gettypenameforfieldtype(fd::type_sint32  ));
  expect_streq("sint64"  , gettypenameforfieldtype(fd::type_sint64  ));
}

test_f(misctest, cpptypes) {
  // test that cpp types are assigned correctly.

  typedef fielddescriptor fd;  // avoid ugly line wrapping

  expect_eq(fd::cpptype_double , getcpptypeforfieldtype(fd::type_double  ));
  expect_eq(fd::cpptype_float  , getcpptypeforfieldtype(fd::type_float   ));
  expect_eq(fd::cpptype_int64  , getcpptypeforfieldtype(fd::type_int64   ));
  expect_eq(fd::cpptype_uint64 , getcpptypeforfieldtype(fd::type_uint64  ));
  expect_eq(fd::cpptype_int32  , getcpptypeforfieldtype(fd::type_int32   ));
  expect_eq(fd::cpptype_uint64 , getcpptypeforfieldtype(fd::type_fixed64 ));
  expect_eq(fd::cpptype_uint32 , getcpptypeforfieldtype(fd::type_fixed32 ));
  expect_eq(fd::cpptype_bool   , getcpptypeforfieldtype(fd::type_bool    ));
  expect_eq(fd::cpptype_string , getcpptypeforfieldtype(fd::type_string  ));
  expect_eq(fd::cpptype_message, getcpptypeforfieldtype(fd::type_group   ));
  expect_eq(fd::cpptype_message, getcpptypeforfieldtype(fd::type_message ));
  expect_eq(fd::cpptype_string , getcpptypeforfieldtype(fd::type_bytes   ));
  expect_eq(fd::cpptype_uint32 , getcpptypeforfieldtype(fd::type_uint32  ));
  expect_eq(fd::cpptype_enum   , getcpptypeforfieldtype(fd::type_enum    ));
  expect_eq(fd::cpptype_int32  , getcpptypeforfieldtype(fd::type_sfixed32));
  expect_eq(fd::cpptype_int64  , getcpptypeforfieldtype(fd::type_sfixed64));
  expect_eq(fd::cpptype_int32  , getcpptypeforfieldtype(fd::type_sint32  ));
  expect_eq(fd::cpptype_int64  , getcpptypeforfieldtype(fd::type_sint64  ));
}

test_f(misctest, cpptypenames) {
  // test that correct cpp type names are returned.

  typedef fielddescriptor fd;  // avoid ugly line wrapping

  expect_streq("double" , getcpptypenameforfieldtype(fd::type_double  ));
  expect_streq("float"  , getcpptypenameforfieldtype(fd::type_float   ));
  expect_streq("int64"  , getcpptypenameforfieldtype(fd::type_int64   ));
  expect_streq("uint64" , getcpptypenameforfieldtype(fd::type_uint64  ));
  expect_streq("int32"  , getcpptypenameforfieldtype(fd::type_int32   ));
  expect_streq("uint64" , getcpptypenameforfieldtype(fd::type_fixed64 ));
  expect_streq("uint32" , getcpptypenameforfieldtype(fd::type_fixed32 ));
  expect_streq("bool"   , getcpptypenameforfieldtype(fd::type_bool    ));
  expect_streq("string" , getcpptypenameforfieldtype(fd::type_string  ));
  expect_streq("message", getcpptypenameforfieldtype(fd::type_group   ));
  expect_streq("message", getcpptypenameforfieldtype(fd::type_message ));
  expect_streq("string" , getcpptypenameforfieldtype(fd::type_bytes   ));
  expect_streq("uint32" , getcpptypenameforfieldtype(fd::type_uint32  ));
  expect_streq("enum"   , getcpptypenameforfieldtype(fd::type_enum    ));
  expect_streq("int32"  , getcpptypenameforfieldtype(fd::type_sfixed32));
  expect_streq("int64"  , getcpptypenameforfieldtype(fd::type_sfixed64));
  expect_streq("int32"  , getcpptypenameforfieldtype(fd::type_sint32  ));
  expect_streq("int64"  , getcpptypenameforfieldtype(fd::type_sint64  ));
}

test_f(misctest, defaultvalues) {
  // test that setting default values works.
  filedescriptorproto file_proto;
  file_proto.set_name("foo.proto");

  enumdescriptorproto* enum_type_proto = addenum(&file_proto, "dummyenum");
  addenumvalue(enum_type_proto, "a", 1);
  addenumvalue(enum_type_proto, "b", 2);

  descriptorproto* message_proto = addmessage(&file_proto, "testmessage");

  typedef fielddescriptorproto fd;  // avoid ugly line wrapping
  const fd::label label = fd::label_optional;

  // create fields of every cpp type with default values.
  addfield(message_proto, "int32" , 1, label, fd::type_int32 )
    ->set_default_value("-1");
  addfield(message_proto, "int64" , 2, label, fd::type_int64 )
    ->set_default_value("-1000000000000");
  addfield(message_proto, "uint32", 3, label, fd::type_uint32)
    ->set_default_value("42");
  addfield(message_proto, "uint64", 4, label, fd::type_uint64)
    ->set_default_value("2000000000000");
  addfield(message_proto, "float" , 5, label, fd::type_float )
    ->set_default_value("4.5");
  addfield(message_proto, "double", 6, label, fd::type_double)
    ->set_default_value("10e100");
  addfield(message_proto, "bool"  , 7, label, fd::type_bool  )
    ->set_default_value("true");
  addfield(message_proto, "string", 8, label, fd::type_string)
    ->set_default_value("hello");
  addfield(message_proto, "data"  , 9, label, fd::type_bytes )
    ->set_default_value("\\001\\002\\003");

  fielddescriptorproto* enum_field =
    addfield(message_proto, "enum", 10, label, fd::type_enum);
  enum_field->set_type_name("dummyenum");
  enum_field->set_default_value("b");

  // strings are allowed to have empty defaults.  (at one point, due to
  // a bug, empty defaults for strings were rejected.  oops.)
  addfield(message_proto, "empty_string", 11, label, fd::type_string)
    ->set_default_value("");

  // add a second set of fields with implicit defalut values.
  addfield(message_proto, "implicit_int32" , 21, label, fd::type_int32 );
  addfield(message_proto, "implicit_int64" , 22, label, fd::type_int64 );
  addfield(message_proto, "implicit_uint32", 23, label, fd::type_uint32);
  addfield(message_proto, "implicit_uint64", 24, label, fd::type_uint64);
  addfield(message_proto, "implicit_float" , 25, label, fd::type_float );
  addfield(message_proto, "implicit_double", 26, label, fd::type_double);
  addfield(message_proto, "implicit_bool"  , 27, label, fd::type_bool  );
  addfield(message_proto, "implicit_string", 28, label, fd::type_string);
  addfield(message_proto, "implicit_data"  , 29, label, fd::type_bytes );
  addfield(message_proto, "implicit_enum"  , 30, label, fd::type_enum)
    ->set_type_name("dummyenum");

  // build it.
  descriptorpool pool;
  const filedescriptor* file = pool.buildfile(file_proto);
  assert_true(file != null);

  assert_eq(1, file->enum_type_count());
  const enumdescriptor* enum_type = file->enum_type(0);
  assert_eq(2, enum_type->value_count());
  const enumvaluedescriptor* enum_value_a = enum_type->value(0);
  const enumvaluedescriptor* enum_value_b = enum_type->value(1);

  assert_eq(1, file->message_type_count());
  const descriptor* message = file->message_type(0);

  assert_eq(21, message->field_count());

  // check the default values.
  assert_true(message->field(0)->has_default_value());
  assert_true(message->field(1)->has_default_value());
  assert_true(message->field(2)->has_default_value());
  assert_true(message->field(3)->has_default_value());
  assert_true(message->field(4)->has_default_value());
  assert_true(message->field(5)->has_default_value());
  assert_true(message->field(6)->has_default_value());
  assert_true(message->field(7)->has_default_value());
  assert_true(message->field(8)->has_default_value());
  assert_true(message->field(9)->has_default_value());
  assert_true(message->field(10)->has_default_value());

  expect_eq(-1              , message->field(0)->default_value_int32 ());
  expect_eq(-google_ulonglong(1000000000000),
            message->field(1)->default_value_int64 ());
  expect_eq(42              , message->field(2)->default_value_uint32());
  expect_eq(google_ulonglong(2000000000000),
            message->field(3)->default_value_uint64());
  expect_eq(4.5             , message->field(4)->default_value_float ());
  expect_eq(10e100          , message->field(5)->default_value_double());
  expect_true(                message->field(6)->default_value_bool  ());
  expect_eq("hello"         , message->field(7)->default_value_string());
  expect_eq("\001\002\003"  , message->field(8)->default_value_string());
  expect_eq(enum_value_b    , message->field(9)->default_value_enum  ());
  expect_eq(""              , message->field(10)->default_value_string());

  assert_false(message->field(11)->has_default_value());
  assert_false(message->field(12)->has_default_value());
  assert_false(message->field(13)->has_default_value());
  assert_false(message->field(14)->has_default_value());
  assert_false(message->field(15)->has_default_value());
  assert_false(message->field(16)->has_default_value());
  assert_false(message->field(17)->has_default_value());
  assert_false(message->field(18)->has_default_value());
  assert_false(message->field(19)->has_default_value());
  assert_false(message->field(20)->has_default_value());

  expect_eq(0    , message->field(11)->default_value_int32 ());
  expect_eq(0    , message->field(12)->default_value_int64 ());
  expect_eq(0    , message->field(13)->default_value_uint32());
  expect_eq(0    , message->field(14)->default_value_uint64());
  expect_eq(0.0f , message->field(15)->default_value_float ());
  expect_eq(0.0  , message->field(16)->default_value_double());
  expect_false(    message->field(17)->default_value_bool  ());
  expect_eq(""   , message->field(18)->default_value_string());
  expect_eq(""   , message->field(19)->default_value_string());
  expect_eq(enum_value_a, message->field(20)->default_value_enum());
}

test_f(misctest, fieldoptions) {
  // try setting field options.

  filedescriptorproto file_proto;
  file_proto.set_name("foo.proto");

  descriptorproto* message_proto = addmessage(&file_proto, "testmessage");
  addfield(message_proto, "foo", 1,
           fielddescriptorproto::label_optional,
           fielddescriptorproto::type_int32);
  fielddescriptorproto* bar_proto =
    addfield(message_proto, "bar", 2,
             fielddescriptorproto::label_optional,
             fielddescriptorproto::type_int32);

  fieldoptions* options = bar_proto->mutable_options();
  options->set_ctype(fieldoptions::cord);

  // build the descriptors and get the pointers.
  descriptorpool pool;
  const filedescriptor* file = pool.buildfile(file_proto);
  assert_true(file != null);

  assert_eq(1, file->message_type_count());
  const descriptor* message = file->message_type(0);

  assert_eq(2, message->field_count());
  const fielddescriptor* foo = message->field(0);
  const fielddescriptor* bar = message->field(1);

  // "foo" had no options set, so it should return the default options.
  expect_eq(&fieldoptions::default_instance(), &foo->options());

  // "bar" had options set.
  expect_ne(&fieldoptions::default_instance(), options);
  expect_true(bar->options().has_ctype());
  expect_eq(fieldoptions::cord, bar->options().ctype());
}

// ===================================================================
enum descriptorpoolmode {
  no_database,
  fallback_database
};

class allowunknowndependenciestest
    : public testing::testwithparam<descriptorpoolmode> {
 protected:
  descriptorpoolmode mode() {
    return getparam();
   }

  virtual void setup() {
    filedescriptorproto foo_proto, bar_proto;

    switch (mode()) {
      case no_database:
        pool_.reset(new descriptorpool);
        break;
      case fallback_database:
        pool_.reset(new descriptorpool(&db_));
        break;
    }

    pool_->allowunknowndependencies();

    assert_true(textformat::parsefromstring(
      "name: 'foo.proto'"
      "dependency: 'bar.proto'"
      "dependency: 'baz.proto'"
      "message_type {"
      "  name: 'foo'"
      "  field { name:'bar' number:1 label:label_optional type_name:'bar' }"
      "  field { name:'baz' number:2 label:label_optional type_name:'baz' }"
      "  field { name:'qux' number:3 label:label_optional"
      "    type_name: '.corge.qux'"
      "    type: type_enum"
      "    options {"
      "      uninterpreted_option {"
      "        name {"
      "          name_part: 'grault'"
      "          is_extension: true"
      "        }"
      "        positive_int_value: 1234"
      "      }"
      "    }"
      "  }"
      "}",
      &foo_proto));
    assert_true(textformat::parsefromstring(
      "name: 'bar.proto'"
      "message_type { name: 'bar' }",
      &bar_proto));

    // collect pointers to stuff.
    bar_file_ = buildfile(bar_proto);
    assert_true(bar_file_ != null);

    assert_eq(1, bar_file_->message_type_count());
    bar_type_ = bar_file_->message_type(0);

    foo_file_ = buildfile(foo_proto);
    assert_true(foo_file_ != null);

    assert_eq(1, foo_file_->message_type_count());
    foo_type_ = foo_file_->message_type(0);

    assert_eq(3, foo_type_->field_count());
    bar_field_ = foo_type_->field(0);
    baz_field_ = foo_type_->field(1);
    qux_field_ = foo_type_->field(2);
  }

  const filedescriptor* buildfile(const filedescriptorproto& proto) {
    switch (mode()) {
      case no_database:
        return pool_->buildfile(proto);
        break;
      case fallback_database: {
        expect_true(db_.add(proto));
        return pool_->findfilebyname(proto.name());
      }
    }
    google_log(fatal) << "can't get here.";
    return null;
  }

  const filedescriptor* bar_file_;
  const descriptor* bar_type_;
  const filedescriptor* foo_file_;
  const descriptor* foo_type_;
  const fielddescriptor* bar_field_;
  const fielddescriptor* baz_field_;
  const fielddescriptor* qux_field_;

  simpledescriptordatabase db_;        // used if in fallback_database mode.
  scoped_ptr<descriptorpool> pool_;
};

test_p(allowunknowndependenciestest, placeholderfile) {
  assert_eq(2, foo_file_->dependency_count());
  expect_eq(bar_file_, foo_file_->dependency(0));

  const filedescriptor* baz_file = foo_file_->dependency(1);
  expect_eq("baz.proto", baz_file->name());
  expect_eq(0, baz_file->message_type_count());

  // placeholder files should not be findable.
  expect_eq(bar_file_, pool_->findfilebyname(bar_file_->name()));
  expect_true(pool_->findfilebyname(baz_file->name()) == null);
}

test_p(allowunknowndependenciestest, placeholdertypes) {
  assert_eq(fielddescriptor::type_message, bar_field_->type());
  expect_eq(bar_type_, bar_field_->message_type());

  assert_eq(fielddescriptor::type_message, baz_field_->type());
  const descriptor* baz_type = baz_field_->message_type();
  expect_eq("baz", baz_type->name());
  expect_eq("baz", baz_type->full_name());
  expect_eq("baz.placeholder.proto", baz_type->file()->name());
  expect_eq(0, baz_type->extension_range_count());

  assert_eq(fielddescriptor::type_enum, qux_field_->type());
  const enumdescriptor* qux_type = qux_field_->enum_type();
  expect_eq("qux", qux_type->name());
  expect_eq("corge.qux", qux_type->full_name());
  expect_eq("corge.qux.placeholder.proto", qux_type->file()->name());

  // placeholder types should not be findable.
  expect_eq(bar_type_, pool_->findmessagetypebyname(bar_type_->full_name()));
  expect_true(pool_->findmessagetypebyname(baz_type->full_name()) == null);
  expect_true(pool_->findenumtypebyname(qux_type->full_name()) == null);
}

test_p(allowunknowndependenciestest, copyto) {
  // fielddescriptor::copyto() should write non-fully-qualified type names
  // for placeholder types which were not originally fully-qualified.
  fielddescriptorproto proto;

  // bar is not a placeholder, so it is fully-qualified.
  bar_field_->copyto(&proto);
  expect_eq(".bar", proto.type_name());
  expect_eq(fielddescriptorproto::type_message, proto.type());

  // baz is an unqualified placeholder.
  proto.clear();
  baz_field_->copyto(&proto);
  expect_eq("baz", proto.type_name());
  expect_false(proto.has_type());

  // qux is a fully-qualified placeholder.
  proto.clear();
  qux_field_->copyto(&proto);
  expect_eq(".corge.qux", proto.type_name());
  expect_eq(fielddescriptorproto::type_enum, proto.type());
}

test_p(allowunknowndependenciestest, customoptions) {
  // qux should still have the uninterpreted option attached.
  assert_eq(1, qux_field_->options().uninterpreted_option_size());
  const uninterpretedoption& option =
    qux_field_->options().uninterpreted_option(0);
  assert_eq(1, option.name_size());
  expect_eq("grault", option.name(0).name_part());
}

test_p(allowunknowndependenciestest, unknownextendee) {
  // test that we can extend an unknown type.  this is slightly tricky because
  // it means that the placeholder type must have an extension range.

  filedescriptorproto extension_proto;

  assert_true(textformat::parsefromstring(
    "name: 'extension.proto'"
    "extension { extendee: 'unknowntype' name:'some_extension' number:123"
    "            label:label_optional type:type_int32 }",
    &extension_proto));
  const filedescriptor* file = buildfile(extension_proto);

  assert_true(file != null);

  assert_eq(1, file->extension_count());
  const descriptor* extendee = file->extension(0)->containing_type();
  expect_eq("unknowntype", extendee->name());
  assert_eq(1, extendee->extension_range_count());
  expect_eq(1, extendee->extension_range(0)->start);
  expect_eq(fielddescriptor::kmaxnumber + 1, extendee->extension_range(0)->end);
}

test_p(allowunknowndependenciestest, customoption) {
  // test that we can use a custom option without having parsed
  // descriptor.proto.

  filedescriptorproto option_proto;

  assert_true(textformat::parsefromstring(
    "name: \"unknown_custom_options.proto\" "
    "dependency: \"google/protobuf/descriptor.proto\" "
    "extension { "
    "  extendee: \"google.protobuf.fileoptions\" "
    "  name: \"some_option\" "
    "  number: 123456 "
    "  label: label_optional "
    "  type: type_int32 "
    "} "
    "options { "
    "  uninterpreted_option { "
    "    name { "
    "      name_part: \"some_option\" "
    "      is_extension: true "
    "    } "
    "    positive_int_value: 1234 "
    "  } "
    "  uninterpreted_option { "
    "    name { "
    "      name_part: \"unknown_option\" "
    "      is_extension: true "
    "    } "
    "    positive_int_value: 1234 "
    "  } "
    "  uninterpreted_option { "
    "    name { "
    "      name_part: \"optimize_for\" "
    "      is_extension: false "
    "    } "
    "    identifier_value: \"speed\" "
    "  } "
    "}",
    &option_proto));

  const filedescriptor* file = buildfile(option_proto);
  assert_true(file != null);

  // verify that no extension options were set, but they were left as
  // uninterpreted_options.
  vector<const fielddescriptor*> fields;
  file->options().getreflection()->listfields(file->options(), &fields);
  assert_eq(2, fields.size());
  expect_true(file->options().has_optimize_for());
  expect_eq(2, file->options().uninterpreted_option_size());
}

test_p(allowunknowndependenciestest,
       undeclareddependencytriggersbuildofdependency) {
  // crazy case: suppose foo.proto refers to a symbol without declaring the
  // dependency that finds it. in the event that the pool is backed by a
  // descriptordatabase, the pool will attempt to find the symbol in the
  // database. if successful, it will build the undeclared dependency to verify
  // that the file does indeed contain the symbol. if that file fails to build,
  // then its descriptors must be rolled back. however, we still want foo.proto
  // to build successfully, since we are allowing unknown dependencies.

  filedescriptorproto undeclared_dep_proto;
  // we make this file fail to build by giving it two fields with tag 1.
  assert_true(textformat::parsefromstring(
    "name: \"invalid_file_as_undeclared_dep.proto\" "
    "package: \"undeclared\" "
    "message_type: {  "
    "  name: \"quux\"  "
    "  field { "
    "    name:'qux' number:1 label:label_optional type: type_int32 "
    "  }"
    "  field { "
    "    name:'quux' number:1 label:label_optional type: type_int64 "
    "  }"
    "}",
    &undeclared_dep_proto));
  // we can't use the buildfile() helper because we don't actually want to build
  // it into the descriptor pool in the fallback database case: it just needs to
  // be sitting in the database so that it gets built during the building of
  // test.proto below.
  switch (mode()) {
    case no_database: {
      assert_true(pool_->buildfile(undeclared_dep_proto) == null);
      break;
    }
    case fallback_database: {
      assert_true(db_.add(undeclared_dep_proto));
    }
  }

  filedescriptorproto test_proto;
  assert_true(textformat::parsefromstring(
    "name: \"test.proto\" "
    "message_type: { "
    "  name: \"corge\" "
    "  field { "
    "    name:'quux' number:1 label: label_optional "
    "    type_name:'undeclared.quux' type: type_message "
    "  }"
    "}",
    &test_proto));

  const filedescriptor* file = buildfile(test_proto);
  assert_true(file != null);
  google_log(info) << file->debugstring();

  expect_eq(0, file->dependency_count());
  assert_eq(1, file->message_type_count());
  const descriptor* corge_desc = file->message_type(0);
  assert_eq("corge", corge_desc->name());
  assert_eq(1, corge_desc->field_count());

  const fielddescriptor* quux_field = corge_desc->field(0);
  assert_eq(fielddescriptor::type_message, quux_field->type());
  assert_eq("quux", quux_field->message_type()->name());
  assert_eq("undeclared.quux", quux_field->message_type()->full_name());
  expect_eq("undeclared.quux.placeholder.proto",
            quux_field->message_type()->file()->name());
  // the place holder type should not be findable.
  assert_true(pool_->findmessagetypebyname("undeclared.quux") == null);
}

instantiate_test_case_p(databasesource,
                        allowunknowndependenciestest,
                        testing::values(no_database, fallback_database));

// ===================================================================

test(customoptions, optionlocations) {
  const descriptor* message =
      protobuf_unittest::testmessagewithcustomoptions::descriptor();
  const filedescriptor* file = message->file();
  const fielddescriptor* field = message->findfieldbyname("field1");
  const enumdescriptor* enm = message->findenumtypebyname("anenum");
  // todo(benjy): support enumvalue options, once the compiler does.
  const servicedescriptor* service =
      file->findservicebyname("testservicewithcustomoptions");
  const methoddescriptor* method = service->findmethodbyname("foo");

  expect_eq(google_longlong(9876543210),
            file->options().getextension(protobuf_unittest::file_opt1));
  expect_eq(-56,
            message->options().getextension(protobuf_unittest::message_opt1));
  expect_eq(google_longlong(8765432109),
            field->options().getextension(protobuf_unittest::field_opt1));
  expect_eq(42,  // check that we get the default for an option we don't set.
            field->options().getextension(protobuf_unittest::field_opt2));
  expect_eq(-789,
            enm->options().getextension(protobuf_unittest::enum_opt1));
  expect_eq(123,
            enm->value(1)->options().getextension(
              protobuf_unittest::enum_value_opt1));
  expect_eq(google_longlong(-9876543210),
            service->options().getextension(protobuf_unittest::service_opt1));
  expect_eq(protobuf_unittest::methodopt1_val2,
            method->options().getextension(protobuf_unittest::method_opt1));

  // see that the regular options went through unscathed.
  expect_true(message->options().has_message_set_wire_format());
  expect_eq(fieldoptions::cord, field->options().ctype());
}

test(customoptions, optiontypes) {
  const messageoptions* options = null;

  options =
      &protobuf_unittest::customoptionminintegervalues::descriptor()->options();
  expect_false(        options->getextension(protobuf_unittest::bool_opt));
  expect_eq(kint32min, options->getextension(protobuf_unittest::int32_opt));
  expect_eq(kint64min, options->getextension(protobuf_unittest::int64_opt));
  expect_eq(0        , options->getextension(protobuf_unittest::uint32_opt));
  expect_eq(0        , options->getextension(protobuf_unittest::uint64_opt));
  expect_eq(kint32min, options->getextension(protobuf_unittest::sint32_opt));
  expect_eq(kint64min, options->getextension(protobuf_unittest::sint64_opt));
  expect_eq(0        , options->getextension(protobuf_unittest::fixed32_opt));
  expect_eq(0        , options->getextension(protobuf_unittest::fixed64_opt));
  expect_eq(kint32min, options->getextension(protobuf_unittest::sfixed32_opt));
  expect_eq(kint64min, options->getextension(protobuf_unittest::sfixed64_opt));

  options =
      &protobuf_unittest::customoptionmaxintegervalues::descriptor()->options();
  expect_true(          options->getextension(protobuf_unittest::bool_opt));
  expect_eq(kint32max , options->getextension(protobuf_unittest::int32_opt));
  expect_eq(kint64max , options->getextension(protobuf_unittest::int64_opt));
  expect_eq(kuint32max, options->getextension(protobuf_unittest::uint32_opt));
  expect_eq(kuint64max, options->getextension(protobuf_unittest::uint64_opt));
  expect_eq(kint32max , options->getextension(protobuf_unittest::sint32_opt));
  expect_eq(kint64max , options->getextension(protobuf_unittest::sint64_opt));
  expect_eq(kuint32max, options->getextension(protobuf_unittest::fixed32_opt));
  expect_eq(kuint64max, options->getextension(protobuf_unittest::fixed64_opt));
  expect_eq(kint32max , options->getextension(protobuf_unittest::sfixed32_opt));
  expect_eq(kint64max , options->getextension(protobuf_unittest::sfixed64_opt));

  options =
      &protobuf_unittest::customoptionothervalues::descriptor()->options();
  expect_eq(-100, options->getextension(protobuf_unittest::int32_opt));
  expect_float_eq(12.3456789,
                  options->getextension(protobuf_unittest::float_opt));
  expect_double_eq(1.234567890123456789,
                   options->getextension(protobuf_unittest::double_opt));
  expect_eq("hello, \"world\"",
            options->getextension(protobuf_unittest::string_opt));

  expect_eq(string("hello\0world", 11),
            options->getextension(protobuf_unittest::bytes_opt));

  expect_eq(protobuf_unittest::dummymessagecontainingenum::test_option_enum_type2,
            options->getextension(protobuf_unittest::enum_opt));

  options =
      &protobuf_unittest::settingrealsfrompositiveints::descriptor()->options();
  expect_float_eq(12, options->getextension(protobuf_unittest::float_opt));
  expect_double_eq(154, options->getextension(protobuf_unittest::double_opt));

  options =
      &protobuf_unittest::settingrealsfromnegativeints::descriptor()->options();
  expect_float_eq(-12, options->getextension(protobuf_unittest::float_opt));
  expect_double_eq(-154, options->getextension(protobuf_unittest::double_opt));
}

test(customoptions, complexextensionoptions) {
  const messageoptions* options =
      &protobuf_unittest::variouscomplexoptions::descriptor()->options();
  expect_eq(options->getextension(protobuf_unittest::complex_opt1).foo(), 42);
  expect_eq(options->getextension(protobuf_unittest::complex_opt1).
            getextension(protobuf_unittest::quux), 324);
  expect_eq(options->getextension(protobuf_unittest::complex_opt1).
            getextension(protobuf_unittest::corge).qux(), 876);
  expect_eq(options->getextension(protobuf_unittest::complex_opt2).baz(), 987);
  expect_eq(options->getextension(protobuf_unittest::complex_opt2).
            getextension(protobuf_unittest::grault), 654);
  expect_eq(options->getextension(protobuf_unittest::complex_opt2).bar().foo(),
            743);
  expect_eq(options->getextension(protobuf_unittest::complex_opt2).bar().
            getextension(protobuf_unittest::quux), 1999);
  expect_eq(options->getextension(protobuf_unittest::complex_opt2).bar().
            getextension(protobuf_unittest::corge).qux(), 2008);
  expect_eq(options->getextension(protobuf_unittest::complex_opt2).
            getextension(protobuf_unittest::garply).foo(), 741);
  expect_eq(options->getextension(protobuf_unittest::complex_opt2).
            getextension(protobuf_unittest::garply).
            getextension(protobuf_unittest::quux), 1998);
  expect_eq(options->getextension(protobuf_unittest::complex_opt2).
            getextension(protobuf_unittest::garply).
            getextension(protobuf_unittest::corge).qux(), 2121);
  expect_eq(options->getextension(
      protobuf_unittest::complexoptiontype2::complexoptiontype4::complex_opt4).
            waldo(), 1971);
  expect_eq(options->getextension(protobuf_unittest::complex_opt2).
            fred().waldo(), 321);
  expect_eq(9, options->getextension(protobuf_unittest::complex_opt3).qux());
  expect_eq(22, options->getextension(protobuf_unittest::complex_opt3).
                complexoptiontype5().plugh());
  expect_eq(24, options->getextension(protobuf_unittest::complexopt6).xyzzy());
}

test(customoptions, optionsfromotherfile) {
  // test that to use a custom option, we only need to import the file
  // defining the option; we do not also have to import descriptor.proto.
  descriptorpool pool;

  filedescriptorproto file_proto;
  filedescriptorproto::descriptor()->file()->copyto(&file_proto);
  assert_true(pool.buildfile(file_proto) != null);

  protobuf_unittest::testmessagewithcustomoptions::descriptor()
    ->file()->copyto(&file_proto);
  assert_true(pool.buildfile(file_proto) != null);

  assert_true(textformat::parsefromstring(
    "name: \"custom_options_import.proto\" "
    "package: \"protobuf_unittest\" "
    "dependency: \"google/protobuf/unittest_custom_options.proto\" "
    "options { "
    "  uninterpreted_option { "
    "    name { "
    "      name_part: \"file_opt1\" "
    "      is_extension: true "
    "    } "
    "    positive_int_value: 1234 "
    "  } "
    // test a non-extension option too.  (at one point this failed due to a
    // bug.)
    "  uninterpreted_option { "
    "    name { "
    "      name_part: \"java_package\" "
    "      is_extension: false "
    "    } "
    "    string_value: \"foo\" "
    "  } "
    // test that enum-typed options still work too.  (at one point this also
    // failed due to a bug.)
    "  uninterpreted_option { "
    "    name { "
    "      name_part: \"optimize_for\" "
    "      is_extension: false "
    "    } "
    "    identifier_value: \"speed\" "
    "  } "
    "}"
    ,
    &file_proto));

  const filedescriptor* file = pool.buildfile(file_proto);
  assert_true(file != null);
  expect_eq(1234, file->options().getextension(protobuf_unittest::file_opt1));
  expect_true(file->options().has_java_package());
  expect_eq("foo", file->options().java_package());
  expect_true(file->options().has_optimize_for());
  expect_eq(fileoptions::speed, file->options().optimize_for());
}

test(customoptions, messageoptionthreefieldsset) {
  // this tests a bug which previously existed in custom options parsing.  the
  // bug occurred when you defined a custom option with message type and then
  // set three fields of that option on a single definition (see the example
  // below).  the bug is a bit hard to explain, so check the change history if
  // you want to know more.
  descriptorpool pool;

  filedescriptorproto file_proto;
  filedescriptorproto::descriptor()->file()->copyto(&file_proto);
  assert_true(pool.buildfile(file_proto) != null);

  protobuf_unittest::testmessagewithcustomoptions::descriptor()
    ->file()->copyto(&file_proto);
  assert_true(pool.buildfile(file_proto) != null);

  // the following represents the definition:
  //
  //   import "google/protobuf/unittest_custom_options.proto"
  //   package protobuf_unittest;
  //   message foo {
  //     option (complex_opt1).foo  = 1234;
  //     option (complex_opt1).foo2 = 1234;
  //     option (complex_opt1).foo3 = 1234;
  //   }
  assert_true(textformat::parsefromstring(
    "name: \"custom_options_import.proto\" "
    "package: \"protobuf_unittest\" "
    "dependency: \"google/protobuf/unittest_custom_options.proto\" "
    "message_type { "
    "  name: \"foo\" "
    "  options { "
    "    uninterpreted_option { "
    "      name { "
    "        name_part: \"complex_opt1\" "
    "        is_extension: true "
    "      } "
    "      name { "
    "        name_part: \"foo\" "
    "        is_extension: false "
    "      } "
    "      positive_int_value: 1234 "
    "    } "
    "    uninterpreted_option { "
    "      name { "
    "        name_part: \"complex_opt1\" "
    "        is_extension: true "
    "      } "
    "      name { "
    "        name_part: \"foo2\" "
    "        is_extension: false "
    "      } "
    "      positive_int_value: 1234 "
    "    } "
    "    uninterpreted_option { "
    "      name { "
    "        name_part: \"complex_opt1\" "
    "        is_extension: true "
    "      } "
    "      name { "
    "        name_part: \"foo3\" "
    "        is_extension: false "
    "      } "
    "      positive_int_value: 1234 "
    "    } "
    "  } "
    "}",
    &file_proto));

  const filedescriptor* file = pool.buildfile(file_proto);
  assert_true(file != null);
  assert_eq(1, file->message_type_count());

  const messageoptions& options = file->message_type(0)->options();
  expect_eq(1234, options.getextension(protobuf_unittest::complex_opt1).foo());
}

// check that aggregate options were parsed and saved correctly in
// the appropriate descriptors.
test(customoptions, aggregateoptions) {
  const descriptor* msg = protobuf_unittest::aggregatemessage::descriptor();
  const filedescriptor* file = msg->file();
  const fielddescriptor* field = msg->findfieldbyname("fieldname");
  const enumdescriptor* enumd = file->findenumtypebyname("aggregateenum");
  const enumvaluedescriptor* enumv = enumd->findvaluebyname("value");
  const servicedescriptor* service = file->findservicebyname(
      "aggregateservice");
  const methoddescriptor* method = service->findmethodbyname("method");

  // tests for the different types of data embedded in fileopt
  const protobuf_unittest::aggregate& file_options =
      file->options().getextension(protobuf_unittest::fileopt);
  expect_eq(100, file_options.i());
  expect_eq("fileannotation", file_options.s());
  expect_eq("nestedfileannotation", file_options.sub().s());
  expect_eq("fileextensionannotation",
            file_options.file().getextension(protobuf_unittest::fileopt).s());
  expect_eq("embeddedmessagesetelement",
            file_options.mset().getextension(
                protobuf_unittest::aggregatemessagesetelement
                ::message_set_extension).s());

  // simple tests for all the other types of annotations
  expect_eq("messageannotation",
            msg->options().getextension(protobuf_unittest::msgopt).s());
  expect_eq("fieldannotation",
            field->options().getextension(protobuf_unittest::fieldopt).s());
  expect_eq("enumannotation",
            enumd->options().getextension(protobuf_unittest::enumopt).s());
  expect_eq("enumvalueannotation",
            enumv->options().getextension(protobuf_unittest::enumvalopt).s());
  expect_eq("serviceannotation",
            service->options().getextension(protobuf_unittest::serviceopt).s());
  expect_eq("methodannotation",
            method->options().getextension(protobuf_unittest::methodopt).s());
}

// ===================================================================

// the tests below trigger every unique call to adderror() in descriptor.cc,
// in the order in which they appear in that file.  i'm using textformat here
// to specify the input descriptors because building them using code would
// be too bulky.

class mockerrorcollector : public descriptorpool::errorcollector {
 public:
  mockerrorcollector() {}
  ~mockerrorcollector() {}

  string text_;

  // implements errorcollector ---------------------------------------
  void adderror(const string& filename,
                const string& element_name, const message* descriptor,
                errorlocation location, const string& message) {
    const char* location_name = null;
    switch (location) {
      case name         : location_name = "name"         ; break;
      case number       : location_name = "number"       ; break;
      case type         : location_name = "type"         ; break;
      case extendee     : location_name = "extendee"     ; break;
      case default_value: location_name = "default_value"; break;
      case option_name  : location_name = "option_name"  ; break;
      case option_value : location_name = "option_value" ; break;
      case input_type   : location_name = "input_type"   ; break;
      case output_type  : location_name = "output_type"  ; break;
      case other        : location_name = "other"        ; break;
    }

    strings::substituteandappend(
      &text_, "$0: $1: $2: $3\n",
      filename, element_name, location_name, message);
  }
};

class validationerrortest : public testing::test {
 protected:
  // parse file_text as a filedescriptorproto in text format and add it
  // to the descriptorpool.  expect no errors.
  void buildfile(const string& file_text) {
    filedescriptorproto file_proto;
    assert_true(textformat::parsefromstring(file_text, &file_proto));
    assert_true(pool_.buildfile(file_proto) != null);
  }

  // parse file_text as a filedescriptorproto in text format and add it
  // to the descriptorpool.  expect errors to be produced which match the
  // given error text.
  void buildfilewitherrors(const string& file_text,
                           const string& expected_errors) {
    filedescriptorproto file_proto;
    assert_true(textformat::parsefromstring(file_text, &file_proto));

    mockerrorcollector error_collector;
    expect_true(
      pool_.buildfilecollectingerrors(file_proto, &error_collector) == null);
    expect_eq(expected_errors, error_collector.text_);
  }

  // builds some already-parsed file in our test pool.
  void buildfileintestpool(const filedescriptor* file) {
    filedescriptorproto file_proto;
    file->copyto(&file_proto);
    assert_true(pool_.buildfile(file_proto) != null);
  }

  // build descriptor.proto in our test pool. this allows us to extend it in
  // the test pool, so we can test custom options.
  void builddescriptormessagesintestpool() {
    buildfileintestpool(descriptorproto::descriptor()->file());
  }

  descriptorpool pool_;
};

test_f(validationerrortest, alreadydefined) {
  buildfilewitherrors(
    "name: \"foo.proto\" "
    "message_type { name: \"foo\" }"
    "message_type { name: \"foo\" }",

    "foo.proto: foo: name: \"foo\" is already defined.\n");
}

test_f(validationerrortest, alreadydefinedinpackage) {
  buildfilewitherrors(
    "name: \"foo.proto\" "
    "package: \"foo.bar\" "
    "message_type { name: \"foo\" }"
    "message_type { name: \"foo\" }",

    "foo.proto: foo.bar.foo: name: \"foo\" is already defined in "
      "\"foo.bar\".\n");
}

test_f(validationerrortest, alreadydefinedinotherfile) {
  buildfile(
    "name: \"foo.proto\" "
    "message_type { name: \"foo\" }");

  buildfilewitherrors(
    "name: \"bar.proto\" "
    "message_type { name: \"foo\" }",

    "bar.proto: foo: name: \"foo\" is already defined in file "
      "\"foo.proto\".\n");
}

test_f(validationerrortest, packagealreadydefined) {
  buildfile(
    "name: \"foo.proto\" "
    "message_type { name: \"foo\" }");
  buildfilewitherrors(
    "name: \"bar.proto\" "
    "package: \"foo.bar\"",

    "bar.proto: foo: name: \"foo\" is already defined (as something other "
      "than a package) in file \"foo.proto\".\n");
}

test_f(validationerrortest, enumvaluealreadydefinedinparent) {
  buildfilewitherrors(
    "name: \"foo.proto\" "
    "enum_type { name: \"foo\" value { name: \"foo\" number: 1 } } "
    "enum_type { name: \"bar\" value { name: \"foo\" number: 1 } } ",

    "foo.proto: foo: name: \"foo\" is already defined.\n"
    "foo.proto: foo: name: note that enum values use c++ scoping rules, "
      "meaning that enum values are siblings of their type, not children of "
      "it.  therefore, \"foo\" must be unique within the global scope, not "
      "just within \"bar\".\n");
}

test_f(validationerrortest, enumvaluealreadydefinedinparentnonglobal) {
  buildfilewitherrors(
    "name: \"foo.proto\" "
    "package: \"pkg\" "
    "enum_type { name: \"foo\" value { name: \"foo\" number: 1 } } "
    "enum_type { name: \"bar\" value { name: \"foo\" number: 1 } } ",

    "foo.proto: pkg.foo: name: \"foo\" is already defined in \"pkg\".\n"
    "foo.proto: pkg.foo: name: note that enum values use c++ scoping rules, "
      "meaning that enum values are siblings of their type, not children of "
      "it.  therefore, \"foo\" must be unique within \"pkg\", not just within "
      "\"bar\".\n");
}

test_f(validationerrortest, missingname) {
  buildfilewitherrors(
    "name: \"foo.proto\" "
    "message_type { }",

    "foo.proto: : name: missing name.\n");
}

test_f(validationerrortest, invalidname) {
  buildfilewitherrors(
    "name: \"foo.proto\" "
    "message_type { name: \"$\" }",

    "foo.proto: $: name: \"$\" is not a valid identifier.\n");
}

test_f(validationerrortest, invalidpackagename) {
  buildfilewitherrors(
    "name: \"foo.proto\" "
    "package: \"foo.$\"",

    "foo.proto: foo.$: name: \"$\" is not a valid identifier.\n");
}

test_f(validationerrortest, missingfilename) {
  buildfilewitherrors(
    "",

    ": : other: missing field: filedescriptorproto.name.\n");
}

test_f(validationerrortest, dupedependency) {
  buildfile("name: \"foo.proto\"");
  buildfilewitherrors(
    "name: \"bar.proto\" "
    "dependency: \"foo.proto\" "
    "dependency: \"foo.proto\" ",

    "bar.proto: bar.proto: other: import \"foo.proto\" was listed twice.\n");
}

test_f(validationerrortest, unknowndependency) {
  buildfilewitherrors(
    "name: \"bar.proto\" "
    "dependency: \"foo.proto\" ",

    "bar.proto: bar.proto: other: import \"foo.proto\" has not been loaded.\n");
}

test_f(validationerrortest, invalidpublicdependencyindex) {
  buildfile("name: \"foo.proto\"");
  buildfilewitherrors(
    "name: \"bar.proto\" "
    "dependency: \"foo.proto\" "
    "public_dependency: 1",
    "bar.proto: bar.proto: other: invalid public dependency index.\n");
}

test_f(validationerrortest, foreignunimportedpackagenocrash) {
  // used to crash:  if we depend on a non-existent file and then refer to a
  // package defined in a file that we didn't import, and that package is
  // nested within a parent package which this file is also in, and we don't
  // include that parent package in the name (i.e. we do a relative lookup)...
  // yes, really.
  buildfile(
    "name: 'foo.proto' "
    "package: 'outer.foo' ");
  buildfilewitherrors(
    "name: 'bar.proto' "
    "dependency: 'baz.proto' "
    "package: 'outer.bar' "
    "message_type { "
    "  name: 'bar' "
    "  field { name:'bar' number:1 label:label_optional type_name:'foo.foo' }"
    "}",

    "bar.proto: bar.proto: other: import \"baz.proto\" has not been loaded.\n"
    "bar.proto: outer.bar.bar.bar: type: \"outer.foo\" seems to be defined in "
      "\"foo.proto\", which is not imported by \"bar.proto\".  to use it here, "
      "please add the necessary import.\n");
}

test_f(validationerrortest, dupefile) {
  buildfile(
    "name: \"foo.proto\" "
    "message_type { name: \"foo\" }");
  // note:  we should *not* get redundant errors about "foo" already being
  //   defined.
  buildfilewitherrors(
    "name: \"foo.proto\" "
    "message_type { name: \"foo\" } "
    // add another type so that the files aren't identical (in which case there
    // would be no error).
    "enum_type { name: \"bar\" }",

    "foo.proto: foo.proto: other: a file with this name is already in the "
      "pool.\n");
}

test_f(validationerrortest, fieldinextensionrange) {
  buildfilewitherrors(
    "name: \"foo.proto\" "
    "message_type {"
    "  name: \"foo\""
    "  field { name: \"foo\" number:  9 label:label_optional type:type_int32 }"
    "  field { name: \"bar\" number: 10 label:label_optional type:type_int32 }"
    "  field { name: \"baz\" number: 19 label:label_optional type:type_int32 }"
    "  field { name: \"qux\" number: 20 label:label_optional type:type_int32 }"
    "  extension_range { start: 10 end: 20 }"
    "}",

    "foo.proto: foo.bar: number: extension range 10 to 19 includes field "
      "\"bar\" (10).\n"
    "foo.proto: foo.baz: number: extension range 10 to 19 includes field "
      "\"baz\" (19).\n");
}

test_f(validationerrortest, overlappingextensionranges) {
  buildfilewitherrors(
    "name: \"foo.proto\" "
    "message_type {"
    "  name: \"foo\""
    "  extension_range { start: 10 end: 20 }"
    "  extension_range { start: 20 end: 30 }"
    "  extension_range { start: 19 end: 21 }"
    "}",

    "foo.proto: foo: number: extension range 19 to 20 overlaps with "
      "already-defined range 10 to 19.\n"
    "foo.proto: foo: number: extension range 19 to 20 overlaps with "
      "already-defined range 20 to 29.\n");
}

test_f(validationerrortest, invaliddefaults) {
  buildfilewitherrors(
    "name: \"foo.proto\" "
    "message_type {"
    "  name: \"foo\""

    // invalid number.
    "  field { name: \"foo\" number: 1 label: label_optional type: type_int32"
    "          default_value: \"abc\" }"

    // empty default value.
    "  field { name: \"bar\" number: 2 label: label_optional type: type_int32"
    "          default_value: \"\" }"

    // invalid boolean.
    "  field { name: \"baz\" number: 3 label: label_optional type: type_bool"
    "          default_value: \"abc\" }"

    // messages can't have defaults.
    "  field { name: \"qux\" number: 4 label: label_optional type: type_message"
    "          default_value: \"abc\" type_name: \"foo\" }"

    // same thing, but we don't know that this field has message type until
    // we look up the type name.
    "  field { name: \"quux\" number: 5 label: label_optional"
    "          default_value: \"abc\" type_name: \"foo\" }"

    // repeateds can't have defaults.
    "  field { name: \"corge\" number: 6 label: label_repeated type: type_int32"
    "          default_value: \"1\" }"
    "}",

    "foo.proto: foo.foo: default_value: couldn't parse default value.\n"
    "foo.proto: foo.bar: default_value: couldn't parse default value.\n"
    "foo.proto: foo.baz: default_value: boolean default must be true or "
      "false.\n"
    "foo.proto: foo.qux: default_value: messages can't have default values.\n"
    "foo.proto: foo.corge: default_value: repeated fields can't have default "
      "values.\n"
    // this ends up being reported later because the error is detected at
    // cross-linking time.
    "foo.proto: foo.quux: default_value: messages can't have default "
      "values.\n");
}

test_f(validationerrortest, negativefieldnumber) {
  buildfilewitherrors(
    "name: \"foo.proto\" "
    "message_type {"
    "  name: \"foo\""
    "  field { name: \"foo\" number: -1 label:label_optional type:type_int32 }"
    "}",

    "foo.proto: foo.foo: number: field numbers must be positive integers.\n");
}

test_f(validationerrortest, hugefieldnumber) {
  buildfilewitherrors(
    "name: \"foo.proto\" "
    "message_type {"
    "  name: \"foo\""
    "  field { name: \"foo\" number: 0x70000000 "
    "          label:label_optional type:type_int32 }"
    "}",

    "foo.proto: foo.foo: number: field numbers cannot be greater than "
      "536870911.\n");
}

test_f(validationerrortest, reservedfieldnumber) {
  buildfilewitherrors(
    "name: \"foo.proto\" "
    "message_type {"
    "  name: \"foo\""
    "  field {name:\"foo\" number: 18999 label:label_optional type:type_int32 }"
    "  field {name:\"bar\" number: 19000 label:label_optional type:type_int32 }"
    "  field {name:\"baz\" number: 19999 label:label_optional type:type_int32 }"
    "  field {name:\"qux\" number: 20000 label:label_optional type:type_int32 }"
    "}",

    "foo.proto: foo.bar: number: field numbers 19000 through 19999 are "
      "reserved for the protocol buffer library implementation.\n"
    "foo.proto: foo.baz: number: field numbers 19000 through 19999 are "
      "reserved for the protocol buffer library implementation.\n");
}

test_f(validationerrortest, extensionmissingextendee) {
  buildfilewitherrors(
    "name: \"foo.proto\" "
    "message_type {"
    "  name: \"foo\""
    "  extension { name: \"foo\" number: 1 label: label_optional"
    "              type_name: \"foo\" }"
    "}",

    "foo.proto: foo.foo: extendee: fielddescriptorproto.extendee not set for "
      "extension field.\n");
}

test_f(validationerrortest, nonextensionwithextendee) {
  buildfilewitherrors(
    "name: \"foo.proto\" "
    "message_type {"
    "  name: \"bar\""
    "  extension_range { start: 1 end: 2 }"
    "}"
    "message_type {"
    "  name: \"foo\""
    "  field { name: \"foo\" number: 1 label: label_optional"
    "          type_name: \"foo\" extendee: \"bar\" }"
    "}",

    "foo.proto: foo.foo: extendee: fielddescriptorproto.extendee set for "
      "non-extension field.\n");
}

test_f(validationerrortest, fieldnumberconflict) {
  buildfilewitherrors(
    "name: \"foo.proto\" "
    "message_type {"
    "  name: \"foo\""
    "  field { name: \"foo\" number: 1 label:label_optional type:type_int32 }"
    "  field { name: \"bar\" number: 1 label:label_optional type:type_int32 }"
    "}",

    "foo.proto: foo.bar: number: field number 1 has already been used in "
      "\"foo\" by field \"foo\".\n");
}

test_f(validationerrortest, badmessagesetextensiontype) {
  buildfilewitherrors(
    "name: \"foo.proto\" "
    "message_type {"
    "  name: \"messageset\""
    "  options { message_set_wire_format: true }"
    "  extension_range { start: 4 end: 5 }"
    "}"
    "message_type {"
    "  name: \"foo\""
    "  extension { name:\"foo\" number:4 label:label_optional type:type_int32"
    "              extendee: \"messageset\" }"
    "}",

    "foo.proto: foo.foo: type: extensions of messagesets must be optional "
      "messages.\n");
}

test_f(validationerrortest, badmessagesetextensionlabel) {
  buildfilewitherrors(
    "name: \"foo.proto\" "
    "message_type {"
    "  name: \"messageset\""
    "  options { message_set_wire_format: true }"
    "  extension_range { start: 4 end: 5 }"
    "}"
    "message_type {"
    "  name: \"foo\""
    "  extension { name:\"foo\" number:4 label:label_repeated type:type_message"
    "              type_name: \"foo\" extendee: \"messageset\" }"
    "}",

    "foo.proto: foo.foo: type: extensions of messagesets must be optional "
      "messages.\n");
}

test_f(validationerrortest, fieldinmessageset) {
  buildfilewitherrors(
    "name: \"foo.proto\" "
    "message_type {"
    "  name: \"foo\""
    "  options { message_set_wire_format: true }"
    "  field { name: \"foo\" number: 1 label:label_optional type:type_int32 }"
    "}",

    "foo.proto: foo.foo: name: messagesets cannot have fields, only "
      "extensions.\n");
}

test_f(validationerrortest, negativeextensionrangenumber) {
  buildfilewitherrors(
    "name: \"foo.proto\" "
    "message_type {"
    "  name: \"foo\""
    "  extension_range { start: -10 end: -1 }"
    "}",

    "foo.proto: foo: number: extension numbers must be positive integers.\n");
}

test_f(validationerrortest, hugeextensionrangenumber) {
  buildfilewitherrors(
    "name: \"foo.proto\" "
    "message_type {"
    "  name: \"foo\""
    "  extension_range { start: 1 end: 0x70000000 }"
    "}",

    "foo.proto: foo: number: extension numbers cannot be greater than "
      "536870911.\n");
}

test_f(validationerrortest, extensionrangeendbeforestart) {
  buildfilewitherrors(
    "name: \"foo.proto\" "
    "message_type {"
    "  name: \"foo\""
    "  extension_range { start: 10 end: 10 }"
    "  extension_range { start: 10 end: 5 }"
    "}",

    "foo.proto: foo: number: extension range end number must be greater than "
      "start number.\n"
    "foo.proto: foo: number: extension range end number must be greater than "
      "start number.\n");
}

test_f(validationerrortest, emptyenum) {
  buildfilewitherrors(
    "name: \"foo.proto\" "
    "enum_type { name: \"foo\" }"
    // also use the empty enum in a message to make sure there are no crashes
    // during validation (possible if the code attempts to derive a default
    // value for the field).
    "message_type {"
    "  name: \"bar\""
    "  field { name: \"foo\" number: 1 label:label_optional type_name:\"foo\" }"
    "  field { name: \"bar\" number: 2 label:label_optional type_name:\"foo\" "
    "          default_value: \"no_such_value\" }"
    "}",

    "foo.proto: foo: name: enums must contain at least one value.\n"
    "foo.proto: bar.bar: default_value: enum type \"foo\" has no value named "
      "\"no_such_value\".\n");
}

test_f(validationerrortest, undefinedextendee) {
  buildfilewitherrors(
    "name: \"foo.proto\" "
    "message_type {"
    "  name: \"foo\""
    "  extension { name:\"foo\" number:1 label:label_optional type:type_int32"
    "              extendee: \"bar\" }"
    "}",

    "foo.proto: foo.foo: extendee: \"bar\" is not defined.\n");
}

test_f(validationerrortest, nonmessageextendee) {
  buildfilewitherrors(
    "name: \"foo.proto\" "
    "enum_type { name: \"bar\" value { name:\"dummy\" number:0 } }"
    "message_type {"
    "  name: \"foo\""
    "  extension { name:\"foo\" number:1 label:label_optional type:type_int32"
    "              extendee: \"bar\" }"
    "}",

    "foo.proto: foo.foo: extendee: \"bar\" is not a message type.\n");
}

test_f(validationerrortest, notanextensionnumber) {
  buildfilewitherrors(
    "name: \"foo.proto\" "
    "message_type {"
    "  name: \"bar\""
    "}"
    "message_type {"
    "  name: \"foo\""
    "  extension { name:\"foo\" number:1 label:label_optional type:type_int32"
    "              extendee: \"bar\" }"
    "}",

    "foo.proto: foo.foo: number: \"bar\" does not declare 1 as an extension "
      "number.\n");
}

test_f(validationerrortest, undefinedfieldtype) {
  buildfilewitherrors(
    "name: \"foo.proto\" "
    "message_type {"
    "  name: \"foo\""
    "  field { name:\"foo\" number:1 label:label_optional type_name:\"bar\" }"
    "}",

    "foo.proto: foo.foo: type: \"bar\" is not defined.\n");
}

test_f(validationerrortest, fieldtypedefinedinundeclareddependency) {
  buildfile(
    "name: \"bar.proto\" "
    "message_type { name: \"bar\" } ");

  buildfilewitherrors(
    "name: \"foo.proto\" "
    "message_type {"
    "  name: \"foo\""
    "  field { name:\"foo\" number:1 label:label_optional type_name:\"bar\" }"
    "}",
    "foo.proto: foo.foo: type: \"bar\" seems to be defined in \"bar.proto\", "
      "which is not imported by \"foo.proto\".  to use it here, please add the "
      "necessary import.\n");
}

test_f(validationerrortest, fieldtypedefinedinindirectdependency) {
  // test for hidden dependencies.
  //
  // // bar.proto
  // message bar{}
  //
  // // forward.proto
  // import "bar.proto"
  //
  // // foo.proto
  // import "forward.proto"
  // message foo {
  //   optional bar foo = 1;  // error, needs to import bar.proto explicitly.
  // }
  //
  buildfile(
    "name: \"bar.proto\" "
    "message_type { name: \"bar\" }");

  buildfile(
    "name: \"forward.proto\""
    "dependency: \"bar.proto\"");

  buildfilewitherrors(
    "name: \"foo.proto\" "
    "dependency: \"forward.proto\" "
    "message_type {"
    "  name: \"foo\""
    "  field { name:\"foo\" number:1 label:label_optional type_name:\"bar\" }"
    "}",
    "foo.proto: foo.foo: type: \"bar\" seems to be defined in \"bar.proto\", "
      "which is not imported by \"foo.proto\".  to use it here, please add the "
      "necessary import.\n");
}

test_f(validationerrortest, fieldtypedefinedinpublicdependency) {
  // test for public dependencies.
  //
  // // bar.proto
  // message bar{}
  //
  // // forward.proto
  // import public "bar.proto"
  //
  // // foo.proto
  // import "forward.proto"
  // message foo {
  //   optional bar foo = 1;  // correct. "bar.proto" is public imported into
  //                          // forward.proto, so when "foo.proto" imports
  //                          // "forward.proto", it imports "bar.proto" too.
  // }
  //
  buildfile(
    "name: \"bar.proto\" "
    "message_type { name: \"bar\" }");

  buildfile(
    "name: \"forward.proto\""
    "dependency: \"bar.proto\" "
    "public_dependency: 0");

  buildfile(
    "name: \"foo.proto\" "
    "dependency: \"forward.proto\" "
    "message_type {"
    "  name: \"foo\""
    "  field { name:\"foo\" number:1 label:label_optional type_name:\"bar\" }"
    "}");
}

test_f(validationerrortest, fieldtypedefinedintransitivepublicdependency) {
  // test for public dependencies.
  //
  // // bar.proto
  // message bar{}
  //
  // // forward.proto
  // import public "bar.proto"
  //
  // // forward2.proto
  // import public "forward.proto"
  //
  // // foo.proto
  // import "forward2.proto"
  // message foo {
  //   optional bar foo = 1;  // correct, public imports are transitive.
  // }
  //
  buildfile(
    "name: \"bar.proto\" "
    "message_type { name: \"bar\" }");

  buildfile(
    "name: \"forward.proto\""
    "dependency: \"bar.proto\" "
    "public_dependency: 0");

  buildfile(
    "name: \"forward2.proto\""
    "dependency: \"forward.proto\" "
    "public_dependency: 0");

  buildfile(
    "name: \"foo.proto\" "
    "dependency: \"forward2.proto\" "
    "message_type {"
    "  name: \"foo\""
    "  field { name:\"foo\" number:1 label:label_optional type_name:\"bar\" }"
    "}");
}

test_f(validationerrortest,
       fieldtypedefinedinprivatedependencyofpublicdependency) {
  // test for public dependencies.
  //
  // // bar.proto
  // message bar{}
  //
  // // forward.proto
  // import "bar.proto"
  //
  // // forward2.proto
  // import public "forward.proto"
  //
  // // foo.proto
  // import "forward2.proto"
  // message foo {
  //   optional bar foo = 1;  // error, the "bar.proto" is not public imported
  //                          // into "forward.proto", so will not be imported
  //                          // into either "forward2.proto" or "foo.proto".
  // }
  //
  buildfile(
    "name: \"bar.proto\" "
    "message_type { name: \"bar\" }");

  buildfile(
    "name: \"forward.proto\""
    "dependency: \"bar.proto\"");

  buildfile(
    "name: \"forward2.proto\""
    "dependency: \"forward.proto\" "
    "public_dependency: 0");

  buildfilewitherrors(
    "name: \"foo.proto\" "
    "dependency: \"forward2.proto\" "
    "message_type {"
    "  name: \"foo\""
    "  field { name:\"foo\" number:1 label:label_optional type_name:\"bar\" }"
    "}",
    "foo.proto: foo.foo: type: \"bar\" seems to be defined in \"bar.proto\", "
      "which is not imported by \"foo.proto\".  to use it here, please add the "
      "necessary import.\n");
}


test_f(validationerrortest, searchmostlocalfirst) {
  // the following should produce an error that bar.baz is not defined:
  //   message bar { message baz {} }
  //   message foo {
  //     message bar {
  //       // placing "message baz{}" here, or removing foo.bar altogether,
  //       // would fix the error.
  //     }
  //     optional bar.baz baz = 1;
  //   }
  // an one point the lookup code incorrectly did not produce an error in this
  // case, because when looking for bar.baz, it would try "foo.bar.baz" first,
  // fail, and ten try "bar.baz" and succeed, even though "bar" should actually
  // refer to the inner bar, not the outer one.
  buildfilewitherrors(
    "name: \"foo.proto\" "
    "message_type {"
    "  name: \"bar\""
    "  nested_type { name: \"baz\" }"
    "}"
    "message_type {"
    "  name: \"foo\""
    "  nested_type { name: \"bar\" }"
    "  field { name:\"baz\" number:1 label:label_optional"
    "          type_name:\"bar.baz\" }"
    "}",

    "foo.proto: foo.baz: type: \"bar.baz\" is not defined.\n");
}

test_f(validationerrortest, searchmostlocalfirst2) {
  // this test would find the most local "bar" first, and does, but
  // proceeds to find the outer one because the inner one's not an
  // aggregate.
  buildfile(
    "name: \"foo.proto\" "
    "message_type {"
    "  name: \"bar\""
    "  nested_type { name: \"baz\" }"
    "}"
    "message_type {"
    "  name: \"foo\""
    "  field { name: \"bar\" number:1 type:type_bytes } "
    "  field { name:\"baz\" number:2 label:label_optional"
    "          type_name:\"bar.baz\" }"
    "}");
}

test_f(validationerrortest, packageoriginallydeclaredintransitivedependent) {
  // imagine we have the following:
  //
  // foo.proto:
  //   package foo.bar;
  // bar.proto:
  //   package foo.bar;
  //   import "foo.proto";
  //   message bar {}
  // baz.proto:
  //   package foo;
  //   import "bar.proto"
  //   message baz { optional bar.bar qux = 1; }
  //
  // when validating baz.proto, we will look up "bar.bar".  as part of this
  // lookup, we first lookup "bar" then try to find "bar" within it.  "bar"
  // should resolve to "foo.bar".  note, though, that "foo.bar" was originally
  // defined in foo.proto, which is not a direct dependency of baz.proto.  the
  // implementation of findsymbol() normally only returns symbols in direct
  // dependencies, not indirect ones.  this test insures that this does not
  // prevent it from finding "foo.bar".

  buildfile(
    "name: \"foo.proto\" "
    "package: \"foo.bar\" ");
  buildfile(
    "name: \"bar.proto\" "
    "package: \"foo.bar\" "
    "dependency: \"foo.proto\" "
    "message_type { name: \"bar\" }");
  buildfile(
    "name: \"baz.proto\" "
    "package: \"foo\" "
    "dependency: \"bar.proto\" "
    "message_type { "
    "  name: \"baz\" "
    "  field { name:\"qux\" number:1 label:label_optional "
    "          type_name:\"bar.bar\" }"
    "}");
}

test_f(validationerrortest, fieldtypenotatype) {
  buildfilewitherrors(
    "name: \"foo.proto\" "
    "message_type {"
    "  name: \"foo\""
    "  field { name:\"foo\" number:1 label:label_optional "
    "          type_name:\".foo.bar\" }"
    "  field { name:\"bar\" number:2 label:label_optional type:type_int32 }"
    "}",

    "foo.proto: foo.foo: type: \".foo.bar\" is not a type.\n");
}

test_f(validationerrortest, relativefieldtypenotatype) {
  buildfilewitherrors(
    "name: \"foo.proto\" "
    "message_type {"
    "  nested_type {"
    "    name: \"bar\""
    "    field { name:\"baz\" number:2 label:label_optional type:type_int32 }"
    "  }"
    "  name: \"foo\""
    "  field { name:\"foo\" number:1 label:label_optional "
    "          type_name:\"bar.baz\" }"
    "}",
    "foo.proto: foo.foo: type: \"bar.baz\" is not a type.\n");
}

test_f(validationerrortest, fieldtypemaybeitsname) {
  buildfile(
    "name: \"foo.proto\" "
    "message_type {"
    "  name: \"bar\""
    "}"
    "message_type {"
    "  name: \"foo\""
    "  field { name:\"bar\" number:1 label:label_optional type_name:\"bar\" }"
    "}");
}

test_f(validationerrortest, enumfieldtypeismessage) {
  buildfilewitherrors(
    "name: \"foo.proto\" "
    "message_type { name: \"bar\" } "
    "message_type {"
    "  name: \"foo\""
    "  field { name:\"foo\" number:1 label:label_optional type:type_enum"
    "          type_name:\"bar\" }"
    "}",

    "foo.proto: foo.foo: type: \"bar\" is not an enum type.\n");
}

test_f(validationerrortest, messagefieldtypeisenum) {
  buildfilewitherrors(
    "name: \"foo.proto\" "
    "enum_type { name: \"bar\" value { name:\"dummy\" number:0 } } "
    "message_type {"
    "  name: \"foo\""
    "  field { name:\"foo\" number:1 label:label_optional type:type_message"
    "          type_name:\"bar\" }"
    "}",

    "foo.proto: foo.foo: type: \"bar\" is not a message type.\n");
}

test_f(validationerrortest, badenumdefaultvalue) {
  buildfilewitherrors(
    "name: \"foo.proto\" "
    "enum_type { name: \"bar\" value { name:\"dummy\" number:0 } } "
    "message_type {"
    "  name: \"foo\""
    "  field { name:\"foo\" number:1 label:label_optional type_name:\"bar\""
    "          default_value:\"no_such_value\" }"
    "}",

    "foo.proto: foo.foo: default_value: enum type \"bar\" has no value named "
      "\"no_such_value\".\n");
}

test_f(validationerrortest, primitivewithtypename) {
  buildfilewitherrors(
    "name: \"foo.proto\" "
    "message_type {"
    "  name: \"foo\""
    "  field { name:\"foo\" number:1 label:label_optional type:type_int32"
    "          type_name:\"foo\" }"
    "}",

    "foo.proto: foo.foo: type: field with primitive type has type_name.\n");
}

test_f(validationerrortest, nonprimitivewithouttypename) {
  buildfilewitherrors(
    "name: \"foo.proto\" "
    "message_type {"
    "  name: \"foo\""
    "  field { name:\"foo\" number:1 label:label_optional type:type_message }"
    "}",

    "foo.proto: foo.foo: type: field with message or enum type missing "
      "type_name.\n");
}

test_f(validationerrortest, inputtypenotdefined) {
  buildfilewitherrors(
    "name: \"foo.proto\" "
    "message_type { name: \"foo\" } "
    "service {"
    "  name: \"testservice\""
    "  method { name: \"a\" input_type: \"bar\" output_type: \"foo\" }"
    "}",

    "foo.proto: testservice.a: input_type: \"bar\" is not defined.\n"
    );
}

test_f(validationerrortest, inputtypenotamessage) {
  buildfilewitherrors(
    "name: \"foo.proto\" "
    "message_type { name: \"foo\" } "
    "enum_type { name: \"bar\" value { name:\"dummy\" number:0 } } "
    "service {"
    "  name: \"testservice\""
    "  method { name: \"a\" input_type: \"bar\" output_type: \"foo\" }"
    "}",

    "foo.proto: testservice.a: input_type: \"bar\" is not a message type.\n"
    );
}

test_f(validationerrortest, outputtypenotdefined) {
  buildfilewitherrors(
    "name: \"foo.proto\" "
    "message_type { name: \"foo\" } "
    "service {"
    "  name: \"testservice\""
    "  method { name: \"a\" input_type: \"foo\" output_type: \"bar\" }"
    "}",

    "foo.proto: testservice.a: output_type: \"bar\" is not defined.\n"
    );
}

test_f(validationerrortest, outputtypenotamessage) {
  buildfilewitherrors(
    "name: \"foo.proto\" "
    "message_type { name: \"foo\" } "
    "enum_type { name: \"bar\" value { name:\"dummy\" number:0 } } "
    "service {"
    "  name: \"testservice\""
    "  method { name: \"a\" input_type: \"foo\" output_type: \"bar\" }"
    "}",

    "foo.proto: testservice.a: output_type: \"bar\" is not a message type.\n"
    );
}


test_f(validationerrortest, illegalpackedfield) {
  buildfilewitherrors(
    "name: \"foo.proto\" "
    "message_type {\n"
    "  name: \"foo\""
    "  field { name:\"packed_string\" number:1 label:label_repeated "
    "          type:type_string "
    "          options { uninterpreted_option {"
    "            name { name_part: \"packed\" is_extension: false }"
    "            identifier_value: \"true\" }}}\n"
    "  field { name:\"packed_message\" number:3 label:label_repeated "
    "          type_name: \"foo\""
    "          options { uninterpreted_option {"
    "            name { name_part: \"packed\" is_extension: false }"
    "            identifier_value: \"true\" }}}\n"
    "  field { name:\"optional_int32\" number: 4 label: label_optional "
    "          type:type_int32 "
    "          options { uninterpreted_option {"
    "            name { name_part: \"packed\" is_extension: false }"
    "            identifier_value: \"true\" }}}\n"
    "}",

    "foo.proto: foo.packed_string: type: [packed = true] can only be "
        "specified for repeated primitive fields.\n"
    "foo.proto: foo.packed_message: type: [packed = true] can only be "
        "specified for repeated primitive fields.\n"
    "foo.proto: foo.optional_int32: type: [packed = true] can only be "
        "specified for repeated primitive fields.\n"
        );
}

test_f(validationerrortest, optionwrongtype) {
  buildfilewitherrors(
    "name: \"foo.proto\" "
    "message_type { "
    "  name: \"testmessage\" "
    "  field { name:\"foo\" number:1 label:label_optional type:type_string "
    "          options { uninterpreted_option { name { name_part: \"ctype\" "
    "                                                  is_extension: false }"
    "                                           positive_int_value: 1 }"
    "          }"
    "  }"
    "}\n",

    "foo.proto: testmessage.foo: option_value: value must be identifier for "
    "enum-valued option \"google.protobuf.fieldoptions.ctype\".\n");
}

test_f(validationerrortest, optionextendsatomictype) {
  buildfilewitherrors(
    "name: \"foo.proto\" "
    "message_type { "
    "  name: \"testmessage\" "
    "  field { name:\"foo\" number:1 label:label_optional type:type_string "
    "          options { uninterpreted_option { name { name_part: \"ctype\" "
    "                                                  is_extension: false }"
    "                                           name { name_part: \"foo\" "
    "                                                  is_extension: true }"
    "                                           positive_int_value: 1 }"
    "          }"
    "  }"
    "}\n",

    "foo.proto: testmessage.foo: option_name: option \"ctype\" is an "
    "atomic type, not a message.\n");
}

test_f(validationerrortest, dupoption) {
  buildfilewitherrors(
    "name: \"foo.proto\" "
    "message_type { "
    "  name: \"testmessage\" "
    "  field { name:\"foo\" number:1 label:label_optional type:type_uint32 "
    "          options { uninterpreted_option { name { name_part: \"ctype\" "
    "                                                  is_extension: false }"
    "                                           identifier_value: \"cord\" }"
    "                    uninterpreted_option { name { name_part: \"ctype\" "
    "                                                  is_extension: false }"
    "                                           identifier_value: \"cord\" }"
    "          }"
    "  }"
    "}\n",

    "foo.proto: testmessage.foo: option_name: option \"ctype\" was "
    "already set.\n");
}

test_f(validationerrortest, invalidoptionname) {
  buildfilewitherrors(
    "name: \"foo.proto\" "
    "message_type { "
    "  name: \"testmessage\" "
    "  field { name:\"foo\" number:1 label:label_optional type:type_bool "
    "          options { uninterpreted_option { "
    "                      name { name_part: \"uninterpreted_option\" "
    "                             is_extension: false }"
    "                      positive_int_value: 1 "
    "                    }"
    "          }"
    "  }"
    "}\n",

    "foo.proto: testmessage.foo: option_name: option must not use "
    "reserved name \"uninterpreted_option\".\n");
}

test_f(validationerrortest, repeatedoption) {
  builddescriptormessagesintestpool();

  buildfilewitherrors(
    "name: \"foo.proto\" "
    "dependency: \"google/protobuf/descriptor.proto\" "
    "extension { name: \"foo\" number: 7672757 label: label_repeated "
    "            type: type_float extendee: \"google.protobuf.fileoptions\" }"
    "options { uninterpreted_option { name { name_part: \"foo\" "
    "                                        is_extension: true } "
    "                                 double_value: 1.2 } }",

    "foo.proto: foo.proto: option_name: option field \"(foo)\" is repeated. "
    "repeated options are not supported.\n");
}

test_f(validationerrortest, customoptionconflictingfieldnumber) {
  builddescriptormessagesintestpool();

  buildfilewitherrors(
    "name: \"foo.proto\" "
    "dependency: \"google/protobuf/descriptor.proto\" "
    "extension { name: \"foo1\" number: 7672757 label: label_optional "
    "            type: type_int32 extendee: \"google.protobuf.fieldoptions\" }"
    "extension { name: \"foo2\" number: 7672757 label: label_optional "
    "            type: type_int32 extendee: \"google.protobuf.fieldoptions\" }",

    "foo.proto: foo2: number: extension number 7672757 has already been used "
    "in \"google.protobuf.fieldoptions\" by extension \"foo1\".\n");
}

test_f(validationerrortest, int32optionvalueoutofpositiverange) {
  builddescriptormessagesintestpool();

  buildfilewitherrors(
    "name: \"foo.proto\" "
    "dependency: \"google/protobuf/descriptor.proto\" "
    "extension { name: \"foo\" number: 7672757 label: label_optional "
    "            type: type_int32 extendee: \"google.protobuf.fileoptions\" }"
    "options { uninterpreted_option { name { name_part: \"foo\" "
    "                                        is_extension: true } "
    "                                 positive_int_value: 0x80000000 } "
    "}",

    "foo.proto: foo.proto: option_value: value out of range "
    "for int32 option \"foo\".\n");
}

test_f(validationerrortest, int32optionvalueoutofnegativerange) {
  builddescriptormessagesintestpool();

  buildfilewitherrors(
    "name: \"foo.proto\" "
    "dependency: \"google/protobuf/descriptor.proto\" "
    "extension { name: \"foo\" number: 7672757 label: label_optional "
    "            type: type_int32 extendee: \"google.protobuf.fileoptions\" }"
    "options { uninterpreted_option { name { name_part: \"foo\" "
    "                                        is_extension: true } "
    "                                 negative_int_value: -0x80000001 } "
    "}",

    "foo.proto: foo.proto: option_value: value out of range "
    "for int32 option \"foo\".\n");
}

test_f(validationerrortest, int32optionvalueisnotpositiveint) {
  builddescriptormessagesintestpool();

  buildfilewitherrors(
    "name: \"foo.proto\" "
    "dependency: \"google/protobuf/descriptor.proto\" "
    "extension { name: \"foo\" number: 7672757 label: label_optional "
    "            type: type_int32 extendee: \"google.protobuf.fileoptions\" }"
    "options { uninterpreted_option { name { name_part: \"foo\" "
    "                                        is_extension: true } "
    "                                 string_value: \"5\" } }",

    "foo.proto: foo.proto: option_value: value must be integer "
    "for int32 option \"foo\".\n");
}

test_f(validationerrortest, int64optionvalueoutofrange) {
  builddescriptormessagesintestpool();

  buildfilewitherrors(
    "name: \"foo.proto\" "
    "dependency: \"google/protobuf/descriptor.proto\" "
    "extension { name: \"foo\" number: 7672757 label: label_optional "
    "            type: type_int64 extendee: \"google.protobuf.fileoptions\" }"
    "options { uninterpreted_option { name { name_part: \"foo\" "
    "                                        is_extension: true } "
    "                                 positive_int_value: 0x8000000000000000 } "
    "}",

    "foo.proto: foo.proto: option_value: value out of range "
    "for int64 option \"foo\".\n");
}

test_f(validationerrortest, int64optionvalueisnotpositiveint) {
  builddescriptormessagesintestpool();

  buildfilewitherrors(
    "name: \"foo.proto\" "
    "dependency: \"google/protobuf/descriptor.proto\" "
    "extension { name: \"foo\" number: 7672757 label: label_optional "
    "            type: type_int64 extendee: \"google.protobuf.fileoptions\" }"
    "options { uninterpreted_option { name { name_part: \"foo\" "
    "                                        is_extension: true } "
    "                                 identifier_value: \"5\" } }",

    "foo.proto: foo.proto: option_value: value must be integer "
    "for int64 option \"foo\".\n");
}

test_f(validationerrortest, uint32optionvalueoutofrange) {
  builddescriptormessagesintestpool();

  buildfilewitherrors(
    "name: \"foo.proto\" "
    "dependency: \"google/protobuf/descriptor.proto\" "
    "extension { name: \"foo\" number: 7672757 label: label_optional "
    "            type: type_uint32 extendee: \"google.protobuf.fileoptions\" }"
    "options { uninterpreted_option { name { name_part: \"foo\" "
    "                                        is_extension: true } "
    "                                 positive_int_value: 0x100000000 } }",

    "foo.proto: foo.proto: option_value: value out of range "
    "for uint32 option \"foo\".\n");
}

test_f(validationerrortest, uint32optionvalueisnotpositiveint) {
  builddescriptormessagesintestpool();

  buildfilewitherrors(
    "name: \"foo.proto\" "
    "dependency: \"google/protobuf/descriptor.proto\" "
    "extension { name: \"foo\" number: 7672757 label: label_optional "
    "            type: type_uint32 extendee: \"google.protobuf.fileoptions\" }"
    "options { uninterpreted_option { name { name_part: \"foo\" "
    "                                        is_extension: true } "
    "                                 double_value: -5.6 } }",

    "foo.proto: foo.proto: option_value: value must be non-negative integer "
    "for uint32 option \"foo\".\n");
}

test_f(validationerrortest, uint64optionvalueisnotpositiveint) {
  builddescriptormessagesintestpool();

  buildfilewitherrors(
    "name: \"foo.proto\" "
    "dependency: \"google/protobuf/descriptor.proto\" "
    "extension { name: \"foo\" number: 7672757 label: label_optional "
    "            type: type_uint64 extendee: \"google.protobuf.fileoptions\" }"
    "options { uninterpreted_option { name { name_part: \"foo\" "
    "                                        is_extension: true } "
    "                                 negative_int_value: -5 } }",

    "foo.proto: foo.proto: option_value: value must be non-negative integer "
    "for uint64 option \"foo\".\n");
}

test_f(validationerrortest, floatoptionvalueisnotnumber) {
  builddescriptormessagesintestpool();

  buildfilewitherrors(
    "name: \"foo.proto\" "
    "dependency: \"google/protobuf/descriptor.proto\" "
    "extension { name: \"foo\" number: 7672757 label: label_optional "
    "            type: type_float extendee: \"google.protobuf.fileoptions\" }"
    "options { uninterpreted_option { name { name_part: \"foo\" "
    "                                        is_extension: true } "
    "                                 string_value: \"bar\" } }",

    "foo.proto: foo.proto: option_value: value must be number "
    "for float option \"foo\".\n");
}

test_f(validationerrortest, doubleoptionvalueisnotnumber) {
  builddescriptormessagesintestpool();

  buildfilewitherrors(
    "name: \"foo.proto\" "
    "dependency: \"google/protobuf/descriptor.proto\" "
    "extension { name: \"foo\" number: 7672757 label: label_optional "
    "            type: type_double extendee: \"google.protobuf.fileoptions\" }"
    "options { uninterpreted_option { name { name_part: \"foo\" "
    "                                        is_extension: true } "
    "                                 string_value: \"bar\" } }",

    "foo.proto: foo.proto: option_value: value must be number "
    "for double option \"foo\".\n");
}

test_f(validationerrortest, booloptionvalueisnottrueorfalse) {
  builddescriptormessagesintestpool();

  buildfilewitherrors(
    "name: \"foo.proto\" "
    "dependency: \"google/protobuf/descriptor.proto\" "
    "extension { name: \"foo\" number: 7672757 label: label_optional "
    "            type: type_bool extendee: \"google.protobuf.fileoptions\" }"
    "options { uninterpreted_option { name { name_part: \"foo\" "
    "                                        is_extension: true } "
    "                                 identifier_value: \"bar\" } }",

    "foo.proto: foo.proto: option_value: value must be \"true\" or \"false\" "
    "for boolean option \"foo\".\n");
}

test_f(validationerrortest, enumoptionvalueisnotidentifier) {
  builddescriptormessagesintestpool();

  buildfilewitherrors(
    "name: \"foo.proto\" "
    "dependency: \"google/protobuf/descriptor.proto\" "
    "enum_type { name: \"fooenum\" value { name: \"bar\" number: 1 } "
    "                              value { name: \"baz\" number: 2 } }"
    "extension { name: \"foo\" number: 7672757 label: label_optional "
    "            type: type_enum type_name: \"fooenum\" "
    "            extendee: \"google.protobuf.fileoptions\" }"
    "options { uninterpreted_option { name { name_part: \"foo\" "
    "                                        is_extension: true } "
    "                                 string_value: \"quux\" } }",

    "foo.proto: foo.proto: option_value: value must be identifier for "
    "enum-valued option \"foo\".\n");
}

test_f(validationerrortest, enumoptionvalueisnotenumvaluename) {
  builddescriptormessagesintestpool();

  buildfilewitherrors(
    "name: \"foo.proto\" "
    "dependency: \"google/protobuf/descriptor.proto\" "
    "enum_type { name: \"fooenum\" value { name: \"bar\" number: 1 } "
    "                              value { name: \"baz\" number: 2 } }"
    "extension { name: \"foo\" number: 7672757 label: label_optional "
    "            type: type_enum type_name: \"fooenum\" "
    "            extendee: \"google.protobuf.fileoptions\" }"
    "options { uninterpreted_option { name { name_part: \"foo\" "
    "                                        is_extension: true } "
    "                                 identifier_value: \"quux\" } }",

    "foo.proto: foo.proto: option_value: enum type \"fooenum\" has no value "
    "named \"quux\" for option \"foo\".\n");
}

test_f(validationerrortest, enumoptionvalueissiblingenumvaluename) {
  builddescriptormessagesintestpool();

  buildfilewitherrors(
    "name: \"foo.proto\" "
    "dependency: \"google/protobuf/descriptor.proto\" "
    "enum_type { name: \"fooenum1\" value { name: \"bar\" number: 1 } "
    "                               value { name: \"baz\" number: 2 } }"
    "enum_type { name: \"fooenum2\" value { name: \"qux\" number: 1 } "
    "                               value { name: \"quux\" number: 2 } }"
    "extension { name: \"foo\" number: 7672757 label: label_optional "
    "            type: type_enum type_name: \"fooenum1\" "
    "            extendee: \"google.protobuf.fileoptions\" }"
    "options { uninterpreted_option { name { name_part: \"foo\" "
    "                                        is_extension: true } "
    "                                 identifier_value: \"quux\" } }",

    "foo.proto: foo.proto: option_value: enum type \"fooenum1\" has no value "
    "named \"quux\" for option \"foo\". this appears to be a value from a "
    "sibling type.\n");
}

test_f(validationerrortest, stringoptionvalueisnotstring) {
  builddescriptormessagesintestpool();

  buildfilewitherrors(
    "name: \"foo.proto\" "
    "dependency: \"google/protobuf/descriptor.proto\" "
    "extension { name: \"foo\" number: 7672757 label: label_optional "
    "            type: type_string extendee: \"google.protobuf.fileoptions\" }"
    "options { uninterpreted_option { name { name_part: \"foo\" "
    "                                        is_extension: true } "
    "                                 identifier_value: \"quux\" } }",

    "foo.proto: foo.proto: option_value: value must be quoted string for "
    "string option \"foo\".\n");
}

// helper function for tests that check for aggregate value parsing
// errors.  the "value" argument is embedded inside the
// "uninterpreted_option" portion of the result.
static string embedaggregatevalue(const char* value) {
  return strings::substitute(
      "name: \"foo.proto\" "
      "dependency: \"google/protobuf/descriptor.proto\" "
      "message_type { name: \"foo\" } "
      "extension { name: \"foo\" number: 7672757 label: label_optional "
      "            type: type_message type_name: \"foo\" "
      "            extendee: \"google.protobuf.fileoptions\" }"
      "options { uninterpreted_option { name { name_part: \"foo\" "
      "                                        is_extension: true } "
      "                                 $0 } }",
      value);
}

test_f(validationerrortest, aggregatevaluenotfound) {
  builddescriptormessagesintestpool();

  buildfilewitherrors(
      embedaggregatevalue("string_value: \"\""),
      "foo.proto: foo.proto: option_value: option \"foo\" is a message. "
      "to set the entire message, use syntax like "
      "\"foo = { <proto text format> }\". to set fields within it, use "
      "syntax like \"foo.foo = value\".\n");
}

test_f(validationerrortest, aggregatevalueparseerror) {
  builddescriptormessagesintestpool();

  buildfilewitherrors(
      embedaggregatevalue("aggregate_value: \"1+2\""),
      "foo.proto: foo.proto: option_value: error while parsing option "
      "value for \"foo\": expected identifier.\n");
}

test_f(validationerrortest, aggregatevalueunknownfields) {
  builddescriptormessagesintestpool();

  buildfilewitherrors(
      embedaggregatevalue("aggregate_value: \"x:100\""),
      "foo.proto: foo.proto: option_value: error while parsing option "
      "value for \"foo\": message type \"foo\" has no field named \"x\".\n");
}

test_f(validationerrortest, notliteimportslite) {
  buildfile(
    "name: \"bar.proto\" "
    "options { optimize_for: lite_runtime } ");

  buildfilewitherrors(
    "name: \"foo.proto\" "
    "dependency: \"bar.proto\" ",

    "foo.proto: foo.proto: other: files that do not use optimize_for = "
      "lite_runtime cannot import files which do use this option.  this file "
      "is not lite, but it imports \"bar.proto\" which is.\n");
}

test_f(validationerrortest, liteextendsnotlite) {
  buildfile(
    "name: \"bar.proto\" "
    "message_type: {"
    "  name: \"bar\""
    "  extension_range { start: 1 end: 1000 }"
    "}");

  buildfilewitherrors(
    "name: \"foo.proto\" "
    "dependency: \"bar.proto\" "
    "options { optimize_for: lite_runtime } "
    "extension { name: \"ext\" number: 123 label: label_optional "
    "            type: type_int32 extendee: \"bar\" }",

    "foo.proto: ext: extendee: extensions to non-lite types can only be "
      "declared in non-lite files.  note that you cannot extend a non-lite "
      "type to contain a lite type, but the reverse is allowed.\n");
}

test_f(validationerrortest, noliteservices) {
  buildfilewitherrors(
    "name: \"foo.proto\" "
    "options {"
    "  optimize_for: lite_runtime"
    "  cc_generic_services: true"
    "  java_generic_services: true"
    "} "
    "service { name: \"foo\" }",

    "foo.proto: foo: name: files with optimize_for = lite_runtime cannot "
    "define services unless you set both options cc_generic_services and "
    "java_generic_sevices to false.\n");

  buildfile(
    "name: \"bar.proto\" "
    "options {"
    "  optimize_for: lite_runtime"
    "  cc_generic_services: false"
    "  java_generic_services: false"
    "} "
    "service { name: \"bar\" }");
}

test_f(validationerrortest, rollbackaftererror) {
  // build a file which contains every kind of construct but references an
  // undefined type.  all these constructs will be added to the symbol table
  // before the undefined type error is noticed.  the descriptorpool will then
  // have to roll everything back.
  buildfilewitherrors(
    "name: \"foo.proto\" "
    "message_type {"
    "  name: \"testmessage\""
    "  field { name:\"foo\" label:label_optional type:type_int32 number:1 }"
    "} "
    "enum_type {"
    "  name: \"testenum\""
    "  value { name:\"bar\" number:1 }"
    "} "
    "service {"
    "  name: \"testservice\""
    "  method {"
    "    name: \"baz\""
    "    input_type: \"nosuchtype\""    // error
    "    output_type: \"testmessage\""
    "  }"
    "}",

    "foo.proto: testservice.baz: input_type: \"nosuchtype\" is not defined.\n"
    );

  // make sure that if we build the same file again with the error fixed,
  // it works.  if the above rollback was incomplete, then some symbols will
  // be left defined, and this second attempt will fail since it tries to
  // re-define the same symbols.
  buildfile(
    "name: \"foo.proto\" "
    "message_type {"
    "  name: \"testmessage\""
    "  field { name:\"foo\" label:label_optional type:type_int32 number:1 }"
    "} "
    "enum_type {"
    "  name: \"testenum\""
    "  value { name:\"bar\" number:1 }"
    "} "
    "service {"
    "  name: \"testservice\""
    "  method { name:\"baz\""
    "           input_type:\"testmessage\""
    "           output_type:\"testmessage\" }"
    "}");
}

test_f(validationerrortest, errorsreportedtologerror) {
  // test that errors are reported to google_log(error) if no error collector is
  // provided.

  filedescriptorproto file_proto;
  assert_true(textformat::parsefromstring(
    "name: \"foo.proto\" "
    "message_type { name: \"foo\" } "
    "message_type { name: \"foo\" } ",
    &file_proto));

  vector<string> errors;

  {
    scopedmemorylog log;
    expect_true(pool_.buildfile(file_proto) == null);
    errors = log.getmessages(error);
  }

  assert_eq(2, errors.size());

  expect_eq("invalid proto descriptor for file \"foo.proto\":", errors[0]);
  expect_eq("  foo: \"foo\" is already defined.", errors[1]);
}

test_f(validationerrortest, disallowenumalias) {
  buildfilewitherrors(
    "name: \"foo.proto\" "
    "enum_type {"
    "  name: \"bar\""
    "  value { name:\"enum_a\" number:0 }"
    "  value { name:\"enum_b\" number:0 }"
    "  options { allow_alias: false }"
    "}",
    "foo.proto: bar: number: "
    "\"enum_b\" uses the same enum value as \"enum_a\". "
    "if this is intended, set 'option allow_alias = true;' to the enum "
    "definition.\n");
}

// ===================================================================
// descriptordatabase

static void addtodatabase(simpledescriptordatabase* database,
                          const char* file_text) {
  filedescriptorproto file_proto;
  expect_true(textformat::parsefromstring(file_text, &file_proto));
  database->add(file_proto);
}

class databasebackedpooltest : public testing::test {
 protected:
  databasebackedpooltest() {}

  simpledescriptordatabase database_;

  virtual void setup() {
    addtodatabase(&database_,
      "name: 'foo.proto' "
      "message_type { name:'foo' extension_range { start: 1 end: 100 } } "
      "enum_type { name:'testenum' value { name:'dummy' number:0 } } "
      "service { name:'testservice' } ");
    addtodatabase(&database_,
      "name: 'bar.proto' "
      "dependency: 'foo.proto' "
      "message_type { name:'bar' } "
      "extension { name:'foo_ext' extendee: '.foo' number:5 "
      "            label:label_optional type:type_int32 } ");
    // baz has an undeclared dependency on foo.
    addtodatabase(&database_,
      "name: 'baz.proto' "
      "message_type { "
      "  name:'baz' "
      "  field { name:'foo' number:1 label:label_optional type_name:'foo' } "
      "}");
  }

  // we can't inject a file containing errors into a descriptorpool, so we
  // need an actual mock descriptordatabase to test errors.
  class errordescriptordatabase : public descriptordatabase {
   public:
    errordescriptordatabase() {}
    ~errordescriptordatabase() {}

    // implements descriptordatabase ---------------------------------
    bool findfilebyname(const string& filename,
                        filedescriptorproto* output) {
      // error.proto and error2.proto cyclically import each other.
      if (filename == "error.proto") {
        output->clear();
        output->set_name("error.proto");
        output->add_dependency("error2.proto");
        return true;
      } else if (filename == "error2.proto") {
        output->clear();
        output->set_name("error2.proto");
        output->add_dependency("error.proto");
        return true;
      } else {
        return false;
      }
    }
    bool findfilecontainingsymbol(const string& symbol_name,
                                  filedescriptorproto* output) {
      return false;
    }
    bool findfilecontainingextension(const string& containing_type,
                                     int field_number,
                                     filedescriptorproto* output) {
      return false;
    }
  };

  // a descriptordatabase that counts how many times each method has been
  // called and forwards to some other descriptordatabase.
  class callcountingdatabase : public descriptordatabase {
   public:
    callcountingdatabase(descriptordatabase* wrapped_db)
      : wrapped_db_(wrapped_db) {
      clear();
    }
    ~callcountingdatabase() {}

    descriptordatabase* wrapped_db_;

    int call_count_;

    void clear() {
      call_count_ = 0;
    }

    // implements descriptordatabase ---------------------------------
    bool findfilebyname(const string& filename,
                        filedescriptorproto* output) {
      ++call_count_;
      return wrapped_db_->findfilebyname(filename, output);
    }
    bool findfilecontainingsymbol(const string& symbol_name,
                                  filedescriptorproto* output) {
      ++call_count_;
      return wrapped_db_->findfilecontainingsymbol(symbol_name, output);
    }
    bool findfilecontainingextension(const string& containing_type,
                                     int field_number,
                                     filedescriptorproto* output) {
      ++call_count_;
      return wrapped_db_->findfilecontainingextension(
        containing_type, field_number, output);
    }
  };

  // a descriptordatabase which falsely always returns foo.proto when searching
  // for any symbol or extension number.  this shouldn't cause the
  // descriptorpool to reload foo.proto if it is already loaded.
  class falsepositivedatabase : public descriptordatabase {
   public:
    falsepositivedatabase(descriptordatabase* wrapped_db)
      : wrapped_db_(wrapped_db) {}
    ~falsepositivedatabase() {}

    descriptordatabase* wrapped_db_;

    // implements descriptordatabase ---------------------------------
    bool findfilebyname(const string& filename,
                        filedescriptorproto* output) {
      return wrapped_db_->findfilebyname(filename, output);
    }
    bool findfilecontainingsymbol(const string& symbol_name,
                                  filedescriptorproto* output) {
      return findfilebyname("foo.proto", output);
    }
    bool findfilecontainingextension(const string& containing_type,
                                     int field_number,
                                     filedescriptorproto* output) {
      return findfilebyname("foo.proto", output);
    }
  };
};

test_f(databasebackedpooltest, findfilebyname) {
  descriptorpool pool(&database_);

  const filedescriptor* foo = pool.findfilebyname("foo.proto");
  assert_true(foo != null);
  expect_eq("foo.proto", foo->name());
  assert_eq(1, foo->message_type_count());
  expect_eq("foo", foo->message_type(0)->name());

  expect_eq(foo, pool.findfilebyname("foo.proto"));

  expect_true(pool.findfilebyname("no_such_file.proto") == null);
}

test_f(databasebackedpooltest, finddependencybeforedependent) {
  descriptorpool pool(&database_);

  const filedescriptor* foo = pool.findfilebyname("foo.proto");
  assert_true(foo != null);
  expect_eq("foo.proto", foo->name());
  assert_eq(1, foo->message_type_count());
  expect_eq("foo", foo->message_type(0)->name());

  const filedescriptor* bar = pool.findfilebyname("bar.proto");
  assert_true(bar != null);
  expect_eq("bar.proto", bar->name());
  assert_eq(1, bar->message_type_count());
  expect_eq("bar", bar->message_type(0)->name());

  assert_eq(1, bar->dependency_count());
  expect_eq(foo, bar->dependency(0));
}

test_f(databasebackedpooltest, finddependentbeforedependency) {
  descriptorpool pool(&database_);

  const filedescriptor* bar = pool.findfilebyname("bar.proto");
  assert_true(bar != null);
  expect_eq("bar.proto", bar->name());
  assert_eq(1, bar->message_type_count());
  assert_eq("bar", bar->message_type(0)->name());

  const filedescriptor* foo = pool.findfilebyname("foo.proto");
  assert_true(foo != null);
  expect_eq("foo.proto", foo->name());
  assert_eq(1, foo->message_type_count());
  assert_eq("foo", foo->message_type(0)->name());

  assert_eq(1, bar->dependency_count());
  expect_eq(foo, bar->dependency(0));
}

test_f(databasebackedpooltest, findfilecontainingsymbol) {
  descriptorpool pool(&database_);

  const filedescriptor* file = pool.findfilecontainingsymbol("foo");
  assert_true(file != null);
  expect_eq("foo.proto", file->name());
  expect_eq(file, pool.findfilebyname("foo.proto"));

  expect_true(pool.findfilecontainingsymbol("nosuchsymbol") == null);
}

test_f(databasebackedpooltest, findmessagetypebyname) {
  descriptorpool pool(&database_);

  const descriptor* type = pool.findmessagetypebyname("foo");
  assert_true(type != null);
  expect_eq("foo", type->name());
  expect_eq(type->file(), pool.findfilebyname("foo.proto"));

  expect_true(pool.findmessagetypebyname("nosuchtype") == null);
}

test_f(databasebackedpooltest, findextensionbynumber) {
  descriptorpool pool(&database_);

  const descriptor* foo = pool.findmessagetypebyname("foo");
  assert_true(foo != null);

  const fielddescriptor* extension = pool.findextensionbynumber(foo, 5);
  assert_true(extension != null);
  expect_eq("foo_ext", extension->name());
  expect_eq(extension->file(), pool.findfilebyname("bar.proto"));

  expect_true(pool.findextensionbynumber(foo, 12) == null);
}

test_f(databasebackedpooltest, findallextensions) {
  descriptorpool pool(&database_);

  const descriptor* foo = pool.findmessagetypebyname("foo");

  for (int i = 0; i < 2; ++i) {
    // repeat the lookup twice, to check that we get consistent
    // results despite the fallback database lookup mutating the pool.
    vector<const fielddescriptor*> extensions;
    pool.findallextensions(foo, &extensions);
    assert_eq(1, extensions.size());
    expect_eq(5, extensions[0]->number());
  }
}

test_f(databasebackedpooltest, errorwithouterrorcollector) {
  errordescriptordatabase error_database;
  descriptorpool pool(&error_database);

  vector<string> errors;

  {
    scopedmemorylog log;
    expect_true(pool.findfilebyname("error.proto") == null);
    errors = log.getmessages(error);
  }

  expect_false(errors.empty());
}

test_f(databasebackedpooltest, errorwitherrorcollector) {
  errordescriptordatabase error_database;
  mockerrorcollector error_collector;
  descriptorpool pool(&error_database, &error_collector);

  expect_true(pool.findfilebyname("error.proto") == null);
  expect_eq(
    "error.proto: error.proto: other: file recursively imports itself: "
      "error.proto -> error2.proto -> error.proto\n"
    "error2.proto: error2.proto: other: import \"error.proto\" was not "
      "found or had errors.\n"
    "error.proto: error.proto: other: import \"error2.proto\" was not "
      "found or had errors.\n",
    error_collector.text_);
}

test_f(databasebackedpooltest, undeclareddependencyonunbuilttype) {
  // check that we find and report undeclared dependencies on types that exist
  // in the descriptor database but that have not not been built yet.
  mockerrorcollector error_collector;
  descriptorpool pool(&database_, &error_collector);
  expect_true(pool.findmessagetypebyname("baz") == null);
  expect_eq(
    "baz.proto: baz.foo: type: \"foo\" seems to be defined in \"foo.proto\", "
    "which is not imported by \"baz.proto\".  to use it here, please add "
    "the necessary import.\n",
    error_collector.text_);
}

test_f(databasebackedpooltest, rollbackaftererror) {
  // make sure that all traces of bad types are removed from the pool. this used
  // to be b/4529436, due to the fact that a symbol resolution failure could
  // potentially cause another file to be recursively built, which would trigger
  // a checkpoint _past_ possibly invalid symbols.
  // baz is defined in the database, but the file is invalid because it is
  // missing a necessary import.
  descriptorpool pool(&database_);
  expect_true(pool.findmessagetypebyname("baz") == null);
  // make sure that searching again for the file or the type fails.
  expect_true(pool.findfilebyname("baz.proto") == null);
  expect_true(pool.findmessagetypebyname("baz") == null);
}

test_f(databasebackedpooltest, unittestproto) {
  // try to load all of unittest.proto from a descriptordatabase.  this should
  // thoroughly test all paths through descriptorbuilder to insure that there
  // are no deadlocking problems when pool_->mutex_ is non-null.
  const filedescriptor* original_file =
    protobuf_unittest::testalltypes::descriptor()->file();

  descriptorpooldatabase database(*descriptorpool::generated_pool());
  descriptorpool pool(&database);
  const filedescriptor* file_from_database =
    pool.findfilebyname(original_file->name());

  assert_true(file_from_database != null);

  filedescriptorproto original_file_proto;
  original_file->copyto(&original_file_proto);

  filedescriptorproto file_from_database_proto;
  file_from_database->copyto(&file_from_database_proto);

  expect_eq(original_file_proto.debugstring(),
            file_from_database_proto.debugstring());
}

test_f(databasebackedpooltest, doesntretrydbunnecessarily) {
  // searching for a child of an existing descriptor should never fall back
  // to the descriptordatabase even if it isn't found, because we know all
  // children are already loaded.
  callcountingdatabase call_counter(&database_);
  descriptorpool pool(&call_counter);

  const filedescriptor* file = pool.findfilebyname("foo.proto");
  assert_true(file != null);
  const descriptor* foo = pool.findmessagetypebyname("foo");
  assert_true(foo != null);
  const enumdescriptor* test_enum = pool.findenumtypebyname("testenum");
  assert_true(test_enum != null);
  const servicedescriptor* test_service = pool.findservicebyname("testservice");
  assert_true(test_service != null);

  expect_ne(0, call_counter.call_count_);
  call_counter.clear();

  expect_true(foo->findfieldbyname("no_such_field") == null);
  expect_true(foo->findextensionbyname("no_such_extension") == null);
  expect_true(foo->findnestedtypebyname("nosuchmessagetype") == null);
  expect_true(foo->findenumtypebyname("nosuchenumtype") == null);
  expect_true(foo->findenumvaluebyname("no_such_value") == null);
  expect_true(test_enum->findvaluebyname("no_such_value") == null);
  expect_true(test_service->findmethodbyname("nosuchmethod") == null);

  expect_true(file->findmessagetypebyname("nosuchmessagetype") == null);
  expect_true(file->findenumtypebyname("nosuchenumtype") == null);
  expect_true(file->findenumvaluebyname("no_such_value") == null);
  expect_true(file->findservicebyname("no_such_value") == null);
  expect_true(file->findextensionbyname("no_such_extension") == null);

  expect_true(pool.findfilecontainingsymbol("foo.no.such.field") == null);
  expect_true(pool.findfilecontainingsymbol("foo.no_such_field") == null);
  expect_true(pool.findmessagetypebyname("foo.nosuchmessagetype") == null);
  expect_true(pool.findfieldbyname("foo.no_such_field") == null);
  expect_true(pool.findextensionbyname("foo.no_such_extension") == null);
  expect_true(pool.findenumtypebyname("foo.nosuchenumtype") == null);
  expect_true(pool.findenumvaluebyname("foo.no_such_value") == null);
  expect_true(pool.findmethodbyname("testservice.nosuchmethod") == null);

  expect_eq(0, call_counter.call_count_);
}

test_f(databasebackedpooltest, doesntreloadfilesuncesessarily) {
  // if findfilecontainingsymbol() or findfilecontainingextension() return a
  // file that is already in the descriptorpool, it should not attempt to
  // reload the file.
  falsepositivedatabase false_positive_database(&database_);
  mockerrorcollector error_collector;
  descriptorpool pool(&false_positive_database, &error_collector);

  // first make sure foo.proto is loaded.
  const descriptor* foo = pool.findmessagetypebyname("foo");
  assert_true(foo != null);

  // try inducing false positives.
  expect_true(pool.findmessagetypebyname("nosuchsymbol") == null);
  expect_true(pool.findextensionbynumber(foo, 22) == null);

  // no errors should have been reported.  (if foo.proto was incorrectly
  // loaded multiple times, errors would have been reported.)
  expect_eq("", error_collector.text_);
}

test_f(databasebackedpooltest, doesntreloadknownbadfiles) {
  errordescriptordatabase error_database;
  mockerrorcollector error_collector;
  descriptorpool pool(&error_database, &error_collector);

  expect_true(pool.findfilebyname("error.proto") == null);
  error_collector.text_.clear();
  expect_true(pool.findfilebyname("error.proto") == null);
  expect_eq("", error_collector.text_);
}

test_f(databasebackedpooltest, doesntfallbackonwrongtype) {
  // if a lookup finds a symbol of the wrong type (e.g. we pass a type name
  // to findfieldbyname()), we should fail fast, without checking the fallback
  // database.
  callcountingdatabase call_counter(&database_);
  descriptorpool pool(&call_counter);

  const filedescriptor* file = pool.findfilebyname("foo.proto");
  assert_true(file != null);
  const descriptor* foo = pool.findmessagetypebyname("foo");
  assert_true(foo != null);
  const enumdescriptor* test_enum = pool.findenumtypebyname("testenum");
  assert_true(test_enum != null);

  expect_ne(0, call_counter.call_count_);
  call_counter.clear();

  expect_true(pool.findmessagetypebyname("testenum") == null);
  expect_true(pool.findfieldbyname("foo") == null);
  expect_true(pool.findextensionbyname("foo") == null);
  expect_true(pool.findenumtypebyname("foo") == null);
  expect_true(pool.findenumvaluebyname("foo") == null);
  expect_true(pool.findservicebyname("foo") == null);
  expect_true(pool.findmethodbyname("foo") == null);

  expect_eq(0, call_counter.call_count_);
}

// ===================================================================

class abortingerrorcollector : public descriptorpool::errorcollector {
 public:
  abortingerrorcollector() {}

  virtual void adderror(
      const string &filename,
      const string &element_name,
      const message *message,
      errorlocation location,
      const string &error_message) {
    google_log(fatal) << "adderror() called unexpectedly: " << filename << ": "
               << error_message;
  }
 private:
  google_disallow_evil_constructors(abortingerrorcollector);
};

// a source tree containing only one file.
class singletonsourcetree : public compiler::sourcetree {
 public:
  singletonsourcetree(const string& filename, const string& contents)
      : filename_(filename), contents_(contents) {}

  virtual io::zerocopyinputstream* open(const string& filename) {
    return filename == filename_ ?
        new io::arrayinputstream(contents_.data(), contents_.size()) : null;
  }

 private:
  const string filename_;
  const string contents_;

  google_disallow_evil_constructors(singletonsourcetree);
};

const char *const ksourcelocationtestinput =
  "syntax = \"proto2\";\n"
  "message a {\n"
  "  optional int32 a = 1;\n"
  "  message b {\n"
  "    required double b = 1;\n"
  "  }\n"
  "}\n"
  "enum indecision {\n"
  "  yes   = 1;\n"
  "  no    = 2;\n"
  "  maybe = 3;\n"
  "}\n"
  "service s {\n"
  "  rpc method(a) returns (a.b);\n"
  // put an empty line here to make the source location range match.
  "\n"
  "}\n";

class sourcelocationtest : public testing::test {
 public:
  sourcelocationtest()
      : source_tree_("/test/test.proto", ksourcelocationtestinput),
        db_(&source_tree_),
        pool_(&db_, &collector_) {}

  static string printsourcelocation(const sourcelocation &loc) {
    return strings::substitute("$0:$1-$2:$3",
                               1 + loc.start_line,
                               1 + loc.start_column,
                               1 + loc.end_line,
                               1 + loc.end_column);
  }

 private:
  abortingerrorcollector collector_;
  singletonsourcetree source_tree_;
  compiler::sourcetreedescriptordatabase db_;

 protected:
  descriptorpool pool_;
};

// todo(adonovan): implement support for option fields and for
// subparts of declarations.

test_f(sourcelocationtest, getsourcelocation) {
  sourcelocation loc;

  const filedescriptor *file_desc =
      google_check_notnull(pool_.findfilebyname("/test/test.proto"));

  const descriptor *a_desc = file_desc->findmessagetypebyname("a");
  expect_true(a_desc->getsourcelocation(&loc));
  expect_eq("2:1-7:2", printsourcelocation(loc));

  const descriptor *a_b_desc = a_desc->findnestedtypebyname("b");
  expect_true(a_b_desc->getsourcelocation(&loc));
  expect_eq("4:3-6:4", printsourcelocation(loc));

  const enumdescriptor *e_desc = file_desc->findenumtypebyname("indecision");
  expect_true(e_desc->getsourcelocation(&loc));
  expect_eq("8:1-12:2", printsourcelocation(loc));

  const enumvaluedescriptor *yes_desc = e_desc->findvaluebyname("yes");
  expect_true(yes_desc->getsourcelocation(&loc));
  expect_eq("9:3-9:13", printsourcelocation(loc));

  const servicedescriptor *s_desc = file_desc->findservicebyname("s");
  expect_true(s_desc->getsourcelocation(&loc));
  expect_eq("13:1-16:2", printsourcelocation(loc));

  const methoddescriptor *m_desc = s_desc->findmethodbyname("method");
  expect_true(m_desc->getsourcelocation(&loc));
  expect_eq("14:3-14:31", printsourcelocation(loc));

}

// missing sourcecodeinfo doesn't cause crash:
test_f(sourcelocationtest, getsourcelocation_missingsourcecodeinfo) {
  sourcelocation loc;

  const filedescriptor *file_desc =
      google_check_notnull(pool_.findfilebyname("/test/test.proto"));

  filedescriptorproto proto;
  file_desc->copyto(&proto);  // note, this discards the sourcecodeinfo.
  expect_false(proto.has_source_code_info());

  descriptorpool bad1_pool(&pool_);
  const filedescriptor* bad1_file_desc =
      google_check_notnull(bad1_pool.buildfile(proto));
  const descriptor *bad1_a_desc = bad1_file_desc->findmessagetypebyname("a");
  expect_false(bad1_a_desc->getsourcelocation(&loc));
}

// corrupt sourcecodeinfo doesn't cause crash:
test_f(sourcelocationtest, getsourcelocation_bogussourcecodeinfo) {
  sourcelocation loc;

  const filedescriptor *file_desc =
      google_check_notnull(pool_.findfilebyname("/test/test.proto"));

  filedescriptorproto proto;
  file_desc->copyto(&proto);  // note, this discards the sourcecodeinfo.
  expect_false(proto.has_source_code_info());
  sourcecodeinfo_location *loc_msg =
      proto.mutable_source_code_info()->add_location();
  loc_msg->add_path(1);
  loc_msg->add_path(2);
  loc_msg->add_path(3);
  loc_msg->add_span(4);
  loc_msg->add_span(5);
  loc_msg->add_span(6);

  descriptorpool bad2_pool(&pool_);
  const filedescriptor* bad2_file_desc =
      google_check_notnull(bad2_pool.buildfile(proto));
  const descriptor *bad2_a_desc = bad2_file_desc->findmessagetypebyname("a");
  expect_false(bad2_a_desc->getsourcelocation(&loc));
}

// ===================================================================

const char* const kcopysourcecodeinfototestinput =
  "syntax = \"proto2\";\n"
  "message foo {}\n";

// required since source code information is not preserved by
// filedescriptortest.
class copysourcecodeinfototest : public testing::test {
 public:
  copysourcecodeinfototest()
      : source_tree_("/test/test.proto", kcopysourcecodeinfototestinput),
        db_(&source_tree_),
        pool_(&db_, &collector_) {}

 private:
  abortingerrorcollector collector_;
  singletonsourcetree source_tree_;
  compiler::sourcetreedescriptordatabase db_;

 protected:
  descriptorpool pool_;
};

test_f(copysourcecodeinfototest, copyto_doesnotcopysourcecodeinfo) {
  const filedescriptor* file_desc =
      google_check_notnull(pool_.findfilebyname("/test/test.proto"));
  filedescriptorproto file_desc_proto;
  assert_false(file_desc_proto.has_source_code_info());

  file_desc->copyto(&file_desc_proto);
  expect_false(file_desc_proto.has_source_code_info());
}

test_f(copysourcecodeinfototest, copysourcecodeinfoto) {
  const filedescriptor* file_desc =
      google_check_notnull(pool_.findfilebyname("/test/test.proto"));
  filedescriptorproto file_desc_proto;
  assert_false(file_desc_proto.has_source_code_info());

  file_desc->copysourcecodeinfoto(&file_desc_proto);
  const sourcecodeinfo& info = file_desc_proto.source_code_info();
  assert_eq(3, info.location_size());
  // get the foo message location
  const sourcecodeinfo_location& foo_location = info.location(1);
  assert_eq(2, foo_location.path_size());
  expect_eq(filedescriptorproto::kmessagetypefieldnumber, foo_location.path(0));
  expect_eq(0, foo_location.path(1));      // foo is the first message defined
  assert_eq(3, foo_location.span_size());  // foo spans one line
  expect_eq(1, foo_location.span(0));      // foo is declared on line 1
  expect_eq(0, foo_location.span(1));      // foo starts at column 0
  expect_eq(14, foo_location.span(2));     // foo ends on column 14
}

// ===================================================================


}  // namespace descriptor_unittest
}  // namespace protobuf
}  // namespace google
