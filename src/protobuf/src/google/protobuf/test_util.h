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

#ifndef google_protobuf_test_util_h__
#define google_protobuf_test_util_h__

#include <stack>
#include <string>
#include <google/protobuf/message.h>
#include <google/protobuf/unittest.pb.h>

namespace google {
namespace protobuf {

namespace unittest = ::protobuf_unittest;
namespace unittest_import = protobuf_unittest_import;

class testutil {
 public:
  // set every field in the message to a unique value.
  static void setallfields(unittest::testalltypes* message);
  static void setoptionalfields(unittest::testalltypes* message);
  static void addrepeatedfields1(unittest::testalltypes* message);
  static void addrepeatedfields2(unittest::testalltypes* message);
  static void setdefaultfields(unittest::testalltypes* message);
  static void setallextensions(unittest::testallextensions* message);
  static void setallfieldsandextensions(unittest::testfieldorderings* message);
  static void setpackedfields(unittest::testpackedtypes* message);
  static void setpackedextensions(unittest::testpackedextensions* message);
  static void setunpackedfields(unittest::testunpackedtypes* message);

  // use the repeated versions of the set_*() accessors to modify all the
  // repeated fields of the messsage (which should already have been
  // initialized with set*fields()).  set*fields() itself only tests
  // the add_*() accessors.
  static void modifyrepeatedfields(unittest::testalltypes* message);
  static void modifyrepeatedextensions(unittest::testallextensions* message);
  static void modifypackedfields(unittest::testpackedtypes* message);
  static void modifypackedextensions(unittest::testpackedextensions* message);

  // check that all fields have the values that they should have after
  // set*fields() is called.
  static void expectallfieldsset(const unittest::testalltypes& message);
  static void expectallextensionsset(
      const unittest::testallextensions& message);
  static void expectpackedfieldsset(const unittest::testpackedtypes& message);
  static void expectpackedextensionsset(
      const unittest::testpackedextensions& message);
  static void expectunpackedfieldsset(
      const unittest::testunpackedtypes& message);

  // expect that the message is modified as would be expected from
  // modify*fields().
  static void expectrepeatedfieldsmodified(
      const unittest::testalltypes& message);
  static void expectrepeatedextensionsmodified(
      const unittest::testallextensions& message);
  static void expectpackedfieldsmodified(
      const unittest::testpackedtypes& message);
  static void expectpackedextensionsmodified(
      const unittest::testpackedextensions& message);

  // check that all fields have their default values.
  static void expectclear(const unittest::testalltypes& message);
  static void expectextensionsclear(const unittest::testallextensions& message);
  static void expectpackedclear(const unittest::testpackedtypes& message);
  static void expectpackedextensionsclear(
      const unittest::testpackedextensions& message);

  // check that the passed-in serialization is the canonical serialization we
  // expect for a testfieldorderings message filled in by
  // setallfieldsandextensions().
  static void expectallfieldsandextensionsinorder(const string& serialized);

  // check that all repeated fields have had their last elements removed.
  static void expectlastrepeatedsremoved(
      const unittest::testalltypes& message);
  static void expectlastrepeatedextensionsremoved(
      const unittest::testallextensions& message);
  static void expectlastrepeatedsreleased(
      const unittest::testalltypes& message);
  static void expectlastrepeatedextensionsreleased(
      const unittest::testallextensions& message);

  // check that all repeated fields have had their first and last elements
  // swapped.
  static void expectrepeatedsswapped(const unittest::testalltypes& message);
  static void expectrepeatedextensionsswapped(
      const unittest::testallextensions& message);

  // like above, but use the reflection interface.
  class reflectiontester {
   public:
    // base_descriptor must be a descriptor for testalltypes or
    // testallextensions.  in the former case, reflectiontester fetches from
    // it the fielddescriptors needed to use the reflection interface.  in
    // the latter case, reflectiontester searches for extension fields in
    // its file.
    explicit reflectiontester(const descriptor* base_descriptor);

    void setallfieldsviareflection(message* message);
    void modifyrepeatedfieldsviareflection(message* message);
    void expectallfieldssetviareflection(const message& message);
    void expectclearviareflection(const message& message);

    void setpackedfieldsviareflection(message* message);
    void modifypackedfieldsviareflection(message* message);
    void expectpackedfieldssetviareflection(const message& message);
    void expectpackedclearviareflection(const message& message);

    void removelastrepeatedsviareflection(message* message);
    void releaselastrepeatedsviareflection(
        message* message, bool expect_extensions_notnull);
    void swaprepeatedsviareflection(message* message);

    enum messagereleasestate {
      is_null,
      can_be_null,
      not_null,
    };
    void expectmessagesreleasedviareflection(
        message* message, messagereleasestate expected_release_state);

   private:
    const fielddescriptor* f(const string& name);

    const descriptor* base_descriptor_;

    const fielddescriptor* group_a_;
    const fielddescriptor* repeated_group_a_;
    const fielddescriptor* nested_b_;
    const fielddescriptor* foreign_c_;
    const fielddescriptor* import_d_;
    const fielddescriptor* import_e_;

    const enumvaluedescriptor* nested_foo_;
    const enumvaluedescriptor* nested_bar_;
    const enumvaluedescriptor* nested_baz_;
    const enumvaluedescriptor* foreign_foo_;
    const enumvaluedescriptor* foreign_bar_;
    const enumvaluedescriptor* foreign_baz_;
    const enumvaluedescriptor* import_foo_;
    const enumvaluedescriptor* import_bar_;
    const enumvaluedescriptor* import_baz_;

    // we have to split this into three function otherwise it creates a stack
    // frame so large that it triggers a warning.
    void expectallfieldssetviareflection1(const message& message);
    void expectallfieldssetviareflection2(const message& message);
    void expectallfieldssetviareflection3(const message& message);

    google_disallow_evil_constructors(reflectiontester);
  };

 private:
  google_disallow_evil_constructors(testutil);
};

}  // namespace protobuf

}  // namespace google
#endif  // google_protobuf_test_util_h__
