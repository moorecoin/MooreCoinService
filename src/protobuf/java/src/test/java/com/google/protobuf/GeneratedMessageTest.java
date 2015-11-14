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

import com.google.protobuf.descriptors.descriptor;
import com.google.protobuf.descriptors.fielddescriptor;
import com.google.protobuf.unittestlite.testallextensionslite;
import com.google.protobuf.test.unittestimport;
import protobuf_unittest.enumwithnoouter;
import protobuf_unittest.messagewithnoouter;
import protobuf_unittest.multiplefilestestproto;
import protobuf_unittest.nestedextension.mynestedextension;
import protobuf_unittest.nestedextensionlite.mynestedextensionlite;
import protobuf_unittest.nonnestedextension;
import protobuf_unittest.nonnestedextension.messagetobeextended;
import protobuf_unittest.nonnestedextension.mynonnestedextension;
import protobuf_unittest.nonnestedextensionlite;
import protobuf_unittest.nonnestedextensionlite.messagelitetobeextended;
import protobuf_unittest.nonnestedextensionlite.mynonnestedextensionlite;
import protobuf_unittest.servicewithnoouter;
import protobuf_unittest.unittestoptimizefor.testoptimizedforsize;
import protobuf_unittest.unittestoptimizefor.testoptionaloptimizedforsize;
import protobuf_unittest.unittestoptimizefor.testrequiredoptimizedforsize;
import protobuf_unittest.unittestproto;
import protobuf_unittest.unittestproto.foreignenum;
import protobuf_unittest.unittestproto.foreignmessage;
import protobuf_unittest.unittestproto.foreignmessageorbuilder;
import protobuf_unittest.unittestproto.testallextensions;
import protobuf_unittest.unittestproto.testalltypes;
import protobuf_unittest.unittestproto.testalltypes.nestedmessage;
import protobuf_unittest.unittestproto.testalltypesorbuilder;
import protobuf_unittest.unittestproto.testextremedefaultvalues;
import protobuf_unittest.unittestproto.testpackedtypes;
import protobuf_unittest.unittestproto.testunpackedtypes;

import junit.framework.testcase;

import java.io.bytearrayinputstream;
import java.io.bytearrayoutputstream;
import java.io.objectinputstream;
import java.io.objectoutputstream;
import java.util.arrays;
import java.util.collections;
import java.util.list;

/**
 * unit test for generated messages and generated code.  see also
 * {@link messagetest}, which tests some generated message functionality.
 *
 * @author kenton@google.com kenton varda
 */
public class generatedmessagetest extends testcase {
  testutil.reflectiontester reflectiontester =
    new testutil.reflectiontester(testalltypes.getdescriptor(), null);

  public void testdefaultinstance() throws exception {
    assertsame(testalltypes.getdefaultinstance(),
               testalltypes.getdefaultinstance().getdefaultinstancefortype());
    assertsame(testalltypes.getdefaultinstance(),
               testalltypes.newbuilder().getdefaultinstancefortype());
  }

  public void testmessageorbuilder() throws exception {
    testalltypes.builder builder = testalltypes.newbuilder();
    testutil.setallfields(builder);
    testalltypes message = builder.build();
    testutil.assertallfieldsset(message);
  }

  public void testusingbuildermultipletimes() throws exception {
    testalltypes.builder builder = testalltypes.newbuilder();
    // primitive field scalar and repeated
    builder.setoptionalsfixed64(100);
    builder.addrepeatedint32(100);
    // enum field scalar and repeated
    builder.setoptionalimportenum(unittestimport.importenum.import_bar);
    builder.addrepeatedimportenum(unittestimport.importenum.import_bar);
    // proto field scalar and repeated
    builder.setoptionalforeignmessage(foreignmessage.newbuilder().setc(1));
    builder.addrepeatedforeignmessage(foreignmessage.newbuilder().setc(1));

    testalltypes value1 = builder.build();

    assertequals(100, value1.getoptionalsfixed64());
    assertequals(100, value1.getrepeatedint32(0));
    assertequals(unittestimport.importenum.import_bar,
        value1.getoptionalimportenum());
    assertequals(unittestimport.importenum.import_bar,
        value1.getrepeatedimportenum(0));
    assertequals(1, value1.getoptionalforeignmessage().getc());
    assertequals(1, value1.getrepeatedforeignmessage(0).getc());

    // make sure that builder didn't update previously created values
    builder.setoptionalsfixed64(200);
    builder.setrepeatedint32(0, 200);
    builder.setoptionalimportenum(unittestimport.importenum.import_foo);
    builder.setrepeatedimportenum(0, unittestimport.importenum.import_foo);
    builder.setoptionalforeignmessage(foreignmessage.newbuilder().setc(2));
    builder.setrepeatedforeignmessage(0, foreignmessage.newbuilder().setc(2));

    testalltypes value2 = builder.build();

    // make sure value1 didn't change.
    assertequals(100, value1.getoptionalsfixed64());
    assertequals(100, value1.getrepeatedint32(0));
    assertequals(unittestimport.importenum.import_bar,
        value1.getoptionalimportenum());
    assertequals(unittestimport.importenum.import_bar,
        value1.getrepeatedimportenum(0));
    assertequals(1, value1.getoptionalforeignmessage().getc());
    assertequals(1, value1.getrepeatedforeignmessage(0).getc());

    // make sure value2 is correct
    assertequals(200, value2.getoptionalsfixed64());
    assertequals(200, value2.getrepeatedint32(0));
    assertequals(unittestimport.importenum.import_foo,
        value2.getoptionalimportenum());
    assertequals(unittestimport.importenum.import_foo,
        value2.getrepeatedimportenum(0));
    assertequals(2, value2.getoptionalforeignmessage().getc());
    assertequals(2, value2.getrepeatedforeignmessage(0).getc());
  }

