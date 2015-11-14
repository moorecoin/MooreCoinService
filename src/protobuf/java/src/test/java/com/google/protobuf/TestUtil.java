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

import protobuf_unittest.unittestproto;
import com.google.protobuf.unittestlite;

// the static imports are to avoid 100+ char lines.  the following is roughly equivalent to
// import static protobuf_unittest.unittestproto.*;
import static protobuf_unittest.unittestproto.defaultint32extension;
import static protobuf_unittest.unittestproto.defaultint64extension;
import static protobuf_unittest.unittestproto.defaultuint32extension;
import static protobuf_unittest.unittestproto.defaultuint64extension;
import static protobuf_unittest.unittestproto.defaultsint32extension;
import static protobuf_unittest.unittestproto.defaultsint64extension;
import static protobuf_unittest.unittestproto.defaultfixed32extension;
import static protobuf_unittest.unittestproto.defaultfixed64extension;
import static protobuf_unittest.unittestproto.defaultsfixed32extension;
import static protobuf_unittest.unittestproto.defaultsfixed64extension;
import static protobuf_unittest.unittestproto.defaultfloatextension;
import static protobuf_unittest.unittestproto.defaultdoubleextension;
import static protobuf_unittest.unittestproto.defaultboolextension;
import static protobuf_unittest.unittestproto.defaultstringextension;
import static protobuf_unittest.unittestproto.defaultbytesextension;
import static protobuf_unittest.unittestproto.defaultnestedenumextension;
import static protobuf_unittest.unittestproto.defaultforeignenumextension;
import static protobuf_unittest.unittestproto.defaultimportenumextension;
import static protobuf_unittest.unittestproto.defaultstringpieceextension;
import static protobuf_unittest.unittestproto.defaultcordextension;

import static protobuf_unittest.unittestproto.optionalint32extension;
import static protobuf_unittest.unittestproto.optionalint64extension;
import static protobuf_unittest.unittestproto.optionaluint32extension;
import static protobuf_unittest.unittestproto.optionaluint64extension;
import static protobuf_unittest.unittestproto.optionalsint32extension;
import static protobuf_unittest.unittestproto.optionalsint64extension;
import static protobuf_unittest.unittestproto.optionalfixed32extension;
import static protobuf_unittest.unittestproto.optionalfixed64extension;
import static protobuf_unittest.unittestproto.optionalsfixed32extension;
import static protobuf_unittest.unittestproto.optionalsfixed64extension;
import static protobuf_unittest.unittestproto.optionalfloatextension;
import static protobuf_unittest.unittestproto.optionaldoubleextension;
import static protobuf_unittest.unittestproto.optionalboolextension;
import static protobuf_unittest.unittestproto.optionalstringextension;
import static protobuf_unittest.unittestproto.optionalbytesextension;
import static protobuf_unittest.unittestproto.optionalgroupextension;
import static protobuf_unittest.unittestproto.optionalcordextension;
import static protobuf_unittest.unittestproto.optionalforeignenumextension;
import static protobuf_unittest.unittestproto.optionalforeignmessageextension;
import static protobuf_unittest.unittestproto.optionalimportenumextension;
import static protobuf_unittest.unittestproto.optionalimportmessageextension;
import static protobuf_unittest.unittestproto.optionalnestedenumextension;
import static protobuf_unittest.unittestproto.optionalnestedmessageextension;
import static protobuf_unittest.unittestproto.optionalpublicimportmessageextension;
import static protobuf_unittest.unittestproto.optionallazymessageextension;
import static protobuf_unittest.unittestproto.optionalstringpieceextension;

import static protobuf_unittest.unittestproto.repeatedint32extension;
import static protobuf_unittest.unittestproto.repeatedint64extension;
import static protobuf_unittest.unittestproto.repeateduint32extension;
import static protobuf_unittest.unittestproto.repeateduint64extension;
import static protobuf_unittest.unittestproto.repeatedsint32extension;
import static protobuf_unittest.unittestproto.repeatedsint64extension;
import static protobuf_unittest.unittestproto.repeatedfixed32extension;
import static protobuf_unittest.unittestproto.repeatedfixed64extension;
import static protobuf_unittest.unittestproto.repeatedsfixed32extension;
import static protobuf_unittest.unittestproto.repeatedsfixed64extension;
import static protobuf_unittest.unittestproto.repeatedfloatextension;
import static protobuf_unittest.unittestproto.repeateddoubleextension;
import static protobuf_unittest.unittestproto.repeatedboolextension;
import static protobuf_unittest.unittestproto.repeatedstringextension;
import static protobuf_unittest.unittestproto.repeatedbytesextension;
import static protobuf_unittest.unittestproto.repeatedgroupextension;
import static protobuf_unittest.unittestproto.repeatednestedmessageextension;
import static protobuf_unittest.unittestproto.repeatedforeignmessageextension;
import static protobuf_unittest.unittestproto.repeatedimportmessageextension;
import static protobuf_unittest.unittestproto.repeatedlazymessageextension;
import static protobuf_unittest.unittestproto.repeatednestedenumextension;
import static protobuf_unittest.unittestproto.repeatedforeignenumextension;
import static protobuf_unittest.unittestproto.repeatedimportenumextension;
import static protobuf_unittest.unittestproto.repeatedstringpieceextension;
import static protobuf_unittest.unittestproto.repeatedcordextension;

import static protobuf_unittest.unittestproto.optionalgroup_extension;
import static protobuf_unittest.unittestproto.repeatedgroup_extension;

import static protobuf_unittest.unittestproto.packedint32extension;
import static protobuf_unittest.unittestproto.packedint64extension;
import static protobuf_unittest.unittestproto.packeduint32extension;
import static protobuf_unittest.unittestproto.packeduint64extension;
import static protobuf_unittest.unittestproto.packedsint32extension;
import static protobuf_unittest.unittestproto.packedsint64extension;
import static protobuf_unittest.unittestproto.packedfixed32extension;
import static protobuf_unittest.unittestproto.packedfixed64extension;
import static protobuf_unittest.unittestproto.packedsfixed32extension;
import static protobuf_unittest.unittestproto.packedsfixed64extension;
import static protobuf_unittest.unittestproto.packedfloatextension;
import static protobuf_unittest.unittestproto.packeddoubleextension;
import static protobuf_unittest.unittestproto.packedboolextension;
import static protobuf_unittest.unittestproto.packedenumextension;

import static com.google.protobuf.unittestlite.defaultint32extensionlite;
import static com.google.protobuf.unittestlite.defaultint64extensionlite;
import static com.google.protobuf.unittestlite.defaultuint32extensionlite;
import static com.google.protobuf.unittestlite.defaultuint64extensionlite;
import static com.google.protobuf.unittestlite.defaultsint32extensionlite;
import static com.google.protobuf.unittestlite.defaultsint64extensionlite;
import static com.google.protobuf.unittestlite.defaultfixed32extensionlite;
import static com.google.protobuf.unittestlite.defaultfixed64extensionlite;
import static com.google.protobuf.unittestlite.defaultsfixed32extensionlite;
import static com.google.protobuf.unittestlite.defaultsfixed64extensionlite;
import static com.google.protobuf.unittestlite.defaultfloatextensionlite;
import static com.google.protobuf.unittestlite.defaultdoubleextensionlite;
import static com.google.protobuf.unittestlite.defaultboolextensionlite;
import static com.google.protobuf.unittestlite.defaultstringextensionlite;
import static com.google.protobuf.unittestlite.defaultbytesextensionlite;
import static com.google.protobuf.unittestlite.defaultnestedenumextensionlite;
import static com.google.protobuf.unittestlite.defaultforeignenumextensionlite;
import static com.google.protobuf.unittestlite.defaultimportenumextensionlite;
import static com.google.protobuf.unittestlite.defaultstringpieceextensionlite;
import static com.google.protobuf.unittestlite.defaultcordextensionlite;

import static com.google.protobuf.unittestlite.optionalint32extensionlite;
import static com.google.protobuf.unittestlite.optionalint64extensionlite;
import static com.google.protobuf.unittestlite.optionaluint32extensionlite;
import static com.google.protobuf.unittestlite.optionaluint64extensionlite;
import static com.google.protobuf.unittestlite.optionalsint32extensionlite;
import static com.google.protobuf.unittestlite.optionalsint64extensionlite;
import static com.google.protobuf.unittestlite.optionalfixed32extensionlite;
import static com.google.protobuf.unittestlite.optionalfixed64extensionlite;
import static com.google.protobuf.unittestlite.optionalsfixed32extensionlite;
import static com.google.protobuf.unittestlite.optionalsfixed64extensionlite;
import static com.google.protobuf.unittestlite.optionalfloatextensionlite;
import static com.google.protobuf.unittestlite.optionaldoubleextensionlite;
import static com.google.protobuf.unittestlite.optionalboolextensionlite;
import static com.google.protobuf.unittestlite.optionalstringextensionlite;
import static com.google.protobuf.unittestlite.optionalbytesextensionlite;
import static com.google.protobuf.unittestlite.optionalgroupextensionlite;
import static com.google.protobuf.unittestlite.optionalnestedmessageextensionlite;
import static com.google.protobuf.unittestlite.optionalforeignenumextensionlite;
import static com.google.protobuf.unittestlite.optionalforeignmessageextensionlite;
import static com.google.protobuf.unittestlite.optionalimportenumextensionlite;
import static com.google.protobuf.unittestlite.optionalimportmessageextensionlite;
import static com.google.protobuf.unittestlite.optionalnestedenumextensionlite;
import static com.google.protobuf.unittestlite.optionalpublicimportmessageextensionlite;
import static com.google.protobuf.unittestlite.optionallazymessageextensionlite;
import static com.google.protobuf.unittestlite.optionalstringpieceextensionlite;
import static com.google.protobuf.unittestlite.optionalcordextensionlite;

import static com.google.protobuf.unittestlite.repeatedint32extensionlite;
import static com.google.protobuf.unittestlite.repeatedint64extensionlite;
import static com.google.protobuf.unittestlite.repeateduint32extensionlite;
import static com.google.protobuf.unittestlite.repeateduint64extensionlite;
import static com.google.protobuf.unittestlite.repeatedsint32extensionlite;
import static com.google.protobuf.unittestlite.repeatedsint64extensionlite;
import static com.google.protobuf.unittestlite.repeatedfixed32extensionlite;
import static com.google.protobuf.unittestlite.repeatedfixed64extensionlite;
import static com.google.protobuf.unittestlite.repeatedsfixed32extensionlite;
import static com.google.protobuf.unittestlite.repeatedsfixed64extensionlite;
import static com.google.protobuf.unittestlite.repeatedfloatextensionlite;
import static com.google.protobuf.unittestlite.repeateddoubleextensionlite;
import static com.google.protobuf.unittestlite.repeatedboolextensionlite;
import static com.google.protobuf.unittestlite.repeatedstringextensionlite;
import static com.google.protobuf.unittestlite.repeatedbytesextensionlite;
import static com.google.protobuf.unittestlite.repeatedgroupextensionlite;
import static com.google.protobuf.unittestlite.repeatednestedmessageextensionlite;
import static com.google.protobuf.unittestlite.repeatedforeignmessageextensionlite;
import static com.google.protobuf.unittestlite.repeatedimportmessageextensionlite;
import static com.google.protobuf.unittestlite.repeatedlazymessageextensionlite;
import static com.google.protobuf.unittestlite.repeatednestedenumextensionlite;
import static com.google.protobuf.unittestlite.repeatedforeignenumextensionlite;
import static com.google.protobuf.unittestlite.repeatedimportenumextensionlite;
import static com.google.protobuf.unittestlite.repeatedstringpieceextensionlite;
import static com.google.protobuf.unittestlite.repeatedcordextensionlite;

import static com.google.protobuf.unittestlite.optionalgroup_extension_lite;
import static com.google.protobuf.unittestlite.repeatedgroup_extension_lite;

import static com.google.protobuf.unittestlite.packedint32extensionlite;
import static com.google.protobuf.unittestlite.packedint64extensionlite;
import static com.google.protobuf.unittestlite.packeduint32extensionlite;
import static com.google.protobuf.unittestlite.packeduint64extensionlite;
import static com.google.protobuf.unittestlite.packedsint32extensionlite;
import static com.google.protobuf.unittestlite.packedsint64extensionlite;
import static com.google.protobuf.unittestlite.packedfixed32extensionlite;
import static com.google.protobuf.unittestlite.packedfixed64extensionlite;
import static com.google.protobuf.unittestlite.packedsfixed32extensionlite;
import static com.google.protobuf.unittestlite.packedsfixed64extensionlite;
import static com.google.protobuf.unittestlite.packedfloatextensionlite;
import static com.google.protobuf.unittestlite.packeddoubleextensionlite;
import static com.google.protobuf.unittestlite.packedboolextensionlite;
import static com.google.protobuf.unittestlite.packedenumextensionlite;

import protobuf_unittest.unittestproto.testallextensions;
import protobuf_unittest.unittestproto.testallextensionsorbuilder;
import protobuf_unittest.unittestproto.testalltypes;
import protobuf_unittest.unittestproto.testalltypesorbuilder;
import protobuf_unittest.unittestproto.testpackedextensions;
import protobuf_unittest.unittestproto.testpackedtypes;
import protobuf_unittest.unittestproto.testunpackedtypes;
import protobuf_unittest.unittestproto.foreignmessage;
import protobuf_unittest.unittestproto.foreignenum;
import com.google.protobuf.test.unittestimport.importenum;
import com.google.protobuf.test.unittestimport.importmessage;
import com.google.protobuf.test.unittestimportpublic.publicimportmessage;

import com.google.protobuf.unittestlite.testalltypeslite;
import com.google.protobuf.unittestlite.testallextensionslite;
import com.google.protobuf.unittestlite.testallextensionsliteorbuilder;
import com.google.protobuf.unittestlite.testpackedextensionslite;
import com.google.protobuf.unittestlite.foreignmessagelite;
import com.google.protobuf.unittestlite.foreignenumlite;
import com.google.protobuf.unittestimportlite.importenumlite;
import com.google.protobuf.unittestimportlite.importmessagelite;
import com.google.protobuf.unittestimportpubliclite.publicimportmessagelite;

import junit.framework.assert;

import java.io.file;
import java.io.ioexception;
import java.io.randomaccessfile;

/**
 * contains methods for setting all fields of {@code testalltypes} to
 * some values as well as checking that all the fields are set to those values.
 * these are useful for testing various protocol message features, e.g.
 * set all fields of a message, serialize it, parse it, and check that all
 * fields are set.
 *
 * <p>this code is not to be used outside of {@code com.google.protobuf} and
 * subpackages.
 *
 * @author kenton@google.com kenton varda
 */
public final class testutil {
  private testutil() {}

  /** helper to convert a string to bytestring. */
  static bytestring tobytes(string str) {
    try {
      return bytestring.copyfrom(str.getbytes("utf-8"));
    } catch(java.io.unsupportedencodingexception e) {
      throw new runtimeexception("utf-8 not supported.", e);
    }
  }

  /**
   * get a {@code testalltypes} with all fields set as they would be by
   * {@link #setallfields(testalltypes.builder)}.
   */
  public static testalltypes getallset() {
    testalltypes.builder builder = testalltypes.newbuilder();
    setallfields(builder);
    return builder.build();
  }

  /**
   * get a {@code testalltypes.builder} with all fields set as they would be by
   * {@link #setallfields(testalltypes.builder)}.
   */
  public static testalltypes.builder getallsetbuilder() {
    testalltypes.builder builder = testalltypes.newbuilder();
    setallfields(builder);
    return builder;
  }

  /**
   * get a {@code testallextensions} with all fields set as they would be by
   * {@link #setallextensions(testallextensions.builder)}.
   */
  public static testallextensions getallextensionsset() {
    testallextensions.builder builder = testallextensions.newbuilder();
    setallextensions(builder);
    return builder.build();
  }

  public static testallextensionslite getallliteextensionsset() {
    testallextensionslite.builder builder = testallextensionslite.newbuilder();
    setallextensions(builder);
    return builder.build();
  }

  public static testpackedtypes getpackedset() {
    testpackedtypes.builder builder = testpackedtypes.newbuilder();
    setpackedfields(builder);
    return builder.build();
  }

  public static testunpackedtypes getunpackedset() {
    testunpackedtypes.builder builder = testunpackedtypes.newbuilder();
    setunpackedfields(builder);
    return builder.build();
  }

  public static testpackedextensions getpackedextensionsset() {
    testpackedextensions.builder builder = testpackedextensions.newbuilder();
    setpackedextensions(builder);
    return builder.build();
  }

  public static testpackedextensionslite getlitepackedextensionsset() {
    testpackedextensionslite.builder builder =
        testpackedextensionslite.newbuilder();
    setpackedextensions(builder);
    return builder.build();
  }

  /**
   * set every field of {@code message} to the values expected by
   * {@code assertallfieldsset()}.
   */
  public static void setallfields(testalltypes.builder message) {
    message.setoptionalint32   (101);
    message.setoptionalint64   (102);
    message.setoptionaluint32  (103);
    message.setoptionaluint64  (104);
    message.setoptionalsint32  (105);
    message.setoptionalsint64  (106);
    message.setoptionalfixed32 (107);
    message.setoptionalfixed64 (108);
    message.setoptionalsfixed32(109);
    message.setoptionalsfixed64(110);
    message.setoptionalfloat   (111);
    message.setoptionaldouble  (112);
    message.setoptionalbool    (true);
    message.setoptionalstring  ("115");
    message.setoptionalbytes   (tobytes("116"));

    message.setoptionalgroup(
      testalltypes.optionalgroup.newbuilder().seta(117).build());
    message.setoptionalnestedmessage(
      testalltypes.nestedmessage.newbuilder().setbb(118).build());
    message.setoptionalforeignmessage(
      foreignmessage.newbuilder().setc(119).build());
    message.setoptionalimportmessage(
      importmessage.newbuilder().setd(120).build());
    message.setoptionalpublicimportmessage(
      publicimportmessage.newbuilder().sete(126).build());
    message.setoptionallazymessage(
      testalltypes.nestedmessage.newbuilder().setbb(127).build());

    message.setoptionalnestedenum (testalltypes.nestedenum.baz);
    message.setoptionalforeignenum(foreignenum.foreign_baz);
    message.setoptionalimportenum (importenum.import_baz);

    message.setoptionalstringpiece("124");
    message.setoptionalcord("125");

    // -----------------------------------------------------------------

    message.addrepeatedint32   (201);
    message.addrepeatedint64   (202);
    message.addrepeateduint32  (203);
    message.addrepeateduint64  (204);
    message.addrepeatedsint32  (205);
    message.addrepeatedsint64  (206);
    message.addrepeatedfixed32 (207);
    message.addrepeatedfixed64 (208);
    message.addrepeatedsfixed32(209);
    message.addrepeatedsfixed64(210);
    message.addrepeatedfloat   (211);
    message.addrepeateddouble  (212);
    message.addrepeatedbool    (true);
    message.addrepeatedstring  ("215");
    message.addrepeatedbytes   (tobytes("216"));

    message.addrepeatedgroup(
      testalltypes.repeatedgroup.newbuilder().seta(217).build());
    message.addrepeatednestedmessage(
      testalltypes.nestedmessage.newbuilder().setbb(218).build());
    message.addrepeatedforeignmessage(
      foreignmessage.newbuilder().setc(219).build());
    message.addrepeatedimportmessage(
      importmessage.newbuilder().setd(220).build());
    message.addrepeatedlazymessage(
      testalltypes.nestedmessage.newbuilder().setbb(227).build());

    message.addrepeatednestedenum (testalltypes.nestedenum.bar);
    message.addrepeatedforeignenum(foreignenum.foreign_bar);
    message.addrepeatedimportenum (importenum.import_bar);

    message.addrepeatedstringpiece("224");
    message.addrepeatedcord("225");

    // add a second one of each field.
    message.addrepeatedint32   (301);
    message.addrepeatedint64   (302);
    message.addrepeateduint32  (303);
    message.addrepeateduint64  (304);
    message.addrepeatedsint32  (305);
    message.addrepeatedsint64  (306);
    message.addrepeatedfixed32 (307);
    message.addrepeatedfixed64 (308);
    message.addrepeatedsfixed32(309);
    message.addrepeatedsfixed64(310);
    message.addrepeatedfloat   (311);
    message.addrepeateddouble  (312);
    message.addrepeatedbool    (false);
    message.addrepeatedstring  ("315");
    message.addrepeatedbytes   (tobytes("316"));

    message.addrepeatedgroup(
      testalltypes.repeatedgroup.newbuilder().seta(317).build());
    message.addrepeatednestedmessage(
      testalltypes.nestedmessage.newbuilder().setbb(318).build());
    message.addrepeatedforeignmessage(
      foreignmessage.newbuilder().setc(319).build());
    message.addrepeatedimportmessage(
      importmessage.newbuilder().setd(320).build());
    message.addrepeatedlazymessage(
      testalltypes.nestedmessage.newbuilder().setbb(327).build());

    message.addrepeatednestedenum (testalltypes.nestedenum.baz);
    message.addrepeatedforeignenum(foreignenum.foreign_baz);
    message.addrepeatedimportenum (importenum.import_baz);

    message.addrepeatedstringpiece("324");
    message.addrepeatedcord("325");

    // -----------------------------------------------------------------

    message.setdefaultint32   (401);
    message.setdefaultint64   (402);
    message.setdefaultuint32  (403);
    message.setdefaultuint64  (404);
    message.setdefaultsint32  (405);
    message.setdefaultsint64  (406);
    message.setdefaultfixed32 (407);
    message.setdefaultfixed64 (408);
    message.setdefaultsfixed32(409);
    message.setdefaultsfixed64(410);
    message.setdefaultfloat   (411);
    message.setdefaultdouble  (412);
    message.setdefaultbool    (false);
    message.setdefaultstring  ("415");
    message.setdefaultbytes   (tobytes("416"));

    message.setdefaultnestedenum (testalltypes.nestedenum.foo);
    message.setdefaultforeignenum(foreignenum.foreign_foo);
    message.setdefaultimportenum (importenum.import_foo);

    message.setdefaultstringpiece("424");
    message.setdefaultcord("425");
  }

  // -------------------------------------------------------------------

  /**
   * modify the repeated fields of {@code message} to contain the values
   * expected by {@code assertrepeatedfieldsmodified()}.
   */
  public static void modifyrepeatedfields(testalltypes.builder message) {
    message.setrepeatedint32   (1, 501);
    message.setrepeatedint64   (1, 502);
    message.setrepeateduint32  (1, 503);
    message.setrepeateduint64  (1, 504);
    message.setrepeatedsint32  (1, 505);
    message.setrepeatedsint64  (1, 506);
    message.setrepeatedfixed32 (1, 507);
    message.setrepeatedfixed64 (1, 508);
    message.setrepeatedsfixed32(1, 509);
    message.setrepeatedsfixed64(1, 510);
    message.setrepeatedfloat   (1, 511);
    message.setrepeateddouble  (1, 512);
    message.setrepeatedbool    (1, true);
    message.setrepeatedstring  (1, "515");
    message.setrepeatedbytes   (1, tobytes("516"));

    message.setrepeatedgroup(1,
      testalltypes.repeatedgroup.newbuilder().seta(517).build());
    message.setrepeatednestedmessage(1,
      testalltypes.nestedmessage.newbuilder().setbb(518).build());
    message.setrepeatedforeignmessage(1,
      foreignmessage.newbuilder().setc(519).build());
    message.setrepeatedimportmessage(1,
      importmessage.newbuilder().setd(520).build());
    message.setrepeatedlazymessage(1,
      testalltypes.nestedmessage.newbuilder().setbb(527).build());

    message.setrepeatednestedenum (1, testalltypes.nestedenum.foo);
    message.setrepeatedforeignenum(1, foreignenum.foreign_foo);
    message.setrepeatedimportenum (1, importenum.import_foo);

    message.setrepeatedstringpiece(1, "524");
    message.setrepeatedcord(1, "525");
  }

  // -------------------------------------------------------------------

