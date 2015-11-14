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

import com.google.protobuf.unittestlite.testalltypeslite;
import com.google.protobuf.unittestlite.testpackedextensionslite;
import com.google.protobuf.unittestlite.testparsingmergelite;
import com.google.protobuf.unittestlite;
import protobuf_unittest.unittestoptimizefor.testoptimizedforsize;
import protobuf_unittest.unittestoptimizefor.testrequiredoptimizedforsize;
import protobuf_unittest.unittestoptimizefor;
import protobuf_unittest.unittestproto.foreignmessage;
import protobuf_unittest.unittestproto.testalltypes;
import protobuf_unittest.unittestproto.testemptymessage;
import protobuf_unittest.unittestproto.testrequired;
import protobuf_unittest.unittestproto.testparsingmerge;
import protobuf_unittest.unittestproto;

import junit.framework.testcase;

import java.io.bytearrayinputstream;
import java.io.bytearrayoutputstream;
import java.io.ioexception;
import java.io.inputstream;

/**
 * unit test for {@link parser}.
 *
 * @author liujisi@google.com (pherl liu)
 */
public class parsertest extends testcase {
  public void testgeneratedmessageparsersingleton() throws exception {
    for (int i = 0; i < 10; i++) {
      assertequals(testalltypes.parser,
                   testutil.getallset().getparserfortype());
    }
  }

  private void assertroundtripequals(messagelite message,
                                     extensionregistrylite registry)
      throws exception {
    final byte[] data = message.tobytearray();
    final int offset = 20;
    final int length = data.length;
    final int padding = 30;
    parser<? extends messagelite> parser = message.getparserfortype();
    assertmessageequals(message, parser.parsefrom(data, registry));
    assertmessageequals(message, parser.parsefrom(
        generatepaddingarray(data, offset, padding),
        offset, length, registry));
    assertmessageequals(message, parser.parsefrom(
        message.tobytestring(), registry));
    assertmessageequals(message, parser.parsefrom(
        new bytearrayinputstream(data), registry));
    assertmessageequals(message, parser.parsefrom(
        codedinputstream.newinstance(data), registry));
  }

  private void assertroundtripequals(messagelite message) throws exception {
    final byte[] data = message.tobytearray();
    final int offset = 20;
    final int length = data.length;
    final int padding = 30;
    parser<? extends messagelite> parser = message.getparserfortype();
    assertmessageequals(message, parser.parsefrom(data));
    assertmessageequals(message, parser.parsefrom(
        generatepaddingarray(data, offset, padding),
        offset, length));
    assertmessageequals(message, parser.parsefrom(message.tobytestring()));
    assertmessageequals(message, parser.parsefrom(
        new bytearrayinputstream(data)));
    assertmessageequals(message, parser.parsefrom(
        codedinputstream.newinstance(data)));
  }

  private void assertmessageequals(messagelite expected, messagelite actual)
      throws exception {
    if (expected instanceof message) {
      assertequals(expected, actual);
    } else {
      assertequals(expected.tobytestring(), actual.tobytestring());
    }
  }

  private byte[] generatepaddingarray(byte[] data, int offset, int padding) {
    byte[] result = new byte[offset + data.length + padding];
    system.arraycopy(data, 0, result, offset, data.length);
    return result;
  }

  public void testnormalmessage() throws exception {
    assertroundtripequals(testutil.getallset());
  }

  public void testparsepartial() throws exception {
    parser<testrequired> parser = testrequired.parser;
    final string errorstring =
        "should throw exceptions when the parsed message isn't initialized.";

    // testrequired.b and testrequired.c are not set.
    testrequired partialmessage = testrequired.newbuilder()
        .seta(1).buildpartial();

    // parsepartialfrom should pass.
    byte[] data = partialmessage.tobytearray();
    assertequals(partialmessage, parser.parsepartialfrom(data));
    assertequals(partialmessage, parser.parsepartialfrom(
        partialmessage.tobytestring()));
    assertequals(partialmessage, parser.parsepartialfrom(
        new bytearrayinputstream(data)));
    assertequals(partialmessage, parser.parsepartialfrom(
        codedinputstream.newinstance(data)));

    // parsefrom(bytearray)
    try {
      parser.parsefrom(partialmessage.tobytearray());
      fail(errorstring);
    } catch (invalidprotocolbufferexception e) {
      // pass.
    }

    // parsefrom(bytestring)
    try {
      parser.parsefrom(partialmessage.tobytestring());
      fail(errorstring);
    } catch (invalidprotocolbufferexception e) {
      // pass.
    }

    // parsefrom(inputstream)
    try {
      parser.parsefrom(new bytearrayinputstream(partialmessage.tobytearray()));
      fail(errorstring);
    } catch (ioexception e) {
      // pass.
    }

    // parsefrom(codedinputstream)
    try {
      parser.parsefrom(codedinputstream.newinstance(
          partialmessage.tobytearray()));
      fail(errorstring);
    } catch (ioexception e) {
      // pass.
    }
  }