  public void testprotossharerepeatedarraysifdidntchange() throws exception {
    testalltypes.builder builder = testalltypes.newbuilder();
    builder.addrepeatedint32(100);
    builder.addrepeatedimportenum(unittestimport.importenum.import_bar);
    builder.addrepeatedforeignmessage(foreignmessage.getdefaultinstance());

    testalltypes value1 = builder.build();
    testalltypes value2 = value1.tobuilder().build();

    assertsame(value1.getrepeatedint32list(), value2.getrepeatedint32list());
    assertsame(value1.getrepeatedimportenumlist(),
        value2.getrepeatedimportenumlist());
    assertsame(value1.getrepeatedforeignmessagelist(),
        value2.getrepeatedforeignmessagelist());
  }

  public void testrepeatedarraysareimmutable() throws exception {
    testalltypes.builder builder = testalltypes.newbuilder();
    builder.addrepeatedint32(100);
    builder.addrepeatedimportenum(unittestimport.importenum.import_bar);
    builder.addrepeatedforeignmessage(foreignmessage.getdefaultinstance());
    assertisunmodifiable(builder.getrepeatedint32list());
    assertisunmodifiable(builder.getrepeatedimportenumlist());
    assertisunmodifiable(builder.getrepeatedforeignmessagelist());
    assertisunmodifiable(builder.getrepeatedfloatlist());


    testalltypes value = builder.build();
    assertisunmodifiable(value.getrepeatedint32list());
    assertisunmodifiable(value.getrepeatedimportenumlist());
    assertisunmodifiable(value.getrepeatedforeignmessagelist());
    assertisunmodifiable(value.getrepeatedfloatlist());
  }

  public void testparsedmessagesareimmutable() throws exception {
    testalltypes value = testalltypes.parser.parsefrom(
        testutil.getallset().tobytestring());
    assertisunmodifiable(value.getrepeatedint32list());
    assertisunmodifiable(value.getrepeatedint64list());
    assertisunmodifiable(value.getrepeateduint32list());
    assertisunmodifiable(value.getrepeateduint64list());
    assertisunmodifiable(value.getrepeatedsint32list());
    assertisunmodifiable(value.getrepeatedsint64list());
    assertisunmodifiable(value.getrepeatedfixed32list());
    assertisunmodifiable(value.getrepeatedfixed64list());
    assertisunmodifiable(value.getrepeatedsfixed32list());
    assertisunmodifiable(value.getrepeatedsfixed64list());
    assertisunmodifiable(value.getrepeatedfloatlist());
    assertisunmodifiable(value.getrepeateddoublelist());
    assertisunmodifiable(value.getrepeatedboollist());
    assertisunmodifiable(value.getrepeatedstringlist());
    assertisunmodifiable(value.getrepeatedbyteslist());
    assertisunmodifiable(value.getrepeatedgrouplist());
    assertisunmodifiable(value.getrepeatednestedmessagelist());
    assertisunmodifiable(value.getrepeatedforeignmessagelist());
    assertisunmodifiable(value.getrepeatedimportmessagelist());
    assertisunmodifiable(value.getrepeatednestedenumlist());
    assertisunmodifiable(value.getrepeatedforeignenumlist());
    assertisunmodifiable(value.getrepeatedimportenumlist());
  }

  private void assertisunmodifiable(list<?> list) {
    if (list == collections.emptylist()) {
      // okay -- need to check this b/c emptylist allows you to call clear.
    } else {
      try {
        list.clear();
        fail("list wasn't immutable");
      } catch (unsupportedoperationexception e) {
        // good
      }
    }
  }

  public void testsettersrejectnull() throws exception {
    testalltypes.builder builder = testalltypes.newbuilder();
    try {
      builder.setoptionalstring(null);
      fail("exception was not thrown");
    } catch (nullpointerexception e) {
      // we expect this exception.
    }
    try {
      builder.setoptionalbytes(null);
      fail("exception was not thrown");
    } catch (nullpointerexception e) {
      // we expect this exception.
    }
    try {
      builder.setoptionalnestedmessage((testalltypes.nestedmessage) null);
      fail("exception was not thrown");
    } catch (nullpointerexception e) {
      // we expect this exception.
    }
    try {
      builder.setoptionalnestedmessage(
          (testalltypes.nestedmessage.builder) null);
      fail("exception was not thrown");
    } catch (nullpointerexception e) {
      // we expect this exception.
    }
    try {
      builder.setoptionalnestedenum(null);
      fail("exception was not thrown");
    } catch (nullpointerexception e) {
      // we expect this exception.
    }
    try {
      builder.addrepeatedstring(null);
      fail("exception was not thrown");
    } catch (nullpointerexception e) {
      // we expect this exception.
    }
    try {
      builder.addrepeatedbytes(null);
      fail("exception was not thrown");
    } catch (nullpointerexception e) {
      // we expect this exception.
    }
    try {
      builder.addrepeatednestedmessage((testalltypes.nestedmessage) null);
      fail("exception was not thrown");
    } catch (nullpointerexception e) {
      // we expect this exception.
    }
    try {
      builder.addrepeatednestedmessage(
          (testalltypes.nestedmessage.builder) null);
      fail("exception was not thrown");
    } catch (nullpointerexception e) {
      // we expect this exception.
    }
    try {
      builder.addrepeatednestedenum(null);
      fail("exception was not thrown");
    } catch (nullpointerexception e) {
      // we expect this exception.
    }
  }

  public void testrepeatedsetters() throws exception {
    testalltypes.builder builder = testalltypes.newbuilder();
    testutil.setallfields(builder);
    testutil.modifyrepeatedfields(builder);
    testalltypes message = builder.build();
    testutil.assertrepeatedfieldsmodified(message);
  }