  /**
   * assert (using {@code junit.framework.assert}} that all fields of
   * {@code message} are set to the values assigned by {@code setallfields}.
   */
  public static void assertallfieldsset(testalltypesorbuilder message) {
    assert.asserttrue(message.hasoptionalint32   ());
    assert.asserttrue(message.hasoptionalint64   ());
    assert.asserttrue(message.hasoptionaluint32  ());
    assert.asserttrue(message.hasoptionaluint64  ());
    assert.asserttrue(message.hasoptionalsint32  ());
    assert.asserttrue(message.hasoptionalsint64  ());
    assert.asserttrue(message.hasoptionalfixed32 ());
    assert.asserttrue(message.hasoptionalfixed64 ());
    assert.asserttrue(message.hasoptionalsfixed32());
    assert.asserttrue(message.hasoptionalsfixed64());
    assert.asserttrue(message.hasoptionalfloat   ());
    assert.asserttrue(message.hasoptionaldouble  ());
    assert.asserttrue(message.hasoptionalbool    ());
    assert.asserttrue(message.hasoptionalstring  ());
    assert.asserttrue(message.hasoptionalbytes   ());

    assert.asserttrue(message.hasoptionalgroup         ());
    assert.asserttrue(message.hasoptionalnestedmessage ());
    assert.asserttrue(message.hasoptionalforeignmessage());
    assert.asserttrue(message.hasoptionalimportmessage ());

    assert.asserttrue(message.getoptionalgroup         ().hasa());
    assert.asserttrue(message.getoptionalnestedmessage ().hasbb());
    assert.asserttrue(message.getoptionalforeignmessage().hasc());
    assert.asserttrue(message.getoptionalimportmessage ().hasd());

    assert.asserttrue(message.hasoptionalnestedenum ());
    assert.asserttrue(message.hasoptionalforeignenum());
    assert.asserttrue(message.hasoptionalimportenum ());

    assert.asserttrue(message.hasoptionalstringpiece());
    assert.asserttrue(message.hasoptionalcord());

    assert.assertequals(101  , message.getoptionalint32   ());
    assert.assertequals(102  , message.getoptionalint64   ());
    assert.assertequals(103  , message.getoptionaluint32  ());
    assert.assertequals(104  , message.getoptionaluint64  ());
    assert.assertequals(105  , message.getoptionalsint32  ());
    assert.assertequals(106  , message.getoptionalsint64  ());
    assert.assertequals(107  , message.getoptionalfixed32 ());
    assert.assertequals(108  , message.getoptionalfixed64 ());
    assert.assertequals(109  , message.getoptionalsfixed32());
    assert.assertequals(110  , message.getoptionalsfixed64());
    assert.assertequals(111  , message.getoptionalfloat   (), 0.0);
    assert.assertequals(112  , message.getoptionaldouble  (), 0.0);
    assert.assertequals(true , message.getoptionalbool    ());
    assert.assertequals("115", message.getoptionalstring  ());
    assert.assertequals(tobytes("116"), message.getoptionalbytes());

    assert.assertequals(117, message.getoptionalgroup              ().geta());
    assert.assertequals(118, message.getoptionalnestedmessage      ().getbb());
    assert.assertequals(119, message.getoptionalforeignmessage     ().getc());
    assert.assertequals(120, message.getoptionalimportmessage      ().getd());
    assert.assertequals(126, message.getoptionalpublicimportmessage().gete());
    assert.assertequals(127, message.getoptionallazymessage        ().getbb());

    assert.assertequals(testalltypes.nestedenum.baz, message.getoptionalnestedenum());
    assert.assertequals(foreignenum.foreign_baz, message.getoptionalforeignenum());
    assert.assertequals(importenum.import_baz, message.getoptionalimportenum());

    assert.assertequals("124", message.getoptionalstringpiece());
    assert.assertequals("125", message.getoptionalcord());

    // -----------------------------------------------------------------

    assert.assertequals(2, message.getrepeatedint32count   ());
    assert.assertequals(2, message.getrepeatedint64count   ());
    assert.assertequals(2, message.getrepeateduint32count  ());
    assert.assertequals(2, message.getrepeateduint64count  ());
    assert.assertequals(2, message.getrepeatedsint32count  ());
    assert.assertequals(2, message.getrepeatedsint64count  ());
    assert.assertequals(2, message.getrepeatedfixed32count ());
    assert.assertequals(2, message.getrepeatedfixed64count ());
    assert.assertequals(2, message.getrepeatedsfixed32count());
    assert.assertequals(2, message.getrepeatedsfixed64count());
    assert.assertequals(2, message.getrepeatedfloatcount   ());
    assert.assertequals(2, message.getrepeateddoublecount  ());
    assert.assertequals(2, message.getrepeatedboolcount    ());
    assert.assertequals(2, message.getrepeatedstringcount  ());
    assert.assertequals(2, message.getrepeatedbytescount   ());

    assert.assertequals(2, message.getrepeatedgroupcount         ());
    assert.assertequals(2, message.getrepeatednestedmessagecount ());
    assert.assertequals(2, message.getrepeatedforeignmessagecount());
    assert.assertequals(2, message.getrepeatedimportmessagecount ());
    assert.assertequals(2, message.getrepeatedlazymessagecount   ());
    assert.assertequals(2, message.getrepeatednestedenumcount    ());
    assert.assertequals(2, message.getrepeatedforeignenumcount   ());
    assert.assertequals(2, message.getrepeatedimportenumcount    ());

    assert.assertequals(2, message.getrepeatedstringpiececount());
    assert.assertequals(2, message.getrepeatedcordcount());

    assert.assertequals(201  , message.getrepeatedint32   (0));
    assert.assertequals(202  , message.getrepeatedint64   (0));
    assert.assertequals(203  , message.getrepeateduint32  (0));
    assert.assertequals(204  , message.getrepeateduint64  (0));
    assert.assertequals(205  , message.getrepeatedsint32  (0));
    assert.assertequals(206  , message.getrepeatedsint64  (0));
    assert.assertequals(207  , message.getrepeatedfixed32 (0));
    assert.assertequals(208  , message.getrepeatedfixed64 (0));
    assert.assertequals(209  , message.getrepeatedsfixed32(0));
    assert.assertequals(210  , message.getrepeatedsfixed64(0));
    assert.assertequals(211  , message.getrepeatedfloat   (0), 0.0);
    assert.assertequals(212  , message.getrepeateddouble  (0), 0.0);
    assert.assertequals(true , message.getrepeatedbool    (0));
    assert.assertequals("215", message.getrepeatedstring  (0));
    assert.assertequals(tobytes("216"), message.getrepeatedbytes(0));

    assert.assertequals(217, message.getrepeatedgroup         (0).geta());
    assert.assertequals(218, message.getrepeatednestedmessage (0).getbb());
    assert.assertequals(219, message.getrepeatedforeignmessage(0).getc());
    assert.assertequals(220, message.getrepeatedimportmessage (0).getd());
    assert.assertequals(227, message.getrepeatedlazymessage   (0).getbb());

    assert.assertequals(testalltypes.nestedenum.bar, message.getrepeatednestedenum (0));
    assert.assertequals(foreignenum.foreign_bar, message.getrepeatedforeignenum(0));
    assert.assertequals(importenum.import_bar, message.getrepeatedimportenum(0));

    assert.assertequals("224", message.getrepeatedstringpiece(0));
    assert.assertequals("225", message.getrepeatedcord(0));

    assert.assertequals(301  , message.getrepeatedint32   (1));
    assert.assertequals(302  , message.getrepeatedint64   (1));
    assert.assertequals(303  , message.getrepeateduint32  (1));
    assert.assertequals(304  , message.getrepeateduint64  (1));
    assert.assertequals(305  , message.getrepeatedsint32  (1));
    assert.assertequals(306  , message.getrepeatedsint64  (1));
    assert.assertequals(307  , message.getrepeatedfixed32 (1));
    assert.assertequals(308  , message.getrepeatedfixed64 (1));
    assert.assertequals(309  , message.getrepeatedsfixed32(1));
    assert.assertequals(310  , message.getrepeatedsfixed64(1));
    assert.assertequals(311  , message.getrepeatedfloat   (1), 0.0);
    assert.assertequals(312  , message.getrepeateddouble  (1), 0.0);
    assert.assertequals(false, message.getrepeatedbool    (1));
    assert.assertequals("315", message.getrepeatedstring  (1));
    assert.assertequals(tobytes("316"), message.getrepeatedbytes(1));

    assert.assertequals(317, message.getrepeatedgroup         (1).geta());
    assert.assertequals(318, message.getrepeatednestedmessage (1).getbb());
    assert.assertequals(319, message.getrepeatedforeignmessage(1).getc());
    assert.assertequals(320, message.getrepeatedimportmessage (1).getd());
    assert.assertequals(327, message.getrepeatedlazymessage   (1).getbb());

    assert.assertequals(testalltypes.nestedenum.baz, message.getrepeatednestedenum (1));
    assert.assertequals(foreignenum.foreign_baz, message.getrepeatedforeignenum(1));
    assert.assertequals(importenum.import_baz, message.getrepeatedimportenum(1));

    assert.assertequals("324", message.getrepeatedstringpiece(1));
    assert.assertequals("325", message.getrepeatedcord(1));

    // -----------------------------------------------------------------

    assert.asserttrue(message.hasdefaultint32   ());
    assert.asserttrue(message.hasdefaultint64   ());
    assert.asserttrue(message.hasdefaultuint32  ());
    assert.asserttrue(message.hasdefaultuint64  ());
    assert.asserttrue(message.hasdefaultsint32  ());
    assert.asserttrue(message.hasdefaultsint64  ());
    assert.asserttrue(message.hasdefaultfixed32 ());
    assert.asserttrue(message.hasdefaultfixed64 ());
    assert.asserttrue(message.hasdefaultsfixed32());
    assert.asserttrue(message.hasdefaultsfixed64());
    assert.asserttrue(message.hasdefaultfloat   ());
    assert.asserttrue(message.hasdefaultdouble  ());
    assert.asserttrue(message.hasdefaultbool    ());
    assert.asserttrue(message.hasdefaultstring  ());
    assert.asserttrue(message.hasdefaultbytes   ());

    assert.asserttrue(message.hasdefaultnestedenum ());
    assert.asserttrue(message.hasdefaultforeignenum());
    assert.asserttrue(message.hasdefaultimportenum ());

    assert.asserttrue(message.hasdefaultstringpiece());
    assert.asserttrue(message.hasdefaultcord());

    assert.assertequals(401  , message.getdefaultint32   ());
    assert.assertequals(402  , message.getdefaultint64   ());
    assert.assertequals(403  , message.getdefaultuint32  ());
    assert.assertequals(404  , message.getdefaultuint64  ());
    assert.assertequals(405  , message.getdefaultsint32  ());
    assert.assertequals(406  , message.getdefaultsint64  ());
    assert.assertequals(407  , message.getdefaultfixed32 ());
    assert.assertequals(408  , message.getdefaultfixed64 ());
    assert.assertequals(409  , message.getdefaultsfixed32());
    assert.assertequals(410  , message.getdefaultsfixed64());
    assert.assertequals(411  , message.getdefaultfloat   (), 0.0);
    assert.assertequals(412  , message.getdefaultdouble  (), 0.0);
    assert.assertequals(false, message.getdefaultbool    ());
    assert.assertequals("415", message.getdefaultstring  ());
    assert.assertequals(tobytes("416"), message.getdefaultbytes());

    assert.assertequals(testalltypes.nestedenum.foo, message.getdefaultnestedenum ());
    assert.assertequals(foreignenum.foreign_foo, message.getdefaultforeignenum());
    assert.assertequals(importenum.import_foo, message.getdefaultimportenum());

    assert.assertequals("424", message.getdefaultstringpiece());
    assert.assertequals("425", message.getdefaultcord());
  }

  // -------------------------------------------------------------------
  /**
   * assert (using {@code junit.framework.assert}} that all fields of
   * {@code message} are cleared, and that getting the fields returns their
   * default values.
   */
  public static void assertclear(testalltypesorbuilder message) {
    // hasblah() should initially be false for all optional fields.
    assert.assertfalse(message.hasoptionalint32   ());
    assert.assertfalse(message.hasoptionalint64   ());
    assert.assertfalse(message.hasoptionaluint32  ());
    assert.assertfalse(message.hasoptionaluint64  ());
    assert.assertfalse(message.hasoptionalsint32  ());
    assert.assertfalse(message.hasoptionalsint64  ());
    assert.assertfalse(message.hasoptionalfixed32 ());
    assert.assertfalse(message.hasoptionalfixed64 ());
    assert.assertfalse(message.hasoptionalsfixed32());
    assert.assertfalse(message.hasoptionalsfixed64());
    assert.assertfalse(message.hasoptionalfloat   ());
    assert.assertfalse(message.hasoptionaldouble  ());
    assert.assertfalse(message.hasoptionalbool    ());
    assert.assertfalse(message.hasoptionalstring  ());
    assert.assertfalse(message.hasoptionalbytes   ());

    assert.assertfalse(message.hasoptionalgroup         ());
    assert.assertfalse(message.hasoptionalnestedmessage ());
    assert.assertfalse(message.hasoptionalforeignmessage());
    assert.assertfalse(message.hasoptionalimportmessage ());

    assert.assertfalse(message.hasoptionalnestedenum ());
    assert.assertfalse(message.hasoptionalforeignenum());
    assert.assertfalse(message.hasoptionalimportenum ());

    assert.assertfalse(message.hasoptionalstringpiece());
    assert.assertfalse(message.hasoptionalcord());

    // optional fields without defaults are set to zero or something like it.
    assert.assertequals(0    , message.getoptionalint32   ());
    assert.assertequals(0    , message.getoptionalint64   ());
    assert.assertequals(0    , message.getoptionaluint32  ());
    assert.assertequals(0    , message.getoptionaluint64  ());
    assert.assertequals(0    , message.getoptionalsint32  ());
    assert.assertequals(0    , message.getoptionalsint64  ());
    assert.assertequals(0    , message.getoptionalfixed32 ());
    assert.assertequals(0    , message.getoptionalfixed64 ());
    assert.assertequals(0    , message.getoptionalsfixed32());
    assert.assertequals(0    , message.getoptionalsfixed64());
    assert.assertequals(0    , message.getoptionalfloat   (), 0.0);
    assert.assertequals(0    , message.getoptionaldouble  (), 0.0);
    assert.assertequals(false, message.getoptionalbool    ());
    assert.assertequals(""   , message.getoptionalstring  ());
    assert.assertequals(bytestring.empty, message.getoptionalbytes());

    // embedded messages should also be clear.
    assert.assertfalse(message.getoptionalgroup              ().hasa());
    assert.assertfalse(message.getoptionalnestedmessage      ().hasbb());
    assert.assertfalse(message.getoptionalforeignmessage     ().hasc());
    assert.assertfalse(message.getoptionalimportmessage      ().hasd());
    assert.assertfalse(message.getoptionalpublicimportmessage().hase());
    assert.assertfalse(message.getoptionallazymessage        ().hasbb());

    assert.assertequals(0, message.getoptionalgroup              ().geta());
    assert.assertequals(0, message.getoptionalnestedmessage      ().getbb());
    assert.assertequals(0, message.getoptionalforeignmessage     ().getc());
    assert.assertequals(0, message.getoptionalimportmessage      ().getd());
    assert.assertequals(0, message.getoptionalpublicimportmessage().gete());
    assert.assertequals(0, message.getoptionallazymessage        ().getbb());

    // enums without defaults are set to the first value in the enum.
    assert.assertequals(testalltypes.nestedenum.foo, message.getoptionalnestedenum ());
    assert.assertequals(foreignenum.foreign_foo, message.getoptionalforeignenum());
    assert.assertequals(importenum.import_foo, message.getoptionalimportenum());

    assert.assertequals("", message.getoptionalstringpiece());
    assert.assertequals("", message.getoptionalcord());

    // repeated fields are empty.
    assert.assertequals(0, message.getrepeatedint32count   ());
    assert.assertequals(0, message.getrepeatedint64count   ());
    assert.assertequals(0, message.getrepeateduint32count  ());
    assert.assertequals(0, message.getrepeateduint64count  ());
    assert.assertequals(0, message.getrepeatedsint32count  ());
    assert.assertequals(0, message.getrepeatedsint64count  ());
    assert.assertequals(0, message.getrepeatedfixed32count ());
    assert.assertequals(0, message.getrepeatedfixed64count ());
    assert.assertequals(0, message.getrepeatedsfixed32count());
    assert.assertequals(0, message.getrepeatedsfixed64count());
    assert.assertequals(0, message.getrepeatedfloatcount   ());
    assert.assertequals(0, message.getrepeateddoublecount  ());
    assert.assertequals(0, message.getrepeatedboolcount    ());
    assert.assertequals(0, message.getrepeatedstringcount  ());
    assert.assertequals(0, message.getrepeatedbytescount   ());

    assert.assertequals(0, message.getrepeatedgroupcount         ());
    assert.assertequals(0, message.getrepeatednestedmessagecount ());
    assert.assertequals(0, message.getrepeatedforeignmessagecount());
    assert.assertequals(0, message.getrepeatedimportmessagecount ());
    assert.assertequals(0, message.getrepeatedlazymessagecount   ());
    assert.assertequals(0, message.getrepeatednestedenumcount    ());
    assert.assertequals(0, message.getrepeatedforeignenumcount   ());
    assert.assertequals(0, message.getrepeatedimportenumcount    ());

    assert.assertequals(0, message.getrepeatedstringpiececount());
    assert.assertequals(0, message.getrepeatedcordcount());

    // hasblah() should also be false for all default fields.
    assert.assertfalse(message.hasdefaultint32   ());
    assert.assertfalse(message.hasdefaultint64   ());
    assert.assertfalse(message.hasdefaultuint32  ());
    assert.assertfalse(message.hasdefaultuint64  ());
    assert.assertfalse(message.hasdefaultsint32  ());
    assert.assertfalse(message.hasdefaultsint64  ());
    assert.assertfalse(message.hasdefaultfixed32 ());
    assert.assertfalse(message.hasdefaultfixed64 ());
    assert.assertfalse(message.hasdefaultsfixed32());
    assert.assertfalse(message.hasdefaultsfixed64());
    assert.assertfalse(message.hasdefaultfloat   ());
    assert.assertfalse(message.hasdefaultdouble  ());
    assert.assertfalse(message.hasdefaultbool    ());
    assert.assertfalse(message.hasdefaultstring  ());
    assert.assertfalse(message.hasdefaultbytes   ());

    assert.assertfalse(message.hasdefaultnestedenum ());
    assert.assertfalse(message.hasdefaultforeignenum());
    assert.assertfalse(message.hasdefaultimportenum ());

    assert.assertfalse(message.hasdefaultstringpiece());
    assert.assertfalse(message.hasdefaultcord());

    // fields with defaults have their default values (duh).
    assert.assertequals( 41    , message.getdefaultint32   ());
    assert.assertequals( 42    , message.getdefaultint64   ());
    assert.assertequals( 43    , message.getdefaultuint32  ());
    assert.assertequals( 44    , message.getdefaultuint64  ());
    assert.assertequals(-45    , message.getdefaultsint32  ());
    assert.assertequals( 46    , message.getdefaultsint64  ());
    assert.assertequals( 47    , message.getdefaultfixed32 ());
    assert.assertequals( 48    , message.getdefaultfixed64 ());
    assert.assertequals( 49    , message.getdefaultsfixed32());
    assert.assertequals(-50    , message.getdefaultsfixed64());
    assert.assertequals( 51.5  , message.getdefaultfloat   (), 0.0);
    assert.assertequals( 52e3  , message.getdefaultdouble  (), 0.0);
    assert.assertequals(true   , message.getdefaultbool    ());
    assert.assertequals("hello", message.getdefaultstring  ());
    assert.assertequals(tobytes("world"), message.getdefaultbytes());

    assert.assertequals(testalltypes.nestedenum.bar, message.getdefaultnestedenum ());
    assert.assertequals(foreignenum.foreign_bar, message.getdefaultforeignenum());
    assert.assertequals(importenum.import_bar, message.getdefaultimportenum());

    assert.assertequals("abc", message.getdefaultstringpiece());
    assert.assertequals("123", message.getdefaultcord());
  }

  // -------------------------------------------------------------------

  /**
   * assert (using {@code junit.framework.assert}} that all fields of
   * {@code message} are set to the values assigned by {@code setallfields}
   * followed by {@code modifyrepeatedfields}.
   */
  public static void assertrepeatedfieldsmodified(
      testalltypesorbuilder message) {
    // modifyrepeatedfields only sets the second repeated element of each
    // field.  in addition to verifying this, we also verify that the first
    // element and size were *not* modified.
    assert.assertequals(2, message.getrepeatedint32count   ());
    assert.assertequals(2, message.getrepeatedint64count   ());
    assert.assertequals(2, message.getrepeateduint32count  ());
    assert.assertequals(2, message.getrepeateduint64count  ());
    assert.assertequals(2, message.getrepeatedsint32count  ());
    assert.assertequals(2, message.getrepeatedsint64count  ());
    assert.assertequals(2, message.getrepeatedfixed32count ());
    assert.assertequals(2, message.getrepeatedfixed64count ());
    assert.assertequals(2, message.getrepeatedsfixed32count());
    assert.assertequals(2, message.getrepeatedsfixed64count());
    assert.assertequals(2, message.getrepeatedfloatcount   ());
    assert.assertequals(2, message.getrepeateddoublecount  ());
    assert.assertequals(2, message.getrepeatedboolcount    ());
    assert.assertequals(2, message.getrepeatedstringcount  ());
    assert.assertequals(2, message.getrepeatedbytescount   ());

    assert.assertequals(2, message.getrepeatedgroupcount         ());
    assert.assertequals(2, message.getrepeatednestedmessagecount ());
    assert.assertequals(2, message.getrepeatedforeignmessagecount());
    assert.assertequals(2, message.getrepeatedimportmessagecount ());
    assert.assertequals(2, message.getrepeatedlazymessagecount   ());
    assert.assertequals(2, message.getrepeatednestedenumcount    ());
    assert.assertequals(2, message.getrepeatedforeignenumcount   ());
    assert.assertequals(2, message.getrepeatedimportenumcount    ());

    assert.assertequals(2, message.getrepeatedstringpiececount());
    assert.assertequals(2, message.getrepeatedcordcount());

    assert.assertequals(201  , message.getrepeatedint32   (0));
    assert.assertequals(202l , message.getrepeatedint64   (0));
    assert.assertequals(203  , message.getrepeateduint32  (0));
    assert.assertequals(204l , message.getrepeateduint64  (0));
    assert.assertequals(205  , message.getrepeatedsint32  (0));
    assert.assertequals(206l , message.getrepeatedsint64  (0));
    assert.assertequals(207  , message.getrepeatedfixed32 (0));
    assert.assertequals(208l , message.getrepeatedfixed64 (0));
    assert.assertequals(209  , message.getrepeatedsfixed32(0));
    assert.assertequals(210l , message.getrepeatedsfixed64(0));
    assert.assertequals(211f , message.getrepeatedfloat   (0));
    assert.assertequals(212d , message.getrepeateddouble  (0));
    assert.assertequals(true , message.getrepeatedbool    (0));
    assert.assertequals("215", message.getrepeatedstring  (0));
    assert.assertequals(tobytes("216"), message.getrepeatedbytes(0));

    assert.assertequals(217, message.getrepeatedgroup         (0).geta());
    assert.assertequals(218, message.getrepeatednestedmessage (0).getbb());
    assert.assertequals(219, message.getrepeatedforeignmessage(0).getc());
    assert.assertequals(220, message.getrepeatedimportmessage (0).getd());
    assert.assertequals(227, message.getrepeatedlazymessage   (0).getbb());

    assert.assertequals(testalltypes.nestedenum.bar, message.getrepeatednestedenum (0));
    assert.assertequals(foreignenum.foreign_bar, message.getrepeatedforeignenum(0));
    assert.assertequals(importenum.import_bar, message.getrepeatedimportenum(0));

    assert.assertequals("224", message.getrepeatedstringpiece(0));
    assert.assertequals("225", message.getrepeatedcord(0));

    // actually verify the second (modified) elements now.
    assert.assertequals(501  , message.getrepeatedint32   (1));
    assert.assertequals(502l , message.getrepeatedint64   (1));
    assert.assertequals(503  , message.getrepeateduint32  (1));
    assert.assertequals(504l , message.getrepeateduint64  (1));
    assert.assertequals(505  , message.getrepeatedsint32  (1));
    assert.assertequals(506l , message.getrepeatedsint64  (1));
    assert.assertequals(507  , message.getrepeatedfixed32 (1));
    assert.assertequals(508l , message.getrepeatedfixed64 (1));
    assert.assertequals(509  , message.getrepeatedsfixed32(1));
    assert.assertequals(510l , message.getrepeatedsfixed64(1));
    assert.assertequals(511f , message.getrepeatedfloat   (1));
    assert.assertequals(512d , message.getrepeateddouble  (1));
    assert.assertequals(true , message.getrepeatedbool    (1));
    assert.assertequals("515", message.getrepeatedstring  (1));
    assert.assertequals(tobytes("516"), message.getrepeatedbytes(1));

    assert.assertequals(517, message.getrepeatedgroup         (1).geta());
    assert.assertequals(518, message.getrepeatednestedmessage (1).getbb());
    assert.assertequals(519, message.getrepeatedforeignmessage(1).getc());
    assert.assertequals(520, message.getrepeatedimportmessage (1).getd());
    assert.assertequals(527, message.getrepeatedlazymessage   (1).getbb());

    assert.assertequals(testalltypes.nestedenum.foo, message.getrepeatednestedenum (1));
    assert.assertequals(foreignenum.foreign_foo, message.getrepeatedforeignenum(1));
    assert.assertequals(importenum.import_foo, message.getrepeatedimportenum(1));

    assert.assertequals("524", message.getrepeatedstringpiece(1));
    assert.assertequals("525", message.getrepeatedcord(1));
  }

  /**
   * set every field of {@code message} to a unique value.
   */
  public static void setpackedfields(testpackedtypes.builder message) {
    message.addpackedint32   (601);
    message.addpackedint64   (602);
    message.addpackeduint32  (603);
    message.addpackeduint64  (604);
    message.addpackedsint32  (605);
    message.addpackedsint64  (606);
    message.addpackedfixed32 (607);
    message.addpackedfixed64 (608);
    message.addpackedsfixed32(609);
    message.addpackedsfixed64(610);
    message.addpackedfloat   (611);
    message.addpackeddouble  (612);
    message.addpackedbool    (true);
    message.addpackedenum    (foreignenum.foreign_bar);
    // add a second one of each field.
    message.addpackedint32   (701);
    message.addpackedint64   (702);
    message.addpackeduint32  (703);
    message.addpackeduint64  (704);
    message.addpackedsint32  (705);
    message.addpackedsint64  (706);
    message.addpackedfixed32 (707);
    message.addpackedfixed64 (708);
    message.addpackedsfixed32(709);
    message.addpackedsfixed64(710);
    message.addpackedfloat   (711);
    message.addpackeddouble  (712);
    message.addpackedbool    (false);
    message.addpackedenum    (foreignenum.foreign_baz);
  }

  /**
   * set every field of {@code message} to a unique value. must correspond with
   * the values applied by {@code setpackedfields}.
   */
  public static void setunpackedfields(testunpackedtypes.builder message) {
    message.addunpackedint32   (601);
    message.addunpackedint64   (602);
    message.addunpackeduint32  (603);
    message.addunpackeduint64  (604);
    message.addunpackedsint32  (605);
    message.addunpackedsint64  (606);
    message.addunpackedfixed32 (607);
    message.addunpackedfixed64 (608);
    message.addunpackedsfixed32(609);
    message.addunpackedsfixed64(610);
    message.addunpackedfloat   (611);
    message.addunpackeddouble  (612);
    message.addunpackedbool    (true);
    message.addunpackedenum    (foreignenum.foreign_bar);
    // add a second one of each field.
    message.addunpackedint32   (701);
    message.addunpackedint64   (702);
    message.addunpackeduint32  (703);
    message.addunpackeduint64  (704);
    message.addunpackedsint32  (705);
    message.addunpackedsint64  (706);
    message.addunpackedfixed32 (707);
    message.addunpackedfixed64 (708);
    message.addunpackedsfixed32(709);
    message.addunpackedsfixed64(710);
    message.addunpackedfloat   (711);
    message.addunpackeddouble  (712);
    message.addunpackedbool    (false);
    message.addunpackedenum    (foreignenum.foreign_baz);
  }

  /**
   * assert (using {@code junit.framework.assert}} that all fields of
   * {@code message} are set to the values assigned by {@code setpackedfields}.
   */
  public static void assertpackedfieldsset(testpackedtypes message) {
    assert.assertequals(2, message.getpackedint32count   ());
    assert.assertequals(2, message.getpackedint64count   ());
    assert.assertequals(2, message.getpackeduint32count  ());
    assert.assertequals(2, message.getpackeduint64count  ());
    assert.assertequals(2, message.getpackedsint32count  ());
    assert.assertequals(2, message.getpackedsint64count  ());
    assert.assertequals(2, message.getpackedfixed32count ());
    assert.assertequals(2, message.getpackedfixed64count ());
    assert.assertequals(2, message.getpackedsfixed32count());
    assert.assertequals(2, message.getpackedsfixed64count());
    assert.assertequals(2, message.getpackedfloatcount   ());
    assert.assertequals(2, message.getpackeddoublecount  ());
    assert.assertequals(2, message.getpackedboolcount    ());
    assert.assertequals(2, message.getpackedenumcount   ());
    assert.assertequals(601  , message.getpackedint32   (0));
    assert.assertequals(602  , message.getpackedint64   (0));
    assert.assertequals(603  , message.getpackeduint32  (0));
    assert.assertequals(604  , message.getpackeduint64  (0));
    assert.assertequals(605  , message.getpackedsint32  (0));
    assert.assertequals(606  , message.getpackedsint64  (0));
    assert.assertequals(607  , message.getpackedfixed32 (0));
    assert.assertequals(608  , message.getpackedfixed64 (0));
    assert.assertequals(609  , message.getpackedsfixed32(0));
    assert.assertequals(610  , message.getpackedsfixed64(0));
    assert.assertequals(611  , message.getpackedfloat   (0), 0.0);
    assert.assertequals(612  , message.getpackeddouble  (0), 0.0);
    assert.assertequals(true , message.getpackedbool    (0));
    assert.assertequals(foreignenum.foreign_bar, message.getpackedenum(0));
    assert.assertequals(701  , message.getpackedint32   (1));
    assert.assertequals(702  , message.getpackedint64   (1));
    assert.assertequals(703  , message.getpackeduint32  (1));
    assert.assertequals(704  , message.getpackeduint64  (1));
    assert.assertequals(705  , message.getpackedsint32  (1));
    assert.assertequals(706  , message.getpackedsint64  (1));
    assert.assertequals(707  , message.getpackedfixed32 (1));
    assert.assertequals(708  , message.getpackedfixed64 (1));
    assert.assertequals(709  , message.getpackedsfixed32(1));
    assert.assertequals(710  , message.getpackedsfixed64(1));
    assert.assertequals(711  , message.getpackedfloat   (1), 0.0);
    assert.assertequals(712  , message.getpackeddouble  (1), 0.0);
    assert.assertequals(false, message.getpackedbool    (1));
    assert.assertequals(foreignenum.foreign_baz, message.getpackedenum(1));
  }