  public void testparseextensions() throws exception {
    assertroundtripequals(testutil.getallextensionsset(),
                          testutil.getextensionregistry());
    assertroundtripequals(testutil.getallliteextensionsset(),
                          testutil.getextensionregistrylite());
  }

  public void testparsepacked() throws exception {
    assertroundtripequals(testutil.getpackedset());
    assertroundtripequals(testutil.getpackedextensionsset(),
                          testutil.getextensionregistry());
    assertroundtripequals(testutil.getlitepackedextensionsset(),
                          testutil.getextensionregistrylite());
  }

  public void testparsedelimitedto() throws exception {
    // write normal message.
    testalltypes normalmessage = testutil.getallset();
    bytearrayoutputstream output = new bytearrayoutputstream();
    normalmessage.writedelimitedto(output);

    // write messagelite with packed extension fields.
    testpackedextensionslite packedmessage =
        testutil.getlitepackedextensionsset();
    packedmessage.writedelimitedto(output);

    inputstream input = new bytearrayinputstream(output.tobytearray());
    assertmessageequals(
        normalmessage,
        normalmessage.getparserfortype().parsedelimitedfrom(input));
    assertmessageequals(
        packedmessage,
        packedmessage.getparserfortype().parsedelimitedfrom(
            input, testutil.getextensionregistrylite()));
  }

  public void testparseunknownfields() throws exception {
    // all fields will be treated as unknown fields in emptymessage.
    testemptymessage emptymessage = testemptymessage.parser.parsefrom(
        testutil.getallset().tobytestring());
    assertequals(
        testutil.getallset().tobytestring(),
        emptymessage.tobytestring());
  }

  public void testoptimizeforsize() throws exception {
    testoptimizedforsize.builder builder = testoptimizedforsize.newbuilder();
    builder.seti(12).setmsg(foreignmessage.newbuilder().setc(34).build());
    builder.setextension(testoptimizedforsize.testextension, 56);
    builder.setextension(testoptimizedforsize.testextension2,
        testrequiredoptimizedforsize.newbuilder().setx(78).build());

    testoptimizedforsize message = builder.build();
    extensionregistry registry = extensionregistry.newinstance();
    unittestoptimizefor.registerallextensions(registry);

    assertroundtripequals(message, registry);
  }

  /** helper method for {@link #testparsingmerge()}.*/
  private void assertmessagemerged(testalltypes alltypes)
      throws exception {
    assertequals(3, alltypes.getoptionalint32());
    assertequals(2, alltypes.getoptionalint64());
    assertequals("hello", alltypes.getoptionalstring());
  }

  /** helper method for {@link #testparsingmergelite()}.*/
  private void assertmessagemerged(testalltypeslite alltypes)
      throws exception {
    assertequals(3, alltypes.getoptionalint32());
    assertequals(2, alltypes.getoptionalint64());
    assertequals("hello", alltypes.getoptionalstring());
  }

  public void testparsingmerge() throws exception {
    // build messages.
    testalltypes.builder builder = testalltypes.newbuilder();
    testalltypes msg1 = builder.setoptionalint32(1).build();
    builder.clear();
    testalltypes msg2 = builder.setoptionalint64(2).build();
    builder.clear();
    testalltypes msg3 = builder.setoptionalint32(3)
        .setoptionalstring("hello").build();

    // build groups.
    testparsingmerge.repeatedfieldsgenerator.group1 optionalg1 =
        testparsingmerge.repeatedfieldsgenerator.group1.newbuilder()
        .setfield1(msg1).build();
    testparsingmerge.repeatedfieldsgenerator.group1 optionalg2 =
        testparsingmerge.repeatedfieldsgenerator.group1.newbuilder()
        .setfield1(msg2).build();
    testparsingmerge.repeatedfieldsgenerator.group1 optionalg3 =
        testparsingmerge.repeatedfieldsgenerator.group1.newbuilder()
        .setfield1(msg3).build();
    testparsingmerge.repeatedfieldsgenerator.group2 repeatedg1 =
        testparsingmerge.repeatedfieldsgenerator.group2.newbuilder()
        .setfield1(msg1).build();
    testparsingmerge.repeatedfieldsgenerator.group2 repeatedg2 =
        testparsingmerge.repeatedfieldsgenerator.group2.newbuilder()
        .setfield1(msg2).build();
    testparsingmerge.repeatedfieldsgenerator.group2 repeatedg3 =
        testparsingmerge.repeatedfieldsgenerator.group2.newbuilder()
        .setfield1(msg3).build();

    // assign and serialize repeatedfieldsgenerator.
    bytestring data = testparsingmerge.repeatedfieldsgenerator.newbuilder()
        .addfield1(msg1).addfield1(msg2).addfield1(msg3)
        .addfield2(msg1).addfield2(msg2).addfield2(msg3)
        .addfield3(msg1).addfield3(msg2).addfield3(msg3)
        .addgroup1(optionalg1).addgroup1(optionalg2).addgroup1(optionalg3)
        .addgroup2(repeatedg1).addgroup2(repeatedg2).addgroup2(repeatedg3)
        .addext1(msg1).addext1(msg2).addext1(msg3)
        .addext2(msg1).addext2(msg2).addext2(msg3)
        .build().tobytestring();

    // parse testparsingmerge.
    extensionregistry registry = extensionregistry.newinstance();
    unittestproto.registerallextensions(registry);
    testparsingmerge parsingmerge =
        testparsingmerge.parser.parsefrom(data, registry);

    // required and optional fields should be merged.
    assertmessagemerged(parsingmerge.getrequiredalltypes());
    assertmessagemerged(parsingmerge.getoptionalalltypes());
    assertmessagemerged(
        parsingmerge.getoptionalgroup().getoptionalgroupalltypes());
    assertmessagemerged(parsingmerge.getextension(
        testparsingmerge.optionalext));

    // repeated fields should not be merged.
    assertequals(3, parsingmerge.getrepeatedalltypescount());
    assertequals(3, parsingmerge.getrepeatedgroupcount());
    assertequals(3, parsingmerge.getextensioncount(
        testparsingmerge.repeatedext));
  }

