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
import protobuf_unittest.unittestmset.testmessageset;
import protobuf_unittest.unittestmset.testmessagesetextension1;
import protobuf_unittest.unittestmset.testmessagesetextension2;
import protobuf_unittest.unittestproto.onestring;
import protobuf_unittest.unittestproto.testallextensions;
import protobuf_unittest.unittestproto.testalltypes;
import protobuf_unittest.unittestproto.testalltypes.nestedmessage;
import protobuf_unittest.unittestproto.testemptymessage;

import junit.framework.testcase;

import java.io.stringreader;

/**
 * test case for {@link textformat}.
 *
 * todo(wenboz): extensiontest and rest of text_format_unittest.cc.
 *
 * @author wenboz@google.com (wenbo zhu)
 */
public class textformattest extends testcase {

  // a basic string with different escapable characters for testing.
  private final static string kescapeteststring =
      "\"a string with ' characters \n and \r newlines and \t tabs and \001 "
          + "slashes \\";

  // a representation of the above string with all the characters escaped.
  private final static string kescapeteststringescaped =
      "\\\"a string with \\' characters \\n and \\r newlines "
          + "and \\t tabs and \\001 slashes \\\\";

  private static string allfieldssettext = testutil.readtextfromfile(
    "text_format_unittest_data.txt");
  private static string allextensionssettext = testutil.readtextfromfile(
    "text_format_unittest_extensions_data.txt");

  private static string exotictext =
    "repeated_int32: -1\n" +
    "repeated_int32: -2147483648\n" +
    "repeated_int64: -1\n" +
    "repeated_int64: -9223372036854775808\n" +
    "repeated_uint32: 4294967295\n" +
    "repeated_uint32: 2147483648\n" +
    "repeated_uint64: 18446744073709551615\n" +
    "repeated_uint64: 9223372036854775808\n" +
    "repeated_double: 123.0\n" +
    "repeated_double: 123.5\n" +
    "repeated_double: 0.125\n" +
    "repeated_double: .125\n" +
    "repeated_double: -.125\n" +
    "repeated_double: 1.23e17\n" +
    "repeated_double: 1.23e+17\n" +
    "repeated_double: -1.23e-17\n" +
    "repeated_double: .23e+17\n" +
    "repeated_double: -.23e17\n" +
    "repeated_double: 1.235e22\n" +
    "repeated_double: 1.235e-18\n" +
    "repeated_double: 123.456789\n" +
    "repeated_double: infinity\n" +
    "repeated_double: -infinity\n" +
    "repeated_double: nan\n" +
    "repeated_string: \"\\000\\001\\a\\b\\f\\n\\r\\t\\v\\\\\\'\\\"" +
      "\\341\\210\\264\"\n" +
    "repeated_bytes: \"\\000\\001\\a\\b\\f\\n\\r\\t\\v\\\\\\'\\\"\\376\"\n";

  private static string canonicalexotictext =
      exotictext.replace(": .", ": 0.").replace(": -.", ": -0.")   // short-form double
      .replace("23e", "23e").replace("e+", "e").replace("0.23e17", "2.3e16");

  private string messagesettext =
    "[protobuf_unittest.testmessagesetextension1] {\n" +
    "  i: 123\n" +
    "}\n" +
    "[protobuf_unittest.testmessagesetextension2] {\n" +
    "  str: \"foo\"\n" +
    "}\n";

  /** print testalltypes and compare with golden file. */
  public void testprintmessage() throws exception {
    string javatext = textformat.printtostring(testutil.getallset());

    // java likes to add a trailing ".0" to floats and doubles.  c printf
    // (with %g format) does not.  our golden files are used for both
    // c++ and java textformat classes, so we need to conform.
    javatext = javatext.replace(".0\n", "\n");

    assertequals(allfieldssettext, javatext);
  }

  /** print testalltypes as builder and compare with golden file. */
  public void testprintmessagebuilder() throws exception {
    string javatext = textformat.printtostring(testutil.getallsetbuilder());

    // java likes to add a trailing ".0" to floats and doubles.  c printf
    // (with %g format) does not.  our golden files are used for both
    // c++ and java textformat classes, so we need to conform.
    javatext = javatext.replace(".0\n", "\n");

    assertequals(allfieldssettext, javatext);
  }