  public void testrepeatedsettersrejectnull() throws exception {
    testalltypes.builder builder = testalltypes.newbuilder();

    builder.addrepeatedstring("one");
    builder.addrepeatedstring("two");
    try {
      builder.setrepeatedstring(1, null);
      fail("exception was not thrown");
    } catch (nullpointerexception e) {
      // we expect this exception.
    }

    builder.addrepeatedbytes(testutil.tobytes("one"));
    builder.addrepeatedbytes(testutil.tobytes("two"));
    try {
      builder.setrepeatedbytes(1, null);
      fail("exception was not thrown");
    } catch (nullpointerexception e) {
      // we expect this exception.
    }

    builder.addrepeatednestedmessage(
      testalltypes.nestedmessage.newbuilder().setbb(218).build());
    builder.addrepeatednestedmessage(
      testalltypes.nestedmessage.newbuilder().setbb(456).build());
    try {
      builder.setrepeatednestedmessage(1, (testalltypes.nestedmessage) null);
      fail("exception was not thrown");
    } catch (nullpointerexception e) {
      // we expect this exception.
    }
    try {
      builder.setrepeatednestedmessage(
          1, (testalltypes.nestedmessage.builder) null);
      fail("exception was not thrown");
    } catch (nullpointerexception e) {
      // we expect this exception.
    }

    builder.addrepeatednestedenum(testalltypes.nestedenum.foo);
    builder.addrepeatednestedenum(testalltypes.nestedenum.bar);
    try {
      builder.setrepeatednestedenum(1, null);
      fail("exception was not thrown");
    } catch (nullpointerexception e) {
      // we expect this exception.
    }
  }

  public void testrepeatedappend() throws exception {
    testalltypes.builder builder = testalltypes.newbuilder();

    builder.addallrepeatedint32(arrays.aslist(1, 2, 3, 4));
    builder.addallrepeatedforeignenum(arrays.aslist(foreignenum.foreign_baz));

    foreignmessage foreignmessage =
        foreignmessage.newbuilder().setc(12).build();
    builder.addallrepeatedforeignmessage(arrays.aslist(foreignmessage));

    testalltypes message = builder.build();
    assertequals(message.getrepeatedint32list(), arrays.aslist(1, 2, 3, 4));
    assertequals(message.getrepeatedforeignenumlist(),
        arrays.aslist(foreignenum.foreign_baz));
    assertequals(1, message.getrepeatedforeignmessagecount());
    assertequals(12, message.getrepeatedforeignmessage(0).getc());
  }

  public void testrepeatedappendrejectsnull() throws exception {
    testalltypes.builder builder = testalltypes.newbuilder();

    foreignmessage foreignmessage =
        foreignmessage.newbuilder().setc(12).build();
    try {
      builder.addallrepeatedforeignmessage(
          arrays.aslist(foreignmessage, (foreignmessage) null));
      fail("exception was not thrown");
    } catch (nullpointerexception e) {
      // we expect this exception.
    }

    try {
      builder.addallrepeatedforeignenum(
          arrays.aslist(foreignenum.foreign_baz, null));
      fail("exception was not thrown");
    } catch (nullpointerexception e) {
      // we expect this exception.
    }

    try {
      builder.addallrepeatedstring(arrays.aslist("one", null));
      fail("exception was not thrown");
    } catch (nullpointerexception e) {
      // we expect this exception.
    }

    try {
      builder.addallrepeatedbytes(arrays.aslist(testutil.tobytes("one"), null));
      fail("exception was not thrown");
    } catch (nullpointerexception e) {
      // we expect this exception.
    }
  }

  public void testsettingforeignmessageusingbuilder() throws exception {
    testalltypes message = testalltypes.newbuilder()
        // pass builder for foreign message instance.
        .setoptionalforeignmessage(foreignmessage.newbuilder().setc(123))
        .build();
    testalltypes expectedmessage = testalltypes.newbuilder()
        // create expected version passing foreign message instance explicitly.
        .setoptionalforeignmessage(
            foreignmessage.newbuilder().setc(123).build())
        .build();
    // todo(ngd): upgrade to using real #equals method once implemented
    assertequals(expectedmessage.tostring(), message.tostring());
  }

  public void testsettingrepeatedforeignmessageusingbuilder() throws exception {
    testalltypes message = testalltypes.newbuilder()
        // pass builder for foreign message instance.
        .addrepeatedforeignmessage(foreignmessage.newbuilder().setc(456))
        .build();
    testalltypes expectedmessage = testalltypes.newbuilder()
        // create expected version passing foreign message instance explicitly.
        .addrepeatedforeignmessage(
            foreignmessage.newbuilder().setc(456).build())
        .build();
    assertequals(expectedmessage.tostring(), message.tostring());
  }

  public void testdefaults() throws exception {
    testutil.assertclear(testalltypes.getdefaultinstance());
    testutil.assertclear(testalltypes.newbuilder().build());

    testextremedefaultvalues message =
        testextremedefaultvalues.getdefaultinstance();
    assertequals("\u1234", message.getutf8string());
    assertequals(double.positive_infinity, message.getinfdouble());
    assertequals(double.negative_infinity, message.getneginfdouble());
    asserttrue(double.isnan(message.getnandouble()));
    assertequals(float.positive_infinity, message.getinffloat());
    assertequals(float.negative_infinity, message.getneginffloat());
    asserttrue(float.isnan(message.getnanfloat()));
    assertequals("? ? ?? ?? ??? ??/ ??-", message.getcpptrigraph());
  }

  public void testclear() throws exception {
    testalltypes.builder builder = testalltypes.newbuilder();
    testutil.assertclear(builder);
    testutil.setallfields(builder);
    builder.clear();
    testutil.assertclear(builder);
  }

