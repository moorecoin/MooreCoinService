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

package com.google.protobuf;

import protobuf_unittest.unittestproto.testallextensions;
import protobuf_unittest.unittestproto.testalltypes;
import protobuf_unittest.unittestproto.testemptymessage;
import protobuf_unittest.unittestproto.testpackedtypes;

import junit.framework.testcase;
import java.util.arrays;

/**
 * unit test for {@link dynamicmessage}.  see also {@link messagetest}, which
 * tests some {@link dynamicmessage} functionality.
 *
 * @author kenton@google.com kenton varda
 */
public class dynamicmessagetest extends testcase {
  testutil.reflectiontester reflectiontester =
    new testutil.reflectiontester(testalltypes.getdescriptor(), null);

  testutil.reflectiontester extensionsreflectiontester =
    new testutil.reflectiontester(testallextensions.getdescriptor(),
                                  testutil.getextensionregistry());
  testutil.reflectiontester packedreflectiontester =
    new testutil.reflectiontester(testpackedtypes.getdescriptor(), null);

  public void testdynamicmessageaccessors() throws exception {
    message.builder builder =
      dynamicmessage.newbuilder(testalltypes.getdescriptor());
    reflectiontester.setallfieldsviareflection(builder);
    message message = builder.build();
    reflectiontester.assertallfieldssetviareflection(message);
  }

  public void testsettersafterbuild() throws exception {
    message.builder builder =
      dynamicmessage.newbuilder(testalltypes.getdescriptor());
    message firstmessage = builder.build();
    // double build()
    builder.build();
    // clear() after build()
    builder.clear();
    // setters after build()
    reflectiontester.setallfieldsviareflection(builder);
    message message = builder.build();
    reflectiontester.assertallfieldssetviareflection(message);
    // repeated setters after build()
    reflectiontester.modifyrepeatedfieldsviareflection(builder);
    message = builder.build();
    reflectiontester.assertrepeatedfieldsmodifiedviareflection(message);
    // firstmessage shouldn't have been modified.
    reflectiontester.assertclearviareflection(firstmessage);
  }

  public void testunknownfields() throws exception {
    message.builder builder =
        dynamicmessage.newbuilder(testemptymessage.getdescriptor());
    builder.setunknownfields(unknownfieldset.newbuilder()
        .addfield(1, unknownfieldset.field.newbuilder().addvarint(1).build())
        .addfield(2, unknownfieldset.field.newbuilder().addfixed32(1).build())
        .build());
    message message = builder.build();
    assertequals(2, message.getunknownfields().asmap().size());
    // clone() with unknown fields
    message.builder newbuilder = builder.clone();
    assertequals(2, newbuilder.getunknownfields().asmap().size());
    // clear() with unknown fields
    newbuilder.clear();
    asserttrue(newbuilder.getunknownfields().asmap().isempty());
    // serialize/parse with unknown fields
    newbuilder.mergefrom(message.tobytestring());
    assertequals(2, newbuilder.getunknownfields().asmap().size());
  }

  public void testdynamicmessagesettersrejectnull() throws exception {
    message.builder builder =
      dynamicmessage.newbuilder(testalltypes.getdescriptor());
    reflectiontester.assertreflectionsettersrejectnull(builder);
  }

  public void testdynamicmessageextensionaccessors() throws exception {
    // we don't need to extensively test dynamicmessage's handling of
    // extensions because, frankly, it doesn't do anything special with them.
    // it treats them just like any other fields.
    message.builder builder =
      dynamicmessage.newbuilder(testallextensions.getdescriptor());
    extensionsreflectiontester.setallfieldsviareflection(builder);
    message message = builder.build();
    extensionsreflectiontester.assertallfieldssetviareflection(message);
  }

  public void testdynamicmessageextensionsettersrejectnull() throws exception {
    message.builder builder =
      dynamicmessage.newbuilder(testallextensions.getdescriptor());
    extensionsreflectiontester.assertreflectionsettersrejectnull(builder);
  }

  public void testdynamicmessagerepeatedsetters() throws exception {
    message.builder builder =
      dynamicmessage.newbuilder(testalltypes.getdescriptor());
    reflectiontester.setallfieldsviareflection(builder);
    reflectiontester.modifyrepeatedfieldsviareflection(builder);
    message message = builder.build();
    reflectiontester.assertrepeatedfieldsmodifiedviareflection(message);
  }

  public void testdynamicmessagerepeatedsettersrejectnull() throws exception {
    message.builder builder =
      dynamicmessage.newbuilder(testalltypes.getdescriptor());
    reflectiontester.assertreflectionrepeatedsettersrejectnull(builder);
  }