  /** print testallextensions and compare with golden file. */
  public void testprintextensions() throws exception {
    string javatext = textformat.printtostring(testutil.getallextensionsset());

    // java likes to add a trailing ".0" to floats and doubles.  c printf
    // (with %g format) does not.  our golden files are used for both
    // c++ and java textformat classes, so we need to conform.
    javatext = javatext.replace(".0\n", "\n");

    assertequals(allextensionssettext, javatext);
  }

  // creates an example unknown field set.
  private unknownfieldset makeunknownfieldset() {
    return unknownfieldset.newbuilder()
        .addfield(5,
            unknownfieldset.field.newbuilder()
            .addvarint(1)
            .addfixed32(2)
            .addfixed64(3)
            .addlengthdelimited(bytestring.copyfromutf8("4"))
            .addgroup(
                unknownfieldset.newbuilder()
                .addfield(10,
                    unknownfieldset.field.newbuilder()
                    .addvarint(5)
                    .build())
                .build())
            .build())
        .addfield(8,
            unknownfieldset.field.newbuilder()
            .addvarint(1)
            .addvarint(2)
            .addvarint(3)
            .build())
        .addfield(15,
            unknownfieldset.field.newbuilder()
            .addvarint(0xabcdef1234567890l)
            .addfixed32(0xabcd1234)
            .addfixed64(0xabcdef1234567890l)
            .build())
        .build();
  }

  public void testprintunknownfields() throws exception {
    // test printing of unknown fields in a message.

    testemptymessage message =
      testemptymessage.newbuilder()
        .setunknownfields(makeunknownfieldset())
        .build();

    assertequals(
      "5: 1\n" +
      "5: 0x00000002\n" +
      "5: 0x0000000000000003\n" +
      "5: \"4\"\n" +
      "5 {\n" +
      "  10: 5\n" +
      "}\n" +
      "8: 1\n" +
      "8: 2\n" +
      "8: 3\n" +
      "15: 12379813812177893520\n" +
      "15: 0xabcd1234\n" +
      "15: 0xabcdef1234567890\n",
      textformat.printtostring(message));
  }

  public void testprintfield() throws exception {
    final fielddescriptor datafield =
      onestring.getdescriptor().findfieldbyname("data");
    assertequals(
      "data: \"test data\"\n",
      textformat.printfieldtostring(datafield, "test data"));

    final fielddescriptor optionalfield =
      testalltypes.getdescriptor().findfieldbyname("optional_nested_message");
    final object value = nestedmessage.newbuilder().setbb(42).build();

    assertequals(
      "optional_nested_message {\n  bb: 42\n}\n",
      textformat.printfieldtostring(optionalfield, value));
  }

  /**
   * helper to construct a bytestring from a string containing only 8-bit
   * characters.  the characters are converted directly to bytes, *not*
   * encoded using utf-8.
   */
  private bytestring bytes(string str) throws exception {
    return bytestring.copyfrom(str.getbytes("iso-8859-1"));
  }

  /**
   * helper to construct a bytestring from a bunch of bytes.  the inputs are
   * actually ints so that i can use hex notation and not get stupid errors
   * about precision.
   */
  private bytestring bytes(int... bytesasints) {
    byte[] bytes = new byte[bytesasints.length];
    for (int i = 0; i < bytesasints.length; i++) {
      bytes[i] = (byte) bytesasints[i];
    }
    return bytestring.copyfrom(bytes);
  }

  public void testprintexotic() throws exception {
    message message = testalltypes.newbuilder()
      // signed vs. unsigned numbers.
      .addrepeatedint32 (-1)
      .addrepeateduint32(-1)
      .addrepeatedint64 (-1)
      .addrepeateduint64(-1)

      .addrepeatedint32 (1  << 31)
      .addrepeateduint32(1  << 31)
      .addrepeatedint64 (1l << 63)
      .addrepeateduint64(1l << 63)

      // floats of various precisions and exponents.
      .addrepeateddouble(123)
      .addrepeateddouble(123.5)
      .addrepeateddouble(0.125)
      .addrepeateddouble(.125)
      .addrepeateddouble(-.125)
      .addrepeateddouble(123e15)
      .addrepeateddouble(123e15)
      .addrepeateddouble(-1.23e-17)
      .addrepeateddouble(.23e17)
      .addrepeateddouble(-23e15)
      .addrepeateddouble(123.5e20)
      .addrepeateddouble(123.5e-20)
      .addrepeateddouble(123.456789)
      .addrepeateddouble(double.positive_infinity)
      .addrepeateddouble(double.negative_infinity)
      .addrepeateddouble(double.nan)

      // strings and bytes that needing escaping.
      .addrepeatedstring("\0\001\007\b\f\n\r\t\013\\\'\"\u1234")
      .addrepeatedbytes(bytes("\0\001\007\b\f\n\r\t\013\\\'\"\u00fe"))
      .build();

    assertequals(canonicalexotictext, message.tostring());
  }