  public void testreflectiongetters() throws exception {
    testalltypes.builder builder = testalltypes.newbuilder();
    testutil.setallfields(builder);
    reflectiontester.assertallfieldssetviareflection(builder);

    testalltypes message = builder.build();
    reflectiontester.assertallfieldssetviareflection(message);
  }

  public void testreflectionsetters() throws exception {
    testalltypes.builder builder = testalltypes.newbuilder();
    reflectiontester.setallfieldsviareflection(builder);
    testutil.assertallfieldsset(builder);

    testalltypes message = builder.build();
    testutil.assertallfieldsset(message);
  }

  public void testreflectionsettersrejectnull() throws exception {
    testalltypes.builder builder = testalltypes.newbuilder();
    reflectiontester.assertreflectionsettersrejectnull(builder);
  }

  public void testreflectionrepeatedsetters() throws exception {
    testalltypes.builder builder = testalltypes.newbuilder();
    reflectiontester.setallfieldsviareflection(builder);
    reflectiontester.modifyrepeatedfieldsviareflection(builder);
    testutil.assertrepeatedfieldsmodified(builder);

    testalltypes message = builder.build();
    testutil.assertrepeatedfieldsmodified(message);
  }

  public void testreflectionrepeatedsettersrejectnull() throws exception {
    testalltypes.builder builder = testalltypes.newbuilder();
    reflectiontester.assertreflectionrepeatedsettersrejectnull(builder);
  }

  public void testreflectiondefaults() throws exception {
    reflectiontester.assertclearviareflection(
      testalltypes.getdefaultinstance());
    reflectiontester.assertclearviareflection(
      testalltypes.newbuilder().build());
  }

  public void testenuminterface() throws exception {
    asserttrue(testalltypes.getdefaultinstance().getdefaultnestedenum()
        instanceof protocolmessageenum);
  }

  public void testenummap() throws exception {
    internal.enumlitemap<foreignenum> map = foreignenum.internalgetvaluemap();

    for (foreignenum value : foreignenum.values()) {
      assertequals(value, map.findvaluebynumber(value.getnumber()));
    }

    asserttrue(map.findvaluebynumber(12345) == null);
  }

  public void testparsepackedtounpacked() throws exception {
    testunpackedtypes.builder builder = testunpackedtypes.newbuilder();
    testunpackedtypes message =
      builder.mergefrom(testutil.getpackedset().tobytestring()).build();
    testutil.assertunpackedfieldsset(message);
  }

  public void testparseunpackedtopacked() throws exception {
    testpackedtypes.builder builder = testpackedtypes.newbuilder();
    testpackedtypes message =
      builder.mergefrom(testutil.getunpackedset().tobytestring()).build();
    testutil.assertpackedfieldsset(message);
  }

  // =================================================================
  // extensions.

  testutil.reflectiontester extensionsreflectiontester =
    new testutil.reflectiontester(testallextensions.getdescriptor(),
                                  testutil.getextensionregistry());

  public void testextensionmessageorbuilder() throws exception {
    testallextensions.builder builder = testallextensions.newbuilder();
    testutil.setallextensions(builder);
    testallextensions message = builder.build();
    testutil.assertallextensionsset(message);
  }

  public void testextensionrepeatedsetters() throws exception {
    testallextensions.builder builder = testallextensions.newbuilder();
    testutil.setallextensions(builder);
    testutil.modifyrepeatedextensions(builder);
    testallextensions message = builder.build();
    testutil.assertrepeatedextensionsmodified(message);
  }

  public void testextensiondefaults() throws exception {
    testutil.assertextensionsclear(testallextensions.getdefaultinstance());
    testutil.assertextensionsclear(testallextensions.newbuilder().build());
  }

  public void testextensionreflectiongetters() throws exception {
    testallextensions.builder builder = testallextensions.newbuilder();
    testutil.setallextensions(builder);
    extensionsreflectiontester.assertallfieldssetviareflection(builder);

    testallextensions message = builder.build();
    extensionsreflectiontester.assertallfieldssetviareflection(message);
  }

  public void testextensionreflectionsetters() throws exception {
    testallextensions.builder builder = testallextensions.newbuilder();
    extensionsreflectiontester.setallfieldsviareflection(builder);
    testutil.assertallextensionsset(builder);

    testallextensions message = builder.build();
    testutil.assertallextensionsset(message);
  }

  public void testextensionreflectionsettersrejectnull() throws exception {
    testallextensions.builder builder = testallextensions.newbuilder();
    extensionsreflectiontester.assertreflectionsettersrejectnull(builder);
  }

  public void testextensionreflectionrepeatedsetters() throws exception {
    testallextensions.builder builder = testallextensions.newbuilder();
    extensionsreflectiontester.setallfieldsviareflection(builder);
    extensionsreflectiontester.modifyrepeatedfieldsviareflection(builder);
    testutil.assertrepeatedextensionsmodified(builder);

    testallextensions message = builder.build();
    testutil.assertrepeatedextensionsmodified(message);
  }

  public void testextensionreflectionrepeatedsettersrejectnull()
      throws exception {
    testallextensions.builder builder = testallextensions.newbuilder();
    extensionsreflectiontester.assertreflectionrepeatedsettersrejectnull(
        builder);
  }

  public void testextensionreflectiondefaults() throws exception {
    extensionsreflectiontester.assertclearviareflection(
      testallextensions.getdefaultinstance());
    extensionsreflectiontester.assertclearviareflection(
      testallextensions.newbuilder().build());
  }