  /**
   * assert (using {@code junit.framework.assert}} that all fields of
   * {@code message} are set to the values assigned by {@code setunpackedfields}.
   */
  public static void assertunpackedfieldsset(testunpackedtypes message) {
    assert.assertequals(2, message.getunpackedint32count   ());
    assert.assertequals(2, message.getunpackedint64count   ());
    assert.assertequals(2, message.getunpackeduint32count  ());
    assert.assertequals(2, message.getunpackeduint64count  ());
    assert.assertequals(2, message.getunpackedsint32count  ());
    assert.assertequals(2, message.getunpackedsint64count  ());
    assert.assertequals(2, message.getunpackedfixed32count ());
    assert.assertequals(2, message.getunpackedfixed64count ());
    assert.assertequals(2, message.getunpackedsfixed32count());
    assert.assertequals(2, message.getunpackedsfixed64count());
    assert.assertequals(2, message.getunpackedfloatcount   ());
    assert.assertequals(2, message.getunpackeddoublecount  ());
    assert.assertequals(2, message.getunpackedboolcount    ());
    assert.assertequals(2, message.getunpackedenumcount   ());
    assert.assertequals(601  , message.getunpackedint32   (0));
    assert.assertequals(602  , message.getunpackedint64   (0));
    assert.assertequals(603  , message.getunpackeduint32  (0));
    assert.assertequals(604  , message.getunpackeduint64  (0));
    assert.assertequals(605  , message.getunpackedsint32  (0));
    assert.assertequals(606  , message.getunpackedsint64  (0));
    assert.assertequals(607  , message.getunpackedfixed32 (0));
    assert.assertequals(608  , message.getunpackedfixed64 (0));
    assert.assertequals(609  , message.getunpackedsfixed32(0));
    assert.assertequals(610  , message.getunpackedsfixed64(0));
    assert.assertequals(611  , message.getunpackedfloat   (0), 0.0);
    assert.assertequals(612  , message.getunpackeddouble  (0), 0.0);
    assert.assertequals(true , message.getunpackedbool    (0));
    assert.assertequals(foreignenum.foreign_bar, message.getunpackedenum(0));
    assert.assertequals(701  , message.getunpackedint32   (1));
    assert.assertequals(702  , message.getunpackedint64   (1));
    assert.assertequals(703  , message.getunpackeduint32  (1));
    assert.assertequals(704  , message.getunpackeduint64  (1));
    assert.assertequals(705  , message.getunpackedsint32  (1));
    assert.assertequals(706  , message.getunpackedsint64  (1));
    assert.assertequals(707  , message.getunpackedfixed32 (1));
    assert.assertequals(708  , message.getunpackedfixed64 (1));
    assert.assertequals(709  , message.getunpackedsfixed32(1));
    assert.assertequals(710  , message.getunpackedsfixed64(1));
    assert.assertequals(711  , message.getunpackedfloat   (1), 0.0);
    assert.assertequals(712  , message.getunpackeddouble  (1), 0.0);
    assert.assertequals(false, message.getunpackedbool    (1));
    assert.assertequals(foreignenum.foreign_baz, message.getunpackedenum(1));
  }

  // ===================================================================
  // like above, but for extensions

  // java gets confused with things like assertequals(int, integer):  it can't
  // decide whether to call assertequals(int, int) or assertequals(object,
  // object).  so we define these methods to help it.
  private static void assertequalsexacttype(int a, int b) {
    assert.assertequals(a, b);
  }
  private static void assertequalsexacttype(long a, long b) {
    assert.assertequals(a, b);
  }
  private static void assertequalsexacttype(float a, float b) {
    assert.assertequals(a, b, 0.0);
  }
  private static void assertequalsexacttype(double a, double b) {
    assert.assertequals(a, b, 0.0);
  }
  private static void assertequalsexacttype(boolean a, boolean b) {
    assert.assertequals(a, b);
  }
  private static void assertequalsexacttype(string a, string b) {
    assert.assertequals(a, b);
  }
  private static void assertequalsexacttype(bytestring a, bytestring b) {
    assert.assertequals(a, b);
  }
  private static void assertequalsexacttype(testalltypes.nestedenum a,
                                            testalltypes.nestedenum b) {
    assert.assertequals(a, b);
  }
  private static void assertequalsexacttype(foreignenum a, foreignenum b) {
    assert.assertequals(a, b);
  }
  private static void assertequalsexacttype(importenum a, importenum b) {
    assert.assertequals(a, b);
  }
  private static void assertequalsexacttype(testalltypeslite.nestedenum a,
                                            testalltypeslite.nestedenum b) {
    assert.assertequals(a, b);
  }
  private static void assertequalsexacttype(foreignenumlite a,
                                            foreignenumlite b) {
    assert.assertequals(a, b);
  }
  private static void assertequalsexacttype(importenumlite a,
                                            importenumlite b) {
    assert.assertequals(a, b);
  }

  /**
   * get an unmodifiable {@link extensionregistry} containing all the
   * extensions of {@code testallextensions}.
   */
  public static extensionregistry getextensionregistry() {
    extensionregistry registry = extensionregistry.newinstance();
    registerallextensions(registry);
    return registry.getunmodifiable();
  }

  public static extensionregistrylite getextensionregistrylite() {
    extensionregistrylite registry = extensionregistrylite.newinstance();
    registerallextensionslite(registry);
    return registry.getunmodifiable();
  }

  /**
   * register all of {@code testallextensions}'s extensions with the
   * given {@link extensionregistry}.
   */
  public static void registerallextensions(extensionregistry registry) {
    unittestproto.registerallextensions(registry);
    registerallextensionslite(registry);
  }

  public static void registerallextensionslite(extensionregistrylite registry) {
    unittestlite.registerallextensions(registry);
  }

  /**
   * set every field of {@code message} to the values expected by
   * {@code assertallextensionsset()}.
   */
  public static void setallextensions(testallextensions.builder message) {
    message.setextension(optionalint32extension   , 101);
    message.setextension(optionalint64extension   , 102l);
    message.setextension(optionaluint32extension  , 103);
    message.setextension(optionaluint64extension  , 104l);
    message.setextension(optionalsint32extension  , 105);
    message.setextension(optionalsint64extension  , 106l);
    message.setextension(optionalfixed32extension , 107);
    message.setextension(optionalfixed64extension , 108l);
    message.setextension(optionalsfixed32extension, 109);
    message.setextension(optionalsfixed64extension, 110l);
    message.setextension(optionalfloatextension   , 111f);
    message.setextension(optionaldoubleextension  , 112d);
    message.setextension(optionalboolextension    , true);
    message.setextension(optionalstringextension  , "115");
    message.setextension(optionalbytesextension   , tobytes("116"));

    message.setextension(optionalgroupextension,
      optionalgroup_extension.newbuilder().seta(117).build());
    message.setextension(optionalnestedmessageextension,
      testalltypes.nestedmessage.newbuilder().setbb(118).build());
    message.setextension(optionalforeignmessageextension,
      foreignmessage.newbuilder().setc(119).build());
    message.setextension(optionalimportmessageextension,
      importmessage.newbuilder().setd(120).build());
    message.setextension(optionalpublicimportmessageextension,
      publicimportmessage.newbuilder().sete(126).build());
    message.setextension(optionallazymessageextension,
      testalltypes.nestedmessage.newbuilder().setbb(127).build());

    message.setextension(optionalnestedenumextension, testalltypes.nestedenum.baz);
    message.setextension(optionalforeignenumextension, foreignenum.foreign_baz);
    message.setextension(optionalimportenumextension, importenum.import_baz);

    message.setextension(optionalstringpieceextension, "124");
    message.setextension(optionalcordextension, "125");

    // -----------------------------------------------------------------

    message.addextension(repeatedint32extension   , 201);
    message.addextension(repeatedint64extension   , 202l);
    message.addextension(repeateduint32extension  , 203);
    message.addextension(repeateduint64extension  , 204l);
    message.addextension(repeatedsint32extension  , 205);
    message.addextension(repeatedsint64extension  , 206l);
    message.addextension(repeatedfixed32extension , 207);
    message.addextension(repeatedfixed64extension , 208l);
    message.addextension(repeatedsfixed32extension, 209);
    message.addextension(repeatedsfixed64extension, 210l);
    message.addextension(repeatedfloatextension   , 211f);
    message.addextension(repeateddoubleextension  , 212d);
    message.addextension(repeatedboolextension    , true);
    message.addextension(repeatedstringextension  , "215");
    message.addextension(repeatedbytesextension   , tobytes("216"));

    message.addextension(repeatedgroupextension,
      repeatedgroup_extension.newbuilder().seta(217).build());
    message.addextension(repeatednestedmessageextension,
      testalltypes.nestedmessage.newbuilder().setbb(218).build());
    message.addextension(repeatedforeignmessageextension,
      foreignmessage.newbuilder().setc(219).build());
    message.addextension(repeatedimportmessageextension,
      importmessage.newbuilder().setd(220).build());
    message.addextension(repeatedlazymessageextension,
      testalltypes.nestedmessage.newbuilder().setbb(227).build());

    message.addextension(repeatednestedenumextension, testalltypes.nestedenum.bar);
    message.addextension(repeatedforeignenumextension, foreignenum.foreign_bar);
    message.addextension(repeatedimportenumextension, importenum.import_bar);

    message.addextension(repeatedstringpieceextension, "224");
    message.addextension(repeatedcordextension, "225");

    // add a second one of each field.
    message.addextension(repeatedint32extension   , 301);
    message.addextension(repeatedint64extension   , 302l);
    message.addextension(repeateduint32extension  , 303);
    message.addextension(repeateduint64extension  , 304l);
    message.addextension(repeatedsint32extension  , 305);
    message.addextension(repeatedsint64extension  , 306l);
    message.addextension(repeatedfixed32extension , 307);
    message.addextension(repeatedfixed64extension , 308l);
    message.addextension(repeatedsfixed32extension, 309);
    message.addextension(repeatedsfixed64extension, 310l);
    message.addextension(repeatedfloatextension   , 311f);
    message.addextension(repeateddoubleextension  , 312d);
    message.addextension(repeatedboolextension    , false);
    message.addextension(repeatedstringextension  , "315");
    message.addextension(repeatedbytesextension   , tobytes("316"));

    message.addextension(repeatedgroupextension,
      repeatedgroup_extension.newbuilder().seta(317).build());
    message.addextension(repeatednestedmessageextension,
      testalltypes.nestedmessage.newbuilder().setbb(318).build());
    message.addextension(repeatedforeignmessageextension,
      foreignmessage.newbuilder().setc(319).build());
    message.addextension(repeatedimportmessageextension,
      importmessage.newbuilder().setd(320).build());
    message.addextension(repeatedlazymessageextension,
      testalltypes.nestedmessage.newbuilder().setbb(327).build());

    message.addextension(repeatednestedenumextension, testalltypes.nestedenum.baz);
    message.addextension(repeatedforeignenumextension, foreignenum.foreign_baz);
    message.addextension(repeatedimportenumextension, importenum.import_baz);

    message.addextension(repeatedstringpieceextension, "324");
    message.addextension(repeatedcordextension, "325");

    // -----------------------------------------------------------------

    message.setextension(defaultint32extension   , 401);
    message.setextension(defaultint64extension   , 402l);
    message.setextension(defaultuint32extension  , 403);
    message.setextension(defaultuint64extension  , 404l);
    message.setextension(defaultsint32extension  , 405);
    message.setextension(defaultsint64extension  , 406l);
    message.setextension(defaultfixed32extension , 407);
    message.setextension(defaultfixed64extension , 408l);
    message.setextension(defaultsfixed32extension, 409);
    message.setextension(defaultsfixed64extension, 410l);
    message.setextension(defaultfloatextension   , 411f);
    message.setextension(defaultdoubleextension  , 412d);
    message.setextension(defaultboolextension    , false);
    message.setextension(defaultstringextension  , "415");
    message.setextension(defaultbytesextension   , tobytes("416"));

    message.setextension(defaultnestedenumextension, testalltypes.nestedenum.foo);
    message.setextension(defaultforeignenumextension, foreignenum.foreign_foo);
    message.setextension(defaultimportenumextension, importenum.import_foo);

    message.setextension(defaultstringpieceextension, "424");
    message.setextension(defaultcordextension, "425");
  }

  // -------------------------------------------------------------------

  /**
   * modify the repeated extensions of {@code message} to contain the values
   * expected by {@code assertrepeatedextensionsmodified()}.
   */
  public static void modifyrepeatedextensions(
      testallextensions.builder message) {
    message.setextension(repeatedint32extension   , 1, 501);
    message.setextension(repeatedint64extension   , 1, 502l);
    message.setextension(repeateduint32extension  , 1, 503);
    message.setextension(repeateduint64extension  , 1, 504l);
    message.setextension(repeatedsint32extension  , 1, 505);
    message.setextension(repeatedsint64extension  , 1, 506l);
    message.setextension(repeatedfixed32extension , 1, 507);
    message.setextension(repeatedfixed64extension , 1, 508l);
    message.setextension(repeatedsfixed32extension, 1, 509);
    message.setextension(repeatedsfixed64extension, 1, 510l);
    message.setextension(repeatedfloatextension   , 1, 511f);
    message.setextension(repeateddoubleextension  , 1, 512d);
    message.setextension(repeatedboolextension    , 1, true);
    message.setextension(repeatedstringextension  , 1, "515");
    message.setextension(repeatedbytesextension   , 1, tobytes("516"));

    message.setextension(repeatedgroupextension, 1,
      repeatedgroup_extension.newbuilder().seta(517).build());
    message.setextension(repeatednestedmessageextension, 1,
      testalltypes.nestedmessage.newbuilder().setbb(518).build());
    message.setextension(repeatedforeignmessageextension, 1,
      foreignmessage.newbuilder().setc(519).build());
    message.setextension(repeatedimportmessageextension, 1,
      importmessage.newbuilder().setd(520).build());
    message.setextension(repeatedlazymessageextension, 1,
      testalltypes.nestedmessage.newbuilder().setbb(527).build());

    message.setextension(repeatednestedenumextension , 1, testalltypes.nestedenum.foo);
    message.setextension(repeatedforeignenumextension, 1, foreignenum.foreign_foo);
    message.setextension(repeatedimportenumextension , 1, importenum.import_foo);

    message.setextension(repeatedstringpieceextension, 1, "524");
    message.setextension(repeatedcordextension, 1, "525");
  }

  // -------------------------------------------------------------------

  /**
   * assert (using {@code junit.framework.assert}} that all extensions of
   * {@code message} are set to the values assigned by {@code setallextensions}.
   */
  public static void assertallextensionsset(
      testallextensionsorbuilder message) {
    assert.asserttrue(message.hasextension(optionalint32extension   ));
    assert.asserttrue(message.hasextension(optionalint64extension   ));
    assert.asserttrue(message.hasextension(optionaluint32extension  ));
    assert.asserttrue(message.hasextension(optionaluint64extension  ));
    assert.asserttrue(message.hasextension(optionalsint32extension  ));
    assert.asserttrue(message.hasextension(optionalsint64extension  ));
    assert.asserttrue(message.hasextension(optionalfixed32extension ));
    assert.asserttrue(message.hasextension(optionalfixed64extension ));
    assert.asserttrue(message.hasextension(optionalsfixed32extension));
    assert.asserttrue(message.hasextension(optionalsfixed64extension));
    assert.asserttrue(message.hasextension(optionalfloatextension   ));
    assert.asserttrue(message.hasextension(optionaldoubleextension  ));
    assert.asserttrue(message.hasextension(optionalboolextension    ));
    assert.asserttrue(message.hasextension(optionalstringextension  ));
    assert.asserttrue(message.hasextension(optionalbytesextension   ));

    assert.asserttrue(message.hasextension(optionalgroupextension         ));
    assert.asserttrue(message.hasextension(optionalnestedmessageextension ));
    assert.asserttrue(message.hasextension(optionalforeignmessageextension));
    assert.asserttrue(message.hasextension(optionalimportmessageextension ));

    assert.asserttrue(message.getextension(optionalgroupextension         ).hasa());
    assert.asserttrue(message.getextension(optionalnestedmessageextension ).hasbb());
    assert.asserttrue(message.getextension(optionalforeignmessageextension).hasc());
    assert.asserttrue(message.getextension(optionalimportmessageextension ).hasd());

    assert.asserttrue(message.hasextension(optionalnestedenumextension ));
    assert.asserttrue(message.hasextension(optionalforeignenumextension));
    assert.asserttrue(message.hasextension(optionalimportenumextension ));

    assert.asserttrue(message.hasextension(optionalstringpieceextension));
    assert.asserttrue(message.hasextension(optionalcordextension));

    assertequalsexacttype(101  , message.getextension(optionalint32extension   ));
    assertequalsexacttype(102l , message.getextension(optionalint64extension   ));
    assertequalsexacttype(103  , message.getextension(optionaluint32extension  ));
    assertequalsexacttype(104l , message.getextension(optionaluint64extension  ));
    assertequalsexacttype(105  , message.getextension(optionalsint32extension  ));
    assertequalsexacttype(106l , message.getextension(optionalsint64extension  ));
    assertequalsexacttype(107  , message.getextension(optionalfixed32extension ));
    assertequalsexacttype(108l , message.getextension(optionalfixed64extension ));
    assertequalsexacttype(109  , message.getextension(optionalsfixed32extension));
    assertequalsexacttype(110l , message.getextension(optionalsfixed64extension));
    assertequalsexacttype(111f , message.getextension(optionalfloatextension   ));
    assertequalsexacttype(112d , message.getextension(optionaldoubleextension  ));
    assertequalsexacttype(true , message.getextension(optionalboolextension    ));
    assertequalsexacttype("115", message.getextension(optionalstringextension  ));
    assertequalsexacttype(tobytes("116"), message.getextension(optionalbytesextension));

    assertequalsexacttype(117, message.getextension(optionalgroupextension              ).geta());
    assertequalsexacttype(118, message.getextension(optionalnestedmessageextension      ).getbb());
    assertequalsexacttype(119, message.getextension(optionalforeignmessageextension     ).getc());
    assertequalsexacttype(120, message.getextension(optionalimportmessageextension      ).getd());
    assertequalsexacttype(126, message.getextension(optionalpublicimportmessageextension).gete());
    assertequalsexacttype(127, message.getextension(optionallazymessageextension        ).getbb());

    assertequalsexacttype(testalltypes.nestedenum.baz,
      message.getextension(optionalnestedenumextension));
    assertequalsexacttype(foreignenum.foreign_baz,
      message.getextension(optionalforeignenumextension));
    assertequalsexacttype(importenum.import_baz,
      message.getextension(optionalimportenumextension));

    assertequalsexacttype("124", message.getextension(optionalstringpieceextension));
    assertequalsexacttype("125", message.getextension(optionalcordextension));

    // -----------------------------------------------------------------

    assert.assertequals(2, message.getextensioncount(repeatedint32extension   ));
    assert.assertequals(2, message.getextensioncount(repeatedint64extension   ));
    assert.assertequals(2, message.getextensioncount(repeateduint32extension  ));
    assert.assertequals(2, message.getextensioncount(repeateduint64extension  ));
    assert.assertequals(2, message.getextensioncount(repeatedsint32extension  ));
    assert.assertequals(2, message.getextensioncount(repeatedsint64extension  ));
    assert.assertequals(2, message.getextensioncount(repeatedfixed32extension ));
    assert.assertequals(2, message.getextensioncount(repeatedfixed64extension ));
    assert.assertequals(2, message.getextensioncount(repeatedsfixed32extension));
    assert.assertequals(2, message.getextensioncount(repeatedsfixed64extension));
    assert.assertequals(2, message.getextensioncount(repeatedfloatextension   ));
    assert.assertequals(2, message.getextensioncount(repeateddoubleextension  ));
    assert.assertequals(2, message.getextensioncount(repeatedboolextension    ));
    assert.assertequals(2, message.getextensioncount(repeatedstringextension  ));
    assert.assertequals(2, message.getextensioncount(repeatedbytesextension   ));

    assert.assertequals(2, message.getextensioncount(repeatedgroupextension         ));
    assert.assertequals(2, message.getextensioncount(repeatednestedmessageextension ));
    assert.assertequals(2, message.getextensioncount(repeatedforeignmessageextension));
    assert.assertequals(2, message.getextensioncount(repeatedimportmessageextension ));
    assert.assertequals(2, message.getextensioncount(repeatedlazymessageextension   ));
    assert.assertequals(2, message.getextensioncount(repeatednestedenumextension    ));
    assert.assertequals(2, message.getextensioncount(repeatedforeignenumextension   ));
    assert.assertequals(2, message.getextensioncount(repeatedimportenumextension    ));

    assert.assertequals(2, message.getextensioncount(repeatedstringpieceextension));
    assert.assertequals(2, message.getextensioncount(repeatedcordextension));

    assertequalsexacttype(201  , message.getextension(repeatedint32extension   , 0));
    assertequalsexacttype(202l , message.getextension(repeatedint64extension   , 0));
    assertequalsexacttype(203  , message.getextension(repeateduint32extension  , 0));
    assertequalsexacttype(204l , message.getextension(repeateduint64extension  , 0));
    assertequalsexacttype(205  , message.getextension(repeatedsint32extension  , 0));
    assertequalsexacttype(206l , message.getextension(repeatedsint64extension  , 0));
    assertequalsexacttype(207  , message.getextension(repeatedfixed32extension , 0));
    assertequalsexacttype(208l , message.getextension(repeatedfixed64extension , 0));
    assertequalsexacttype(209  , message.getextension(repeatedsfixed32extension, 0));
    assertequalsexacttype(210l , message.getextension(repeatedsfixed64extension, 0));
    assertequalsexacttype(211f , message.getextension(repeatedfloatextension   , 0));
    assertequalsexacttype(212d , message.getextension(repeateddoubleextension  , 0));
    assertequalsexacttype(true , message.getextension(repeatedboolextension    , 0));
    assertequalsexacttype("215", message.getextension(repeatedstringextension  , 0));
    assertequalsexacttype(tobytes("216"), message.getextension(repeatedbytesextension, 0));

    assertequalsexacttype(217, message.getextension(repeatedgroupextension         , 0).geta());
    assertequalsexacttype(218, message.getextension(repeatednestedmessageextension , 0).getbb());
    assertequalsexacttype(219, message.getextension(repeatedforeignmessageextension, 0).getc());
    assertequalsexacttype(220, message.getextension(repeatedimportmessageextension , 0).getd());
    assertequalsexacttype(227, message.getextension(repeatedlazymessageextension   , 0).getbb());

    assertequalsexacttype(testalltypes.nestedenum.bar,
      message.getextension(repeatednestedenumextension, 0));
    assertequalsexacttype(foreignenum.foreign_bar,
      message.getextension(repeatedforeignenumextension, 0));
    assertequalsexacttype(importenum.import_bar,
      message.getextension(repeatedimportenumextension, 0));

    assertequalsexacttype("224", message.getextension(repeatedstringpieceextension, 0));
    assertequalsexacttype("225", message.getextension(repeatedcordextension, 0));

    assertequalsexacttype(301  , message.getextension(repeatedint32extension   , 1));
    assertequalsexacttype(302l , message.getextension(repeatedint64extension   , 1));
    assertequalsexacttype(303  , message.getextension(repeateduint32extension  , 1));
    assertequalsexacttype(304l , message.getextension(repeateduint64extension  , 1));
    assertequalsexacttype(305  , message.getextension(repeatedsint32extension  , 1));
    assertequalsexacttype(306l , message.getextension(repeatedsint64extension  , 1));
    assertequalsexacttype(307  , message.getextension(repeatedfixed32extension , 1));
    assertequalsexacttype(308l , message.getextension(repeatedfixed64extension , 1));
    assertequalsexacttype(309  , message.getextension(repeatedsfixed32extension, 1));
    assertequalsexacttype(310l , message.getextension(repeatedsfixed64extension, 1));
    assertequalsexacttype(311f , message.getextension(repeatedfloatextension   , 1));
    assertequalsexacttype(312d , message.getextension(repeateddoubleextension  , 1));
    assertequalsexacttype(false, message.getextension(repeatedboolextension    , 1));
    assertequalsexacttype("315", message.getextension(repeatedstringextension  , 1));
    assertequalsexacttype(tobytes("316"), message.getextension(repeatedbytesextension, 1));

    assertequalsexacttype(317, message.getextension(repeatedgroupextension         , 1).geta());
    assertequalsexacttype(318, message.getextension(repeatednestedmessageextension , 1).getbb());
    assertequalsexacttype(319, message.getextension(repeatedforeignmessageextension, 1).getc());
    assertequalsexacttype(320, message.getextension(repeatedimportmessageextension , 1).getd());
    assertequalsexacttype(327, message.getextension(repeatedlazymessageextension   , 1).getbb());

    assertequalsexacttype(testalltypes.nestedenum.baz,
      message.getextension(repeatednestedenumextension, 1));
    assertequalsexacttype(foreignenum.foreign_baz,
      message.getextension(repeatedforeignenumextension, 1));
    assertequalsexacttype(importenum.import_baz,
      message.getextension(repeatedimportenumextension, 1));

    assertequalsexacttype("324", message.getextension(repeatedstringpieceextension, 1));
    assertequalsexacttype("325", message.getextension(repeatedcordextension, 1));

    // -----------------------------------------------------------------

    assert.asserttrue(message.hasextension(defaultint32extension   ));
    assert.asserttrue(message.hasextension(defaultint64extension   ));
    assert.asserttrue(message.hasextension(defaultuint32extension  ));
    assert.asserttrue(message.hasextension(defaultuint64extension  ));
    assert.asserttrue(message.hasextension(defaultsint32extension  ));
    assert.asserttrue(message.hasextension(defaultsint64extension  ));
    assert.asserttrue(message.hasextension(defaultfixed32extension ));
    assert.asserttrue(message.hasextension(defaultfixed64extension ));
    assert.asserttrue(message.hasextension(defaultsfixed32extension));
    assert.asserttrue(message.hasextension(defaultsfixed64extension));
    assert.asserttrue(message.hasextension(defaultfloatextension   ));
    assert.asserttrue(message.hasextension(defaultdoubleextension  ));
    assert.asserttrue(message.hasextension(defaultboolextension    ));
    assert.asserttrue(message.hasextension(defaultstringextension  ));
    assert.asserttrue(message.hasextension(defaultbytesextension   ));

    assert.asserttrue(message.hasextension(defaultnestedenumextension ));
    assert.asserttrue(message.hasextension(defaultforeignenumextension));
    assert.asserttrue(message.hasextension(defaultimportenumextension ));

    assert.asserttrue(message.hasextension(defaultstringpieceextension));
    assert.asserttrue(message.hasextension(defaultcordextension));

    assertequalsexacttype(401  , message.getextension(defaultint32extension   ));
    assertequalsexacttype(402l , message.getextension(defaultint64extension   ));
    assertequalsexacttype(403  , message.getextension(defaultuint32extension  ));
    assertequalsexacttype(404l , message.getextension(defaultuint64extension  ));
    assertequalsexacttype(405  , message.getextension(defaultsint32extension  ));
    assertequalsexacttype(406l , message.getextension(defaultsint64extension  ));
    assertequalsexacttype(407  , message.getextension(defaultfixed32extension ));
    assertequalsexacttype(408l , message.getextension(defaultfixed64extension ));
    assertequalsexacttype(409  , message.getextension(defaultsfixed32extension));
    assertequalsexacttype(410l , message.getextension(defaultsfixed64extension));
    assertequalsexacttype(411f , message.getextension(defaultfloatextension   ));
    assertequalsexacttype(412d , message.getextension(defaultdoubleextension  ));
    assertequalsexacttype(false, message.getextension(defaultboolextension    ));
    assertequalsexacttype("415", message.getextension(defaultstringextension  ));
    assertequalsexacttype(tobytes("416"), message.getextension(defaultbytesextension));

    assertequalsexacttype(testalltypes.nestedenum.foo,
      message.getextension(defaultnestedenumextension ));
    assertequalsexacttype(foreignenum.foreign_foo,
      message.getextension(defaultforeignenumextension));
    assertequalsexacttype(importenum.import_foo,
      message.getextension(defaultimportenumextension));

    assertequalsexacttype("424", message.getextension(defaultstringpieceextension));
    assertequalsexacttype("425", message.getextension(defaultcordextension));
  }