  public void testprintmessageset() throws exception {
    testmessageset messageset =
      testmessageset.newbuilder()
        .setextension(
          testmessagesetextension1.messagesetextension,
          testmessagesetextension1.newbuilder().seti(123).build())
        .setextension(
          testmessagesetextension2.messagesetextension,
          testmessagesetextension2.newbuilder().setstr("foo").build())
        .build();

    assertequals(messagesettext, messageset.tostring());
  }

  // =================================================================

  public void testparse() throws exception {
    testalltypes.builder builder = testalltypes.newbuilder();
    textformat.merge(allfieldssettext, builder);
    testutil.assertallfieldsset(builder.build());
  }

  public void testparsereader() throws exception {
    testalltypes.builder builder = testalltypes.newbuilder();
    textformat.merge(new stringreader(allfieldssettext), builder);
    testutil.assertallfieldsset(builder.build());
  }

  public void testparseextensions() throws exception {
    testallextensions.builder builder = testallextensions.newbuilder();
    textformat.merge(allextensionssettext,
                     testutil.getextensionregistry(),
                     builder);
    testutil.assertallextensionsset(builder.build());
  }

  public void testparsecompatibility() throws exception {
    string original = "repeated_float: inf\n" +
                      "repeated_float: -inf\n" +
                      "repeated_float: nan\n" +
                      "repeated_float: inff\n" +
                      "repeated_float: -inff\n" +
                      "repeated_float: nanf\n" +
                      "repeated_float: 1.0f\n" +
                      "repeated_float: infinityf\n" +
                      "repeated_float: -infinityf\n" +
                      "repeated_double: infinity\n" +
                      "repeated_double: -infinity\n" +
                      "repeated_double: nan\n";
    string canonical =  "repeated_float: infinity\n" +
                        "repeated_float: -infinity\n" +
                        "repeated_float: nan\n" +
                        "repeated_float: infinity\n" +
                        "repeated_float: -infinity\n" +
                        "repeated_float: nan\n" +
                        "repeated_float: 1.0\n" +
                        "repeated_float: infinity\n" +
                        "repeated_float: -infinity\n" +
                        "repeated_double: infinity\n" +
                        "repeated_double: -infinity\n" +
                        "repeated_double: nan\n";
    testalltypes.builder builder = testalltypes.newbuilder();
    textformat.merge(original, builder);
    assertequals(canonical, builder.build().tostring());
  }

  public void testparseexotic() throws exception {
    testalltypes.builder builder = testalltypes.newbuilder();
    textformat.merge(exotictext, builder);

    // too lazy to check things individually.  don't try to debug this
    // if testprintexotic() is failing.
    assertequals(canonicalexotictext, builder.build().tostring());
  }

  public void testparsemessageset() throws exception {
    extensionregistry extensionregistry = extensionregistry.newinstance();
    extensionregistry.add(testmessagesetextension1.messagesetextension);
    extensionregistry.add(testmessagesetextension2.messagesetextension);

    testmessageset.builder builder = testmessageset.newbuilder();
    textformat.merge(messagesettext, extensionregistry, builder);
    testmessageset messageset = builder.build();

    asserttrue(messageset.hasextension(
      testmessagesetextension1.messagesetextension));
    assertequals(123, messageset.getextension(
      testmessagesetextension1.messagesetextension).geti());
    asserttrue(messageset.hasextension(
      testmessagesetextension2.messagesetextension));
    assertequals("foo", messageset.getextension(
      testmessagesetextension2.messagesetextension).getstr());
  }