  public void testclearextension() throws exception {
    // clearextension() is not actually used in testutil, so try it manually.
    assertfalse(
      testallextensions.newbuilder()
        .setextension(unittestproto.optionalint32extension, 1)
        .clearextension(unittestproto.optionalint32extension)
        .hasextension(unittestproto.optionalint32extension));
    assertequals(0,
      testallextensions.newbuilder()
        .addextension(unittestproto.repeatedint32extension, 1)
        .clearextension(unittestproto.repeatedint32extension)
        .getextensioncount(unittestproto.repeatedint32extension));
  }

  public void testextensioncopy() throws exception {
    testallextensions original = testutil.getallextensionsset();
    testallextensions copy = testallextensions.newbuilder(original).build();
    testutil.assertallextensionsset(copy);
  }

  public void testextensionmergefrom() throws exception {
    testallextensions original =
      testallextensions.newbuilder()
        .setextension(unittestproto.optionalint32extension, 1).build();
    testallextensions merged =
        testallextensions.newbuilder().mergefrom(original).build();
    asserttrue(merged.hasextension(unittestproto.optionalint32extension));
    assertequals(
        1, (int) merged.getextension(unittestproto.optionalint32extension));
  }

  // =================================================================
  // lite extensions.

  // we test lite extensions directly because they have a separate
  // implementation from full extensions.  in contrast, we do not test
  // lite fields directly since they are implemented exactly the same as
  // regular fields.

  public void testliteextensionmessageorbuilder() throws exception {
    testallextensionslite.builder builder = testallextensionslite.newbuilder();
    testutil.setallextensions(builder);
    testutil.assertallextensionsset(builder);

    testallextensionslite message = builder.build();
    testutil.assertallextensionsset(message);
  }

  public void testliteextensionrepeatedsetters() throws exception {
    testallextensionslite.builder builder = testallextensionslite.newbuilder();
    testutil.setallextensions(builder);
    testutil.modifyrepeatedextensions(builder);
    testutil.assertrepeatedextensionsmodified(builder);

    testallextensionslite message = builder.build();
    testutil.assertrepeatedextensionsmodified(message);
  }

  public void testliteextensiondefaults() throws exception {
    testutil.assertextensionsclear(testallextensionslite.getdefaultinstance());
    testutil.assertextensionsclear(testallextensionslite.newbuilder().build());
  }

  public void testclearliteextension() throws exception {
    // clearextension() is not actually used in testutil, so try it manually.
    assertfalse(
      testallextensionslite.newbuilder()
        .setextension(unittestlite.optionalint32extensionlite, 1)
        .clearextension(unittestlite.optionalint32extensionlite)
        .hasextension(unittestlite.optionalint32extensionlite));
    assertequals(0,
      testallextensionslite.newbuilder()
        .addextension(unittestlite.repeatedint32extensionlite, 1)
        .clearextension(unittestlite.repeatedint32extensionlite)
        .getextensioncount(unittestlite.repeatedint32extensionlite));
  }

  public void testliteextensioncopy() throws exception {
    testallextensionslite original = testutil.getallliteextensionsset();
    testallextensionslite copy =
        testallextensionslite.newbuilder(original).build();
    testutil.assertallextensionsset(copy);
  }

  public void testliteextensionmergefrom() throws exception {
    testallextensionslite original =
      testallextensionslite.newbuilder()
        .setextension(unittestlite.optionalint32extensionlite, 1).build();
    testallextensionslite merged =
        testallextensionslite.newbuilder().mergefrom(original).build();
    asserttrue(merged.hasextension(unittestlite.optionalint32extensionlite));
    assertequals(
        1, (int) merged.getextension(unittestlite.optionalint32extensionlite));
  }

  // =================================================================
  // multiple_files_test

  public void testmultiplefilesoption() throws exception {
    // we mostly just want to check that things compile.
    messagewithnoouter message =
      messagewithnoouter.newbuilder()
        .setnested(messagewithnoouter.nestedmessage.newbuilder().seti(1))
        .addforeign(testalltypes.newbuilder().setoptionalint32(1))
        .setnestedenum(messagewithnoouter.nestedenum.baz)
        .setforeignenum(enumwithnoouter.bar)
        .build();
    assertequals(message, messagewithnoouter.parsefrom(message.tobytestring()));

    assertequals(multiplefilestestproto.getdescriptor(),
                 messagewithnoouter.getdescriptor().getfile());

    descriptors.fielddescriptor field =
      messagewithnoouter.getdescriptor().findfieldbyname("foreign_enum");
    assertequals(enumwithnoouter.bar.getvaluedescriptor(),
                 message.getfield(field));

    assertequals(multiplefilestestproto.getdescriptor(),
                 servicewithnoouter.getdescriptor().getfile());

    assertfalse(
      testallextensions.getdefaultinstance().hasextension(
        multiplefilestestproto.extensionwithouter));
  }

  public void testoptionalfieldwithrequiredsubfieldsoptimizedforsize()
    throws exception {
    testoptionaloptimizedforsize message =
        testoptionaloptimizedforsize.getdefaultinstance();
    asserttrue(message.isinitialized());

    message = testoptionaloptimizedforsize.newbuilder().seto(
        testrequiredoptimizedforsize.newbuilder().buildpartial()
        ).buildpartial();
    assertfalse(message.isinitialized());

    message = testoptionaloptimizedforsize.newbuilder().seto(
        testrequiredoptimizedforsize.newbuilder().setx(5).buildpartial()
        ).buildpartial();
    asserttrue(message.isinitialized());
  }

  public void testuninitializedextensioninoptimizedforsize()
      throws exception {
    testoptimizedforsize.builder builder = testoptimizedforsize.newbuilder();
    builder.setextension(testoptimizedforsize.testextension2,
        testrequiredoptimizedforsize.newbuilder().buildpartial());
    assertfalse(builder.isinitialized());
    assertfalse(builder.buildpartial().isinitialized());

    builder = testoptimizedforsize.newbuilder();
    builder.setextension(testoptimizedforsize.testextension2,
        testrequiredoptimizedforsize.newbuilder().setx(10).buildpartial());
    asserttrue(builder.isinitialized());
    asserttrue(builder.buildpartial().isinitialized());
  }

