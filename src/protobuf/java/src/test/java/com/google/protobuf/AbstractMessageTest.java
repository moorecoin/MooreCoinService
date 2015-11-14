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

import com.google.protobuf.descriptors.fielddescriptor;
import protobuf_unittest.unittestoptimizefor.testoptimizedforsize;
import protobuf_unittest.unittestproto;
import protobuf_unittest.unittestproto.foreignmessage;
import protobuf_unittest.unittestproto.testallextensions;
import protobuf_unittest.unittestproto.testalltypes;
import protobuf_unittest.unittestproto.testpackedtypes;
import protobuf_unittest.unittestproto.testrequired;
import protobuf_unittest.unittestproto.testrequiredforeign;
import protobuf_unittest.unittestproto.testunpackedtypes;

import junit.framework.testcase;

import java.util.map;

/**
 * unit test for {@link abstractmessage}.
 *
 * @author kenton@google.com kenton varda
 */
public class abstractmessagetest extends testcase {
  /**
   * extends abstractmessage and wraps some other message object.  the methods
   * of the message interface which aren't explicitly implemented by
   * abstractmessage are forwarded to the wrapped object.  this allows us to
   * test that abstractmessage's implementations work even if the wrapped
   * object does not use them.
   */
  private static class abstractmessagewrapper extends abstractmessage {
    private final message wrappedmessage;

    public abstractmessagewrapper(message wrappedmessage) {
      this.wrappedmessage = wrappedmessage;
    }

    public descriptors.descriptor getdescriptorfortype() {
      return wrappedmessage.getdescriptorfortype();
    }
    public abstractmessagewrapper getdefaultinstancefortype() {
      return new abstractmessagewrapper(
        wrappedmessage.getdefaultinstancefortype());
    }
    public map<descriptors.fielddescriptor, object> getallfields() {
      return wrappedmessage.getallfields();
    }
    public boolean hasfield(descriptors.fielddescriptor field) {
      return wrappedmessage.hasfield(field);
    }
    public object getfield(descriptors.fielddescriptor field) {
      return wrappedmessage.getfield(field);
    }
    public int getrepeatedfieldcount(descriptors.fielddescriptor field) {
      return wrappedmessage.getrepeatedfieldcount(field);
    }
    public object getrepeatedfield(
        descriptors.fielddescriptor field, int index) {
      return wrappedmessage.getrepeatedfield(field, index);
    }
    public unknownfieldset getunknownfields() {
      return wrappedmessage.getunknownfields();
    }
    public builder newbuilderfortype() {
      return new builder(wrappedmessage.newbuilderfortype());
    }
    public builder tobuilder() {
      return new builder(wrappedmessage.tobuilder());
    }

    static class builder extends abstractmessage.builder<builder> {
      private final message.builder wrappedbuilder;

      public builder(message.builder wrappedbuilder) {
        this.wrappedbuilder = wrappedbuilder;
      }

      public abstractmessagewrapper build() {
        return new abstractmessagewrapper(wrappedbuilder.build());
      }
      public abstractmessagewrapper buildpartial() {
        return new abstractmessagewrapper(wrappedbuilder.buildpartial());
      }
      public builder clone() {
        return new builder(wrappedbuilder.clone());
      }
      public boolean isinitialized() {
        return clone().buildpartial().isinitialized();
      }
      public descriptors.descriptor getdescriptorfortype() {
        return wrappedbuilder.getdescriptorfortype();
      }
      public abstractmessagewrapper getdefaultinstancefortype() {
        return new abstractmessagewrapper(
          wrappedbuilder.getdefaultinstancefortype());
      }
      public map<descriptors.fielddescriptor, object> getallfields() {
        return wrappedbuilder.getallfields();
      }
      public builder newbuilderforfield(descriptors.fielddescriptor field) {
        return new builder(wrappedbuilder.newbuilderforfield(field));
      }
      public boolean hasfield(descriptors.fielddescriptor field) {
        return wrappedbuilder.hasfield(field);
      }
      public object getfield(descriptors.fielddescriptor field) {
        return wrappedbuilder.getfield(field);
      }
      public builder setfield(descriptors.fielddescriptor field, object value) {
        wrappedbuilder.setfield(field, value);
        return this;
      }
      public builder clearfield(descriptors.fielddescriptor field) {
        wrappedbuilder.clearfield(field);
        return this;
      }
      public int getrepeatedfieldcount(descriptors.fielddescriptor field) {
        return wrappedbuilder.getrepeatedfieldcount(field);
      }
      public object getrepeatedfield(
          descriptors.fielddescriptor field, int index) {
        return wrappedbuilder.getrepeatedfield(field, index);
      }
      public builder setrepeatedfield(descriptors.fielddescriptor field,
                                      int index, object value) {
        wrappedbuilder.setrepeatedfield(field, index, value);
        return this;
      }
      public builder addrepeatedfield(
          descriptors.fielddescriptor field, object value) {
        wrappedbuilder.addrepeatedfield(field, value);
        return this;
      }
      public unknownfieldset getunknownfields() {
        return wrappedbuilder.getunknownfields();
      }
      public builder setunknownfields(unknownfieldset unknownfields) {
        wrappedbuilder.setunknownfields(unknownfields);
        return this;
      }
      @override
      public message.builder getfieldbuilder(fielddescriptor field) {
        return wrappedbuilder.getfieldbuilder(field);
      }
    }
    public parser<? extends message> getparserfortype() {
      return wrappedmessage.getparserfortype();
    }
  }