  public void testparsenumericenum() throws exception {
    testalltypes.builder builder = testalltypes.newbuilder();
    textformat.merge("optional_nested_enum: 2", builder);
    assertequals(testalltypes.nestedenum.bar, builder.getoptionalnestedenum());
  }

  public void testparseanglebrackets() throws exception {
    testalltypes.builder builder = testalltypes.newbuilder();
    textformat.merge("optionalgroup: < a: 1 >", builder);
    asserttrue(builder.hasoptionalgroup());
    assertequals(1, builder.getoptionalgroup().geta());
  }

  public void testparsecomment() throws exception {
    testalltypes.builder builder = testalltypes.newbuilder();
    textformat.merge(
      "# this is a comment\n" +
      "optional_int32: 1  # another comment\n" +
      "optional_int64: 2\n" +
      "# eof comment", builder);
    assertequals(1, builder.getoptionalint32());
    assertequals(2, builder.getoptionalint64());
  }

  private void assertparseerror(string error, string text) {
    testalltypes.builder builder = testalltypes.newbuilder();
    try {
      textformat.merge(text, testutil.getextensionregistry(), builder);
      fail("expected parse exception.");
    } catch (textformat.parseexception e) {
      assertequals(error, e.getmessage());
    }
  }

  public void testparseerrors() throws exception {
    assertparseerror(
      "1:16: expected \":\".",
      "optional_int32 123");
    assertparseerror(
      "1:23: expected identifier.",
      "optional_nested_enum: ?");
    assertparseerror(
      "1:18: couldn't parse integer: number must be positive: -1",
      "optional_uint32: -1");
    assertparseerror(
      "1:17: couldn't parse integer: number out of range for 32-bit signed " +
        "integer: 82301481290849012385230157",
      "optional_int32: 82301481290849012385230157");
    assertparseerror(
      "1:16: expected \"true\" or \"false\".",
      "optional_bool: maybe");
    assertparseerror(
      "1:16: expected \"true\" or \"false\".",
      "optional_bool: 2");
    assertparseerror(
      "1:18: expected string.",
      "optional_string: 123");
    assertparseerror(
      "1:18: string missing ending quote.",
      "optional_string: \"ueoauaoe");
    assertparseerror(
      "1:18: string missing ending quote.",
      "optional_string: \"ueoauaoe\n" +
      "optional_int32: 123");
    assertparseerror(
      "1:18: invalid escape sequence: '\\z'",
      "optional_string: \"\\z\"");
    assertparseerror(
      "1:18: string missing ending quote.",
      "optional_string: \"ueoauaoe\n" +
      "optional_int32: 123");
    assertparseerror(
      "1:2: extension \"nosuchext\" not found in the extensionregistry.",
      "[nosuchext]: 123");
    assertparseerror(
      "1:20: extension \"protobuf_unittest.optional_int32_extension\" does " +
        "not extend message type \"protobuf_unittest.testalltypes\".",
      "[protobuf_unittest.optional_int32_extension]: 123");
    assertparseerror(
      "1:1: message type \"protobuf_unittest.testalltypes\" has no field " +
        "named \"nosuchfield\".",
      "nosuchfield: 123");
    assertparseerror(
      "1:21: expected \">\".",
      "optionalgroup < a: 1");
    assertparseerror(
      "1:23: enum type \"protobuf_unittest.testalltypes.nestedenum\" has no " +
        "value named \"no_such_value\".",
      "optional_nested_enum: no_such_value");
    assertparseerror(
      "1:23: enum type \"protobuf_unittest.testalltypes.nestedenum\" has no " +
        "value with number 123.",
      "optional_nested_enum: 123");

    // delimiters must match.
    assertparseerror(
      "1:22: expected identifier.",
      "optionalgroup < a: 1 }");
    assertparseerror(
      "1:22: expected identifier.",
      "optionalgroup { a: 1 >");
  }

  // =================================================================