  public void testtobuilder() throws exception {
    testalltypes.builder builder = testalltypes.newbuilder();
    testutil.setallfields(builder);
    testalltypes message = builder.build();
    testutil.assertallfieldsset(message);
    testutil.assertallfieldsset(message.tobuilder().build());
  }

  public void testfieldconstantvalues() throws exception {
    assertequals(testalltypes.nestedmessage.bb_field_number, 1);
    assertequals(testalltypes.optional_int32_field_number, 1);
    assertequals(testalltypes.optionalgroup_field_number, 16);
    assertequals(testalltypes.optional_nested_message_field_number, 18);
    assertequals(testalltypes.optional_nested_enum_field_number, 21);
    assertequals(testalltypes.repeated_int32_field_number, 31);
    assertequals(testalltypes.repeatedgroup_field_number, 46);
    assertequals(testalltypes.repeated_nested_message_field_number, 48);
    assertequals(testalltypes.repeated_nested_enum_field_number, 51);
  }

  public void testextensionconstantvalues() throws exception {
    assertequals(unittestproto.testrequired.single_field_number, 1000);
    assertequals(unittestproto.testrequired.multi_field_number, 1001);
    assertequals(unittestproto.optional_int32_extension_field_number, 1);
    assertequals(unittestproto.optionalgroup_extension_field_number, 16);
    assertequals(
      unittestproto.optional_nested_message_extension_field_number, 18);
    assertequals(unittestproto.optional_nested_enum_extension_field_number, 21);
    assertequals(unittestproto.repeated_int32_extension_field_number, 31);
    assertequals(unittestproto.repeatedgroup_extension_field_number, 46);
    assertequals(
      unittestproto.repeated_nested_message_extension_field_number, 48);
    assertequals(unittestproto.repeated_nested_enum_extension_field_number, 51);
  }

  public void testrecursivemessagedefaultinstance() throws exception {
    unittestproto.testrecursivemessage message =
        unittestproto.testrecursivemessage.getdefaultinstance();
    asserttrue(message != null);
    asserttrue(message.geta() != null);
    asserttrue(message.geta() == message);
  }

  public void testserialize() throws exception {
    bytearrayoutputstream baos = new bytearrayoutputstream();
    testalltypes.builder builder = testalltypes.newbuilder();
    testutil.setallfields(builder);
    testalltypes expected = builder.build();
    objectoutputstream out = new objectoutputstream(baos);
    try {
      out.writeobject(expected);
    } finally {
      out.close();
    }
    bytearrayinputstream bais = new bytearrayinputstream(baos.tobytearray());
    objectinputstream in = new objectinputstream(bais);
    testalltypes actual = (testalltypes) in.readobject();
    assertequals(expected, actual);
  }

  public void testserializepartial() throws exception {
    bytearrayoutputstream baos = new bytearrayoutputstream();
    testalltypes.builder builder = testalltypes.newbuilder();
    testalltypes expected = builder.buildpartial();
    objectoutputstream out = new objectoutputstream(baos);
    try {
      out.writeobject(expected);
    } finally {
      out.close();
    }
    bytearrayinputstream bais = new bytearrayinputstream(baos.tobytearray());
    objectinputstream in = new objectinputstream(bais);
    testalltypes actual = (testalltypes) in.readobject();
    assertequals(expected, actual);
  }

  public void testenumvalues() {
     assertequals(
         testalltypes.nestedenum.bar.getnumber(),
         testalltypes.nestedenum.bar_value);
    assertequals(
        testalltypes.nestedenum.baz.getnumber(),
        testalltypes.nestedenum.baz_value);
    assertequals(
        testalltypes.nestedenum.foo.getnumber(),
        testalltypes.nestedenum.foo_value);
  }

  public void testnonnestedextensioninitialization() {
    asserttrue(nonnestedextension.nonnestedextension
               .getmessagedefaultinstance() instanceof mynonnestedextension);
    assertequals("nonnestedextension",
                 nonnestedextension.nonnestedextension.getdescriptor().getname());
  }

  public void testnestedextensioninitialization() {
    asserttrue(mynestedextension.recursiveextension.getmessagedefaultinstance()
               instanceof messagetobeextended);
    assertequals("recursiveextension",
                 mynestedextension.recursiveextension.getdescriptor().getname());
  }

  public void testnonnestedextensionliteinitialization() {
    asserttrue(nonnestedextensionlite.nonnestedextensionlite
               .getmessagedefaultinstance() instanceof mynonnestedextensionlite);
  }

  public void testnestedextensionliteinitialization() {
    asserttrue(mynestedextensionlite.recursiveextensionlite
               .getmessagedefaultinstance() instanceof messagelitetobeextended);
  }