  // =================================================================

  testutil.reflectiontester reflectiontester =
    new testutil.reflectiontester(testalltypes.getdescriptor(), null);

  testutil.reflectiontester extensionsreflectiontester =
    new testutil.reflectiontester(testallextensions.getdescriptor(),
                                  testutil.getextensionregistry());

  public void testclear() throws exception {
    abstractmessagewrapper message =
      new abstractmessagewrapper.builder(
          testalltypes.newbuilder(testutil.getallset()))
        .clear().build();
    testutil.assertclear((testalltypes) message.wrappedmessage);
  }

  public void testcopy() throws exception {
    abstractmessagewrapper message =
      new abstractmessagewrapper.builder(testalltypes.newbuilder())
        .mergefrom(testutil.getallset()).build();
    testutil.assertallfieldsset((testalltypes) message.wrappedmessage);
  }

  public void testserializedsize() throws exception {
    testalltypes message = testutil.getallset();
    message abstractmessage = new abstractmessagewrapper(testutil.getallset());

    assertequals(message.getserializedsize(),
                 abstractmessage.getserializedsize());
  }

  public void testserialization() throws exception {
    message abstractmessage = new abstractmessagewrapper(testutil.getallset());

    testutil.assertallfieldsset(
      testalltypes.parsefrom(abstractmessage.tobytestring()));

    assertequals(testutil.getallset().tobytestring(),
                 abstractmessage.tobytestring());
  }

  public void testparsing() throws exception {
    abstractmessagewrapper.builder builder =
      new abstractmessagewrapper.builder(testalltypes.newbuilder());
    abstractmessagewrapper message =
      builder.mergefrom(testutil.getallset().tobytestring()).build();
    testutil.assertallfieldsset((testalltypes) message.wrappedmessage);
  }

  public void testparsinguninitialized() throws exception {
    testrequiredforeign.builder builder = testrequiredforeign.newbuilder();
    builder.getoptionalmessagebuilder().setdummy2(10);
    bytestring bytes = builder.buildpartial().tobytestring();
    message.builder abstractmessagebuilder =
        new abstractmessagewrapper.builder(testrequiredforeign.newbuilder());
    // mergefrom() should not throw initialization error.
    abstractmessagebuilder.mergefrom(bytes).buildpartial();
    try {
      abstractmessagebuilder.mergefrom(bytes).build();
      fail();
    } catch (uninitializedmessageexception ex) {
      // pass
    }

    // test dynamicmessage directly.
    message.builder dynamicmessagebuilder = dynamicmessage.newbuilder(
        testrequiredforeign.getdescriptor());
    // mergefrom() should not throw initialization error.
    dynamicmessagebuilder.mergefrom(bytes).buildpartial();
    try {
      dynamicmessagebuilder.mergefrom(bytes).build();
      fail();
    } catch (uninitializedmessageexception ex) {
      // pass
    }
  }