  // -------------------------------------------------------------------

  /**
   * assert (using {@code junit.framework.assert}} that all extensions of
   * {@code message} are cleared, and that getting the extensions returns their
   * default values.
   */
  public static void assertextensionsclear(testallextensionsorbuilder message) {
    // hasblah() should initially be false for all optional fields.
    assert.assertfalse(message.hasextension(optionalint32extension   ));
    assert.assertfalse(message.hasextension(optionalint64extension   ));
    assert.assertfalse(message.hasextension(optionaluint32extension  ));
    assert.assertfalse(message.hasextension(optionaluint64extension  ));
    assert.assertfalse(message.hasextension(optionalsint32extension  ));
    assert.assertfalse(message.hasextension(optionalsint64extension  ));
    assert.assertfalse(message.hasextension(optionalfixed32extension ));
    assert.assertfalse(message.hasextension(optionalfixed64extension ));
    assert.assertfalse(message.hasextension(optionalsfixed32extension));
    assert.assertfalse(message.hasextension(optionalsfixed64extension));
    assert.assertfalse(message.hasextension(optionalfloatextension   ));
    assert.assertfalse(message.hasextension(optionaldoubleextension  ));
    assert.assertfalse(message.hasextension(optionalboolextension    ));
    assert.assertfalse(message.hasextension(optionalstringextension  ));
    assert.assertfalse(message.hasextension(optionalbytesextension   ));

    assert.assertfalse(message.hasextension(optionalgroupextension         ));
    assert.assertfalse(message.hasextension(optionalnestedmessageextension ));
    assert.assertfalse(message.hasextension(optionalforeignmessageextension));
    assert.assertfalse(message.hasextension(optionalimportmessageextension ));

    assert.assertfalse(message.hasextension(optionalnestedenumextension ));
    assert.assertfalse(message.hasextension(optionalforeignenumextension));
    assert.assertfalse(message.hasextension(optionalimportenumextension ));

    assert.assertfalse(message.hasextension(optionalstringpieceextension));
    assert.assertfalse(message.hasextension(optionalcordextension));

    // optional fields without defaults are set to zero or something like it.
    assertequalsexacttype(0    , message.getextension(optionalint32extension   ));
    assertequalsexacttype(0l   , message.getextension(optionalint64extension   ));
    assertequalsexacttype(0    , message.getextension(optionaluint32extension  ));
    assertequalsexacttype(0l   , message.getextension(optionaluint64extension  ));
    assertequalsexacttype(0    , message.getextension(optionalsint32extension  ));
    assertequalsexacttype(0l   , message.getextension(optionalsint64extension  ));
    assertequalsexacttype(0    , message.getextension(optionalfixed32extension ));
    assertequalsexacttype(0l   , message.getextension(optionalfixed64extension ));
    assertequalsexacttype(0    , message.getextension(optionalsfixed32extension));
    assertequalsexacttype(0l   , message.getextension(optionalsfixed64extension));
    assertequalsexacttype(0f   , message.getextension(optionalfloatextension   ));
    assertequalsexacttype(0d   , message.getextension(optionaldoubleextension  ));
    assertequalsexacttype(false, message.getextension(optionalboolextension    ));
    assertequalsexacttype(""   , message.getextension(optionalstringextension  ));
    assertequalsexacttype(bytestring.empty, message.getextension(optionalbytesextension));

    // embedded messages should also be clear.
    assert.assertfalse(message.getextension(optionalgroupextension         ).hasa());
    assert.assertfalse(message.getextension(optionalnestedmessageextension ).hasbb());
    assert.assertfalse(message.getextension(optionalforeignmessageextension).hasc());
    assert.assertfalse(message.getextension(optionalimportmessageextension ).hasd());

    assertequalsexacttype(0, message.getextension(optionalgroupextension         ).geta());
    assertequalsexacttype(0, message.getextension(optionalnestedmessageextension ).getbb());
    assertequalsexacttype(0, message.getextension(optionalforeignmessageextension).getc());
    assertequalsexacttype(0, message.getextension(optionalimportmessageextension ).getd());

    // enums without defaults are set to the first value in the enum.
    assertequalsexacttype(testalltypes.nestedenum.foo,
      message.getextension(optionalnestedenumextension ));
    assertequalsexacttype(foreignenum.foreign_foo,
      message.getextension(optionalforeignenumextension));
    assertequalsexacttype(importenum.import_foo,
      message.getextension(optionalimportenumextension));

    assertequalsexacttype("", message.getextension(optionalstringpieceextension));
    assertequalsexacttype("", message.getextension(optionalcordextension));

    // repeated fields are empty.
    assert.assertequals(0, message.getextensioncount(repeatedint32extension   ));
    assert.assertequals(0, message.getextensioncount(repeatedint64extension   ));
    assert.assertequals(0, message.getextensioncount(repeateduint32extension  ));
    assert.assertequals(0, message.getextensioncount(repeateduint64extension  ));
    assert.assertequals(0, message.getextensioncount(repeatedsint32extension  ));
    assert.assertequals(0, message.getextensioncount(repeatedsint64extension  ));
    assert.assertequals(0, message.getextensioncount(repeatedfixed32extension ));
    assert.assertequals(0, message.getextensioncount(repeatedfixed64extension ));
    assert.assertequals(0, message.getextensioncount(repeatedsfixed32extension));
    assert.assertequals(0, message.getextensioncount(repeatedsfixed64extension));
    assert.assertequals(0, message.getextensioncount(repeatedfloatextension   ));
    assert.assertequals(0, message.getextensioncount(repeateddoubleextension  ));
    assert.assertequals(0, message.getextensioncount(repeatedboolextension    ));
    assert.assertequals(0, message.getextensioncount(repeatedstringextension  ));
    assert.assertequals(0, message.getextensioncount(repeatedbytesextension   ));

    assert.assertequals(0, message.getextensioncount(repeatedgroupextension         ));
    assert.assertequals(0, message.getextensioncount(repeatednestedmessageextension ));
    assert.assertequals(0, message.getextensioncount(repeatedforeignmessageextension));
    assert.assertequals(0, message.getextensioncount(repeatedimportmessageextension ));
    assert.assertequals(0, message.getextensioncount(repeatedlazymessageextension   ));
    assert.assertequals(0, message.getextensioncount(repeatednestedenumextension    ));
    assert.assertequals(0, message.getextensioncount(repeatedforeignenumextension   ));
    assert.assertequals(0, message.getextensioncount(repeatedimportenumextension    ));

    assert.assertequals(0, message.getextensioncount(repeatedstringpieceextension));
    assert.assertequals(0, message.getextensioncount(repeatedcordextension));

    // repeated fields are empty via getextension().size().
    assert.assertequals(0, message.getextension(repeatedint32extension   ).size());
    assert.assertequals(0, message.getextension(repeatedint64extension   ).size());
    assert.assertequals(0, message.getextension(repeateduint32extension  ).size());
    assert.assertequals(0, message.getextension(repeateduint64extension  ).size());
    assert.assertequals(0, message.getextension(repeatedsint32extension  ).size());
    assert.assertequals(0, message.getextension(repeatedsint64extension  ).size());
    assert.assertequals(0, message.getextension(repeatedfixed32extension ).size());
    assert.assertequals(0, message.getextension(repeatedfixed64extension ).size());
    assert.assertequals(0, message.getextension(repeatedsfixed32extension).size());
    assert.assertequals(0, message.getextension(repeatedsfixed64extension).size());
    assert.assertequals(0, message.getextension(repeatedfloatextension   ).size());
    assert.assertequals(0, message.getextension(repeateddoubleextension  ).size());
    assert.assertequals(0, message.getextension(repeatedboolextension    ).size());
    assert.assertequals(0, message.getextension(repeatedstringextension  ).size());
    assert.assertequals(0, message.getextension(repeatedbytesextension   ).size());

    assert.assertequals(0, message.getextension(repeatedgroupextension         ).size());
    assert.assertequals(0, message.getextension(repeatednestedmessageextension ).size());
    assert.assertequals(0, message.getextension(repeatedforeignmessageextension).size());
    assert.assertequals(0, message.getextension(repeatedimportmessageextension ).size());
    assert.assertequals(0, message.getextension(repeatedlazymessageextension   ).size());
    assert.assertequals(0, message.getextension(repeatednestedenumextension    ).size());
    assert.assertequals(0, message.getextension(repeatedforeignenumextension   ).size());
    assert.assertequals(0, message.getextension(repeatedimportenumextension    ).size());

    assert.assertequals(0, message.getextension(repeatedstringpieceextension).size());
    assert.assertequals(0, message.getextension(repeatedcordextension).size());

    // hasblah() should also be false for all default fields.
    assert.assertfalse(message.hasextension(defaultint32extension   ));
    assert.assertfalse(message.hasextension(defaultint64extension   ));
    assert.assertfalse(message.hasextension(defaultuint32extension  ));
    assert.assertfalse(message.hasextension(defaultuint64extension  ));
    assert.assertfalse(message.hasextension(defaultsint32extension  ));
    assert.assertfalse(message.hasextension(defaultsint64extension  ));
    assert.assertfalse(message.hasextension(defaultfixed32extension ));
    assert.assertfalse(message.hasextension(defaultfixed64extension ));
    assert.assertfalse(message.hasextension(defaultsfixed32extension));
    assert.assertfalse(message.hasextension(defaultsfixed64extension));
    assert.assertfalse(message.hasextension(defaultfloatextension   ));
    assert.assertfalse(message.hasextension(defaultdoubleextension  ));
    assert.assertfalse(message.hasextension(defaultboolextension    ));
    assert.assertfalse(message.hasextension(defaultstringextension  ));
    assert.assertfalse(message.hasextension(defaultbytesextension   ));

    assert.assertfalse(message.hasextension(defaultnestedenumextension ));
    assert.assertfalse(message.hasextension(defaultforeignenumextension));
    assert.assertfalse(message.hasextension(defaultimportenumextension ));

    assert.assertfalse(message.hasextension(defaultstringpieceextension));
    assert.assertfalse(message.hasextension(defaultcordextension));

    // fields with defaults have their default values (duh).
    assertequalsexacttype( 41    , message.getextension(defaultint32extension   ));
    assertequalsexacttype( 42l   , message.getextension(defaultint64extension   ));
    assertequalsexacttype( 43    , message.getextension(defaultuint32extension  ));
    assertequalsexacttype( 44l   , message.getextension(defaultuint64extension  ));
    assertequalsexacttype(-45    , message.getextension(defaultsint32extension  ));
    assertequalsexacttype( 46l   , message.getextension(defaultsint64extension  ));
    assertequalsexacttype( 47    , message.getextension(defaultfixed32extension ));
    assertequalsexacttype( 48l   , message.getextension(defaultfixed64extension ));
    assertequalsexacttype( 49    , message.getextension(defaultsfixed32extension));
    assertequalsexacttype(-50l   , message.getextension(defaultsfixed64extension));
    assertequalsexacttype( 51.5f , message.getextension(defaultfloatextension   ));
    assertequalsexacttype( 52e3d , message.getextension(defaultdoubleextension  ));
    assertequalsexacttype(true   , message.getextension(defaultboolextension    ));
    assertequalsexacttype("hello", message.getextension(defaultstringextension  ));
    assertequalsexacttype(tobytes("world"), message.getextension(defaultbytesextension));

    assertequalsexacttype(testalltypes.nestedenum.bar,
      message.getextension(defaultnestedenumextension ));
    assertequalsexacttype(foreignenum.foreign_bar,
      message.getextension(defaultforeignenumextension));
    assertequalsexacttype(importenum.import_bar,
      message.getextension(defaultimportenumextension));

    assertequalsexacttype("abc", message.getextension(defaultstringpieceextension));
    assertequalsexacttype("123", message.getextension(defaultcordextension));
  }

  // -------------------------------------------------------------------

  /**
   * assert (using {@code junit.framework.assert}} that all extensions of
   * {@code message} are set to the values assigned by {@code setallextensions}
   * followed by {@code modifyrepeatedextensions}.
   */
  public static void assertrepeatedextensionsmodified(
      testallextensionsorbuilder message) {
    // modifyrepeatedfields only sets the second repeated element of each
    // field.  in addition to verifying this, we also verify that the first
    // element and size were *not* modified.
    assert.assertequals(2, message.getextensioncount(repeatedint32extension   ));
    assert.assertequals(2, message.getextensioncount(repeatedint64extension   ));
    assert.assertequals(2, message.getextensioncount(repeateduint32extension  ));
    assert.assertequals(2, message.getextensioncount(repeateduint64extension  ));
    assert.assertequals(2, message.getextensioncount(repeatedsint32extension  ));
    assert.assertequals(2, message.getextensioncount(repeatedsint64extension  ));
    assert.assertequals(2, message.getextensioncount(repeatedfixed32extension ));
    assert.assertequals(2, message.getextensioncount(repeatedfixed64extension ));
    assert.assertequals(2, message.getextensioncount(repeatedsfixed32extension));
    assert.assertequals(2, message.getextensioncount(repeatedsfixed64extension));
    assert.assertequals(2, message.getextensioncount(repeatedfloatextension   ));
    assert.assertequals(2, message.getextensioncount(repeateddoubleextension  ));
    assert.assertequals(2, message.getextensioncount(repeatedboolextension    ));
    assert.assertequals(2, message.getextensioncount(repeatedstringextension  ));
    assert.assertequals(2, message.getextensioncount(repeatedbytesextension   ));

    assert.assertequals(2, message.getextensioncount(repeatedgroupextension         ));
    assert.assertequals(2, message.getextensioncount(repeatednestedmessageextension ));
    assert.assertequals(2, message.getextensioncount(repeatedforeignmessageextension));
    assert.assertequals(2, message.getextensioncount(repeatedimportmessageextension ));
    assert.assertequals(2, message.getextensioncount(repeatedlazymessageextension   ));
    assert.assertequals(2, message.getextensioncount(repeatednestedenumextension    ));
    assert.assertequals(2, message.getextensioncount(repeatedforeignenumextension   ));
    assert.assertequals(2, message.getextensioncount(repeatedimportenumextension    ));

    assert.assertequals(2, message.getextensioncount(repeatedstringpieceextension));
    assert.assertequals(2, message.getextensioncount(repeatedcordextension));

    assertequalsexacttype(201  , message.getextension(repeatedint32extension   , 0));
    assertequalsexacttype(202l , message.getextension(repeatedint64extension   , 0));
    assertequalsexacttype(203  , message.getextension(repeateduint32extension  , 0));
    assertequalsexacttype(204l , message.getextension(repeateduint64extension  , 0));
    assertequalsexacttype(205  , message.getextension(repeatedsint32extension  , 0));
    assertequalsexacttype(206l , message.getextension(repeatedsint64extension  , 0));
    assertequalsexacttype(207  , message.getextension(repeatedfixed32extension , 0));
    assertequalsexacttype(208l , message.getextension(repeatedfixed64extension , 0));
    assertequalsexacttype(209  , message.getextension(repeatedsfixed32extension, 0));
    assertequalsexacttype(210l , message.getextension(repeatedsfixed64extension, 0));
    assertequalsexacttype(211f , message.getextension(repeatedfloatextension   , 0));
    assertequalsexacttype(212d , message.getextension(repeateddoubleextension  , 0));
    assertequalsexacttype(true , message.getextension(repeatedboolextension    , 0));
    assertequalsexacttype("215", message.getextension(repeatedstringextension  , 0));
    assertequalsexacttype(tobytes("216"), message.getextension(repeatedbytesextension, 0));

    assertequalsexacttype(217, message.getextension(repeatedgroupextension         , 0).geta());
    assertequalsexacttype(218, message.getextension(repeatednestedmessageextension , 0).getbb());
    assertequalsexacttype(219, message.getextension(repeatedforeignmessageextension, 0).getc());
    assertequalsexacttype(220, message.getextension(repeatedimportmessageextension , 0).getd());
    assertequalsexacttype(227, message.getextension(repeatedlazymessageextension   , 0).getbb());

    assertequalsexacttype(testalltypes.nestedenum.bar,
      message.getextension(repeatednestedenumextension, 0));
    assertequalsexacttype(foreignenum.foreign_bar,
      message.getextension(repeatedforeignenumextension, 0));
    assertequalsexacttype(importenum.import_bar,
      message.getextension(repeatedimportenumextension, 0));

    assertequalsexacttype("224", message.getextension(repeatedstringpieceextension, 0));
    assertequalsexacttype("225", message.getextension(repeatedcordextension, 0));

    // actually verify the second (modified) elements now.
    assertequalsexacttype(501  , message.getextension(repeatedint32extension   , 1));
    assertequalsexacttype(502l , message.getextension(repeatedint64extension   , 1));
    assertequalsexacttype(503  , message.getextension(repeateduint32extension  , 1));
    assertequalsexacttype(504l , message.getextension(repeateduint64extension  , 1));
    assertequalsexacttype(505  , message.getextension(repeatedsint32extension  , 1));
    assertequalsexacttype(506l , message.getextension(repeatedsint64extension  , 1));
    assertequalsexacttype(507  , message.getextension(repeatedfixed32extension , 1));
    assertequalsexacttype(508l , message.getextension(repeatedfixed64extension , 1));
    assertequalsexacttype(509  , message.getextension(repeatedsfixed32extension, 1));
    assertequalsexacttype(510l , message.getextension(repeatedsfixed64extension, 1));
    assertequalsexacttype(511f , message.getextension(repeatedfloatextension   , 1));
    assertequalsexacttype(512d , message.getextension(repeateddoubleextension  , 1));
    assertequalsexacttype(true , message.getextension(repeatedboolextension    , 1));
    assertequalsexacttype("515", message.getextension(repeatedstringextension  , 1));
    assertequalsexacttype(tobytes("516"), message.getextension(repeatedbytesextension, 1));

    assertequalsexacttype(517, message.getextension(repeatedgroupextension         , 1).geta());
    assertequalsexacttype(518, message.getextension(repeatednestedmessageextension , 1).getbb());
    assertequalsexacttype(519, message.getextension(repeatedforeignmessageextension, 1).getc());
    assertequalsexacttype(520, message.getextension(repeatedimportmessageextension , 1).getd());
    assertequalsexacttype(527, message.getextension(repeatedlazymessageextension   , 1).getbb());

    assertequalsexacttype(testalltypes.nestedenum.foo,
      message.getextension(repeatednestedenumextension, 1));
    assertequalsexacttype(foreignenum.foreign_foo,
      message.getextension(repeatedforeignenumextension, 1));
    assertequalsexacttype(importenum.import_foo,
      message.getextension(repeatedimportenumextension, 1));

    assertequalsexacttype("524", message.getextension(repeatedstringpieceextension, 1));
    assertequalsexacttype("525", message.getextension(repeatedcordextension, 1));
  }

  public static void setpackedextensions(testpackedextensions.builder message) {
    message.addextension(packedint32extension   , 601);
    message.addextension(packedint64extension   , 602l);
    message.addextension(packeduint32extension  , 603);
    message.addextension(packeduint64extension  , 604l);
    message.addextension(packedsint32extension  , 605);
    message.addextension(packedsint64extension  , 606l);
    message.addextension(packedfixed32extension , 607);
    message.addextension(packedfixed64extension , 608l);
    message.addextension(packedsfixed32extension, 609);
    message.addextension(packedsfixed64extension, 610l);
    message.addextension(packedfloatextension   , 611f);
    message.addextension(packeddoubleextension  , 612d);
    message.addextension(packedboolextension    , true);
    message.addextension(packedenumextension, foreignenum.foreign_bar);
    // add a second one of each field.
    message.addextension(packedint32extension   , 701);
    message.addextension(packedint64extension   , 702l);
    message.addextension(packeduint32extension  , 703);
    message.addextension(packeduint64extension  , 704l);
    message.addextension(packedsint32extension  , 705);
    message.addextension(packedsint64extension  , 706l);
    message.addextension(packedfixed32extension , 707);
    message.addextension(packedfixed64extension , 708l);
    message.addextension(packedsfixed32extension, 709);
    message.addextension(packedsfixed64extension, 710l);
    message.addextension(packedfloatextension   , 711f);
    message.addextension(packeddoubleextension  , 712d);
    message.addextension(packedboolextension    , false);
    message.addextension(packedenumextension, foreignenum.foreign_baz);
  }

  public static void assertpackedextensionsset(testpackedextensions message) {
    assert.assertequals(2, message.getextensioncount(packedint32extension   ));
    assert.assertequals(2, message.getextensioncount(packedint64extension   ));
    assert.assertequals(2, message.getextensioncount(packeduint32extension  ));
    assert.assertequals(2, message.getextensioncount(packeduint64extension  ));
    assert.assertequals(2, message.getextensioncount(packedsint32extension  ));
    assert.assertequals(2, message.getextensioncount(packedsint64extension  ));
    assert.assertequals(2, message.getextensioncount(packedfixed32extension ));
    assert.assertequals(2, message.getextensioncount(packedfixed64extension ));
    assert.assertequals(2, message.getextensioncount(packedsfixed32extension));
    assert.assertequals(2, message.getextensioncount(packedsfixed64extension));
    assert.assertequals(2, message.getextensioncount(packedfloatextension   ));
    assert.assertequals(2, message.getextensioncount(packeddoubleextension  ));
    assert.assertequals(2, message.getextensioncount(packedboolextension    ));
    assert.assertequals(2, message.getextensioncount(packedenumextension));
    assertequalsexacttype(601  , message.getextension(packedint32extension   , 0));
    assertequalsexacttype(602l , message.getextension(packedint64extension   , 0));
    assertequalsexacttype(603  , message.getextension(packeduint32extension  , 0));
    assertequalsexacttype(604l , message.getextension(packeduint64extension  , 0));
    assertequalsexacttype(605  , message.getextension(packedsint32extension  , 0));
    assertequalsexacttype(606l , message.getextension(packedsint64extension  , 0));
    assertequalsexacttype(607  , message.getextension(packedfixed32extension , 0));
    assertequalsexacttype(608l , message.getextension(packedfixed64extension , 0));
    assertequalsexacttype(609  , message.getextension(packedsfixed32extension, 0));
    assertequalsexacttype(610l , message.getextension(packedsfixed64extension, 0));
    assertequalsexacttype(611f , message.getextension(packedfloatextension   , 0));
    assertequalsexacttype(612d , message.getextension(packeddoubleextension  , 0));
    assertequalsexacttype(true , message.getextension(packedboolextension    , 0));
    assertequalsexacttype(foreignenum.foreign_bar,
                          message.getextension(packedenumextension, 0));
    assertequalsexacttype(701  , message.getextension(packedint32extension   , 1));
    assertequalsexacttype(702l , message.getextension(packedint64extension   , 1));
    assertequalsexacttype(703  , message.getextension(packeduint32extension  , 1));
    assertequalsexacttype(704l , message.getextension(packeduint64extension  , 1));
    assertequalsexacttype(705  , message.getextension(packedsint32extension  , 1));
    assertequalsexacttype(706l , message.getextension(packedsint64extension  , 1));
    assertequalsexacttype(707  , message.getextension(packedfixed32extension , 1));
    assertequalsexacttype(708l , message.getextension(packedfixed64extension , 1));
    assertequalsexacttype(709  , message.getextension(packedsfixed32extension, 1));
    assertequalsexacttype(710l , message.getextension(packedsfixed64extension, 1));
    assertequalsexacttype(711f , message.getextension(packedfloatextension   , 1));
    assertequalsexacttype(712d , message.getextension(packeddoubleextension  , 1));
    assertequalsexacttype(false, message.getextension(packedboolextension    , 1));
    assertequalsexacttype(foreignenum.foreign_baz,
                          message.getextension(packedenumextension, 1));
  }

  // ===================================================================
  // lite extensions

