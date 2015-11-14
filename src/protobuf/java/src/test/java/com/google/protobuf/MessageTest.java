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

import protobuf_unittest.unittestproto.testalltypes;
import protobuf_unittest.unittestproto.testallextensions;
import protobuf_unittest.unittestproto.testrequired;
import protobuf_unittest.unittestproto.testrequiredforeign;
import protobuf_unittest.unittestproto.foreignmessage;

import junit.framework.testcase;

import java.util.list;

/**
 * misc. unit tests for message operations that apply to both generated
 * and dynamic messages.
 *
 * @author kenton@google.com kenton varda
 */
public class messagetest extends testcase {
  // =================================================================
  // message-merging tests.

  static final testalltypes merge_source =
    testalltypes.newbuilder()
      .setoptionalint32(1)
      .setoptionalstring("foo")
      .setoptionalforeignmessage(foreignmessage.getdefaultinstance())
      .addrepeatedstring("bar")
      .build();

  static final testalltypes merge_dest =
    testalltypes.newbuilder()
      .setoptionalint64(2)
      .setoptionalstring("baz")
      .setoptionalforeignmessage(foreignmessage.newbuilder().setc(3).build())
      .addrepeatedstring("qux")
      .build();

  static final string merge_result_text =
      "optional_int32: 1\n" +
      "optional_int64: 2\n" +
      "optional_string: \"foo\"\n" +
      "optional_foreign_message {\n" +
      "  c: 3\n" +
      "}\n" +
      "repeated_string: \"qux\"\n" +
      "repeated_string: \"bar\"\n";

  public void testmergefrom() throws exception {
    testalltypes result =
      testalltypes.newbuilder(merge_dest)
        .mergefrom(merge_source).build();

    assertequals(merge_result_text, result.tostring());
  }

  /**
   * test merging a dynamicmessage into a generatedmessage.  as long as they
   * have the same descriptor, this should work, but it is an entirely different
   * code path.
   */
  public void testmergefromdynamic() throws exception {
    testalltypes result =
      testalltypes.newbuilder(merge_dest)
        .mergefrom(dynamicmessage.newbuilder(merge_source).build())
        .build();

    assertequals(merge_result_text, result.tostring());
  }

  /** test merging two dynamicmessages. */
  public void testdynamicmergefrom() throws exception {
    dynamicmessage result =
      dynamicmessage.newbuilder(merge_dest)
        .mergefrom(dynamicmessage.newbuilder(merge_source).build())
        .build();

    assertequals(merge_result_text, result.tostring());
  }

  // =================================================================
  // required-field-related tests.

  private static final testrequired test_required_uninitialized =
    testrequired.getdefaultinstance();
  private static final testrequired test_required_initialized =
    testrequired.newbuilder().seta(1).setb(2).setc(3).build();

  public void testrequired() throws exception {
    testrequired.builder builder = testrequired.newbuilder();

    assertfalse(builder.isinitialized());
    builder.seta(1);
    assertfalse(builder.isinitialized());
    builder.setb(1);
    assertfalse(builder.isinitialized());
    builder.setc(1);
    asserttrue(builder.isinitialized());
  }

  public void testrequiredforeign() throws exception {
    testrequiredforeign.builder builder = testrequiredforeign.newbuilder();

    asserttrue(builder.isinitialized());

    builder.setoptionalmessage(test_required_uninitialized);
    assertfalse(builder.isinitialized());

    builder.setoptionalmessage(test_required_initialized);
    asserttrue(builder.isinitialized());

    builder.addrepeatedmessage(test_required_uninitialized);
    assertfalse(builder.isinitialized());

    builder.setrepeatedmessage(0, test_required_initialized);
    asserttrue(builder.isinitialized());
  }

  public void testrequiredextension() throws exception {
    testallextensions.builder builder = testallextensions.newbuilder();

    asserttrue(builder.isinitialized());

    builder.setextension(testrequired.single, test_required_uninitialized);
    assertfalse(builder.isinitialized());

    builder.setextension(testrequired.single, test_required_initialized);
    asserttrue(builder.isinitialized());

    builder.addextension(testrequired.multi, test_required_uninitialized);
    assertfalse(builder.isinitialized());

    builder.setextension(testrequired.multi, 0, test_required_initialized);
    asserttrue(builder.isinitialized());
  }

  public void testrequireddynamic() throws exception {
    descriptors.descriptor descriptor = testrequired.getdescriptor();
    dynamicmessage.builder builder = dynamicmessage.newbuilder(descriptor);

    assertfalse(builder.isinitialized());
    builder.setfield(descriptor.findfieldbyname("a"), 1);
    assertfalse(builder.isinitialized());
    builder.setfield(descriptor.findfieldbyname("b"), 1);
    assertfalse(builder.isinitialized());
    builder.setfield(descriptor.findfieldbyname("c"), 1);
    asserttrue(builder.isinitialized());
  }

  public void testrequireddynamicforeign() throws exception {
    descriptors.descriptor descriptor = testrequiredforeign.getdescriptor();
    dynamicmessage.builder builder = dynamicmessage.newbuilder(descriptor);

    asserttrue(builder.isinitialized());

    builder.setfield(descriptor.findfieldbyname("optional_message"),
                     test_required_uninitialized);
    assertfalse(builder.isinitialized());

    builder.setfield(descriptor.findfieldbyname("optional_message"),
                     test_required_initialized);
    asserttrue(builder.isinitialized());

    builder.addrepeatedfield(descriptor.findfieldbyname("repeated_message"),
                             test_required_uninitialized);
    assertfalse(builder.isinitialized());

    builder.setrepeatedfield(descriptor.findfieldbyname("repeated_message"), 0,
                             test_required_initialized);
    asserttrue(builder.isinitialized());
  }