  public void testescape() throws exception {
    // escape sequences.
    assertequals("\\000\\001\\a\\b\\f\\n\\r\\t\\v\\\\\\'\\\"",
      textformat.escapebytes(bytes("\0\001\007\b\f\n\r\t\013\\\'\"")));
    assertequals("\\000\\001\\a\\b\\f\\n\\r\\t\\v\\\\\\'\\\"",
      textformat.escapetext("\0\001\007\b\f\n\r\t\013\\\'\""));
    assertequals(bytes("\0\001\007\b\f\n\r\t\013\\\'\""),
      textformat.unescapebytes("\\000\\001\\a\\b\\f\\n\\r\\t\\v\\\\\\'\\\""));
    assertequals("\0\001\007\b\f\n\r\t\013\\\'\"",
      textformat.unescapetext("\\000\\001\\a\\b\\f\\n\\r\\t\\v\\\\\\'\\\""));
    assertequals(kescapeteststringescaped,
      textformat.escapetext(kescapeteststring));
    assertequals(kescapeteststring,
      textformat.unescapetext(kescapeteststringescaped));

    // unicode handling.
    assertequals("\\341\\210\\264", textformat.escapetext("\u1234"));
    assertequals("\\341\\210\\264",
                 textformat.escapebytes(bytes(0xe1, 0x88, 0xb4)));
    assertequals("\u1234", textformat.unescapetext("\\341\\210\\264"));
    assertequals(bytes(0xe1, 0x88, 0xb4),
                 textformat.unescapebytes("\\341\\210\\264"));
    assertequals("\u1234", textformat.unescapetext("\\xe1\\x88\\xb4"));
    assertequals(bytes(0xe1, 0x88, 0xb4),
                 textformat.unescapebytes("\\xe1\\x88\\xb4"));

    // handling of strings with unescaped unicode characters > 255.
    final string zh = "\u9999\u6e2f\u4e0a\u6d77\ud84f\udf80\u8c50\u9280\u884c";
    bytestring zhbytestring = bytestring.copyfromutf8(zh);
    assertequals(zhbytestring, textformat.unescapebytes(zh));

    // errors.
    try {
      textformat.unescapetext("\\x");
      fail("should have thrown an exception.");
    } catch (textformat.invalidescapesequenceexception e) {
      // success
    }

    try {
      textformat.unescapetext("\\z");
      fail("should have thrown an exception.");
    } catch (textformat.invalidescapesequenceexception e) {
      // success
    }

    try {
      textformat.unescapetext("\\");
      fail("should have thrown an exception.");
    } catch (textformat.invalidescapesequenceexception e) {
      // success
    }
  }