  /**
   * set every field of {@code message} to the values expected by
   * {@code assertallextensionsset()}.
   */
  public static void setallextensions(testallextensionslite.builder message) {
    message.setextension(optionalint32extensionlite   , 101);
    message.setextension(optionalint64extensionlite   , 102l);
    message.setextension(optionaluint32extensionlite  , 103);
    message.setextension(optionaluint64extensionlite  , 104l);
    message.setextension(optionalsint32extensionlite  , 105);
    message.setextension(optionalsint64extensionlite  , 106l);
    message.setextension(optionalfixed32extensionlite , 107);
    message.setextension(optionalfixed64extensionlite , 108l);
    message.setextension(optionalsfixed32extensionlite, 109);
    message.setextension(optionalsfixed64extensionlite, 110l);
    message.setextension(optionalfloatextensionlite   , 111f);
    message.setextension(optionaldoubleextensionlite  , 112d);
    message.setextension(optionalboolextensionlite    , true);
    message.setextension(optionalstringextensionlite  , "115");
    message.setextension(optionalbytesextensionlite   , tobytes("116"));

    message.setextension(optionalgroupextensionlite,
      optionalgroup_extension_lite.newbuilder().seta(117).build());
    message.setextension(optionalnestedmessageextensionlite,
      testalltypeslite.nestedmessage.newbuilder().setbb(118).build());
    message.setextension(optionalforeignmessageextensionlite,
      foreignmessagelite.newbuilder().setc(119).build());
    message.setextension(optionalimportmessageextensionlite,
      importmessagelite.newbuilder().setd(120).build());
    message.setextension(optionalpublicimportmessageextensionlite,
      publicimportmessagelite.newbuilder().sete(126).build());
    message.setextension(optionallazymessageextensionlite,
      testalltypeslite.nestedmessage.newbuilder().setbb(127).build());

    message.setextension(optionalnestedenumextensionlite, testalltypeslite.nestedenum.baz);
    message.setextension(optionalforeignenumextensionlite, foreignenumlite.foreign_lite_baz);
    message.setextension(optionalimportenumextensionlite, importenumlite.import_lite_baz);

    message.setextension(optionalstringpieceextensionlite, "124");
    message.setextension(optionalcordextensionlite, "125");

    // -----------------------------------------------------------------

    message.addextension(repeatedint32extensionlite   , 201);
    message.addextension(repeatedint64extensionlite   , 202l);
    message.addextension(repeateduint32extensionlite  , 203);
    message.addextension(repeateduint64extensionlite  , 204l);
    message.addextension(repeatedsint32extensionlite  , 205);
    message.addextension(repeatedsint64extensionlite  , 206l);
    message.addextension(repeatedfixed32extensionlite , 207);
    message.addextension(repeatedfixed64extensionlite , 208l);
    message.addextension(repeatedsfixed32extensionlite, 209);
    message.addextension(repeatedsfixed64extensionlite, 210l);
    message.addextension(repeatedfloatextensionlite   , 211f);
    message.addextension(repeateddoubleextensionlite  , 212d);
    message.addextension(repeatedboolextensionlite    , true);
    message.addextension(repeatedstringextensionlite  , "215");
    message.addextension(repeatedbytesextensionlite   , tobytes("216"));

    message.addextension(repeatedgroupextensionlite,
      repeatedgroup_extension_lite.newbuilder().seta(217).build());
    message.addextension(repeatednestedmessageextensionlite,
      testalltypeslite.nestedmessage.newbuilder().setbb(218).build());
    message.addextension(repeatedforeignmessageextensionlite,
      foreignmessagelite.newbuilder().setc(219).build());
    message.addextension(repeatedimportmessageextensionlite,
      importmessagelite.newbuilder().setd(220).build());
    message.addextension(repeatedlazymessageextensionlite,
      testalltypeslite.nestedmessage.newbuilder().setbb(227).build());

    message.addextension(repeatednestedenumextensionlite, testalltypeslite.nestedenum.bar);
    message.addextension(repeatedforeignenumextensionlite, foreignenumlite.foreign_lite_bar);
    message.addextension(repeatedimportenumextensionlite, importenumlite.import_lite_bar);

    message.addextension(repeatedstringpieceextensionlite, "224");
    message.addextension(repeatedcordextensionlite, "225");

    // add a second one of each field.
    message.addextension(repeatedint32extensionlite   , 301);
    message.addextension(repeatedint64extensionlite   , 302l);
    message.addextension(repeateduint32extensionlite  , 303);
    message.addextension(repeateduint64extensionlite  , 304l);
    message.addextension(repeatedsint32extensionlite  , 305);
    message.addextension(repeatedsint64extensionlite  , 306l);
    message.addextension(repeatedfixed32extensionlite , 307);
    message.addextension(repeatedfixed64extensionlite , 308l);
    message.addextension(repeatedsfixed32extensionlite, 309);
    message.addextension(repeatedsfixed64extensionlite, 310l);
    message.addextension(repeatedfloatextensionlite   , 311f);
    message.addextension(repeateddoubleextensionlite  , 312d);
    message.addextension(repeatedboolextensionlite    , false);
    message.addextension(repeatedstringextensionlite  , "315");
    message.addextension(repeatedbytesextensionlite   , tobytes("316"));

    message.addextension(repeatedgroupextensionlite,
      repeatedgroup_extension_lite.newbuilder().seta(317).build());
    message.addextension(repeatednestedmessageextensionlite,
      testalltypeslite.nestedmessage.newbuilder().setbb(318).build());
    message.addextension(repeatedforeignmessageextensionlite,
      foreignmessagelite.newbuilder().setc(319).build());
    message.addextension(repeatedimportmessageextensionlite,
      importmessagelite.newbuilder().setd(320).build());
    message.addextension(repeatedlazymessageextensionlite,
      testalltypeslite.nestedmessage.newbuilder().setbb(327).build());

    message.addextension(repeatednestedenumextensionlite, testalltypeslite.nestedenum.baz);
    message.addextension(repeatedforeignenumextensionlite, foreignenumlite.foreign_lite_baz);
    message.addextension(repeatedimportenumextensionlite, importenumlite.import_lite_baz);

    message.addextension(repeatedstringpieceextensionlite, "324");
    message.addextension(repeatedcordextensionlite, "325");

    // -----------------------------------------------------------------

    message.setextension(defaultint32extensionlite   , 401);
    message.setextension(defaultint64extensionlite   , 402l);
    message.setextension(defaultuint32extensionlite  , 403);
    message.setextension(defaultuint64extensionlite  , 404l);
    message.setextension(defaultsint32extensionlite  , 405);
    message.setextension(defaultsint64extensionlite  , 406l);
    message.setextension(defaultfixed32extensionlite , 407);
    message.setextension(defaultfixed64extensionlite , 408l);
    message.setextension(defaultsfixed32extensionlite, 409);
    message.setextension(defaultsfixed64extensionlite, 410l);
    message.setextension(defaultfloatextensionlite   , 411f);
    message.setextension(defaultdoubleextensionlite  , 412d);
    message.setextension(defaultboolextensionlite    , false);
    message.setextension(defaultstringextensionlite  , "415");
    message.setextension(defaultbytesextensionlite   , tobytes("416"));

    message.setextension(defaultnestedenumextensionlite, testalltypeslite.nestedenum.foo);
    message.setextension(defaultforeignenumextensionlite, foreignenumlite.foreign_lite_foo);
    message.setextension(defaultimportenumextensionlite, importenumlite.import_lite_foo);

    message.setextension(defaultstringpieceextensionlite, "424");
    message.setextension(defaultcordextensionlite, "425");
  }

  // -------------------------------------------------------------------

  /**
   * modify the repeated extensions of {@code message} to contain the values
   * expected by {@code assertrepeatedextensionsmodified()}.
   */
  public static void modifyrepeatedextensions(
      testallextensionslite.builder message) {
    message.setextension(repeatedint32extensionlite   , 1, 501);
    message.setextension(repeatedint64extensionlite   , 1, 502l);
    message.setextension(repeateduint32extensionlite  , 1, 503);
    message.setextension(repeateduint64extensionlite  , 1, 504l);
    message.setextension(repeatedsint32extensionlite  , 1, 505);
    message.setextension(repeatedsint64extensionlite  , 1, 506l);
    message.setextension(repeatedfixed32extensionlite , 1, 507);
    message.setextension(repeatedfixed64extensionlite , 1, 508l);
    message.setextension(repeatedsfixed32extensionlite, 1, 509);
    message.setextension(repeatedsfixed64extensionlite, 1, 510l);
    message.setextension(repeatedfloatextensionlite   , 1, 511f);
    message.setextension(repeateddoubleextensionlite  , 1, 512d);
    message.setextension(repeatedboolextensionlite    , 1, true);
    message.setextension(repeatedstringextensionlite  , 1, "515");
    message.setextension(repeatedbytesextensionlite   , 1, tobytes("516"));

    message.setextension(repeatedgroupextensionlite, 1,
      repeatedgroup_extension_lite.newbuilder().seta(517).build());
    message.setextension(repeatednestedmessageextensionlite, 1,
      testalltypeslite.nestedmessage.newbuilder().setbb(518).build());
    message.setextension(repeatedforeignmessageextensionlite, 1,
      foreignmessagelite.newbuilder().setc(519).build());
    message.setextension(repeatedimportmessageextensionlite, 1,
      importmessagelite.newbuilder().setd(520).build());
    message.setextension(repeatedlazymessageextensionlite, 1,
      testalltypeslite.nestedmessage.newbuilder().setbb(527).build());

    message.setextension(repeatednestedenumextensionlite , 1, testalltypeslite.nestedenum.foo);
    message.setextension(repeatedforeignenumextensionlite, 1, foreignenumlite.foreign_lite_foo);
    message.setextension(repeatedimportenumextensionlite , 1, importenumlite.import_lite_foo);

    message.setextension(repeatedstringpieceextensionlite, 1, "524");
    message.setextension(repeatedcordextensionlite, 1, "525");
  }

  // -------------------------------------------------------------------

  /**
   * assert (using {@code junit.framework.assert}} that all extensions of
   * {@code message} are set to the values assigned by {@code setallextensions}.
   */
  public static void assertallextensionsset(
      testallextensionsliteorbuilder message) {
    assert.asserttrue(message.hasextension(optionalint32extensionlite   ));
    assert.asserttrue(message.hasextension(optionalint64extensionlite   ));
    assert.asserttrue(message.hasextension(optionaluint32extensionlite  ));
    assert.asserttrue(message.hasextension(optionaluint64extensionlite  ));
    assert.asserttrue(message.hasextension(optionalsint32extensionlite  ));
    assert.asserttrue(message.hasextension(optionalsint64extensionlite  ));
    assert.asserttrue(message.hasextension(optionalfixed32extensionlite ));
    assert.asserttrue(message.hasextension(optionalfixed64extensionlite ));
    assert.asserttrue(message.hasextension(optionalsfixed32extensionlite));
    assert.asserttrue(message.hasextension(optionalsfixed64extensionlite));
    assert.asserttrue(message.hasextension(optionalfloatextensionlite   ));
    assert.asserttrue(message.hasextension(optionaldoubleextensionlite  ));
    assert.asserttrue(message.hasextension(optionalboolextensionlite    ));
    assert.asserttrue(message.hasextension(optionalstringextensionlite  ));
    assert.asserttrue(message.hasextension(optionalbytesextensionlite   ));

    assert.asserttrue(message.hasextension(optionalgroupextensionlite         ));
    assert.asserttrue(message.hasextension(optionalnestedmessageextensionlite ));
    assert.asserttrue(message.hasextension(optionalforeignmessageextensionlite));
    assert.asserttrue(message.hasextension(optionalimportmessageextensionlite ));

    assert.asserttrue(message.getextension(optionalgroupextensionlite         ).hasa());
    assert.asserttrue(message.getextension(optionalnestedmessageextensionlite ).hasbb());
    assert.asserttrue(message.getextension(optionalforeignmessageextensionlite).hasc());
    assert.asserttrue(message.getextension(optionalimportmessageextensionlite ).hasd());

    assert.asserttrue(message.hasextension(optionalnestedenumextensionlite ));
    assert.asserttrue(message.hasextension(optionalforeignenumextensionlite));
    assert.asserttrue(message.hasextension(optionalimportenumextensionlite ));

    assert.asserttrue(message.hasextension(optionalstringpieceextensionlite));
    assert.asserttrue(message.hasextension(optionalcordextensionlite));

    assertequalsexacttype(101  , message.getextension(optionalint32extensionlite   ));
    assertequalsexacttype(102l , message.getextension(optionalint64extensionlite   ));
    assertequalsexacttype(103  , message.getextension(optionaluint32extensionlite  ));
    assertequalsexacttype(104l , message.getextension(optionaluint64extensionlite  ));
    assertequalsexacttype(105  , message.getextension(optionalsint32extensionlite  ));
    assertequalsexacttype(106l , message.getextension(optionalsint64extensionlite  ));
    assertequalsexacttype(107  , message.getextension(optionalfixed32extensionlite ));
    assertequalsexacttype(108l , message.getextension(optionalfixed64extensionlite ));
    assertequalsexacttype(109  , message.getextension(optionalsfixed32extensionlite));
    assertequalsexacttype(110l , message.getextension(optionalsfixed64extensionlite));
    assertequalsexacttype(111f , message.getextension(optionalfloatextensionlite   ));
    assertequalsexacttype(112d , message.getextension(optionaldoubleextensionlite  ));
    assertequalsexacttype(true , message.getextension(optionalboolextensionlite    ));
    assertequalsexacttype("115", message.getextension(optionalstringextensionlite  ));
    assertequalsexacttype(tobytes("116"), message.getextension(optionalbytesextensionlite));

    assertequalsexacttype(117, message.getextension(optionalgroupextensionlite         ).geta());
    assertequalsexacttype(118, message.getextension(optionalnestedmessageextensionlite ).getbb());
    assertequalsexacttype(119, message.getextension(optionalforeignmessageextensionlite).getc());
    assertequalsexacttype(120, message.getextension(optionalimportmessageextensionlite ).getd());
    assertequalsexacttype(126, message.getextension(
        optionalpublicimportmessageextensionlite).gete());
    assertequalsexacttype(127, message.getextension(optionallazymessageextensionlite).getbb());

    assertequalsexacttype(testalltypeslite.nestedenum.baz,
      message.getextension(optionalnestedenumextensionlite));
    assertequalsexacttype(foreignenumlite.foreign_lite_baz,
      message.getextension(optionalforeignenumextensionlite));
    assertequalsexacttype(importenumlite.import_lite_baz,
      message.getextension(optionalimportenumextensionlite));

    assertequalsexacttype("124", message.getextension(optionalstringpieceextensionlite));
    assertequalsexacttype("125", message.getextension(optionalcordextensionlite));

    // -----------------------------------------------------------------

    assert.assertequals(2, message.getextensioncount(repeatedint32extensionlite   ));
    assert.assertequals(2, message.getextensioncount(repeatedint64extensionlite   ));
    assert.assertequals(2, message.getextensioncount(repeateduint32extensionlite  ));
    assert.assertequals(2, message.getextensioncount(repeateduint64extensionlite  ));
    assert.assertequals(2, message.getextensioncount(repeatedsint32extensionlite  ));
    assert.assertequals(2, message.getextensioncount(repeatedsint64extensionlite  ));
    assert.assertequals(2, message.getextensioncount(repeatedfixed32extensionlite ));
    assert.assertequals(2, message.getextensioncount(repeatedfixed64extensionlite ));
    assert.assertequals(2, message.getextensioncount(repeatedsfixed32extensionlite));
    assert.assertequals(2, message.getextensioncount(repeatedsfixed64extensionlite));
    assert.assertequals(2, message.getextensioncount(repeatedfloatextensionlite   ));
    assert.assertequals(2, message.getextensioncount(repeateddoubleextensionlite  ));
    assert.assertequals(2, message.getextensioncount(repeatedboolextensionlite    ));
    assert.assertequals(2, message.getextensioncount(repeatedstringextensionlite  ));
    assert.assertequals(2, message.getextensioncount(repeatedbytesextensionlite   ));

    assert.assertequals(2, message.getextensioncount(repeatedgroupextensionlite         ));
    assert.assertequals(2, message.getextensioncount(repeatednestedmessageextensionlite ));
    assert.assertequals(2, message.getextensioncount(repeatedforeignmessageextensionlite));
    assert.assertequals(2, message.getextensioncount(repeatedimportmessageextensionlite ));
    assert.assertequals(2, message.getextensioncount(repeatedlazymessageextensionlite   ));
    assert.assertequals(2, message.getextensioncount(repeatednestedenumextensionlite    ));
    assert.assertequals(2, message.getextensioncount(repeatedforeignenumextensionlite   ));
    assert.assertequals(2, message.getextensioncount(repeatedimportenumextensionlite    ));

    assert.assertequals(2, message.getextensioncount(repeatedstringpieceextensionlite));
    assert.assertequals(2, message.getextensioncount(repeatedcordextensionlite));

    assertequalsexacttype(201  , message.getextension(repeatedint32extensionlite   , 0));
    assertequalsexacttype(202l , message.getextension(repeatedint64extensionlite   , 0));
    assertequalsexacttype(203  , message.getextension(repeateduint32extensionlite  , 0));
    assertequalsexacttype(204l , message.getextension(repeateduint64extensionlite  , 0));
    assertequalsexacttype(205  , message.getextension(repeatedsint32extensionlite  , 0));
    assertequalsexacttype(206l , message.getextension(repeatedsint64extensionlite  , 0));
    assertequalsexacttype(207  , message.getextension(repeatedfixed32extensionlite , 0));
    assertequalsexacttype(208l , message.getextension(repeatedfixed64extensionlite , 0));
    assertequalsexacttype(209  , message.getextension(repeatedsfixed32extensionlite, 0));
    assertequalsexacttype(210l , message.getextension(repeatedsfixed64extensionlite, 0));
    assertequalsexacttype(211f , message.getextension(repeatedfloatextensionlite   , 0));
    assertequalsexacttype(212d , message.getextension(repeateddoubleextensionlite  , 0));
    assertequalsexacttype(true , message.getextension(repeatedboolextensionlite    , 0));
    assertequalsexacttype("215", message.getextension(repeatedstringextensionlite  , 0));
    assertequalsexacttype(tobytes("216"), message.getextension(repeatedbytesextensionlite, 0));

    assertequalsexacttype(217, message.getextension(repeatedgroupextensionlite         ,0).geta());
    assertequalsexacttype(218, message.getextension(repeatednestedmessageextensionlite ,0).getbb());
    assertequalsexacttype(219, message.getextension(repeatedforeignmessageextensionlite,0).getc());
    assertequalsexacttype(220, message.getextension(repeatedimportmessageextensionlite ,0).getd());
    assertequalsexacttype(227, message.getextension(repeatedlazymessageextensionlite   ,0).getbb());

    assertequalsexacttype(testalltypeslite.nestedenum.bar,
      message.getextension(repeatednestedenumextensionlite, 0));
    assertequalsexacttype(foreignenumlite.foreign_lite_bar,
      message.getextension(repeatedforeignenumextensionlite, 0));
    assertequalsexacttype(importenumlite.import_lite_bar,
      message.getextension(repeatedimportenumextensionlite, 0));

    assertequalsexacttype("224", message.getextension(repeatedstringpieceextensionlite, 0));
    assertequalsexacttype("225", message.getextension(repeatedcordextensionlite, 0));

    assertequalsexacttype(301  , message.getextension(repeatedint32extensionlite   , 1));
    assertequalsexacttype(302l , message.getextension(repeatedint64extensionlite   , 1));
    assertequalsexacttype(303  , message.getextension(repeateduint32extensionlite  , 1));
    assertequalsexacttype(304l , message.getextension(repeateduint64extensionlite  , 1));
    assertequalsexacttype(305  , message.getextension(repeatedsint32extensionlite  , 1));
    assertequalsexacttype(306l , message.getextension(repeatedsint64extensionlite  , 1));
    assertequalsexacttype(307  , message.getextension(repeatedfixed32extensionlite , 1));
    assertequalsexacttype(308l , message.getextension(repeatedfixed64extensionlite , 1));
    assertequalsexacttype(309  , message.getextension(repeatedsfixed32extensionlite, 1));
    assertequalsexacttype(310l , message.getextension(repeatedsfixed64extensionlite, 1));
    assertequalsexacttype(311f , message.getextension(repeatedfloatextensionlite   , 1));
    assertequalsexacttype(312d , message.getextension(repeateddoubleextensionlite  , 1));
    assertequalsexacttype(false, message.getextension(repeatedboolextensionlite    , 1));
    assertequalsexacttype("315", message.getextension(repeatedstringextensionlite  , 1));
    assertequalsexacttype(tobytes("316"), message.getextension(repeatedbytesextensionlite, 1));

    assertequalsexacttype(317, message.getextension(repeatedgroupextensionlite         ,1).geta());
    assertequalsexacttype(318, message.getextension(repeatednestedmessageextensionlite ,1).getbb());
    assertequalsexacttype(319, message.getextension(repeatedforeignmessageextensionlite,1).getc());
    assertequalsexacttype(320, message.getextension(repeatedimportmessageextensionlite ,1).getd());
    assertequalsexacttype(327, message.getextension(repeatedlazymessageextensionlite   ,1).getbb());

    assertequalsexacttype(testalltypeslite.nestedenum.baz,
      message.getextension(repeatednestedenumextensionlite, 1));
    assertequalsexacttype(foreignenumlite.foreign_lite_baz,
      message.getextension(repeatedforeignenumextensionlite, 1));
    assertequalsexacttype(importenumlite.import_lite_baz,
      message.getextension(repeatedimportenumextensionlite, 1));

    assertequalsexacttype("324", message.getextension(repeatedstringpieceextensionlite, 1));
    assertequalsexacttype("325", message.getextension(repeatedcordextensionlite, 1));

    // -----------------------------------------------------------------

    assert.asserttrue(message.hasextension(defaultint32extensionlite   ));
    assert.asserttrue(message.hasextension(defaultint64extensionlite   ));
    assert.asserttrue(message.hasextension(defaultuint32extensionlite  ));
    assert.asserttrue(message.hasextension(defaultuint64extensionlite  ));
    assert.asserttrue(message.hasextension(defaultsint32extensionlite  ));
    assert.asserttrue(message.hasextension(defaultsint64extensionlite  ));
    assert.asserttrue(message.hasextension(defaultfixed32extensionlite ));
    assert.asserttrue(message.hasextension(defaultfixed64extensionlite ));
    assert.asserttrue(message.hasextension(defaultsfixed32extensionlite));
    assert.asserttrue(message.hasextension(defaultsfixed64extensionlite));
    assert.asserttrue(message.hasextension(defaultfloatextensionlite   ));
    assert.asserttrue(message.hasextension(defaultdoubleextensionlite  ));
    assert.asserttrue(message.hasextension(defaultboolextensionlite    ));
    assert.asserttrue(message.hasextension(defaultstringextensionlite  ));
    assert.asserttrue(message.hasextension(defaultbytesextensionlite   ));

    assert.asserttrue(message.hasextension(defaultnestedenumextensionlite ));
    assert.asserttrue(message.hasextension(defaultforeignenumextensionlite));
    assert.asserttrue(message.hasextension(defaultimportenumextensionlite ));

    assert.asserttrue(message.hasextension(defaultstringpieceextensionlite));
    assert.asserttrue(message.hasextension(defaultcordextensionlite));

    assertequalsexacttype(401  , message.getextension(defaultint32extensionlite   ));
    assertequalsexacttype(402l , message.getextension(defaultint64extensionlite   ));
    assertequalsexacttype(403  , message.getextension(defaultuint32extensionlite  ));
    assertequalsexacttype(404l , message.getextension(defaultuint64extensionlite  ));
    assertequalsexacttype(405  , message.getextension(defaultsint32extensionlite  ));
    assertequalsexacttype(406l , message.getextension(defaultsint64extensionlite  ));
    assertequalsexacttype(407  , message.getextension(defaultfixed32extensionlite ));
    assertequalsexacttype(408l , message.getextension(defaultfixed64extensionlite ));
    assertequalsexacttype(409  , message.getextension(defaultsfixed32extensionlite));
    assertequalsexacttype(410l , message.getextension(defaultsfixed64extensionlite));
    assertequalsexacttype(411f , message.getextension(defaultfloatextensionlite   ));
    assertequalsexacttype(412d , message.getextension(defaultdoubleextensionlite  ));
    assertequalsexacttype(false, message.getextension(defaultboolextensionlite    ));
    assertequalsexacttype("415", message.getextension(defaultstringextensionlite  ));
    assertequalsexacttype(tobytes("416"), message.getextension(defaultbytesextensionlite));

    assertequalsexacttype(testalltypeslite.nestedenum.foo,
      message.getextension(defaultnestedenumextensionlite ));
    assertequalsexacttype(foreignenumlite.foreign_lite_foo,
      message.getextension(defaultforeignenumextensionlite));
    assertequalsexacttype(importenumlite.import_lite_foo,
      message.getextension(defaultimportenumextensionlite));

    assertequalsexacttype("424", message.getextension(defaultstringpieceextensionlite));
    assertequalsexacttype("425", message.getextension(defaultcordextensionlite));
  }

  // -------------------------------------------------------------------