  public void testinvalidations() throws exception {
    generatedmessage.enablealwaysusefieldbuildersfortesting();
    testalltypes.nestedmessage nestedmessage1 =
        testalltypes.nestedmessage.newbuilder().build();
    testalltypes.nestedmessage nestedmessage2 =
        testalltypes.nestedmessage.newbuilder().build();

    // set all three flavors (enum, primitive, message and singular/repeated)
    // and verify no invalidations fired
    testutil.mockbuilderparent mockparent = new testutil.mockbuilderparent();

    testalltypes.builder builder = (testalltypes.builder)
        ((generatedmessage) testalltypes.getdefaultinstance()).
            newbuilderfortype(mockparent);
    builder.setoptionalint32(1);
    builder.setoptionalnestedenum(testalltypes.nestedenum.bar);
    builder.setoptionalnestedmessage(nestedmessage1);
    builder.addrepeatedint32(1);
    builder.addrepeatednestedenum(testalltypes.nestedenum.bar);
    builder.addrepeatednestedmessage(nestedmessage1);
    assertequals(0, mockparent.getinvalidationcount());

    // now tell it we want changes and make sure it's only fired once
    // and do this for each flavor

    // primitive single
    builder.buildpartial();
    builder.setoptionalint32(2);
    builder.setoptionalint32(3);
    assertequals(1, mockparent.getinvalidationcount());

    // enum single
    builder.buildpartial();
    builder.setoptionalnestedenum(testalltypes.nestedenum.baz);
    builder.setoptionalnestedenum(testalltypes.nestedenum.bar);
    assertequals(2, mockparent.getinvalidationcount());

    // message single
    builder.buildpartial();
    builder.setoptionalnestedmessage(nestedmessage2);
    builder.setoptionalnestedmessage(nestedmessage1);
    assertequals(3, mockparent.getinvalidationcount());

    // primitive repeated
    builder.buildpartial();
    builder.addrepeatedint32(2);
    builder.addrepeatedint32(3);
    assertequals(4, mockparent.getinvalidationcount());

    // enum repeated
    builder.buildpartial();
    builder.addrepeatednestedenum(testalltypes.nestedenum.baz);
    builder.addrepeatednestedenum(testalltypes.nestedenum.baz);
    assertequals(5, mockparent.getinvalidationcount());

    // message repeated
    builder.buildpartial();
    builder.addrepeatednestedmessage(nestedmessage2);
    builder.addrepeatednestedmessage(nestedmessage1);
    assertequals(6, mockparent.getinvalidationcount());

  }

  public void testinvalidations_extensions() throws exception {
    testutil.mockbuilderparent mockparent = new testutil.mockbuilderparent();

    testallextensions.builder builder = (testallextensions.builder)
        ((generatedmessage) testallextensions.getdefaultinstance()).
            newbuilderfortype(mockparent);

    builder.addextension(unittestproto.repeatedint32extension, 1);
    builder.setextension(unittestproto.repeatedint32extension, 0, 2);
    builder.clearextension(unittestproto.repeatedint32extension);
    assertequals(0, mockparent.getinvalidationcount());

    // now tell it we want changes and make sure it's only fired once
    builder.buildpartial();
    builder.addextension(unittestproto.repeatedint32extension, 2);
    builder.addextension(unittestproto.repeatedint32extension, 3);
    assertequals(1, mockparent.getinvalidationcount());

    builder.buildpartial();
    builder.setextension(unittestproto.repeatedint32extension, 0, 4);
    builder.setextension(unittestproto.repeatedint32extension, 1, 5);
    assertequals(2, mockparent.getinvalidationcount());

    builder.buildpartial();
    builder.clearextension(unittestproto.repeatedint32extension);
    builder.clearextension(unittestproto.repeatedint32extension);
    assertequals(3, mockparent.getinvalidationcount());
  }

  public void testbasemessageorbuilder() {
    // mostly just makes sure the base interface exists and has some methods.
    testalltypes.builder builder = testalltypes.newbuilder();
    testalltypes message = builder.buildpartial();
    testalltypesorbuilder builderasinterface = (testalltypesorbuilder) builder;
    testalltypesorbuilder messageasinterface = (testalltypesorbuilder) message;

    assertequals(
        messageasinterface.getdefaultbool(),
        messageasinterface.getdefaultbool());
    assertequals(
        messageasinterface.getoptionaldouble(),
        messageasinterface.getoptionaldouble());
  }

  public void testmessageorbuildergetters() {
    testalltypes.builder builder = testalltypes.newbuilder();

    // single fields
    assertsame(foreignmessage.getdefaultinstance(),
        builder.getoptionalforeignmessageorbuilder());
    foreignmessage.builder subbuilder =
        builder.getoptionalforeignmessagebuilder();
    assertsame(subbuilder, builder.getoptionalforeignmessageorbuilder());

    // repeated fields
    foreignmessage m0 = foreignmessage.newbuilder().buildpartial();
    foreignmessage m1 = foreignmessage.newbuilder().buildpartial();
    foreignmessage m2 = foreignmessage.newbuilder().buildpartial();
    builder.addrepeatedforeignmessage(m0);
    builder.addrepeatedforeignmessage(m1);
    builder.addrepeatedforeignmessage(m2);
    assertsame(m0, builder.getrepeatedforeignmessageorbuilder(0));
    assertsame(m1, builder.getrepeatedforeignmessageorbuilder(1));
    assertsame(m2, builder.getrepeatedforeignmessageorbuilder(2));
    foreignmessage.builder b0 = builder.getrepeatedforeignmessagebuilder(0);
    foreignmessage.builder b1 = builder.getrepeatedforeignmessagebuilder(1);
    assertsame(b0, builder.getrepeatedforeignmessageorbuilder(0));
    assertsame(b1, builder.getrepeatedforeignmessageorbuilder(1));
    assertsame(m2, builder.getrepeatedforeignmessageorbuilder(2));

    list<? extends foreignmessageorbuilder> messageorbuilderlist =
        builder.getrepeatedforeignmessageorbuilderlist();
    assertsame(b0, messageorbuilderlist.get(0));
    assertsame(b1, messageorbuilderlist.get(1));
    assertsame(m2, messageorbuilderlist.get(2));
  }