  public void testparseinteger() throws exception {
    assertequals(          0, textformat.parseint32(          "0"));
    assertequals(          1, textformat.parseint32(          "1"));
    assertequals(         -1, textformat.parseint32(         "-1"));
    assertequals(      12345, textformat.parseint32(      "12345"));
    assertequals(     -12345, textformat.parseint32(     "-12345"));
    assertequals( 2147483647, textformat.parseint32( "2147483647"));
    assertequals(-2147483648, textformat.parseint32("-2147483648"));

    assertequals(                0, textformat.parseuint32(         "0"));
    assertequals(                1, textformat.parseuint32(         "1"));
    assertequals(            12345, textformat.parseuint32(     "12345"));
    assertequals(       2147483647, textformat.parseuint32("2147483647"));
    assertequals((int) 2147483648l, textformat.parseuint32("2147483648"));
    assertequals((int) 4294967295l, textformat.parseuint32("4294967295"));

    assertequals(          0l, textformat.parseint64(          "0"));
    assertequals(          1l, textformat.parseint64(          "1"));
    assertequals(         -1l, textformat.parseint64(         "-1"));
    assertequals(      12345l, textformat.parseint64(      "12345"));
    assertequals(     -12345l, textformat.parseint64(     "-12345"));
    assertequals( 2147483647l, textformat.parseint64( "2147483647"));
    assertequals(-2147483648l, textformat.parseint64("-2147483648"));
    assertequals( 4294967295l, textformat.parseint64( "4294967295"));
    assertequals( 4294967296l, textformat.parseint64( "4294967296"));
    assertequals(9223372036854775807l,
                 textformat.parseint64("9223372036854775807"));
    assertequals(-9223372036854775808l,
                 textformat.parseint64("-9223372036854775808"));

    assertequals(          0l, textformat.parseuint64(          "0"));
    assertequals(          1l, textformat.parseuint64(          "1"));
    assertequals(      12345l, textformat.parseuint64(      "12345"));
    assertequals( 2147483647l, textformat.parseuint64( "2147483647"));
    assertequals( 4294967295l, textformat.parseuint64( "4294967295"));
    assertequals( 4294967296l, textformat.parseuint64( "4294967296"));
    assertequals(9223372036854775807l,
                 textformat.parseuint64("9223372036854775807"));
    assertequals(-9223372036854775808l,
                 textformat.parseuint64("9223372036854775808"));
    assertequals(-1l, textformat.parseuint64("18446744073709551615"));

    // hex
    assertequals(0x1234abcd, textformat.parseint32("0x1234abcd"));
    assertequals(-0x1234abcd, textformat.parseint32("-0x1234abcd"));
    assertequals(-1, textformat.parseuint64("0xffffffffffffffff"));
    assertequals(0x7fffffffffffffffl,
                 textformat.parseint64("0x7fffffffffffffff"));

    // octal
    assertequals(01234567, textformat.parseint32("01234567"));

    // out-of-range
    try {
      textformat.parseint32("2147483648");
      fail("should have thrown an exception.");
    } catch (numberformatexception e) {
      // success
    }

    try {
      textformat.parseint32("-2147483649");
      fail("should have thrown an exception.");
    } catch (numberformatexception e) {
      // success
    }

    try {
      textformat.parseuint32("4294967296");
      fail("should have thrown an exception.");
    } catch (numberformatexception e) {
      // success
    }

    try {
      textformat.parseuint32("-1");
      fail("should have thrown an exception.");
    } catch (numberformatexception e) {
      // success
    }

    try {
      textformat.parseint64("9223372036854775808");
      fail("should have thrown an exception.");
    } catch (numberformatexception e) {
      // success
    }

    try {
      textformat.parseint64("-9223372036854775809");
      fail("should have thrown an exception.");
    } catch (numberformatexception e) {
      // success
    }

    try {
      textformat.parseuint64("18446744073709551616");
      fail("should have thrown an exception.");
    } catch (numberformatexception e) {
      // success
    }

    try {
      textformat.parseuint64("-1");
      fail("should have thrown an exception.");
    } catch (numberformatexception e) {
      // success
    }

    // not a number.
    try {
      textformat.parseint32("abcd");
      fail("should have thrown an exception.");
    } catch (numberformatexception e) {
      // success
    }
  }

  public void testparsestring() throws exception {
    final string zh = "\u9999\u6e2f\u4e0a\u6d77\ud84f\udf80\u8c50\u9280\u884c";
    testalltypes.builder builder = testalltypes.newbuilder();
    textformat.merge("optional_string: \"" + zh + "\"", builder);
    assertequals(zh, builder.getoptionalstring());
  }

  public void testparselongstring() throws exception {
    string longtext =
      "123456789012345678901234567890123456789012345678901234567890" +
      "123456789012345678901234567890123456789012345678901234567890" +
      "123456789012345678901234567890123456789012345678901234567890" +
      "123456789012345678901234567890123456789012345678901234567890" +
      "123456789012345678901234567890123456789012345678901234567890" +
      "123456789012345678901234567890123456789012345678901234567890" +
      "123456789012345678901234567890123456789012345678901234567890" +
      "123456789012345678901234567890123456789012345678901234567890" +
      "123456789012345678901234567890123456789012345678901234567890" +
      "123456789012345678901234567890123456789012345678901234567890" +
      "123456789012345678901234567890123456789012345678901234567890" +
      "123456789012345678901234567890123456789012345678901234567890" +
      "123456789012345678901234567890123456789012345678901234567890" +
      "123456789012345678901234567890123456789012345678901234567890" +
      "123456789012345678901234567890123456789012345678901234567890" +
      "123456789012345678901234567890123456789012345678901234567890" +
      "123456789012345678901234567890123456789012345678901234567890" +
      "123456789012345678901234567890123456789012345678901234567890" +
      "123456789012345678901234567890123456789012345678901234567890" +
      "123456789012345678901234567890123456789012345678901234567890";

    testalltypes.builder builder = testalltypes.newbuilder();
    textformat.merge("optional_string: \"" + longtext + "\"", builder);
    assertequals(longtext, builder.getoptionalstring());
  }