  /**
   * assert (using {@code junit.framework.assert}} that all extensions of
   * {@code message} are cleared, and that getting the extensions returns their
   * default values.
   */
  public static void assertextensionsclear(
      testallextensionsliteorbuilder message) {
    // hasblah() should initially be false for all optional fields.
    assert.assertfalse(message.hasextension(optionalint32extensionlite   ));
    assert.assertfalse(message.hasextension(optionalint64extensionlite   ));
    assert.assertfalse(message.hasextension(optionaluint32extensionlite  ));
    assert.assertfalse(message.hasextension(optionaluint64extensionlite  ));
    assert.assertfalse(message.hasextension(optionalsint32extensionlite  ));
    assert.assertfalse(message.hasextension(optionalsint64extensionlite  ));
    assert.assertfalse(message.hasextension(optionalfixed32extensionlite ));
    assert.assertfalse(message.hasextension(optionalfixed64extensionlite ));
    assert.assertfalse(message.hasextension(optionalsfixed32extensionlite));
    assert.assertfalse(message.hasextension(optionalsfixed64extensionlite));
    assert.assertfalse(message.hasextension(optionalfloatextensionlite   ));
    assert.assertfalse(message.hasextension(optionaldoubleextensionlite  ));
    assert.assertfalse(message.hasextension(optionalboolextensionlite    ));
    assert.assertfalse(message.hasextension(optionalstringextensionlite  ));
    assert.assertfalse(message.hasextension(optionalbytesextensionlite   ));

    assert.assertfalse(message.hasextension(optionalgroupextensionlite              ));
    assert.assertfalse(message.hasextension(optionalnestedmessageextensionlite      ));
    assert.assertfalse(message.hasextension(optionalforeignmessageextensionlite     ));
    assert.assertfalse(message.hasextension(optionalimportmessageextensionlite      ));
    assert.assertfalse(message.hasextension(optionalpublicimportmessageextensionlite));
    assert.assertfalse(message.hasextension(optionallazymessageextensionlite        ));

    assert.assertfalse(message.hasextension(optionalnestedenumextensionlite ));
    assert.assertfalse(message.hasextension(optionalforeignenumextensionlite));
    assert.assertfalse(message.hasextension(optionalimportenumextensionlite ));

    assert.assertfalse(message.hasextension(optionalstringpieceextensionlite));
    assert.assertfalse(message.hasextension(optionalcordextensionlite));

    // optional fields without defaults are set to zero or something like it.
    assertequalsexacttype(0    , message.getextension(optionalint32extensionlite   ));
    assertequalsexacttype(0l   , message.getextension(optionalint64extensionlite   ));
    assertequalsexacttype(0    , message.getextension(optionaluint32extensionlite  ));
    assertequalsexacttype(0l   , message.getextension(optionaluint64extensionlite  ));
    assertequalsexacttype(0    , message.getextension(optionalsint32extensionlite  ));
    assertequalsexacttype(0l   , message.getextension(optionalsint64extensionlite  ));
    assertequalsexacttype(0    , message.getextension(optionalfixed32extensionlite ));
    assertequalsexacttype(0l   , message.getextension(optionalfixed64extensionlite ));
    assertequalsexacttype(0    , message.getextension(optionalsfixed32extensionlite));
    assertequalsexacttype(0l   , message.getextension(optionalsfixed64extensionlite));
    assertequalsexacttype(0f   , message.getextension(optionalfloatextensionlite   ));
    assertequalsexacttype(0d   , message.getextension(optionaldoubleextensionlite  ));
    assertequalsexacttype(false, message.getextension(optionalboolextensionlite    ));
    assertequalsexacttype(""   , message.getextension(optionalstringextensionlite  ));
    assertequalsexacttype(bytestring.empty, message.getextension(optionalbytesextensionlite));

    // embedded messages should also be clear.
    assert.assertfalse(message.getextension(optionalgroupextensionlite              ).hasa());
    assert.assertfalse(message.getextension(optionalnestedmessageextensionlite      ).hasbb());
    assert.assertfalse(message.getextension(optionalforeignmessageextensionlite     ).hasc());
    assert.assertfalse(message.getextension(optionalimportmessageextensionlite      ).hasd());
    assert.assertfalse(message.getextension(optionalpublicimportmessageextensionlite).hase());
    assert.assertfalse(message.getextension(optionallazymessageextensionlite        ).hasbb());

    assertequalsexacttype(0, message.getextension(optionalgroupextensionlite         ).geta());
    assertequalsexacttype(0, message.getextension(optionalnestedmessageextensionlite ).getbb());
    assertequalsexacttype(0, message.getextension(optionalforeignmessageextensionlite).getc());
    assertequalsexacttype(0, message.getextension(optionalimportmessageextensionlite ).getd());
    assertequalsexacttype(0, message.getextension(
        optionalpublicimportmessageextensionlite).gete());
    assertequalsexacttype(0, message.getextension(optionallazymessageextensionlite   ).getbb());

    // enums without defaults are set to the first value in the enum.
    assertequalsexacttype(testalltypeslite.nestedenum.foo,
      message.getextension(optionalnestedenumextensionlite ));
    assertequalsexacttype(foreignenumlite.foreign_lite_foo,
      message.getextension(optionalforeignenumextensionlite));
    assertequalsexacttype(importenumlite.import_lite_foo,
      message.getextension(optionalimportenumextensionlite));

    assertequalsexacttype("", message.getextension(optionalstringpieceextensionlite));
    assertequalsexacttype("", message.getextension(optionalcordextensionlite));

    // repeated fields are empty.
    assert.assertequals(0, message.getextensioncount(repeatedint32extensionlite   ));
    assert.assertequals(0, message.getextensioncount(repeatedint64extensionlite   ));
    assert.assertequals(0, message.getextensioncount(repeateduint32extensionlite  ));
    assert.assertequals(0, message.getextensioncount(repeateduint64extensionlite  ));
    assert.assertequals(0, message.getextensioncount(repeatedsint32extensionlite  ));
    assert.assertequals(0, message.getextensioncount(repeatedsint64extensionlite  ));
    assert.assertequals(0, message.getextensioncount(repeatedfixed32extensionlite ));
    assert.assertequals(0, message.getextensioncount(repeatedfixed64extensionlite ));
    assert.assertequals(0, message.getextensioncount(repeatedsfixed32extensionlite));
    assert.assertequals(0, message.getextensioncount(repeatedsfixed64extensionlite));
    assert.assertequals(0, message.getextensioncount(repeatedfloatextensionlite   ));
    assert.assertequals(0, message.getextensioncount(repeateddoubleextensionlite  ));
    assert.assertequals(0, message.getextensioncount(repeatedboolextensionlite    ));
    assert.assertequals(0, message.getextensioncount(repeatedstringextensionlite  ));
    assert.assertequals(0, message.getextensioncount(repeatedbytesextensionlite   ));

    assert.assertequals(0, message.getextensioncount(repeatedgroupextensionlite         ));
    assert.assertequals(0, message.getextensioncount(repeatednestedmessageextensionlite ));
    assert.assertequals(0, message.getextensioncount(repeatedforeignmessageextensionlite));
    assert.assertequals(0, message.getextensioncount(repeatedimportmessageextensionlite ));
    assert.assertequals(0, message.getextensioncount(repeatedlazymessageextensionlite   ));
    assert.assertequals(0, message.getextensioncount(repeatednestedenumextensionlite    ));
    assert.assertequals(0, message.getextensioncount(repeatedforeignenumextensionlite   ));
    assert.assertequals(0, message.getextensioncount(repeatedimportenumextensionlite    ));

    assert.assertequals(0, message.getextensioncount(repeatedstringpieceextensionlite));
    assert.assertequals(0, message.getextensioncount(repeatedcordextensionlite));

    // hasblah() should also be false for all default fields.
    assert.assertfalse(message.hasextension(defaultint32extensionlite   ));
    assert.assertfalse(message.hasextension(defaultint64extensionlite   ));
    assert.assertfalse(message.hasextension(defaultuint32extensionlite  ));
    assert.assertfalse(message.hasextension(defaultuint64extensionlite  ));
    assert.assertfalse(message.hasextension(defaultsint32extensionlite  ));
    assert.assertfalse(message.hasextension(defaultsint64extensionlite  ));
    assert.assertfalse(message.hasextension(defaultfixed32extensionlite ));
    assert.assertfalse(message.hasextension(defaultfixed64extensionlite ));
    assert.assertfalse(message.hasextension(defaultsfixed32extensionlite));
    assert.assertfalse(message.hasextension(defaultsfixed64extensionlite));
    assert.assertfalse(message.hasextension(defaultfloatextensionlite   ));
    assert.assertfalse(message.hasextension(defaultdoubleextensionlite  ));
    assert.assertfalse(message.hasextension(defaultboolextensionlite    ));
    assert.assertfalse(message.hasextension(defaultstringextensionlite  ));
    assert.assertfalse(message.hasextension(defaultbytesextensionlite   ));

    assert.assertfalse(message.hasextension(defaultnestedenumextensionlite ));
    assert.assertfalse(message.hasextension(defaultforeignenumextensionlite));
    assert.assertfalse(message.hasextension(defaultimportenumextensionlite ));

    assert.assertfalse(message.hasextension(defaultstringpieceextensionlite));
    assert.assertfalse(message.hasextension(defaultcordextensionlite));

    // fields with defaults have their default values (duh).
    assertequalsexacttype( 41    , message.getextension(defaultint32extensionlite   ));
    assertequalsexacttype( 42l   , message.getextension(defaultint64extensionlite   ));
    assertequalsexacttype( 43    , message.getextension(defaultuint32extensionlite  ));
    assertequalsexacttype( 44l   , message.getextension(defaultuint64extensionlite  ));
    assertequalsexacttype(-45    , message.getextension(defaultsint32extensionlite  ));
    assertequalsexacttype( 46l   , message.getextension(defaultsint64extensionlite  ));
    assertequalsexacttype( 47    , message.getextension(defaultfixed32extensionlite ));
    assertequalsexacttype( 48l   , message.getextension(defaultfixed64extensionlite ));
    assertequalsexacttype( 49    , message.getextension(defaultsfixed32extensionlite));
    assertequalsexacttype(-50l   , message.getextension(defaultsfixed64extensionlite));
    assertequalsexacttype( 51.5f , message.getextension(defaultfloatextensionlite   ));
    assertequalsexacttype( 52e3d , message.getextension(defaultdoubleextensionlite  ));
    assertequalsexacttype(true   , message.getextension(defaultboolextensionlite    ));
    assertequalsexacttype("hello", message.getextension(defaultstringextensionlite  ));
    assertequalsexacttype(tobytes("world"), message.getextension(defaultbytesextensionlite));

    assertequalsexacttype(testalltypeslite.nestedenum.bar,
      message.getextension(defaultnestedenumextensionlite ));
    assertequalsexacttype(foreignenumlite.foreign_lite_bar,
      message.getextension(defaultforeignenumextensionlite));
    assertequalsexacttype(importenumlite.import_lite_bar,
      message.getextension(defaultimportenumextensionlite));

    assertequalsexacttype("abc", message.getextension(defaultstringpieceextensionlite));
    assertequalsexacttype("123", message.getextension(defaultcordextensionlite));
  }

  // -------------------------------------------------------------------

  /**
   * assert (using {@code junit.framework.assert}} that all extensions of
   * {@code message} are set to the values assigned by {@code setallextensions}
   * followed by {@code modifyrepeatedextensions}.
   */
  public static void assertrepeatedextensionsmodified(
      testallextensionsliteorbuilder message) {
    // modifyrepeatedfields only sets the second repeated element of each
    // field.  in addition to verifying this, we also verify that the first
    // element and size were *not* modified.
    assert.assertequals(2, message.getextensioncount(repeatedint32extensionlite   ));
    assert.assertequals(2, message.getextensioncount(repeatedint64extensionlite   ));
    assert.assertequals(2, message.getextensioncount(repeateduint32extensionlite  ));
    assert.assertequals(2, message.getextensioncount(repeateduint64extensionlite  ));
    assert.assertequals(2, message.getextensioncount(repeatedsint32extensionlite  ));
    assert.assertequals(2, message.getextensioncount(repeatedsint64extensionlite  ));
    assert.assertequals(2, message.getextensioncount(repeatedfixed32extensionlite ));
    assert.assertequals(2, message.getextensioncount(repeatedfixed64extensionlite ));
    assert.assertequals(2, message.getextensioncount(repeatedsfixed32extensionlite));
    assert.assertequals(2, message.getextensioncount(repeatedsfixed64extensionlite));
    assert.assertequals(2, message.getextensioncount(repeatedfloatextensionlite   ));
    assert.assertequals(2, message.getextensioncount(repeateddoubleextensionlite  ));
    assert.assertequals(2, message.getextensioncount(repeatedboolextensionlite    ));
    assert.assertequals(2, message.getextensioncount(repeatedstringextensionlite  ));
    assert.assertequals(2, message.getextensioncount(repeatedbytesextensionlite   ));

    assert.assertequals(2, message.getextensioncount(repeatedgroupextensionlite         ));
    assert.assertequals(2, message.getextensioncount(repeatednestedmessageextensionlite ));
    assert.assertequals(2, message.getextensioncount(repeatedforeignmessageextensionlite));
    assert.assertequals(2, message.getextensioncount(repeatedimportmessageextensionlite ));
    assert.assertequals(2, message.getextensioncount(repeatedlazymessageextensionlite   ));
    assert.assertequals(2, message.getextensioncount(repeatednestedenumextensionlite    ));
    assert.assertequals(2, message.getextensioncount(repeatedforeignenumextensionlite   ));
    assert.assertequals(2, message.getextensioncount(repeatedimportenumextensionlite    ));

    assert.assertequals(2, message.getextensioncount(repeatedstringpieceextensionlite));
    assert.assertequals(2, message.getextensioncount(repeatedcordextensionlite));

    assertequalsexacttype(201  , message.getextension(repeatedint32extensionlite   , 0));
    assertequalsexacttype(202l , message.getextension(repeatedint64extensionlite   , 0));
    assertequalsexacttype(203  , message.getextension(repeateduint32extensionlite  , 0));
    assertequalsexacttype(204l , message.getextension(repeateduint64extensionlite  , 0));
    assertequalsexacttype(205  , message.getextension(repeatedsint32extensionlite  , 0));
    assertequalsexacttype(206l , message.getextension(repeatedsint64extensionlite  , 0));
    assertequalsexacttype(207  , message.getextension(repeatedfixed32extensionlite , 0));
    assertequalsexacttype(208l , message.getextension(repeatedfixed64extensionlite , 0));
    assertequalsexacttype(209  , message.getextension(repeatedsfixed32extensionlite, 0));
    assertequalsexacttype(210l , message.getextension(repeatedsfixed64extensionlite, 0));
    assertequalsexacttype(211f , message.getextension(repeatedfloatextensionlite   , 0));
    assertequalsexacttype(212d , message.getextension(repeateddoubleextensionlite  , 0));
    assertequalsexacttype(true , message.getextension(repeatedboolextensionlite    , 0));
    assertequalsexacttype("215", message.getextension(repeatedstringextensionlite  , 0));
    assertequalsexacttype(tobytes("216"), message.getextension(repeatedbytesextensionlite, 0));

    assertequalsexacttype(217, message.getextension(repeatedgroupextensionlite         ,0).geta());
    assertequalsexacttype(218, message.getextension(repeatednestedmessageextensionlite ,0).getbb());
    assertequalsexacttype(219, message.getextension(repeatedforeignmessageextensionlite,0).getc());
    assertequalsexacttype(220, message.getextension(repeatedimportmessageextensionlite ,0).getd());
    assertequalsexacttype(227, message.getextension(repeatedlazymessageextensionlite   ,0).getbb());

    assertequalsexacttype(testalltypeslite.nestedenum.bar,
      message.getextension(repeatednestedenumextensionlite, 0));
    assertequalsexacttype(foreignenumlite.foreign_lite_bar,
      message.getextension(repeatedforeignenumextensionlite, 0));
    assertequalsexacttype(importenumlite.import_lite_bar,
      message.getextension(repeatedimportenumextensionlite, 0));

    assertequalsexacttype("224", message.getextension(repeatedstringpieceextensionlite, 0));
    assertequalsexacttype("225", message.getextension(repeatedcordextensionlite, 0));

    // actually verify the second (modified) elements now.
    assertequalsexacttype(501  , message.getextension(repeatedint32extensionlite   , 1));
    assertequalsexacttype(502l , message.getextension(repeatedint64extensionlite   , 1));
    assertequalsexacttype(503  , message.getextension(repeateduint32extensionlite  , 1));
    assertequalsexacttype(504l , message.getextension(repeateduint64extensionlite  , 1));
    assertequalsexacttype(505  , message.getextension(repeatedsint32extensionlite  , 1));
    assertequalsexacttype(506l , message.getextension(repeatedsint64extensionlite  , 1));
    assertequalsexacttype(507  , message.getextension(repeatedfixed32extensionlite , 1));
    assertequalsexacttype(508l , message.getextension(repeatedfixed64extensionlite , 1));
    assertequalsexacttype(509  , message.getextension(repeatedsfixed32extensionlite, 1));
    assertequalsexacttype(510l , message.getextension(repeatedsfixed64extensionlite, 1));
    assertequalsexacttype(511f , message.getextension(repeatedfloatextensionlite   , 1));
    assertequalsexacttype(512d , message.getextension(repeateddoubleextensionlite  , 1));
    assertequalsexacttype(true , message.getextension(repeatedboolextensionlite    , 1));
    assertequalsexacttype("515", message.getextension(repeatedstringextensionlite  , 1));
    assertequalsexacttype(tobytes("516"), message.getextension(repeatedbytesextensionlite, 1));

    assertequalsexacttype(517, message.getextension(repeatedgroupextensionlite         ,1).geta());
    assertequalsexacttype(518, message.getextension(repeatednestedmessageextensionlite ,1).getbb());
    assertequalsexacttype(519, message.getextension(repeatedforeignmessageextensionlite,1).getc());
    assertequalsexacttype(520, message.getextension(repeatedimportmessageextensionlite ,1).getd());
    assertequalsexacttype(527, message.getextension(repeatedlazymessageextensionlite   ,1).getbb());

    assertequalsexacttype(testalltypeslite.nestedenum.foo,
      message.getextension(repeatednestedenumextensionlite, 1));
    assertequalsexacttype(foreignenumlite.foreign_lite_foo,
      message.getextension(repeatedforeignenumextensionlite, 1));
    assertequalsexacttype(importenumlite.import_lite_foo,
      message.getextension(repeatedimportenumextensionlite, 1));

    assertequalsexacttype("524", message.getextension(repeatedstringpieceextensionlite, 1));
    assertequalsexacttype("525", message.getextension(repeatedcordextensionlite, 1));
  }

  public static void setpackedextensions(testpackedextensionslite.builder message) {
    message.addextension(packedint32extensionlite   , 601);
    message.addextension(packedint64extensionlite   , 602l);
    message.addextension(packeduint32extensionlite  , 603);
    message.addextension(packeduint64extensionlite  , 604l);
    message.addextension(packedsint32extensionlite  , 605);
    message.addextension(packedsint64extensionlite  , 606l);
    message.addextension(packedfixed32extensionlite , 607);
    message.addextension(packedfixed64extensionlite , 608l);
    message.addextension(packedsfixed32extensionlite, 609);
    message.addextension(packedsfixed64extensionlite, 610l);
    message.addextension(packedfloatextensionlite   , 611f);
    message.addextension(packeddoubleextensionlite  , 612d);
    message.addextension(packedboolextensionlite    , true);
    message.addextension(packedenumextensionlite, foreignenumlite.foreign_lite_bar);
    // add a second one of each field.
    message.addextension(packedint32extensionlite   , 701);
    message.addextension(packedint64extensionlite   , 702l);
    message.addextension(packeduint32extensionlite  , 703);
    message.addextension(packeduint64extensionlite  , 704l);
    message.addextension(packedsint32extensionlite  , 705);
    message.addextension(packedsint64extensionlite  , 706l);
    message.addextension(packedfixed32extensionlite , 707);
    message.addextension(packedfixed64extensionlite , 708l);
    message.addextension(packedsfixed32extensionlite, 709);
    message.addextension(packedsfixed64extensionlite, 710l);
    message.addextension(packedfloatextensionlite   , 711f);
    message.addextension(packeddoubleextensionlite  , 712d);
    message.addextension(packedboolextensionlite    , false);
    message.addextension(packedenumextensionlite, foreignenumlite.foreign_lite_baz);
  }

  public static void assertpackedextensionsset(testpackedextensionslite message) {
    assert.assertequals(2, message.getextensioncount(packedint32extensionlite   ));
    assert.assertequals(2, message.getextensioncount(packedint64extensionlite   ));
    assert.assertequals(2, message.getextensioncount(packeduint32extensionlite  ));
    assert.assertequals(2, message.getextensioncount(packeduint64extensionlite  ));
    assert.assertequals(2, message.getextensioncount(packedsint32extensionlite  ));
    assert.assertequals(2, message.getextensioncount(packedsint64extensionlite  ));
    assert.assertequals(2, message.getextensioncount(packedfixed32extensionlite ));
    assert.assertequals(2, message.getextensioncount(packedfixed64extensionlite ));
    assert.assertequals(2, message.getextensioncount(packedsfixed32extensionlite));
    assert.assertequals(2, message.getextensioncount(packedsfixed64extensionlite));
    assert.assertequals(2, message.getextensioncount(packedfloatextensionlite   ));
    assert.assertequals(2, message.getextensioncount(packeddoubleextensionlite  ));
    assert.assertequals(2, message.getextensioncount(packedboolextensionlite    ));
    assert.assertequals(2, message.getextensioncount(packedenumextensionlite));
    assertequalsexacttype(601  , message.getextension(packedint32extensionlite   , 0));
    assertequalsexacttype(602l , message.getextension(packedint64extensionlite   , 0));
    assertequalsexacttype(603  , message.getextension(packeduint32extensionlite  , 0));
    assertequalsexacttype(604l , message.getextension(packeduint64extensionlite  , 0));
    assertequalsexacttype(605  , message.getextension(packedsint32extensionlite  , 0));
    assertequalsexacttype(606l , message.getextension(packedsint64extensionlite  , 0));
    assertequalsexacttype(607  , message.getextension(packedfixed32extensionlite , 0));
    assertequalsexacttype(608l , message.getextension(packedfixed64extensionlite , 0));
    assertequalsexacttype(609  , message.getextension(packedsfixed32extensionlite, 0));
    assertequalsexacttype(610l , message.getextension(packedsfixed64extensionlite, 0));
    assertequalsexacttype(611f , message.getextension(packedfloatextensionlite   , 0));
    assertequalsexacttype(612d , message.getextension(packeddoubleextensionlite  , 0));
    assertequalsexacttype(true , message.getextension(packedboolextensionlite    , 0));
    assertequalsexacttype(foreignenumlite.foreign_lite_bar,
                          message.getextension(packedenumextensionlite, 0));
    assertequalsexacttype(701  , message.getextension(packedint32extensionlite   , 1));
    assertequalsexacttype(702l , message.getextension(packedint64extensionlite   , 1));
    assertequalsexacttype(703  , message.getextension(packeduint32extensionlite  , 1));
    assertequalsexacttype(704l , message.getextension(packeduint64extensionlite  , 1));
    assertequalsexacttype(705  , message.getextension(packedsint32extensionlite  , 1));
    assertequalsexacttype(706l , message.getextension(packedsint64extensionlite  , 1));
    assertequalsexacttype(707  , message.getextension(packedfixed32extensionlite , 1));
    assertequalsexacttype(708l , message.getextension(packedfixed64extensionlite , 1));
    assertequalsexacttype(709  , message.getextension(packedsfixed32extensionlite, 1));
    assertequalsexacttype(710l , message.getextension(packedsfixed64extensionlite, 1));
    assertequalsexacttype(711f , message.getextension(packedfloatextensionlite   , 1));
    assertequalsexacttype(712d , message.getextension(packeddoubleextensionlite  , 1));
    assertequalsexacttype(false, message.getextension(packedboolextensionlite    , 1));
    assertequalsexacttype(foreignenumlite.foreign_lite_baz,
                          message.getextension(packedenumextensionlite, 1));
  }

  // =================================================================

  /**
   * performs the same things that the methods of {@code testutil} do, but
   * via the reflection interface.  this is its own class because it needs
   * to know what descriptor to use.
   */
  public static class reflectiontester {
    private final descriptors.descriptor basedescriptor;
    private final extensionregistry extensionregistry;

    private final descriptors.filedescriptor file;
    private final descriptors.filedescriptor importfile;
    private final descriptors.filedescriptor publicimportfile;

    private final descriptors.descriptor optionalgroup;
    private final descriptors.descriptor repeatedgroup;
    private final descriptors.descriptor nestedmessage;
    private final descriptors.descriptor foreignmessage;
    private final descriptors.descriptor importmessage;
    private final descriptors.descriptor publicimportmessage;

    private final descriptors.fielddescriptor groupa;
    private final descriptors.fielddescriptor repeatedgroupa;
    private final descriptors.fielddescriptor nestedb;
    private final descriptors.fielddescriptor foreignc;
    private final descriptors.fielddescriptor importd;
    private final descriptors.fielddescriptor importe;

    private final descriptors.enumdescriptor nestedenum;
    private final descriptors.enumdescriptor foreignenum;
    private final descriptors.enumdescriptor importenum;

    private final descriptors.enumvaluedescriptor nestedfoo;
    private final descriptors.enumvaluedescriptor nestedbar;
    private final descriptors.enumvaluedescriptor nestedbaz;
    private final descriptors.enumvaluedescriptor foreignfoo;
    private final descriptors.enumvaluedescriptor foreignbar;
    private final descriptors.enumvaluedescriptor foreignbaz;
    private final descriptors.enumvaluedescriptor importfoo;
    private final descriptors.enumvaluedescriptor importbar;
    private final descriptors.enumvaluedescriptor importbaz;

    /**
     * construct a {@code reflectiontester} that will expect messages using
     * the given descriptor.
     *
     * normally {@code basedescriptor} should be a descriptor for the type
     * {@code testalltypes}, defined in
     * {@code google/protobuf/unittest.proto}.  however, if
     * {@code extensionregistry} is non-null, then {@code basedescriptor} should
     * be for {@code testallextensions} instead, and instead of reading and
     * writing normal fields, the tester will read and write extensions.
     * all of {@code testallextensions}' extensions must be registered in the
     * registry.
     */
    public reflectiontester(descriptors.descriptor basedescriptor,
                            extensionregistry extensionregistry) {
      this.basedescriptor = basedescriptor;
      this.extensionregistry = extensionregistry;

      this.file = basedescriptor.getfile();
      assert.assertequals(1, file.getdependencies().size());
      this.importfile = file.getdependencies().get(0);
      this.publicimportfile = importfile.getdependencies().get(0);

      descriptors.descriptor testalltypes;
      if (basedescriptor.getname() == "testalltypes") {
        testalltypes = basedescriptor;
      } else {
        testalltypes = file.findmessagetypebyname("testalltypes");
        assert.assertnotnull(testalltypes);
      }

      if (extensionregistry == null) {
        // use testalltypes, rather than basedescriptor, to allow
        // initialization using testpackedtypes descriptors. these objects
        // won't be used by the methods for packed fields.
        this.optionalgroup =
          testalltypes.findnestedtypebyname("optionalgroup");
        this.repeatedgroup =
          testalltypes.findnestedtypebyname("repeatedgroup");
      } else {
        this.optionalgroup =
          file.findmessagetypebyname("optionalgroup_extension");
        this.repeatedgroup =
          file.findmessagetypebyname("repeatedgroup_extension");
      }
      this.nestedmessage = testalltypes.findnestedtypebyname("nestedmessage");
      this.foreignmessage = file.findmessagetypebyname("foreignmessage");
      this.importmessage = importfile.findmessagetypebyname("importmessage");
      this.publicimportmessage = publicimportfile.findmessagetypebyname(
          "publicimportmessage");

      this.nestedenum = testalltypes.findenumtypebyname("nestedenum");
      this.foreignenum = file.findenumtypebyname("foreignenum");
      this.importenum = importfile.findenumtypebyname("importenum");

      assert.assertnotnull(optionalgroup );
      assert.assertnotnull(repeatedgroup );
      assert.assertnotnull(nestedmessage );
      assert.assertnotnull(foreignmessage);
      assert.assertnotnull(importmessage );
      assert.assertnotnull(nestedenum    );
      assert.assertnotnull(foreignenum   );
      assert.assertnotnull(importenum    );

      this.nestedb  = nestedmessage .findfieldbyname("bb");
      this.foreignc = foreignmessage.findfieldbyname("c");
      this.importd  = importmessage .findfieldbyname("d");
      this.importe  = publicimportmessage.findfieldbyname("e");
      this.nestedfoo = nestedenum.findvaluebyname("foo");
      this.nestedbar = nestedenum.findvaluebyname("bar");
      this.nestedbaz = nestedenum.findvaluebyname("baz");
      this.foreignfoo = foreignenum.findvaluebyname("foreign_foo");
      this.foreignbar = foreignenum.findvaluebyname("foreign_bar");
      this.foreignbaz = foreignenum.findvaluebyname("foreign_baz");
      this.importfoo = importenum.findvaluebyname("import_foo");
      this.importbar = importenum.findvaluebyname("import_bar");
      this.importbaz = importenum.findvaluebyname("import_baz");

      this.groupa = optionalgroup.findfieldbyname("a");
      this.repeatedgroupa = repeatedgroup.findfieldbyname("a");

      assert.assertnotnull(groupa        );
      assert.assertnotnull(repeatedgroupa);
      assert.assertnotnull(nestedb       );
      assert.assertnotnull(foreignc      );
      assert.assertnotnull(importd       );
      assert.assertnotnull(importe       );
      assert.assertnotnull(nestedfoo     );
      assert.assertnotnull(nestedbar     );
      assert.assertnotnull(nestedbaz     );
      assert.assertnotnull(foreignfoo    );
      assert.assertnotnull(foreignbar    );
      assert.assertnotnull(foreignbaz    );
      assert.assertnotnull(importfoo     );
      assert.assertnotnull(importbar     );
      assert.assertnotnull(importbaz     );
    }

    /**
     * shorthand to get a fielddescriptor for a field of unittest::testalltypes.
     */
    private descriptors.fielddescriptor f(string name) {
      descriptors.fielddescriptor result;
      if (extensionregistry == null) {
        result = basedescriptor.findfieldbyname(name);
      } else {
        result = file.findextensionbyname(name + "_extension");
      }
      assert.assertnotnull(result);
      return result;
    }

    /**
     * calls {@code parent.newbuilderforfield()} or uses the
     * {@code extensionregistry} to find an appropriate builder, depending
     * on what type is being tested.
     */
    private message.builder newbuilderforfield(
        message.builder parent, descriptors.fielddescriptor field) {
      if (extensionregistry == null) {
        return parent.newbuilderforfield(field);
      } else {
        extensionregistry.extensioninfo extension =
          extensionregistry.findextensionbynumber(field.getcontainingtype(),
                                                  field.getnumber());
        assert.assertnotnull(extension);
        assert.assertnotnull(extension.defaultinstance);
        return extension.defaultinstance.newbuilderfortype();
      }
    }

    // -------------------------------------------------------------------