  public void testpackedserialization() throws exception {
    message abstractmessage =
        new abstractmessagewrapper(testutil.getpackedset());

    testutil.assertpackedfieldsset(
      testpackedtypes.parsefrom(abstractmessage.tobytestring()));

    assertequals(testutil.getpackedset().tobytestring(),
                 abstractmessage.tobytestring());
  }

  public void testpackedparsing() throws exception {
    abstractmessagewrapper.builder builder =
      new abstractmessagewrapper.builder(testpackedtypes.newbuilder());
    abstractmessagewrapper message =
      builder.mergefrom(testutil.getpackedset().tobytestring()).build();
    testutil.assertpackedfieldsset((testpackedtypes) message.wrappedmessage);
  }

  public void testunpackedserialization() throws exception {
    message abstractmessage =
      new abstractmessagewrapper(testutil.getunpackedset());

    testutil.assertunpackedfieldsset(
      testunpackedtypes.parsefrom(abstractmessage.tobytestring()));

    assertequals(testutil.getunpackedset().tobytestring(),
                 abstractmessage.tobytestring());
  }

  public void testparsepackedtounpacked() throws exception {
    abstractmessagewrapper.builder builder =
      new abstractmessagewrapper.builder(testunpackedtypes.newbuilder());
    abstractmessagewrapper message =
      builder.mergefrom(testutil.getpackedset().tobytestring()).build();
    testutil.assertunpackedfieldsset(
      (testunpackedtypes) message.wrappedmessage);
  }

  public void testparseunpackedtopacked() throws exception {
    abstractmessagewrapper.builder builder =
      new abstractmessagewrapper.builder(testpackedtypes.newbuilder());
    abstractmessagewrapper message =
      builder.mergefrom(testutil.getunpackedset().tobytestring()).build();
    testutil.assertpackedfieldsset((testpackedtypes) message.wrappedmessage);
  }

  public void testunpackedparsing() throws exception {
    abstractmessagewrapper.builder builder =
      new abstractmessagewrapper.builder(testunpackedtypes.newbuilder());
    abstractmessagewrapper message =
      builder.mergefrom(testutil.getunpackedset().tobytestring()).build();
    testutil.assertunpackedfieldsset(
      (testunpackedtypes) message.wrappedmessage);
  }

  public void testoptimizedforsize() throws exception {
    // we're mostly only checking that this class was compiled successfully.
    testoptimizedforsize message =
      testoptimizedforsize.newbuilder().seti(1).build();
    message = testoptimizedforsize.parsefrom(message.tobytestring());
    assertequals(2, message.getserializedsize());
  }

  // -----------------------------------------------------------------
  // tests for isinitialized().

  private static final testrequired test_required_uninitialized =
    testrequired.getdefaultinstance();
  private static final testrequired test_required_initialized =
    testrequired.newbuilder().seta(1).setb(2).setc(3).build();

  public void testisinitialized() throws exception {
    testrequired.builder builder = testrequired.newbuilder();
    abstractmessagewrapper.builder abstractbuilder =
      new abstractmessagewrapper.builder(builder);

    assertfalse(abstractbuilder.isinitialized());
    assertequals("a, b, c", abstractbuilder.getinitializationerrorstring());
    builder.seta(1);
    assertfalse(abstractbuilder.isinitialized());
    assertequals("b, c", abstractbuilder.getinitializationerrorstring());
    builder.setb(1);
    assertfalse(abstractbuilder.isinitialized());
    assertequals("c", abstractbuilder.getinitializationerrorstring());
    builder.setc(1);
    asserttrue(abstractbuilder.isinitialized());
    assertequals("", abstractbuilder.getinitializationerrorstring());
  }

