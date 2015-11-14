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

#ifndef google_protobuf_test_util_lite_h__
#define google_protobuf_test_util_lite_h__

#include <google/protobuf/unittest_lite.pb.h>

namespace google {
namespace protobuf {

namespace unittest = protobuf_unittest;
namespace unittest_import = protobuf_unittest_import;

class testutillite {
 public:
  // set every field in the message to a unique value.
  static void setallfields(unittest::testalltypeslite* message);
  static void setallextensions(unittest::testallextensionslite* message);
  static void setpackedfields(unittest::testpackedtypeslite* message);
  static void setpackedextensions(unittest::testpackedextensionslite* message);

  // use the repeated versions of the set_*() accessors to modify all the
  // repeated fields of the messsage (which should already have been
  // initialized with set*fields()).  set*fields() itself only tests
  // the add_*() accessors.
  static void modifyrepeatedfields(unittest::testalltypeslite* message);
  static void modifyrepeatedextensions(
      unittest::testallextensionslite* message);
  static void modifypackedfields(unittest::testpackedtypeslite* message);
  static void modifypackedextensions(
      unittest::testpackedextensionslite* message);

  // check that all fields have the values that they should have after
  // set*fields() is called.
  static void expectallfieldsset(const unittest::testalltypeslite& message);
  static void expectallextensionsset(
      const unittest::testallextensionslite& message);
  static void expectpackedfieldsset(
      const unittest::testpackedtypeslite& message);
  static void expectpackedextensionsset(
      const unittest::testpackedextensionslite& message);

  // expect that the message is modified as would be expected from
  // modify*fields().
  static void expectrepeatedfieldsmodified(
      const unittest::testalltypeslite& message);
  static void expectrepeatedextensionsmodified(
      const unittest::testallextensionslite& message);
  static void expectpackedfieldsmodified(
      const unittest::testpackedtypeslite& message);
  static void expectpackedextensionsmodified(
      const unittest::testpackedextensionslite& message);

  // check that all fields have their default values.
  static void expectclear(const unittest::testalltypeslite& message);
  static void expectextensionsclear(
      const unittest::testallextensionslite& message);
  static void expectpackedclear(const unittest::testpackedtypeslite& message);
  static void expectpackedextensionsclear(
      const unittest::testpackedextensionslite& message);

 private:
  google_disallow_evil_constructors(testutillite);
};

}  // namespace protobuf

}  // namespace google
#endif  // google_protobuf_test_util_lite_h__
