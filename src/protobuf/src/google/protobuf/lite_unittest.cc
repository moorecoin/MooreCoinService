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

#include <string>
#include <iostream>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/test_util_lite.h>
#include <google/protobuf/unittest_lite.pb.h>

using namespace std;

namespace {
// helper methods to test parsing merge behavior.
void expectmessagemerged(const google::protobuf::unittest::testalltypeslite& message) {
  google_check(message.optional_int32() == 3);
  google_check(message.optional_int64() == 2);
  google_check(message.optional_string() == "hello");
}

void assignparsingmergemessages(
    google::protobuf::unittest::testalltypeslite* msg1,
    google::protobuf::unittest::testalltypeslite* msg2,
    google::protobuf::unittest::testalltypeslite* msg3) {
  msg1->set_optional_int32(1);
  msg2->set_optional_int64(2);
  msg3->set_optional_int32(3);
  msg3->set_optional_string("hello");
}

}  // namespace

int main(int argc, char* argv[]) {
  string data, packed_data;

  {
    protobuf_unittest::testalltypeslite message, message2, message3;
    google::protobuf::testutillite::expectclear(message);
    google::protobuf::testutillite::setallfields(&message);
    message2.copyfrom(message);
    data = message.serializeasstring();
    message3.parsefromstring(data);
    google::protobuf::testutillite::expectallfieldsset(message);
    google::protobuf::testutillite::expectallfieldsset(message2);
    google::protobuf::testutillite::expectallfieldsset(message3);
    google::protobuf::testutillite::modifyrepeatedfields(&message);
    google::protobuf::testutillite::expectrepeatedfieldsmodified(message);
    message.clear();
    google::protobuf::testutillite::expectclear(message);
  }

  {
    protobuf_unittest::testallextensionslite message, message2, message3;
    google::protobuf::testutillite::expectextensionsclear(message);
    google::protobuf::testutillite::setallextensions(&message);
    message2.copyfrom(message);
    string extensions_data = message.serializeasstring();
    google_check(extensions_data == data);
    message3.parsefromstring(extensions_data);
    google::protobuf::testutillite::expectallextensionsset(message);
    google::protobuf::testutillite::expectallextensionsset(message2);
    google::protobuf::testutillite::expectallextensionsset(message3);
    google::protobuf::testutillite::modifyrepeatedextensions(&message);
    google::protobuf::testutillite::expectrepeatedextensionsmodified(message);
    message.clear();
    google::protobuf::testutillite::expectextensionsclear(message);
  }

  {
    protobuf_unittest::testpackedtypeslite message, message2, message3;
    google::protobuf::testutillite::expectpackedclear(message);
    google::protobuf::testutillite::setpackedfields(&message);
    message2.copyfrom(message);
    packed_data = message.serializeasstring();
    message3.parsefromstring(packed_data);
    google::protobuf::testutillite::expectpackedfieldsset(message);
    google::protobuf::testutillite::expectpackedfieldsset(message2);
    google::protobuf::testutillite::expectpackedfieldsset(message3);
    google::protobuf::testutillite::modifypackedfields(&message);
    google::protobuf::testutillite::expectpackedfieldsmodified(message);
    message.clear();
    google::protobuf::testutillite::expectpackedclear(message);
  }

  {
    protobuf_unittest::testpackedextensionslite message, message2, message3;
    google::protobuf::testutillite::expectpackedextensionsclear(message);
    google::protobuf::testutillite::setpackedextensions(&message);
    message2.copyfrom(message);
    string packed_extensions_data = message.serializeasstring();
    google_check(packed_extensions_data == packed_data);
    message3.parsefromstring(packed_extensions_data);
    google::protobuf::testutillite::expectpackedextensionsset(message);
    google::protobuf::testutillite::expectpackedextensionsset(message2);
    google::protobuf::testutillite::expectpackedextensionsset(message3);
    google::protobuf::testutillite::modifypackedextensions(&message);
    google::protobuf::testutillite::expectpackedextensionsmodified(message);
    message.clear();
    google::protobuf::testutillite::expectpackedextensionsclear(message);
  }

  {
    // test that if an optional or required message/group field appears multiple
    // times in the input, they need to be merged.
    google::protobuf::unittest::testparsingmergelite::repeatedfieldsgenerator generator;
    google::protobuf::unittest::testalltypeslite* msg1;
    google::protobuf::unittest::testalltypeslite* msg2;
    google::protobuf::unittest::testalltypeslite* msg3;

#define assign_repeated_field(field)                \
  msg1 = generator.add_##field();                   \
  msg2 = generator.add_##field();                   \
  msg3 = generator.add_##field();                   \
  assignparsingmergemessages(msg1, msg2, msg3)

    assign_repeated_field(field1);
    assign_repeated_field(field2);
    assign_repeated_field(field3);
    assign_repeated_field(ext1);
    assign_repeated_field(ext2);

#undef assign_repeated_field
#define assign_repeated_group(field)                \
  msg1 = generator.add_##field()->mutable_field1(); \
  msg2 = generator.add_##field()->mutable_field1(); \
  msg3 = generator.add_##field()->mutable_field1(); \
  assignparsingmergemessages(msg1, msg2, msg3)

    assign_repeated_group(group1);
    assign_repeated_group(group2);

#undef assign_repeated_group

    string buffer;
    generator.serializetostring(&buffer);
    google::protobuf::unittest::testparsingmergelite parsing_merge;
    parsing_merge.parsefromstring(buffer);

    // required and optional fields should be merged.
    expectmessagemerged(parsing_merge.required_all_types());
    expectmessagemerged(parsing_merge.optional_all_types());
    expectmessagemerged(
        parsing_merge.optionalgroup().optional_group_all_types());
    expectmessagemerged(parsing_merge.getextension(
        google::protobuf::unittest::testparsingmergelite::optional_ext));

    // repeated fields should not be merged.
    google_check(parsing_merge.repeated_all_types_size() == 3);
    google_check(parsing_merge.repeatedgroup_size() == 3);
    google_check(parsing_merge.extensionsize(
        google::protobuf::unittest::testparsingmergelite::repeated_ext) == 3);
  }

  cout << "pass" << endl;
  return 0;
}