    /**
     * set every field of {@code message} to the values expected by
     * {@code assertallfieldsset()}, using the {@link message.builder}
     * reflection interface.
     */
    void setallfieldsviareflection(message.builder message) {
      message.setfield(f("optional_int32"   ), 101 );
      message.setfield(f("optional_int64"   ), 102l);
      message.setfield(f("optional_uint32"  ), 103 );
      message.setfield(f("optional_uint64"  ), 104l);
      message.setfield(f("optional_sint32"  ), 105 );
      message.setfield(f("optional_sint64"  ), 106l);
      message.setfield(f("optional_fixed32" ), 107 );
      message.setfield(f("optional_fixed64" ), 108l);
      message.setfield(f("optional_sfixed32"), 109 );
      message.setfield(f("optional_sfixed64"), 110l);
      message.setfield(f("optional_float"   ), 111f);
      message.setfield(f("optional_double"  ), 112d);
      message.setfield(f("optional_bool"    ), true);
      message.setfield(f("optional_string"  ), "115");
      message.setfield(f("optional_bytes"   ), tobytes("116"));

      message.setfield(f("optionalgroup"),
        newbuilderforfield(message, f("optionalgroup"))
               .setfield(groupa, 117).build());
      message.setfield(f("optional_nested_message"),
        newbuilderforfield(message, f("optional_nested_message"))
               .setfield(nestedb, 118).build());
      message.setfield(f("optional_foreign_message"),
        newbuilderforfield(message, f("optional_foreign_message"))
               .setfield(foreignc, 119).build());
      message.setfield(f("optional_import_message"),
        newbuilderforfield(message, f("optional_import_message"))
               .setfield(importd, 120).build());
      message.setfield(f("optional_public_import_message"),
        newbuilderforfield(message, f("optional_public_import_message"))
               .setfield(importe, 126).build());
      message.setfield(f("optional_lazy_message"),
        newbuilderforfield(message, f("optional_lazy_message"))
               .setfield(nestedb, 127).build());

      message.setfield(f("optional_nested_enum" ),  nestedbaz);
      message.setfield(f("optional_foreign_enum"), foreignbaz);
      message.setfield(f("optional_import_enum" ),  importbaz);

      message.setfield(f("optional_string_piece" ), "124");
      message.setfield(f("optional_cord" ), "125");

      // -----------------------------------------------------------------

      message.addrepeatedfield(f("repeated_int32"   ), 201 );
      message.addrepeatedfield(f("repeated_int64"   ), 202l);
      message.addrepeatedfield(f("repeated_uint32"  ), 203 );
      message.addrepeatedfield(f("repeated_uint64"  ), 204l);
      message.addrepeatedfield(f("repeated_sint32"  ), 205 );
      message.addrepeatedfield(f("repeated_sint64"  ), 206l);
      message.addrepeatedfield(f("repeated_fixed32" ), 207 );
      message.addrepeatedfield(f("repeated_fixed64" ), 208l);
      message.addrepeatedfield(f("repeated_sfixed32"), 209 );
      message.addrepeatedfield(f("repeated_sfixed64"), 210l);
      message.addrepeatedfield(f("repeated_float"   ), 211f);
      message.addrepeatedfield(f("repeated_double"  ), 212d);
      message.addrepeatedfield(f("repeated_bool"    ), true);
      message.addrepeatedfield(f("repeated_string"  ), "215");
      message.addrepeatedfield(f("repeated_bytes"   ), tobytes("216"));

      message.addrepeatedfield(f("repeatedgroup"),
        newbuilderforfield(message, f("repeatedgroup"))
               .setfield(repeatedgroupa, 217).build());
      message.addrepeatedfield(f("repeated_nested_message"),
        newbuilderforfield(message, f("repeated_nested_message"))
               .setfield(nestedb, 218).build());
      message.addrepeatedfield(f("repeated_foreign_message"),
        newbuilderforfield(message, f("repeated_foreign_message"))
               .setfield(foreignc, 219).build());
      message.addrepeatedfield(f("repeated_import_message"),
        newbuilderforfield(message, f("repeated_import_message"))
               .setfield(importd, 220).build());
      message.addrepeatedfield(f("repeated_lazy_message"),
        newbuilderforfield(message, f("repeated_lazy_message"))
               .setfield(nestedb, 227).build());

      message.addrepeatedfield(f("repeated_nested_enum" ),  nestedbar);
      message.addrepeatedfield(f("repeated_foreign_enum"), foreignbar);
      message.addrepeatedfield(f("repeated_import_enum" ),  importbar);

      message.addrepeatedfield(f("repeated_string_piece" ), "224");
      message.addrepeatedfield(f("repeated_cord" ), "225");

      // add a second one of each field.
      message.addrepeatedfield(f("repeated_int32"   ), 301 );
      message.addrepeatedfield(f("repeated_int64"   ), 302l);
      message.addrepeatedfield(f("repeated_uint32"  ), 303 );
      message.addrepeatedfield(f("repeated_uint64"  ), 304l);
      message.addrepeatedfield(f("repeated_sint32"  ), 305 );
      message.addrepeatedfield(f("repeated_sint64"  ), 306l);
      message.addrepeatedfield(f("repeated_fixed32" ), 307 );
      message.addrepeatedfield(f("repeated_fixed64" ), 308l);
      message.addrepeatedfield(f("repeated_sfixed32"), 309 );
      message.addrepeatedfield(f("repeated_sfixed64"), 310l);
      message.addrepeatedfield(f("repeated_float"   ), 311f);
      message.addrepeatedfield(f("repeated_double"  ), 312d);
      message.addrepeatedfield(f("repeated_bool"    ), false);
      message.addrepeatedfield(f("repeated_string"  ), "315");
      message.addrepeatedfield(f("repeated_bytes"   ), tobytes("316"));

      message.addrepeatedfield(f("repeatedgroup"),
        newbuilderforfield(message, f("repeatedgroup"))
               .setfield(repeatedgroupa, 317).build());
      message.addrepeatedfield(f("repeated_nested_message"),
        newbuilderforfield(message, f("repeated_nested_message"))
               .setfield(nestedb, 318).build());
      message.addrepeatedfield(f("repeated_foreign_message"),
        newbuilderforfield(message, f("repeated_foreign_message"))
               .setfield(foreignc, 319).build());
      message.addrepeatedfield(f("repeated_import_message"),
        newbuilderforfield(message, f("repeated_import_message"))
               .setfield(importd, 320).build());
      message.addrepeatedfield(f("repeated_lazy_message"),
        newbuilderforfield(message, f("repeated_lazy_message"))
               .setfield(nestedb, 327).build());

      message.addrepeatedfield(f("repeated_nested_enum" ),  nestedbaz);
      message.addrepeatedfield(f("repeated_foreign_enum"), foreignbaz);
      message.addrepeatedfield(f("repeated_import_enum" ),  importbaz);

      message.addrepeatedfield(f("repeated_string_piece" ), "324");
      message.addrepeatedfield(f("repeated_cord" ), "325");

      // -----------------------------------------------------------------

      message.setfield(f("default_int32"   ), 401 );
      message.setfield(f("default_int64"   ), 402l);
      message.setfield(f("default_uint32"  ), 403 );
      message.setfield(f("default_uint64"  ), 404l);
      message.setfield(f("default_sint32"  ), 405 );
      message.setfield(f("default_sint64"  ), 406l);
      message.setfield(f("default_fixed32" ), 407 );
      message.setfield(f("default_fixed64" ), 408l);
      message.setfield(f("default_sfixed32"), 409 );
      message.setfield(f("default_sfixed64"), 410l);
      message.setfield(f("default_float"   ), 411f);
      message.setfield(f("default_double"  ), 412d);
      message.setfield(f("default_bool"    ), false);
      message.setfield(f("default_string"  ), "415");
      message.setfield(f("default_bytes"   ), tobytes("416"));

      message.setfield(f("default_nested_enum" ),  nestedfoo);
      message.setfield(f("default_foreign_enum"), foreignfoo);
      message.setfield(f("default_import_enum" ),  importfoo);

      message.setfield(f("default_string_piece" ), "424");
      message.setfield(f("default_cord" ), "425");
    }

    // -------------------------------------------------------------------

    /**
     * modify the repeated fields of {@code message} to contain the values
     * expected by {@code assertrepeatedfieldsmodified()}, using the
     * {@link message.builder} reflection interface.
     */
    void modifyrepeatedfieldsviareflection(message.builder message) {
      message.setrepeatedfield(f("repeated_int32"   ), 1, 501 );
      message.setrepeatedfield(f("repeated_int64"   ), 1, 502l);
      message.setrepeatedfield(f("repeated_uint32"  ), 1, 503 );
      message.setrepeatedfield(f("repeated_uint64"  ), 1, 504l);
      message.setrepeatedfield(f("repeated_sint32"  ), 1, 505 );
      message.setrepeatedfield(f("repeated_sint64"  ), 1, 506l);
      message.setrepeatedfield(f("repeated_fixed32" ), 1, 507 );
      message.setrepeatedfield(f("repeated_fixed64" ), 1, 508l);
      message.setrepeatedfield(f("repeated_sfixed32"), 1, 509 );
      message.setrepeatedfield(f("repeated_sfixed64"), 1, 510l);
      message.setrepeatedfield(f("repeated_float"   ), 1, 511f);
      message.setrepeatedfield(f("repeated_double"  ), 1, 512d);
      message.setrepeatedfield(f("repeated_bool"    ), 1, true);
      message.setrepeatedfield(f("repeated_string"  ), 1, "515");
      message.setrepeatedfield(f("repeated_bytes"   ), 1, tobytes("516"));

      message.setrepeatedfield(f("repeatedgroup"), 1,
        newbuilderforfield(message, f("repeatedgroup"))
               .setfield(repeatedgroupa, 517).build());
      message.setrepeatedfield(f("repeated_nested_message"), 1,
        newbuilderforfield(message, f("repeated_nested_message"))
               .setfield(nestedb, 518).build());
      message.setrepeatedfield(f("repeated_foreign_message"), 1,
        newbuilderforfield(message, f("repeated_foreign_message"))
               .setfield(foreignc, 519).build());
      message.setrepeatedfield(f("repeated_import_message"), 1,
        newbuilderforfield(message, f("repeated_import_message"))
               .setfield(importd, 520).build());
      message.setrepeatedfield(f("repeated_lazy_message"), 1,
        newbuilderforfield(message, f("repeated_lazy_message"))
               .setfield(nestedb, 527).build());

      message.setrepeatedfield(f("repeated_nested_enum" ), 1,  nestedfoo);
      message.setrepeatedfield(f("repeated_foreign_enum"), 1, foreignfoo);
      message.setrepeatedfield(f("repeated_import_enum" ), 1,  importfoo);

      message.setrepeatedfield(f("repeated_string_piece"), 1, "524");
      message.setrepeatedfield(f("repeated_cord"), 1, "525");
    }

    // -------------------------------------------------------------------

    /**
     * assert (using {@code junit.framework.assert}} that all fields of
     * {@code message} are set to the values assigned by {@code setallfields},
     * using the {@link message} reflection interface.
     */
    public void assertallfieldssetviareflection(messageorbuilder message) {
      assert.asserttrue(message.hasfield(f("optional_int32"   )));
      assert.asserttrue(message.hasfield(f("optional_int64"   )));
      assert.asserttrue(message.hasfield(f("optional_uint32"  )));
      assert.asserttrue(message.hasfield(f("optional_uint64"  )));
      assert.asserttrue(message.hasfield(f("optional_sint32"  )));
      assert.asserttrue(message.hasfield(f("optional_sint64"  )));
      assert.asserttrue(message.hasfield(f("optional_fixed32" )));
      assert.asserttrue(message.hasfield(f("optional_fixed64" )));
      assert.asserttrue(message.hasfield(f("optional_sfixed32")));
      assert.asserttrue(message.hasfield(f("optional_sfixed64")));
      assert.asserttrue(message.hasfield(f("optional_float"   )));
      assert.asserttrue(message.hasfield(f("optional_double"  )));
      assert.asserttrue(message.hasfield(f("optional_bool"    )));
      assert.asserttrue(message.hasfield(f("optional_string"  )));
      assert.asserttrue(message.hasfield(f("optional_bytes"   )));

      assert.asserttrue(message.hasfield(f("optionalgroup"           )));
      assert.asserttrue(message.hasfield(f("optional_nested_message" )));
      assert.asserttrue(message.hasfield(f("optional_foreign_message")));
      assert.asserttrue(message.hasfield(f("optional_import_message" )));

      assert.asserttrue(
        ((message)message.getfield(f("optionalgroup"))).hasfield(groupa));
      assert.asserttrue(
        ((message)message.getfield(f("optional_nested_message")))
                         .hasfield(nestedb));
      assert.asserttrue(
        ((message)message.getfield(f("optional_foreign_message")))
                         .hasfield(foreignc));
      assert.asserttrue(
        ((message)message.getfield(f("optional_import_message")))
                         .hasfield(importd));

      assert.asserttrue(message.hasfield(f("optional_nested_enum" )));
      assert.asserttrue(message.hasfield(f("optional_foreign_enum")));
      assert.asserttrue(message.hasfield(f("optional_import_enum" )));

      assert.asserttrue(message.hasfield(f("optional_string_piece")));
      assert.asserttrue(message.hasfield(f("optional_cord")));

      assert.assertequals(101  , message.getfield(f("optional_int32"   )));
      assert.assertequals(102l , message.getfield(f("optional_int64"   )));
      assert.assertequals(103  , message.getfield(f("optional_uint32"  )));
      assert.assertequals(104l , message.getfield(f("optional_uint64"  )));
      assert.assertequals(105  , message.getfield(f("optional_sint32"  )));
      assert.assertequals(106l , message.getfield(f("optional_sint64"  )));
      assert.assertequals(107  , message.getfield(f("optional_fixed32" )));
      assert.assertequals(108l , message.getfield(f("optional_fixed64" )));
      assert.assertequals(109  , message.getfield(f("optional_sfixed32")));
      assert.assertequals(110l , message.getfield(f("optional_sfixed64")));
      assert.assertequals(111f , message.getfield(f("optional_float"   )));
      assert.assertequals(112d , message.getfield(f("optional_double"  )));
      assert.assertequals(true , message.getfield(f("optional_bool"    )));
      assert.assertequals("115", message.getfield(f("optional_string"  )));
      assert.assertequals(tobytes("116"), message.getfield(f("optional_bytes")));

      assert.assertequals(117,
        ((message)message.getfield(f("optionalgroup"))).getfield(groupa));
      assert.assertequals(118,
        ((message)message.getfield(f("optional_nested_message")))
                         .getfield(nestedb));
      assert.assertequals(119,
        ((message)message.getfield(f("optional_foreign_message")))
                         .getfield(foreignc));
      assert.assertequals(120,
        ((message)message.getfield(f("optional_import_message")))
                         .getfield(importd));
      assert.assertequals(126,
        ((message)message.getfield(f("optional_public_import_message")))
                         .getfield(importe));
      assert.assertequals(127,
        ((message)message.getfield(f("optional_lazy_message")))
                         .getfield(nestedb));

      assert.assertequals( nestedbaz, message.getfield(f("optional_nested_enum" )));
      assert.assertequals(foreignbaz, message.getfield(f("optional_foreign_enum")));
      assert.assertequals( importbaz, message.getfield(f("optional_import_enum" )));

      assert.assertequals("124", message.getfield(f("optional_string_piece")));
      assert.assertequals("125", message.getfield(f("optional_cord")));

      // -----------------------------------------------------------------

      assert.assertequals(2, message.getrepeatedfieldcount(f("repeated_int32"   )));
      assert.assertequals(2, message.getrepeatedfieldcount(f("repeated_int64"   )));
      assert.assertequals(2, message.getrepeatedfieldcount(f("repeated_uint32"  )));
      assert.assertequals(2, message.getrepeatedfieldcount(f("repeated_uint64"  )));
      assert.assertequals(2, message.getrepeatedfieldcount(f("repeated_sint32"  )));
      assert.assertequals(2, message.getrepeatedfieldcount(f("repeated_sint64"  )));
      assert.assertequals(2, message.getrepeatedfieldcount(f("repeated_fixed32" )));
      assert.assertequals(2, message.getrepeatedfieldcount(f("repeated_fixed64" )));
      assert.assertequals(2, message.getrepeatedfieldcount(f("repeated_sfixed32")));
      assert.assertequals(2, message.getrepeatedfieldcount(f("repeated_sfixed64")));
      assert.assertequals(2, message.getrepeatedfieldcount(f("repeated_float"   )));
      assert.assertequals(2, message.getrepeatedfieldcount(f("repeated_double"  )));
      assert.assertequals(2, message.getrepeatedfieldcount(f("repeated_bool"    )));
      assert.assertequals(2, message.getrepeatedfieldcount(f("repeated_string"  )));
      assert.assertequals(2, message.getrepeatedfieldcount(f("repeated_bytes"   )));

      assert.assertequals(2, message.getrepeatedfieldcount(f("repeatedgroup"           )));
      assert.assertequals(2, message.getrepeatedfieldcount(f("repeated_nested_message" )));
      assert.assertequals(2, message.getrepeatedfieldcount(f("repeated_foreign_message")));
      assert.assertequals(2, message.getrepeatedfieldcount(f("repeated_import_message" )));
      assert.assertequals(2, message.getrepeatedfieldcount(f("repeated_lazy_message" )));
      assert.assertequals(2, message.getrepeatedfieldcount(f("repeated_nested_enum"    )));
      assert.assertequals(2, message.getrepeatedfieldcount(f("repeated_foreign_enum"   )));
      assert.assertequals(2, message.getrepeatedfieldcount(f("repeated_import_enum"    )));

      assert.assertequals(2, message.getrepeatedfieldcount(f("repeated_string_piece")));
      assert.assertequals(2, message.getrepeatedfieldcount(f("repeated_cord")));

      assert.assertequals(201  , message.getrepeatedfield(f("repeated_int32"   ), 0));
      assert.assertequals(202l , message.getrepeatedfield(f("repeated_int64"   ), 0));
      assert.assertequals(203  , message.getrepeatedfield(f("repeated_uint32"  ), 0));
      assert.assertequals(204l , message.getrepeatedfield(f("repeated_uint64"  ), 0));
      assert.assertequals(205  , message.getrepeatedfield(f("repeated_sint32"  ), 0));
      assert.assertequals(206l , message.getrepeatedfield(f("repeated_sint64"  ), 0));
      assert.assertequals(207  , message.getrepeatedfield(f("repeated_fixed32" ), 0));
      assert.assertequals(208l , message.getrepeatedfield(f("repeated_fixed64" ), 0));
      assert.assertequals(209  , message.getrepeatedfield(f("repeated_sfixed32"), 0));
      assert.assertequals(210l , message.getrepeatedfield(f("repeated_sfixed64"), 0));
      assert.assertequals(211f , message.getrepeatedfield(f("repeated_float"   ), 0));
      assert.assertequals(212d , message.getrepeatedfield(f("repeated_double"  ), 0));
      assert.assertequals(true , message.getrepeatedfield(f("repeated_bool"    ), 0));
      assert.assertequals("215", message.getrepeatedfield(f("repeated_string"  ), 0));
      assert.assertequals(tobytes("216"), message.getrepeatedfield(f("repeated_bytes"), 0));

      assert.assertequals(217,
        ((message)message.getrepeatedfield(f("repeatedgroup"), 0))
                         .getfield(repeatedgroupa));
      assert.assertequals(218,
        ((message)message.getrepeatedfield(f("repeated_nested_message"), 0))
                         .getfield(nestedb));
      assert.assertequals(219,
        ((message)message.getrepeatedfield(f("repeated_foreign_message"), 0))
                         .getfield(foreignc));
      assert.assertequals(220,
        ((message)message.getrepeatedfield(f("repeated_import_message"), 0))
                         .getfield(importd));
      assert.assertequals(227,
        ((message)message.getrepeatedfield(f("repeated_lazy_message"), 0))
                         .getfield(nestedb));

      assert.assertequals( nestedbar, message.getrepeatedfield(f("repeated_nested_enum" ),0));
      assert.assertequals(foreignbar, message.getrepeatedfield(f("repeated_foreign_enum"),0));
      assert.assertequals( importbar, message.getrepeatedfield(f("repeated_import_enum" ),0));

      assert.assertequals("224", message.getrepeatedfield(f("repeated_string_piece"), 0));
      assert.assertequals("225", message.getrepeatedfield(f("repeated_cord"), 0));

      assert.assertequals(301  , message.getrepeatedfield(f("repeated_int32"   ), 1));
      assert.assertequals(302l , message.getrepeatedfield(f("repeated_int64"   ), 1));
      assert.assertequals(303  , message.getrepeatedfield(f("repeated_uint32"  ), 1));
      assert.assertequals(304l , message.getrepeatedfield(f("repeated_uint64"  ), 1));
      assert.assertequals(305  , message.getrepeatedfield(f("repeated_sint32"  ), 1));
      assert.assertequals(306l , message.getrepeatedfield(f("repeated_sint64"  ), 1));
      assert.assertequals(307  , message.getrepeatedfield(f("repeated_fixed32" ), 1));
      assert.assertequals(308l , message.getrepeatedfield(f("repeated_fixed64" ), 1));
      assert.assertequals(309  , message.getrepeatedfield(f("repeated_sfixed32"), 1));
      assert.assertequals(310l , message.getrepeatedfield(f("repeated_sfixed64"), 1));
      assert.assertequals(311f , message.getrepeatedfield(f("repeated_float"   ), 1));
      assert.assertequals(312d , message.getrepeatedfield(f("repeated_double"  ), 1));
      assert.assertequals(false, message.getrepeatedfield(f("repeated_bool"    ), 1));
      assert.assertequals("315", message.getrepeatedfield(f("repeated_string"  ), 1));
      assert.assertequals(tobytes("316"), message.getrepeatedfield(f("repeated_bytes"), 1));

      assert.assertequals(317,
        ((message)message.getrepeatedfield(f("repeatedgroup"), 1))
                         .getfield(repeatedgroupa));
      assert.assertequals(318,
        ((message)message.getrepeatedfield(f("repeated_nested_message"), 1))
                         .getfield(nestedb));
      assert.assertequals(319,
        ((message)message.getrepeatedfield(f("repeated_foreign_message"), 1))
                         .getfield(foreignc));
      assert.assertequals(320,
        ((message)message.getrepeatedfield(f("repeated_import_message"), 1))
                         .getfield(importd));
      assert.assertequals(327,
        ((message)message.getrepeatedfield(f("repeated_lazy_message"), 1))
                         .getfield(nestedb));

      assert.assertequals( nestedbaz, message.getrepeatedfield(f("repeated_nested_enum" ),1));
      assert.assertequals(foreignbaz, message.getrepeatedfield(f("repeated_foreign_enum"),1));
      assert.assertequals( importbaz, message.getrepeatedfield(f("repeated_import_enum" ),1));

      assert.assertequals("324", message.getrepeatedfield(f("repeated_string_piece"), 1));
      assert.assertequals("325", message.getrepeatedfield(f("repeated_cord"), 1));

      // -----------------------------------------------------------------

      assert.asserttrue(message.hasfield(f("default_int32"   )));
      assert.asserttrue(message.hasfield(f("default_int64"   )));
      assert.asserttrue(message.hasfield(f("default_uint32"  )));
      assert.asserttrue(message.hasfield(f("default_uint64"  )));
      assert.asserttrue(message.hasfield(f("default_sint32"  )));
      assert.asserttrue(message.hasfield(f("default_sint64"  )));
      assert.asserttrue(message.hasfield(f("default_fixed32" )));
      assert.asserttrue(message.hasfield(f("default_fixed64" )));
      assert.asserttrue(message.hasfield(f("default_sfixed32")));
      assert.asserttrue(message.hasfield(f("default_sfixed64")));
      assert.asserttrue(message.hasfield(f("default_float"   )));
      assert.asserttrue(message.hasfield(f("default_double"  )));
      assert.asserttrue(message.hasfield(f("default_bool"    )));
      assert.asserttrue(message.hasfield(f("default_string"  )));
      assert.asserttrue(message.hasfield(f("default_bytes"   )));

      assert.asserttrue(message.hasfield(f("default_nested_enum" )));
      assert.asserttrue(message.hasfield(f("default_foreign_enum")));
      assert.asserttrue(message.hasfield(f("default_import_enum" )));

      assert.asserttrue(message.hasfield(f("default_string_piece")));
      assert.asserttrue(message.hasfield(f("default_cord")));

      assert.assertequals(401  , message.getfield(f("default_int32"   )));
      assert.assertequals(402l , message.getfield(f("default_int64"   )));
      assert.assertequals(403  , message.getfield(f("default_uint32"  )));
      assert.assertequals(404l , message.getfield(f("default_uint64"  )));
      assert.assertequals(405  , message.getfield(f("default_sint32"  )));
      assert.assertequals(406l , message.getfield(f("default_sint64"  )));
      assert.assertequals(407  , message.getfield(f("default_fixed32" )));
      assert.assertequals(408l , message.getfield(f("default_fixed64" )));
      assert.assertequals(409  , message.getfield(f("default_sfixed32")));
      assert.assertequals(410l , message.getfield(f("default_sfixed64")));
      assert.assertequals(411f , message.getfield(f("default_float"   )));
      assert.assertequals(412d , message.getfield(f("default_double"  )));
      assert.assertequals(false, message.getfield(f("default_bool"    )));
      assert.assertequals("415", message.getfield(f("default_string"  )));
      assert.assertequals(tobytes("416"), message.getfield(f("default_bytes")));

      assert.assertequals( nestedfoo, message.getfield(f("default_nested_enum" )));
      assert.assertequals(foreignfoo, message.getfield(f("default_foreign_enum")));
      assert.assertequals( importfoo, message.getfield(f("default_import_enum" )));

      assert.assertequals("424", message.getfield(f("default_string_piece")));
      assert.assertequals("425", message.getfield(f("default_cord")));
    }

    // -------------------------------------------------------------------