  public void testparsingmergelite() throws exception {
    // build messages.
    testalltypeslite.builder builder =
        testalltypeslite.newbuilder();
    testalltypeslite msg1 = builder.setoptionalint32(1).build();
    builder.clear();
    testalltypeslite msg2 = builder.setoptionalint64(2).build();
    builder.clear();
    testalltypeslite msg3 = builder.setoptionalint32(3)
        .setoptionalstring("hello").build();

    // build groups.
    testparsingmergelite.repeatedfieldsgenerator.group1 optionalg1 =
        testparsingmergelite.repeatedfieldsgenerator.group1.newbuilder()
        .setfield1(msg1).build();
    testparsingmergelite.repeatedfieldsgenerator.group1 optionalg2 =
        testparsingmergelite.repeatedfieldsgenerator.group1.newbuilder()
        .setfield1(msg2).build();
    testparsingmergelite.repeatedfieldsgenerator.group1 optionalg3 =
        testparsingmergelite.repeatedfieldsgenerator.group1.newbuilder()
        .setfield1(msg3).build();
    testparsingmergelite.repeatedfieldsgenerator.group2 repeatedg1 =
        testparsingmergelite.repeatedfieldsgenerator.group2.newbuilder()
        .setfield1(msg1).build();
    testparsingmergelite.repeatedfieldsgenerator.group2 repeatedg2 =
        testparsingmergelite.repeatedfieldsgenerator.group2.newbuilder()
        .setfield1(msg2).build();
    testparsingmergelite.repeatedfieldsgenerator.group2 repeatedg3 =
        testparsingmergelite.repeatedfieldsgenerator.group2.newbuilder()
        .setfield1(msg3).build();

    // assign and serialize repeatedfieldsgenerator.
    bytestring data = testparsingmergelite.repeatedfieldsgenerator.newbuilder()
        .addfield1(msg1).addfield1(msg2).addfield1(msg3)
        .addfield2(msg1).addfield2(msg2).addfield2(msg3)
        .addfield3(msg1).addfield3(msg2).addfield3(msg3)
        .addgroup1(optionalg1).addgroup1(optionalg2).addgroup1(optionalg3)
        .addgroup2(repeatedg1).addgroup2(repeatedg2).addgroup2(repeatedg3)
        .addext1(msg1).addext1(msg2).addext1(msg3)
        .addext2(msg1).addext2(msg2).addext2(msg3)
        .build().tobytestring();

    // parse testparsingmergelite.
    extensionregistry registry = extensionregistry.newinstance();
    unittestlite.registerallextensions(registry);
    testparsingmergelite parsingmerge =
        testparsingmergelite.parser.parsefrom(data, registry);

    // required and optional fields should be merged.
    assertmessagemerged(parsingmerge.getrequiredalltypes());
    assertmessagemerged(parsingmerge.getoptionalalltypes());
    assertmessagemerged(
        parsingmerge.getoptionalgroup().getoptionalgroupalltypes());
    assertmessagemerged(parsingmerge.getextension(
        testparsingmergelite.optionalext));

    // repeated fields should not be merged.
    assertequals(3, parsingmerge.getrepeatedalltypescount());
    assertequals(3, parsingmerge.getrepeatedgroupcount());
    assertequals(3, parsingmerge.getextensioncount(
        testparsingmergelite.repeatedext));
  }
}