  public void testparseboolean() throws exception {
    string goodtext =
        "repeated_bool: t  repeated_bool : 0\n" +
        "repeated_bool :f repeated_bool:1";
    string goodtextcanonical =
        "repeated_bool: true\n" +
        "repeated_bool: false\n" +
        "repeated_bool: false\n" +
        "repeated_bool: true\n";
    testalltypes.builder builder = testalltypes.newbuilder();
    textformat.merge(goodtext, builder);
    assertequals(goodtextcanonical, builder.build().tostring());

    try {
      testalltypes.builder badbuilder = testalltypes.newbuilder();
      textformat.merge("optional_bool:2", badbuilder);
      fail("should have thrown an exception.");
    } catch (textformat.parseexception e) {
      // success
    }
    try {
      testalltypes.builder badbuilder = testalltypes.newbuilder();
      textformat.merge("optional_bool: foo", badbuilder);
      fail("should have thrown an exception.");
    } catch (textformat.parseexception e) {
      // success
    }
  }

  public void testparseadjacentstringliterals() throws exception {
    testalltypes.builder builder = testalltypes.newbuilder();
    textformat.merge("optional_string: \"foo\" 'corge' \"grault\"", builder);
    assertequals("foocorgegrault", builder.getoptionalstring());
  }

  public void testprintfieldvalue() throws exception {
    assertprintfieldvalue("\"hello\"", "hello", "repeated_string");
    assertprintfieldvalue("123.0",  123f, "repeated_float");
    assertprintfieldvalue("123.0",  123d, "repeated_double");
    assertprintfieldvalue("123",  123, "repeated_int32");
    assertprintfieldvalue("123",  123l, "repeated_int64");
    assertprintfieldvalue("true",  true, "repeated_bool");
    assertprintfieldvalue("4294967295", 0xffffffff, "repeated_uint32");
    assertprintfieldvalue("18446744073709551615",  0xffffffffffffffffl,
        "repeated_uint64");
    assertprintfieldvalue("\"\\001\\002\\003\"",
        bytestring.copyfrom(new byte[] {1, 2, 3}), "repeated_bytes");
  }

  private void assertprintfieldvalue(string expect, object value,
      string fieldname) throws exception {
    testalltypes.builder builder = testalltypes.newbuilder();
    stringbuilder sb = new stringbuilder();
    textformat.printfieldvalue(
        testalltypes.getdescriptor().findfieldbyname(fieldname),
        value, sb);
    assertequals(expect, sb.tostring());
  }

  public void testshortdebugstring() {
    assertequals("optional_nested_message { bb: 42 } repeated_int32: 1"
        + " repeated_uint32: 2",
        textformat.shortdebugstring(testalltypes.newbuilder()
            .addrepeatedint32(1)
            .addrepeateduint32(2)
            .setoptionalnestedmessage(
                nestedmessage.newbuilder().setbb(42).build())
            .build()));
  }

  public void testshortdebugstring_unknown() {
    assertequals("5: 1 5: 0x00000002 5: 0x0000000000000003 5: \"4\" 5 { 10: 5 }"
        + " 8: 1 8: 2 8: 3 15: 12379813812177893520 15: 0xabcd1234 15:"
        + " 0xabcdef1234567890",
        textformat.shortdebugstring(makeunknownfieldset()));
  }

  public void testprinttounicodestring() {
    assertequals(
        "optional_string: \"abc\u3042efg\"\n" +
        "optional_bytes: \"\\343\\201\\202\"\n" +
        "repeated_string: \"\u3093xyz\"\n",
        textformat.printtounicodestring(testalltypes.newbuilder()
            .setoptionalstring("abc\u3042efg")
            .setoptionalbytes(bytes(0xe3, 0x81, 0x82))
            .addrepeatedstring("\u3093xyz")
            .build()));
  }

  public void testprinttounicodestring_unknown() {
    assertequals(
        "1: \"\\343\\201\\202\"\n",
        textformat.printtounicodestring(unknownfieldset.newbuilder()
            .addfield(1,
                unknownfieldset.field.newbuilder()
                .addlengthdelimited(bytes(0xe3, 0x81, 0x82)).build())
            .build()));
  }
}