    /**
     * assert (using {@code junit.framework.assert}} that all fields of
     * {@code message} are cleared, and that getting the fields returns their
     * default values, using the {@link message} reflection interface.
     */
    public void assertclearviareflection(messageorbuilder message) {
      // has_blah() should initially be false for all optional fields.
      assert.assertfalse(message.hasfield(f("optional_int32"   )));
      assert.assertfalse(message.hasfield(f("optional_int64"   )));
      assert.assertfalse(message.hasfield(f("optional_uint32"  )));
      assert.assertfalse(message.hasfield(f("optional_uint64"  )));
      assert.assertfalse(message.hasfield(f("optional_sint32"  )));
      assert.assertfalse(message.hasfield(f("optional_sint64"  )));
      assert.assertfalse(message.hasfield(f("optional_fixed32" )));
      assert.assertfalse(message.hasfield(f("optional_fixed64" )));
      assert.assertfalse(message.hasfield(f("optional_sfixed32")));
      assert.assertfalse(message.hasfield(f("optional_sfixed64")));
      assert.assertfalse(message.hasfield(f("optional_float"   )));
      assert.assertfalse(message.hasfield(f("optional_double"  )));
      assert.assertfalse(message.hasfield(f("optional_bool"    )));
      assert.assertfalse(message.hasfield(f("optional_string"  )));
      assert.assertfalse(message.hasfield(f("optional_bytes"   )));

      assert.assertfalse(message.hasfield(f("optionalgroup"           )));
      assert.assertfalse(message.hasfield(f("optional_nested_message" )));
      assert.assertfalse(message.hasfield(f("optional_foreign_message")));
      assert.assertfalse(message.hasfield(f("optional_import_message" )));

      assert.assertfalse(message.hasfield(f("optional_nested_enum" )));
      assert.assertfalse(message.hasfield(f("optional_foreign_enum")));
      assert.assertfalse(message.hasfield(f("optional_import_enum" )));

      assert.assertfalse(message.hasfield(f("optional_string_piece")));
      assert.assertfalse(message.hasfield(f("optional_cord")));

      // optional fields without defaults are set to zero or something like it.
      assert.assertequals(0    , message.getfield(f("optional_int32"   )));
      assert.assertequals(0l   , message.getfield(f("optional_int64"   )));
      assert.assertequals(0    , message.getfield(f("optional_uint32"  )));
      assert.assertequals(0l   , message.getfield(f("optional_uint64"  )));
      assert.assertequals(0    , message.getfield(f("optional_sint32"  )));
      assert.assertequals(0l   , message.getfield(f("optional_sint64"  )));
      assert.assertequals(0    , message.getfield(f("optional_fixed32" )));
      assert.assertequals(0l   , message.getfield(f("optional_fixed64" )));
      assert.assertequals(0    , message.getfield(f("optional_sfixed32")));
      assert.assertequals(0l   , message.getfield(f("optional_sfixed64")));
      assert.assertequals(0f   , message.getfield(f("optional_float"   )));
      assert.assertequals(0d   , message.getfield(f("optional_double"  )));
      assert.assertequals(false, message.getfield(f("optional_bool"    )));
      assert.assertequals(""   , message.getfield(f("optional_string"  )));
      assert.assertequals(bytestring.empty, message.getfield(f("optional_bytes")));

      // embedded messages should also be clear.
      assert.assertfalse(
        ((message)message.getfield(f("optionalgroup"))).hasfield(groupa));
      assert.assertfalse(
        ((message)message.getfield(f("optional_nested_message")))
                         .hasfield(nestedb));
      assert.assertfalse(
        ((message)message.getfield(f("optional_foreign_message")))
                         .hasfield(foreignc));
      assert.assertfalse(
        ((message)message.getfield(f("optional_import_message")))
                         .hasfield(importd));
      assert.assertfalse(
        ((message)message.getfield(f("optional_public_import_message")))
                         .hasfield(importe));
      assert.assertfalse(
        ((message)message.getfield(f("optional_lazy_message")))
                         .hasfield(nestedb));

      assert.assertequals(0,
        ((message)message.getfield(f("optionalgroup"))).getfield(groupa));
      assert.assertequals(0,
        ((message)message.getfield(f("optional_nested_message")))
                         .getfield(nestedb));
      assert.assertequals(0,
        ((message)message.getfield(f("optional_foreign_message")))
                         .getfield(foreignc));
      assert.assertequals(0,
        ((message)message.getfield(f("optional_import_message")))
                         .getfield(importd));
      assert.assertequals(0,
        ((message)message.getfield(f("optional_public_import_message")))
                         .getfield(importe));
      assert.assertequals(0,
        ((message)message.getfield(f("optional_lazy_message")))
                         .getfield(nestedb));

      // enums without defaults are set to the first value in the enum.
      assert.assertequals( nestedfoo, message.getfield(f("optional_nested_enum" )));
      assert.assertequals(foreignfoo, message.getfield(f("optional_foreign_enum")));
      assert.assertequals( importfoo, message.getfield(f("optional_import_enum" )));

      assert.assertequals("", message.getfield(f("optional_string_piece")));
      assert.assertequals("", message.getfield(f("optional_cord")));

      // repeated fields are empty.
      assert.assertequals(0, message.getrepeatedfieldcount(f("repeated_int32"   )));
      assert.assertequals(0, message.getrepeatedfieldcount(f("repeated_int64"   )));
      assert.assertequals(0, message.getrepeatedfieldcount(f("repeated_uint32"  )));
      assert.assertequals(0, message.getrepeatedfieldcount(f("repeated_uint64"  )));
      assert.assertequals(0, message.getrepeatedfieldcount(f("repeated_sint32"  )));
      assert.assertequals(0, message.getrepeatedfieldcount(f("repeated_sint64"  )));
      assert.assertequals(0, message.getrepeatedfieldcount(f("repeated_fixed32" )));
      assert.assertequals(0, message.getrepeatedfieldcount(f("repeated_fixed64" )));
      assert.assertequals(0, message.getrepeatedfieldcount(f("repeated_sfixed32")));
      assert.assertequals(0, message.getrepeatedfieldcount(f("repeated_sfixed64")));
      assert.assertequals(0, message.getrepeatedfieldcount(f("repeated_float"   )));
      assert.assertequals(0, message.getrepeatedfieldcount(f("repeated_double"  )));
      assert.assertequals(0, message.getrepeatedfieldcount(f("repeated_bool"    )));
      assert.assertequals(0, message.getrepeatedfieldcount(f("repeated_string"  )));
      assert.assertequals(0, message.getrepeatedfieldcount(f("repeated_bytes"   )));

      assert.assertequals(0, message.getrepeatedfieldcount(f("repeatedgroup"           )));
      assert.assertequals(0, message.getrepeatedfieldcount(f("repeated_nested_message" )));
      assert.assertequals(0, message.getrepeatedfieldcount(f("repeated_foreign_message")));
      assert.assertequals(0, message.getrepeatedfieldcount(f("repeated_import_message" )));
      assert.assertequals(0, message.getrepeatedfieldcount(f("repeated_lazy_message"   )));
      assert.assertequals(0, message.getrepeatedfieldcount(f("repeated_nested_enum"    )));
      assert.assertequals(0, message.getrepeatedfieldcount(f("repeated_foreign_enum"   )));
      assert.assertequals(0, message.getrepeatedfieldcount(f("repeated_import_enum"    )));

      assert.assertequals(0, message.getrepeatedfieldcount(f("repeated_string_piece")));
      assert.assertequals(0, message.getrepeatedfieldcount(f("repeated_cord")));

      // has_blah() should also be false for all default fields.
      assert.assertfalse(message.hasfield(f("default_int32"   )));
      assert.assertfalse(message.hasfield(f("default_int64"   )));
      assert.assertfalse(message.hasfield(f("default_uint32"  )));
      assert.assertfalse(message.hasfield(f("default_uint64"  )));
      assert.assertfalse(message.hasfield(f("default_sint32"  )));
      assert.assertfalse(message.hasfield(f("default_sint64"  )));
      assert.assertfalse(message.hasfield(f("default_fixed32" )));
      assert.assertfalse(message.hasfield(f("default_fixed64" )));
      assert.assertfalse(message.hasfield(f("default_sfixed32")));
      assert.assertfalse(message.hasfield(f("default_sfixed64")));
      assert.assertfalse(message.hasfield(f("default_float"   )));
      assert.assertfalse(message.hasfield(f("default_double"  )));
      assert.assertfalse(message.hasfield(f("default_bool"    )));
      assert.assertfalse(message.hasfield(f("default_string"  )));
      assert.assertfalse(message.hasfield(f("default_bytes"   )));

      assert.assertfalse(message.hasfield(f("default_nested_enum" )));
      assert.assertfalse(message.hasfield(f("default_foreign_enum")));
      assert.assertfalse(message.hasfield(f("default_import_enum" )));

      assert.assertfalse(message.hasfield(f("default_string_piece" )));
      assert.assertfalse(message.hasfield(f("default_cord" )));

      // fields with defaults have their default values (duh).
      assert.assertequals( 41    , message.getfield(f("default_int32"   )));
      assert.assertequals( 42l   , message.getfield(f("default_int64"   )));
      assert.assertequals( 43    , message.getfield(f("default_uint32"  )));
      assert.assertequals( 44l   , message.getfield(f("default_uint64"  )));
      assert.assertequals(-45    , message.getfield(f("default_sint32"  )));
      assert.assertequals( 46l   , message.getfield(f("default_sint64"  )));
      assert.assertequals( 47    , message.getfield(f("default_fixed32" )));
      assert.assertequals( 48l   , message.getfield(f("default_fixed64" )));
      assert.assertequals( 49    , message.getfield(f("default_sfixed32")));
      assert.assertequals(-50l   , message.getfield(f("default_sfixed64")));
      assert.assertequals( 51.5f , message.getfield(f("default_float"   )));
      assert.assertequals( 52e3d , message.getfield(f("default_double"  )));
      assert.assertequals(true   , message.getfield(f("default_bool"    )));
      assert.assertequals("hello", message.getfield(f("default_string"  )));
      assert.assertequals(tobytes("world"), message.getfield(f("default_bytes")));

      assert.assertequals( nestedbar, message.getfield(f("default_nested_enum" )));
      assert.assertequals(foreignbar, message.getfield(f("default_foreign_enum")));
      assert.assertequals( importbar, message.getfield(f("default_import_enum" )));

      assert.assertequals("abc", message.getfield(f("default_string_piece")));
      assert.assertequals("123", message.getfield(f("default_cord")));
    }


    // ---------------------------------------------------------------

    public void assertrepeatedfieldsmodifiedviareflection(
        messageorbuilder message) {
      // modifyrepeatedfields only sets the second repeated element of each
      // field.  in addition to verifying this, we also verify that the first
      // element and size were *not* modified.
      assert.assertequals(2, message.getrepeatedfieldcount(f("repeated_int32"   )));
      assert.assertequals(2, message.getrepeatedfieldcount(f("repeated_int64"   )));
      assert.assertequals(2, message.getrepeatedfieldcount(f("repeated_uint32"  )));
      assert.assertequals(2, message.getrepeatedfieldcount(f("repeated_uint64"  )));
      assert.assertequals(2, message.getrepeatedfieldcount(f("repeated_sint32"  )));
      assert.assertequals(2, message.getrepeatedfieldcount(f("repeated_sint64"  )));
      assert.assertequals(2, message.getrepeatedfieldcount(f("repeated_fixed32" )));
      assert.assertequals(2, message.getrepeatedfieldcount(f("repeated_fixed64" )));
      assert.assertequals(2, message.getrepeatedfieldcount(f("repeated_sfixed32")));
      assert.assertequals(2, message.getrepeatedfieldcount(f("repeated_sfixed64")));
      assert.assertequals(2, message.getrepeatedfieldcount(f("repeated_float"   )));
      assert.assertequals(2, message.getrepeatedfieldcount(f("repeated_double"  )));
      assert.assertequals(2, message.getrepeatedfieldcount(f("repeated_bool"    )));
      assert.assertequals(2, message.getrepeatedfieldcount(f("repeated_string"  )));
      assert.assertequals(2, message.getrepeatedfieldcount(f("repeated_bytes"   )));

      assert.assertequals(2, message.getrepeatedfieldcount(f("repeatedgroup"           )));
      assert.assertequals(2, message.getrepeatedfieldcount(f("repeated_nested_message" )));
      assert.assertequals(2, message.getrepeatedfieldcount(f("repeated_foreign_message")));
      assert.assertequals(2, message.getrepeatedfieldcount(f("repeated_import_message" )));
      assert.assertequals(2, message.getrepeatedfieldcount(f("repeated_lazy_message"   )));
      assert.assertequals(2, message.getrepeatedfieldcount(f("repeated_nested_enum"    )));
      assert.assertequals(2, message.getrepeatedfieldcount(f("repeated_foreign_enum"   )));
      assert.assertequals(2, message.getrepeatedfieldcount(f("repeated_import_enum"    )));

      assert.assertequals(2, message.getrepeatedfieldcount(f("repeated_string_piece")));
      assert.assertequals(2, message.getrepeatedfieldcount(f("repeated_cord")));

      assert.assertequals(201  , message.getrepeatedfield(f("repeated_int32"   ), 0));
      assert.assertequals(202l , message.getrepeatedfield(f("repeated_int64"   ), 0));
      assert.assertequals(203  , message.getrepeatedfield(f("repeated_uint32"  ), 0));
      assert.assertequals(204l , message.getrepeatedfield(f("repeated_uint64"  ), 0));
      assert.assertequals(205  , message.getrepeatedfield(f("repeated_sint32"  ), 0));
      assert.assertequals(206l , message.getrepeatedfield(f("repeated_sint64"  ), 0));
      assert.assertequals(207  , message.getrepeatedfield(f("repeated_fixed32" ), 0));
      assert.assertequals(208l , message.getrepeatedfield(f("repeated_fixed64" ), 0));
      assert.assertequals(209  , message.getrepeatedfield(f("repeated_sfixed32"), 0));
      assert.assertequals(210l , message.getrepeatedfield(f("repeated_sfixed64"), 0));
      assert.assertequals(211f , message.getrepeatedfield(f("repeated_float"   ), 0));
      assert.assertequals(212d , message.getrepeatedfield(f("repeated_double"  ), 0));
      assert.assertequals(true , message.getrepeatedfield(f("repeated_bool"    ), 0));
      assert.assertequals("215", message.getrepeatedfield(f("repeated_string"  ), 0));
      assert.assertequals(tobytes("216"), message.getrepeatedfield(f("repeated_bytes"), 0));

      assert.assertequals(217,
        ((message)message.getrepeatedfield(f("repeatedgroup"), 0))
                         .getfield(repeatedgroupa));
      assert.assertequals(218,
        ((message)message.getrepeatedfield(f("repeated_nested_message"), 0))
                         .getfield(nestedb));
      assert.assertequals(219,
        ((message)message.getrepeatedfield(f("repeated_foreign_message"), 0))
                         .getfield(foreignc));
      assert.assertequals(220,
        ((message)message.getrepeatedfield(f("repeated_import_message"), 0))
                         .getfield(importd));
      assert.assertequals(227,
        ((message)message.getrepeatedfield(f("repeated_lazy_message"), 0))
                         .getfield(nestedb));

      assert.assertequals( nestedbar, message.getrepeatedfield(f("repeated_nested_enum" ),0));
      assert.assertequals(foreignbar, message.getrepeatedfield(f("repeated_foreign_enum"),0));
      assert.assertequals( importbar, message.getrepeatedfield(f("repeated_import_enum" ),0));

      assert.assertequals("224", message.getrepeatedfield(f("repeated_string_piece"), 0));
      assert.assertequals("225", message.getrepeatedfield(f("repeated_cord"), 0));

      assert.assertequals(501  , message.getrepeatedfield(f("repeated_int32"   ), 1));
      assert.assertequals(502l , message.getrepeatedfield(f("repeated_int64"   ), 1));
      assert.assertequals(503  , message.getrepeatedfield(f("repeated_uint32"  ), 1));
      assert.assertequals(504l , message.getrepeatedfield(f("repeated_uint64"  ), 1));
      assert.assertequals(505  , message.getrepeatedfield(f("repeated_sint32"  ), 1));
      assert.assertequals(506l , message.getrepeatedfield(f("repeated_sint64"  ), 1));
      assert.assertequals(507  , message.getrepeatedfield(f("repeated_fixed32" ), 1));
      assert.assertequals(508l , message.getrepeatedfield(f("repeated_fixed64" ), 1));
      assert.assertequals(509  , message.getrepeatedfield(f("repeated_sfixed32"), 1));
      assert.assertequals(510l , message.getrepeatedfield(f("repeated_sfixed64"), 1));
      assert.assertequals(511f , message.getrepeatedfield(f("repeated_float"   ), 1));
      assert.assertequals(512d , message.getrepeatedfield(f("repeated_double"  ), 1));
      assert.assertequals(true , message.getrepeatedfield(f("repeated_bool"    ), 1));
      assert.assertequals("515", message.getrepeatedfield(f("repeated_string"  ), 1));
      assert.assertequals(tobytes("516"), message.getrepeatedfield(f("repeated_bytes"), 1));

      assert.assertequals(517,
        ((message)message.getrepeatedfield(f("repeatedgroup"), 1))
                         .getfield(repeatedgroupa));
      assert.assertequals(518,
        ((message)message.getrepeatedfield(f("repeated_nested_message"), 1))
                         .getfield(nestedb));
      assert.assertequals(519,
        ((message)message.getrepeatedfield(f("repeated_foreign_message"), 1))
                         .getfield(foreignc));
      assert.assertequals(520,
        ((message)message.getrepeatedfield(f("repeated_import_message"), 1))
                         .getfield(importd));
      assert.assertequals(527,
        ((message)message.getrepeatedfield(f("repeated_lazy_message"), 1))
                         .getfield(nestedb));

      assert.assertequals( nestedfoo, message.getrepeatedfield(f("repeated_nested_enum" ),1));
      assert.assertequals(foreignfoo, message.getrepeatedfield(f("repeated_foreign_enum"),1));
      assert.assertequals( importfoo, message.getrepeatedfield(f("repeated_import_enum" ),1));

      assert.assertequals("524", message.getrepeatedfield(f("repeated_string_piece"), 1));
      assert.assertequals("525", message.getrepeatedfield(f("repeated_cord"), 1));
    }

    public void setpackedfieldsviareflection(message.builder message) {
      message.addrepeatedfield(f("packed_int32"   ), 601 );
      message.addrepeatedfield(f("packed_int64"   ), 602l);
      message.addrepeatedfield(f("packed_uint32"  ), 603 );
      message.addrepeatedfield(f("packed_uint64"  ), 604l);
      message.addrepeatedfield(f("packed_sint32"  ), 605 );
      message.addrepeatedfield(f("packed_sint64"  ), 606l);
      message.addrepeatedfield(f("packed_fixed32" ), 607 );
      message.addrepeatedfield(f("packed_fixed64" ), 608l);
      message.addrepeatedfield(f("packed_sfixed32"), 609 );
      message.addrepeatedfield(f("packed_sfixed64"), 610l);
      message.addrepeatedfield(f("packed_float"   ), 611f);
      message.addrepeatedfield(f("packed_double"  ), 612d);
      message.addrepeatedfield(f("packed_bool"    ), true);
      message.addrepeatedfield(f("packed_enum" ),  foreignbar);
      // add a second one of each field.
      message.addrepeatedfield(f("packed_int32"   ), 701 );
      message.addrepeatedfield(f("packed_int64"   ), 702l);
      message.addrepeatedfield(f("packed_uint32"  ), 703 );
      message.addrepeatedfield(f("packed_uint64"  ), 704l);
      message.addrepeatedfield(f("packed_sint32"  ), 705 );
      message.addrepeatedfield(f("packed_sint64"  ), 706l);
      message.addrepeatedfield(f("packed_fixed32" ), 707 );
      message.addrepeatedfield(f("packed_fixed64" ), 708l);
      message.addrepeatedfield(f("packed_sfixed32"), 709 );
      message.addrepeatedfield(f("packed_sfixed64"), 710l);
      message.addrepeatedfield(f("packed_float"   ), 711f);
      message.addrepeatedfield(f("packed_double"  ), 712d);
      message.addrepeatedfield(f("packed_bool"    ), false);
      message.addrepeatedfield(f("packed_enum" ),  foreignbaz);
    }

    public void assertpackedfieldssetviareflection(messageorbuilder message) {
      assert.assertequals(2, message.getrepeatedfieldcount(f("packed_int32"   )));
      assert.assertequals(2, message.getrepeatedfieldcount(f("packed_int64"   )));
      assert.assertequals(2, message.getrepeatedfieldcount(f("packed_uint32"  )));
      assert.assertequals(2, message.getrepeatedfieldcount(f("packed_uint64"  )));
      assert.assertequals(2, message.getrepeatedfieldcount(f("packed_sint32"  )));
      assert.assertequals(2, message.getrepeatedfieldcount(f("packed_sint64"  )));
      assert.assertequals(2, message.getrepeatedfieldcount(f("packed_fixed32" )));
      assert.assertequals(2, message.getrepeatedfieldcount(f("packed_fixed64" )));
      assert.assertequals(2, message.getrepeatedfieldcount(f("packed_sfixed32")));
      assert.assertequals(2, message.getrepeatedfieldcount(f("packed_sfixed64")));
      assert.assertequals(2, message.getrepeatedfieldcount(f("packed_float"   )));
      assert.assertequals(2, message.getrepeatedfieldcount(f("packed_double"  )));
      assert.assertequals(2, message.getrepeatedfieldcount(f("packed_bool"    )));
      assert.assertequals(2, message.getrepeatedfieldcount(f("packed_enum"   )));
      assert.assertequals(601  , message.getrepeatedfield(f("packed_int32"   ), 0));
      assert.assertequals(602l , message.getrepeatedfield(f("packed_int64"   ), 0));
      assert.assertequals(603  , message.getrepeatedfield(f("packed_uint32"  ), 0));
      assert.assertequals(604l , message.getrepeatedfield(f("packed_uint64"  ), 0));
      assert.assertequals(605  , message.getrepeatedfield(f("packed_sint32"  ), 0));
      assert.assertequals(606l , message.getrepeatedfield(f("packed_sint64"  ), 0));
      assert.assertequals(607  , message.getrepeatedfield(f("packed_fixed32" ), 0));
      assert.assertequals(608l , message.getrepeatedfield(f("packed_fixed64" ), 0));
      assert.assertequals(609  , message.getrepeatedfield(f("packed_sfixed32"), 0));
      assert.assertequals(610l , message.getrepeatedfield(f("packed_sfixed64"), 0));
      assert.assertequals(611f , message.getrepeatedfield(f("packed_float"   ), 0));
      assert.assertequals(612d , message.getrepeatedfield(f("packed_double"  ), 0));
      assert.assertequals(true , message.getrepeatedfield(f("packed_bool"    ), 0));
      assert.assertequals(foreignbar, message.getrepeatedfield(f("packed_enum" ),0));
      assert.assertequals(701  , message.getrepeatedfield(f("packed_int32"   ), 1));
      assert.assertequals(702l , message.getrepeatedfield(f("packed_int64"   ), 1));
      assert.assertequals(703  , message.getrepeatedfield(f("packed_uint32"  ), 1));
      assert.assertequals(704l , message.getrepeatedfield(f("packed_uint64"  ), 1));
      assert.assertequals(705  , message.getrepeatedfield(f("packed_sint32"  ), 1));
      assert.assertequals(706l , message.getrepeatedfield(f("packed_sint64"  ), 1));
      assert.assertequals(707  , message.getrepeatedfield(f("packed_fixed32" ), 1));
      assert.assertequals(708l , message.getrepeatedfield(f("packed_fixed64" ), 1));
      assert.assertequals(709  , message.getrepeatedfield(f("packed_sfixed32"), 1));
      assert.assertequals(710l , message.getrepeatedfield(f("packed_sfixed64"), 1));
      assert.assertequals(711f , message.getrepeatedfield(f("packed_float"   ), 1));
      assert.assertequals(712d , message.getrepeatedfield(f("packed_double"  ), 1));
      assert.assertequals(false, message.getrepeatedfield(f("packed_bool"    ), 1));
      assert.assertequals(foreignbaz, message.getrepeatedfield(f("packed_enum" ),1));
    }

    /**
     * verifies that the reflection setters for the given.builder object throw a
     * nullpointerexception if they are passed a null value.  uses assert to throw an
     * appropriate assertion failure, if the condition is not verified.
     */
    public void assertreflectionsettersrejectnull(message.builder builder)
        throws exception {
      try {
        builder.setfield(f("optional_string"), null);
        assert.fail("exception was not thrown");
      } catch (nullpointerexception e) {
        // we expect this exception.
      }
      try {
        builder.setfield(f("optional_bytes"), null);
        assert.fail("exception was not thrown");
      } catch (nullpointerexception e) {
        // we expect this exception.
      }
      try {
        builder.setfield(f("optional_nested_enum"), null);
        assert.fail("exception was not thrown");
      } catch (nullpointerexception e) {
        // we expect this exception.
      }
      try {
        builder.setfield(f("optional_nested_message"),
                         (testalltypes.nestedmessage) null);
        assert.fail("exception was not thrown");
      } catch (nullpointerexception e) {
        // we expect this exception.
      }
      try {
        builder.setfield(f("optional_nested_message"),
                         (testalltypes.nestedmessage.builder) null);
        assert.fail("exception was not thrown");
      } catch (nullpointerexception e) {
        // we expect this exception.
      }

      try {
        builder.addrepeatedfield(f("repeated_string"), null);
        assert.fail("exception was not thrown");
      } catch (nullpointerexception e) {
        // we expect this exception.
      }
      try {
        builder.addrepeatedfield(f("repeated_bytes"), null);
        assert.fail("exception was not thrown");
      } catch (nullpointerexception e) {
        // we expect this exception.
      }
      try {
        builder.addrepeatedfield(f("repeated_nested_enum"), null);
        assert.fail("exception was not thrown");
      } catch (nullpointerexception e) {
        // we expect this exception.
      }
      try {
        builder.addrepeatedfield(f("repeated_nested_message"), null);
        assert.fail("exception was not thrown");
      } catch (nullpointerexception e) {
        // we expect this exception.
      }
    }

    /**
     * verifies that the reflection repeated setters for the given builder object throw a
     * nullpointerexception if they are passed a null value.  uses assert to throw an appropriate
     * assertion failure, if the condition is not verified.
     */
    public void assertreflectionrepeatedsettersrejectnull(message.builder builder)
        throws exception {
      builder.addrepeatedfield(f("repeated_string"), "one");
      try {
        builder.setrepeatedfield(f("repeated_string"), 0, null);
        assert.fail("exception was not thrown");
      } catch (nullpointerexception e) {
        // we expect this exception.
      }

      builder.addrepeatedfield(f("repeated_bytes"), tobytes("one"));
      try {
        builder.setrepeatedfield(f("repeated_bytes"), 0, null);
        assert.fail("exception was not thrown");
      } catch (nullpointerexception e) {
        // we expect this exception.
      }

      builder.addrepeatedfield(f("repeated_nested_enum"), nestedbaz);
      try {
        builder.setrepeatedfield(f("repeated_nested_enum"), 0, null);
        assert.fail("exception was not thrown");
      } catch (nullpointerexception e) {
        // we expect this exception.
      }

      builder.addrepeatedfield(
          f("repeated_nested_message"),
          testalltypes.nestedmessage.newbuilder().setbb(218).build());
      try {
        builder.setrepeatedfield(f("repeated_nested_message"), 0, null);
        assert.fail("exception was not thrown");
      } catch (nullpointerexception e) {
        // we expect this exception.
      }
    }
  }

  /**
   * @param filepath the path relative to
   * {@link #gettestdatadir}.
   */
  public static string readtextfromfile(string filepath) {
    return readbytesfromfile(filepath).tostringutf8();
  }

  private static file gettestdatadir() {
    // search each parent directory looking for "src/google/protobuf".
    file ancestor = new file(".");
    try {
      ancestor = ancestor.getcanonicalfile();
    } catch (ioexception e) {
      throw new runtimeexception(
        "couldn't get canonical name of working directory.", e);
    }
    while (ancestor != null && ancestor.exists()) {
      if (new file(ancestor, "src/google/protobuf").exists()) {
        return new file(ancestor, "src/google/protobuf/testdata");
      }
      ancestor = ancestor.getparentfile();
    }

    throw new runtimeexception(
      "could not find golden files.  this test must be run from within the " +
      "protobuf source package so that it can read test data files from the " +
      "c++ source tree.");
  }

  /**
   * @param filename the path relative to
   * {@link #gettestdatadir}.
   */
  public static bytestring readbytesfromfile(string filename) {
    file fullpath = new file(gettestdatadir(), filename);
    try {
      randomaccessfile file = new randomaccessfile(fullpath, "r");
      byte[] content = new byte[(int) file.length()];
      file.readfully(content);
      return bytestring.copyfrom(content);
    } catch (ioexception e) {
      // throw a runtimeexception here so that we can call this function from
      // static initializers.
      throw new illegalargumentexception(
        "couldn't read file: " + fullpath.getpath(), e);
    }
  }

  /**
   * get the bytes of the "golden message".  this is a serialized testalltypes
   * with all fields set as they would be by
   * {@link #setallfields(testalltypes.builder)}, but it is loaded from a file
   * on disk rather than generated dynamically.  the file is actually generated
   * by c++ code, so testing against it verifies compatibility with c++.
   */
  public static bytestring getgoldenmessage() {
    if (goldenmessage == null) {
      goldenmessage = readbytesfromfile("golden_message");
    }
    return goldenmessage;
  }
  private static bytestring goldenmessage = null;

  /**
   * get the bytes of the "golden packed fields message".  this is a serialized
   * testpackedtypes with all fields set as they would be by
   * {@link #setpackedfields(testpackedtypes.builder)}, but it is loaded from a
   * file on disk rather than generated dynamically.  the file is actually
   * generated by c++ code, so testing against it verifies compatibility with
   * c++.
   */
  public static bytestring getgoldenpackedfieldsmessage() {
    if (goldenpackedfieldsmessage == null) {
      goldenpackedfieldsmessage =
          readbytesfromfile("golden_packed_fields_message");
    }
    return goldenpackedfieldsmessage;
  }
  private static bytestring goldenpackedfieldsmessage = null;

  /**
   * mock implementation of {@link generatedmessage.builderparent} for testing.
   *
   * @author jonp@google.com (jon perlow)
   */
  public static class mockbuilderparent
      implements generatedmessage.builderparent {

    private int invalidations;

    //@override (java 1.6 override semantics, but we must support 1.5)
    public void markdirty() {
      invalidations++;
    }

    public int getinvalidationcount() {
      return invalidations;
    }
  }
}