  public void testuninitializedexception() throws exception {
    try {
      testrequired.newbuilder().build();
      fail("should have thrown an exception.");
    } catch (uninitializedmessageexception e) {
      assertequals("message missing required fields: a, b, c", e.getmessage());
    }
  }

  public void testbuildpartial() throws exception {
    // we're mostly testing that no exception is thrown.
    testrequired message = testrequired.newbuilder().buildpartial();
    assertfalse(message.isinitialized());
  }

  public void testnesteduninitializedexception() throws exception {
    try {
      testrequiredforeign.newbuilder()
        .setoptionalmessage(test_required_uninitialized)
        .addrepeatedmessage(test_required_uninitialized)
        .addrepeatedmessage(test_required_uninitialized)
        .build();
      fail("should have thrown an exception.");
    } catch (uninitializedmessageexception e) {
      assertequals(
        "message missing required fields: " +
        "optional_message.a, " +
        "optional_message.b, " +
        "optional_message.c, " +
        "repeated_message[0].a, " +
        "repeated_message[0].b, " +
        "repeated_message[0].c, " +
        "repeated_message[1].a, " +
        "repeated_message[1].b, " +
        "repeated_message[1].c",
        e.getmessage());
    }
  }

  public void testbuildnestedpartial() throws exception {
    // we're mostly testing that no exception is thrown.
    testrequiredforeign message =
      testrequiredforeign.newbuilder()
        .setoptionalmessage(test_required_uninitialized)
        .addrepeatedmessage(test_required_uninitialized)
        .addrepeatedmessage(test_required_uninitialized)
        .buildpartial();
    assertfalse(message.isinitialized());
  }

  public void testparseunititialized() throws exception {
    try {
      testrequired.parsefrom(bytestring.empty);
      fail("should have thrown an exception.");
    } catch (invalidprotocolbufferexception e) {
      assertequals("message missing required fields: a, b, c", e.getmessage());
    }
  }

  public void testparsenestedunititialized() throws exception {
    bytestring data =
      testrequiredforeign.newbuilder()
        .setoptionalmessage(test_required_uninitialized)
        .addrepeatedmessage(test_required_uninitialized)
        .addrepeatedmessage(test_required_uninitialized)
        .buildpartial().tobytestring();

    try {
      testrequiredforeign.parsefrom(data);
      fail("should have thrown an exception.");
    } catch (invalidprotocolbufferexception e) {
      assertequals(
        "message missing required fields: " +
        "optional_message.a, " +
        "optional_message.b, " +
        "optional_message.c, " +
        "repeated_message[0].a, " +
        "repeated_message[0].b, " +
        "repeated_message[0].c, " +
        "repeated_message[1].a, " +
        "repeated_message[1].b, " +
        "repeated_message[1].c",
        e.getmessage());
    }
  }

  public void testdynamicuninitializedexception() throws exception {
    try {
      dynamicmessage.newbuilder(testrequired.getdescriptor()).build();
      fail("should have thrown an exception.");
    } catch (uninitializedmessageexception e) {
      assertequals("message missing required fields: a, b, c", e.getmessage());
    }
  }

  public void testdynamicbuildpartial() throws exception {
    // we're mostly testing that no exception is thrown.
    dynamicmessage message =
      dynamicmessage.newbuilder(testrequired.getdescriptor())
        .buildpartial();
    assertfalse(message.isinitialized());
  }

  public void testdynamicparseunititialized() throws exception {
    try {
      descriptors.descriptor descriptor = testrequired.getdescriptor();
      dynamicmessage.parsefrom(descriptor, bytestring.empty);
      fail("should have thrown an exception.");
    } catch (invalidprotocolbufferexception e) {
      assertequals("message missing required fields: a, b, c", e.getmessage());
    }
  }
  
  /** test reading unset repeated message from dynamicmessage. */
  public void testdynamicrepeatedmessagenull() throws exception {
    descriptors.descriptor descriptor = testrequired.getdescriptor();
    dynamicmessage result =
      dynamicmessage.newbuilder(testalltypes.getdescriptor())
        .mergefrom(dynamicmessage.newbuilder(merge_source).build())
        .build();

    asserttrue(result.getfield(result.getdescriptorfortype()
        .findfieldbyname("repeated_foreign_message")) instanceof list<?>);
    assertequals(result.getrepeatedfieldcount(result.getdescriptorfortype()
        .findfieldbyname("repeated_foreign_message")), 0);
  }
  
  /** test reading repeated message from dynamicmessage. */
  public void testdynamicrepeatedmessagenotnull() throws exception {

    testalltypes repeated_nested =
      testalltypes.newbuilder()
        .setoptionalint32(1)
        .setoptionalstring("foo")
        .setoptionalforeignmessage(foreignmessage.getdefaultinstance())
        .addrepeatedstring("bar")
        .addrepeatedforeignmessage(foreignmessage.getdefaultinstance())
        .addrepeatedforeignmessage(foreignmessage.getdefaultinstance())
        .build();
    descriptors.descriptor descriptor = testrequired.getdescriptor();
    dynamicmessage result =
      dynamicmessage.newbuilder(testalltypes.getdescriptor())
        .mergefrom(dynamicmessage.newbuilder(repeated_nested).build())
        .build();

    asserttrue(result.getfield(result.getdescriptorfortype()
        .findfieldbyname("repeated_foreign_message")) instanceof list<?>);
    assertequals(result.getrepeatedfieldcount(result.getdescriptorfortype()
        .findfieldbyname("repeated_foreign_message")), 2);
  }
}