  public void testdynamicmessagedefaults() throws exception {
    reflectiontester.assertclearviareflection(
      dynamicmessage.getdefaultinstance(testalltypes.getdescriptor()));
    reflectiontester.assertclearviareflection(
      dynamicmessage.newbuilder(testalltypes.getdescriptor()).build());
  }

  public void testdynamicmessageserializedsize() throws exception {
    testalltypes message = testutil.getallset();

    message.builder dynamicbuilder =
      dynamicmessage.newbuilder(testalltypes.getdescriptor());
    reflectiontester.setallfieldsviareflection(dynamicbuilder);
    message dynamicmessage = dynamicbuilder.build();

    assertequals(message.getserializedsize(),
                 dynamicmessage.getserializedsize());
  }

  public void testdynamicmessageserialization() throws exception {
    message.builder builder =
      dynamicmessage.newbuilder(testalltypes.getdescriptor());
    reflectiontester.setallfieldsviareflection(builder);
    message message = builder.build();

    bytestring rawbytes = message.tobytestring();
    testalltypes message2 = testalltypes.parsefrom(rawbytes);

    testutil.assertallfieldsset(message2);

    // in fact, the serialized forms should be exactly the same, byte-for-byte.
    assertequals(testutil.getallset().tobytestring(), rawbytes);
  }

  public void testdynamicmessageparsing() throws exception {
    testalltypes.builder builder = testalltypes.newbuilder();
    testutil.setallfields(builder);
    testalltypes message = builder.build();

    bytestring rawbytes = message.tobytestring();

    message message2 =
      dynamicmessage.parsefrom(testalltypes.getdescriptor(), rawbytes);
    reflectiontester.assertallfieldssetviareflection(message2);

    // test parser interface.
    message message3 = message2.getparserfortype().parsefrom(rawbytes);
    reflectiontester.assertallfieldssetviareflection(message3);
  }

  public void testdynamicmessageextensionparsing() throws exception {
    bytestring rawbytes = testutil.getallextensionsset().tobytestring();
    message message = dynamicmessage.parsefrom(
        testallextensions.getdescriptor(), rawbytes,
        testutil.getextensionregistry());
    extensionsreflectiontester.assertallfieldssetviareflection(message);

    // test parser interface.
    message message2 = message.getparserfortype().parsefrom(
        rawbytes, testutil.getextensionregistry());
    extensionsreflectiontester.assertallfieldssetviareflection(message2);
  }

  public void testdynamicmessagepackedserialization() throws exception {
    message.builder builder =
        dynamicmessage.newbuilder(testpackedtypes.getdescriptor());
    packedreflectiontester.setpackedfieldsviareflection(builder);
    message message = builder.build();

    bytestring rawbytes = message.tobytestring();
    testpackedtypes message2 = testpackedtypes.parsefrom(rawbytes);

    testutil.assertpackedfieldsset(message2);

    // in fact, the serialized forms should be exactly the same, byte-for-byte.
    assertequals(testutil.getpackedset().tobytestring(), rawbytes);
  }

  public void testdynamicmessagepackedparsing() throws exception {
    testpackedtypes.builder builder = testpackedtypes.newbuilder();
    testutil.setpackedfields(builder);
    testpackedtypes message = builder.build();

    bytestring rawbytes = message.tobytestring();

    message message2 =
      dynamicmessage.parsefrom(testpackedtypes.getdescriptor(), rawbytes);
    packedreflectiontester.assertpackedfieldssetviareflection(message2);

    // test parser interface.
    message message3 = message2.getparserfortype().parsefrom(rawbytes);
    packedreflectiontester.assertpackedfieldssetviareflection(message3);
  }

  public void testdynamicmessagecopy() throws exception {
    testalltypes.builder builder = testalltypes.newbuilder();
    testutil.setallfields(builder);
    testalltypes message = builder.build();

    dynamicmessage copy = dynamicmessage.newbuilder(message).build();
    reflectiontester.assertallfieldssetviareflection(copy);
  }

  public void testtobuilder() throws exception {
    dynamicmessage.builder builder =
        dynamicmessage.newbuilder(testalltypes.getdescriptor());
    reflectiontester.setallfieldsviareflection(builder);
    int unknownfieldnum = 9;
    long unknownfieldval = 90;
    builder.setunknownfields(unknownfieldset.newbuilder()
        .addfield(unknownfieldnum,
            unknownfieldset.field.newbuilder()
                .addvarint(unknownfieldval).build())
        .build());
    dynamicmessage message = builder.build();

    dynamicmessage derived = message.tobuilder().build();
    reflectiontester.assertallfieldssetviareflection(derived);
    assertequals(arrays.aslist(unknownfieldval),
        derived.getunknownfields().getfield(unknownfieldnum).getvarintlist());
  }
}