  public void testgetfieldbuilder() {
    descriptor descriptor = testalltypes.getdescriptor();

    fielddescriptor fielddescriptor =
        descriptor.findfieldbyname("optional_nested_message");
    fielddescriptor foreignfielddescriptor =
        descriptor.findfieldbyname("optional_foreign_message");
    fielddescriptor importfielddescriptor =
        descriptor.findfieldbyname("optional_import_message");

    // mutate the message with new field builder
    // mutate nested message
    testalltypes.builder builder1 = testalltypes.newbuilder();
    message.builder fieldbuilder1 = builder1.newbuilderforfield(fielddescriptor)
        .mergefrom((message) builder1.getfield(fielddescriptor));
    fielddescriptor subfielddescriptor1 =
        fieldbuilder1.getdescriptorfortype().findfieldbyname("bb");
    fieldbuilder1.setfield(subfielddescriptor1, 1);
    builder1.setfield(fielddescriptor, fieldbuilder1.build());

    // mutate foreign message
    message.builder foreignfieldbuilder1 = builder1.newbuilderforfield(
        foreignfielddescriptor)
        .mergefrom((message) builder1.getfield(foreignfielddescriptor));
    fielddescriptor subforeignfielddescriptor1 =
        foreignfieldbuilder1.getdescriptorfortype().findfieldbyname("c");
    foreignfieldbuilder1.setfield(subforeignfielddescriptor1, 2);
    builder1.setfield(foreignfielddescriptor, foreignfieldbuilder1.build());

    // mutate import message
    message.builder importfieldbuilder1 = builder1.newbuilderforfield(
        importfielddescriptor)
        .mergefrom((message) builder1.getfield(importfielddescriptor));
    fielddescriptor subimportfielddescriptor1 =
        importfieldbuilder1.getdescriptorfortype().findfieldbyname("d");
    importfieldbuilder1.setfield(subimportfielddescriptor1, 3);
    builder1.setfield(importfielddescriptor, importfieldbuilder1.build());

    message newmessage1 = builder1.build();

    // mutate the message with existing field builder
    // mutate nested message
    testalltypes.builder builder2 = testalltypes.newbuilder();
    message.builder fieldbuilder2 = builder2.getfieldbuilder(fielddescriptor);
    fielddescriptor subfielddescriptor2 =
        fieldbuilder2.getdescriptorfortype().findfieldbyname("bb");
    fieldbuilder2.setfield(subfielddescriptor2, 1);
    builder2.setfield(fielddescriptor, fieldbuilder2.build());

    // mutate foreign message
    message.builder foreignfieldbuilder2 = builder2.newbuilderforfield(
        foreignfielddescriptor)
        .mergefrom((message) builder2.getfield(foreignfielddescriptor));
    fielddescriptor subforeignfielddescriptor2 =
        foreignfieldbuilder2.getdescriptorfortype().findfieldbyname("c");
    foreignfieldbuilder2.setfield(subforeignfielddescriptor2, 2);
    builder2.setfield(foreignfielddescriptor, foreignfieldbuilder2.build());

    // mutate import message
    message.builder importfieldbuilder2 = builder2.newbuilderforfield(
        importfielddescriptor)
        .mergefrom((message) builder2.getfield(importfielddescriptor));
    fielddescriptor subimportfielddescriptor2 =
        importfieldbuilder2.getdescriptorfortype().findfieldbyname("d");
    importfieldbuilder2.setfield(subimportfielddescriptor2, 3);
    builder2.setfield(importfielddescriptor, importfieldbuilder2.build());

    message newmessage2 = builder2.build();

    // these two messages should be equal.
    assertequals(newmessage1, newmessage2);
  }

  public void testgetfieldbuilderwithinitializedvalue() {
    descriptor descriptor = testalltypes.getdescriptor();
    fielddescriptor fielddescriptor =
        descriptor.findfieldbyname("optional_nested_message");

    // before setting field, builder is initialized by default value. 
    testalltypes.builder builder = testalltypes.newbuilder();
    nestedmessage.builder fieldbuilder =
        (nestedmessage.builder) builder.getfieldbuilder(fielddescriptor);
    assertequals(0, fieldbuilder.getbb());

    // setting field value with new field builder instance.
    builder = testalltypes.newbuilder();
    nestedmessage.builder newfieldbuilder =
        builder.getoptionalnestedmessagebuilder();
    newfieldbuilder.setbb(2);
    // then get the field builder instance by getfieldbuilder().
    fieldbuilder =
        (nestedmessage.builder) builder.getfieldbuilder(fielddescriptor);
    // it should contain new value.
    assertequals(2, fieldbuilder.getbb());
    // these two builder should be equal.
    assertsame(fieldbuilder, newfieldbuilder);
  }

  public void testgetfieldbuildernotsupportedexception() {
    descriptor descriptor = testalltypes.getdescriptor();
    testalltypes.builder builder = testalltypes.newbuilder();
    try {
      builder.getfieldbuilder(descriptor.findfieldbyname("optional_int32"));
      fail("exception was not thrown");
    } catch (unsupportedoperationexception e) {
      // we expect this exception.
    }
    try {
      builder.getfieldbuilder(
          descriptor.findfieldbyname("optional_nested_enum"));
      fail("exception was not thrown");
    } catch (unsupportedoperationexception e) {
      // we expect this exception.
    }
    try {
      builder.getfieldbuilder(descriptor.findfieldbyname("repeated_int32"));
      fail("exception was not thrown");
    } catch (unsupportedoperationexception e) {
      // we expect this exception.
    }
    try {
      builder.getfieldbuilder(
          descriptor.findfieldbyname("repeated_nested_enum"));
      fail("exception was not thrown");
    } catch (unsupportedoperationexception e) {
      // we expect this exception.
    }
    try {
      builder.getfieldbuilder(
          descriptor.findfieldbyname("repeated_nested_message"));
      fail("exception was not thrown");
    } catch (unsupportedoperationexception e) {
      // we expect this exception.
    }
  }
}