  public void testforeignisinitialized() throws exception {
    testrequiredforeign.builder builder = testrequiredforeign.newbuilder();
    abstractmessagewrapper.builder abstractbuilder =
      new abstractmessagewrapper.builder(builder);

    asserttrue(abstractbuilder.isinitialized());
    assertequals("", abstractbuilder.getinitializationerrorstring());

    builder.setoptionalmessage(test_required_uninitialized);
    assertfalse(abstractbuilder.isinitialized());
    assertequals(
        "optional_message.a, optional_message.b, optional_message.c",
        abstractbuilder.getinitializationerrorstring());

    builder.setoptionalmessage(test_required_initialized);
    asserttrue(abstractbuilder.isinitialized());
    assertequals("", abstractbuilder.getinitializationerrorstring());

    builder.addrepeatedmessage(test_required_uninitialized);
    assertfalse(abstractbuilder.isinitialized());
    assertequals(
        "repeated_message[0].a, repeated_message[0].b, repeated_message[0].c",
        abstractbuilder.getinitializationerrorstring());

    builder.setrepeatedmessage(0, test_required_initialized);
    asserttrue(abstractbuilder.isinitialized());
    assertequals("", abstractbuilder.getinitializationerrorstring());
  }

  // -----------------------------------------------------------------
  // tests for mergefrom

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
    abstractmessagewrapper result =
      new abstractmessagewrapper.builder(
        testalltypes.newbuilder(merge_dest))
      .mergefrom(merge_source).build();

    assertequals(merge_result_text, result.tostring());
  }

  // -----------------------------------------------------------------
  // tests for equals and hashcode

  public void testequalsandhashcode() throws exception {
    testalltypes a = testutil.getallset();
    testalltypes b = testalltypes.newbuilder().build();
    testalltypes c = testalltypes.newbuilder(b).addrepeatedstring("x").build();
    testalltypes d = testalltypes.newbuilder(c).addrepeatedstring("y").build();
    testallextensions e = testutil.getallextensionsset();
    testallextensions f = testallextensions.newbuilder(e)
        .addextension(unittestproto.repeatedint32extension, 999).build();

    checkequalsisconsistent(a);
    checkequalsisconsistent(b);
    checkequalsisconsistent(c);
    checkequalsisconsistent(d);
    checkequalsisconsistent(e);
    checkequalsisconsistent(f);

    checknotequal(a, b);
    checknotequal(a, c);
    checknotequal(a, d);
    checknotequal(a, e);
    checknotequal(a, f);

    checknotequal(b, c);
    checknotequal(b, d);
    checknotequal(b, e);
    checknotequal(b, f);

    checknotequal(c, d);
    checknotequal(c, e);
    checknotequal(c, f);

    checknotequal(d, e);
    checknotequal(d, f);

    checknotequal(e, f);

    // deserializing into the testemptymessage such that every field
    // is an {@link unknownfieldset.field}.
    unittestproto.testemptymessage eunknownfields =
        unittestproto.testemptymessage.parsefrom(e.tobytearray());
    unittestproto.testemptymessage funknownfields =
        unittestproto.testemptymessage.parsefrom(f.tobytearray());
    checknotequal(eunknownfields, funknownfields);
    checkequalsisconsistent(eunknownfields);
    checkequalsisconsistent(funknownfields);

    // subsequent reconstitutions should be identical
    unittestproto.testemptymessage eunknownfields2 =
        unittestproto.testemptymessage.parsefrom(e.tobytearray());
    checkequalsisconsistent(eunknownfields, eunknownfields2);
  }


  /**
   * asserts that the given proto has symmetric equals and hashcode methods.
   */
  private void checkequalsisconsistent(message message) {
    // object should be equal to itself.
    assertequals(message, message);

    // object should be equal to a dynamic copy of itself.
    dynamicmessage dynamic = dynamicmessage.newbuilder(message).build();
    checkequalsisconsistent(message, dynamic);
  }

  /**
   * asserts that the given protos are equal and have the same hash code.
   */
  private void checkequalsisconsistent(message message1, message message2) {
    assertequals(message1, message2);
    assertequals(message2, message1);
    assertequals(message2.hashcode(), message1.hashcode());
  }

  /**
   * asserts that the given protos are not equal and have different hash codes.
   *
   * @warning it's valid for non-equal objects to have the same hash code, so
   *   this test is stricter than it needs to be. however, this should happen
   *   relatively rarely.
   */
  private void checknotequal(message m1, message m2) {
    string equalserror = string.format("%s should not be equal to %s", m1, m2);
    assertfalse(equalserror, m1.equals(m2));
    assertfalse(equalserror, m2.equals(m1));

    assertfalse(
        string.format("%s should have a different hash code from %s", m1, m2),
        m1.hashcode() == m2.hashcode());
  }
}
