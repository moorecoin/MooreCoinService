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

import junit.framework.testcase;

import java.io.bytearrayinputstream;
import java.io.bytearrayoutputstream;
import java.util.list;

import protobuf_unittest.unittestproto;
import protobuf_unittest.unittestproto.testallextensions;
import protobuf_unittest.unittestproto.testalltypes;
import protobuf_unittest.unittestproto.testfieldorderings;
import protobuf_unittest.unittestproto.testpackedextensions;
import protobuf_unittest.unittestproto.testpackedtypes;
import protobuf_unittest.unittestmset.testmessageset;
import protobuf_unittest.unittestmset.rawmessageset;
import protobuf_unittest.unittestmset.testmessagesetextension1;
import protobuf_unittest.unittestmset.testmessagesetextension2;
import com.google.protobuf.unittestlite.testallextensionslite;
import com.google.protobuf.unittestlite.testpackedextensionslite;

/**
 * tests related to parsing and serialization.
 *
 * @author kenton@google.com (kenton varda)
 */
public class wireformattest extends testcase {
  public void testserialization() throws exception {
    testalltypes message = testutil.getallset();

    bytestring rawbytes = message.tobytestring();
    assertequals(rawbytes.size(), message.getserializedsize());

    testalltypes message2 = testalltypes.parsefrom(rawbytes);

    testutil.assertallfieldsset(message2);
  }

  public void testserializationpacked() throws exception {
    testpackedtypes message = testutil.getpackedset();

    bytestring rawbytes = message.tobytestring();
    assertequals(rawbytes.size(), message.getserializedsize());

    testpackedtypes message2 = testpackedtypes.parsefrom(rawbytes);

    testutil.assertpackedfieldsset(message2);
  }

  public void testserializeextensions() throws exception {
    // testalltypes and testallextensions should have compatible wire formats,
    // so if we serialize a testallextensions then parse it as testalltypes
    // it should work.

    testallextensions message = testutil.getallextensionsset();
    bytestring rawbytes = message.tobytestring();
    assertequals(rawbytes.size(), message.getserializedsize());

    testalltypes message2 = testalltypes.parsefrom(rawbytes);

    testutil.assertallfieldsset(message2);
  }

  public void testserializepackedextensions() throws exception {
    // testpackedtypes and testpackedextensions should have compatible wire
    // formats; check that they serialize to the same string.
    testpackedextensions message = testutil.getpackedextensionsset();
    bytestring rawbytes = message.tobytestring();

    testpackedtypes message2 = testutil.getpackedset();
    bytestring rawbytes2 = message2.tobytestring();

    assertequals(rawbytes, rawbytes2);
  }

  public void testserializationpackedwithoutgetserializedsize()
      throws exception {
    // write directly to an outputstream, without invoking getserializedsize()
    // this used to be a bug where the size of a packed field was incorrect,
    // since getserializedsize() was never invoked.
    testpackedtypes message = testutil.getpackedset();

    // directly construct a codedoutputstream around the actual outputstream,
    // in case writeto(outputstream output) invokes getserializedsize();
    bytearrayoutputstream outputstream = new bytearrayoutputstream();
    codedoutputstream codedoutput = codedoutputstream.newinstance(outputstream);

    message.writeto(codedoutput);

    codedoutput.flush();

    testpackedtypes message2 = testpackedtypes.parsefrom(
        outputstream.tobytearray());

    testutil.assertpackedfieldsset(message2);
  }

  public void testserializeextensionslite() throws exception {
    // testalltypes and testallextensions should have compatible wire formats,
    // so if we serialize a testallextensions then parse it as testalltypes
    // it should work.

    testallextensionslite message = testutil.getallliteextensionsset();
    bytestring rawbytes = message.tobytestring();
    assertequals(rawbytes.size(), message.getserializedsize());

    testalltypes message2 = testalltypes.parsefrom(rawbytes);

    testutil.assertallfieldsset(message2);
  }

  public void testserializepackedextensionslite() throws exception {
    // testpackedtypes and testpackedextensions should have compatible wire
    // formats; check that they serialize to the same string.
    testpackedextensionslite message = testutil.getlitepackedextensionsset();
    bytestring rawbytes = message.tobytestring();

    testpackedtypes message2 = testutil.getpackedset();
    bytestring rawbytes2 = message2.tobytestring();

    assertequals(rawbytes, rawbytes2);
  }

  public void testparseextensions() throws exception {
    // testalltypes and testallextensions should have compatible wire formats,
    // so if we serialize a testalltypes then parse it as testallextensions
    // it should work.

    testalltypes message = testutil.getallset();
    bytestring rawbytes = message.tobytestring();

    extensionregistry registry = testutil.getextensionregistry();

    testallextensions message2 =
      testallextensions.parsefrom(rawbytes, registry);

    testutil.assertallextensionsset(message2);
  }

  public void testparsepackedextensions() throws exception {
    // ensure that packed extensions can be properly parsed.
    testpackedextensions message = testutil.getpackedextensionsset();
    bytestring rawbytes = message.tobytestring();

    extensionregistry registry = testutil.getextensionregistry();

    testpackedextensions message2 =
        testpackedextensions.parsefrom(rawbytes, registry);

    testutil.assertpackedextensionsset(message2);
  }

  public void testparseextensionslite() throws exception {
    // testalltypes and testallextensions should have compatible wire formats,
    // so if we serialize a testalltypes then parse it as testallextensions
    // it should work.

    testalltypes message = testutil.getallset();
    bytestring rawbytes = message.tobytestring();

    extensionregistrylite registry_lite = testutil.getextensionregistrylite();

    testallextensionslite message2 =
      testallextensionslite.parsefrom(rawbytes, registry_lite);

    testutil.assertallextensionsset(message2);

    // try again using a full extension registry.
    extensionregistry registry = testutil.getextensionregistry();

    testallextensionslite message3 =
      testallextensionslite.parsefrom(rawbytes, registry);

    testutil.assertallextensionsset(message3);
  }

  public void testparsepackedextensionslite() throws exception {
    // ensure that packed extensions can be properly parsed.
    testpackedextensionslite message = testutil.getlitepackedextensionsset();
    bytestring rawbytes = message.tobytestring();

    extensionregistrylite registry = testutil.getextensionregistrylite();

    testpackedextensionslite message2 =
        testpackedextensionslite.parsefrom(rawbytes, registry);

    testutil.assertpackedextensionsset(message2);
  }

  public void testextensionsserializedsize() throws exception {
    assertequals(testutil.getallset().getserializedsize(),
                 testutil.getallextensionsset().getserializedsize());
  }

  public void testserializedelimited() throws exception {
    bytearrayoutputstream output = new bytearrayoutputstream();
    testutil.getallset().writedelimitedto(output);
    output.write(12);
    testutil.getpackedset().writedelimitedto(output);
    output.write(34);

    bytearrayinputstream input = new bytearrayinputstream(output.tobytearray());

    testutil.assertallfieldsset(testalltypes.parsedelimitedfrom(input));
    assertequals(12, input.read());
    testutil.assertpackedfieldsset(testpackedtypes.parsedelimitedfrom(input));
    assertequals(34, input.read());
    assertequals(-1, input.read());

    // we're at eof, so parsing again should return null.
    asserttrue(testalltypes.parsedelimitedfrom(input) == null);
  }

  private void assertfieldsinorder(bytestring data) throws exception {
    codedinputstream input = data.newcodedinput();
    int previoustag = 0;

    while (true) {
      int tag = input.readtag();
      if (tag == 0) {
        break;
      }

      asserttrue(tag > previoustag);
      previoustag = tag;
      input.skipfield(tag);
    }
  }

  public void testinterleavedfieldsandextensions() throws exception {
    // tests that fields are written in order even when extension ranges
    // are interleaved with field numbers.
    bytestring data =
      testfieldorderings.newbuilder()
        .setmyint(1)
        .setmystring("foo")
        .setmyfloat(1.0f)
        .setextension(unittestproto.myextensionint, 23)
        .setextension(unittestproto.myextensionstring, "bar")
        .build().tobytestring();
    assertfieldsinorder(data);

    descriptors.descriptor descriptor = testfieldorderings.getdescriptor();
    bytestring dynamic_data =
      dynamicmessage.newbuilder(testfieldorderings.getdescriptor())
        .setfield(descriptor.findfieldbyname("my_int"), 1l)
        .setfield(descriptor.findfieldbyname("my_string"), "foo")
        .setfield(descriptor.findfieldbyname("my_float"), 1.0f)
        .setfield(unittestproto.myextensionint.getdescriptor(), 23)
        .setfield(unittestproto.myextensionstring.getdescriptor(), "bar")
        .build().tobytestring();
    assertfieldsinorder(dynamic_data);
  }

  private extensionregistry gettestfieldorderingsregistry() {
    extensionregistry result = extensionregistry.newinstance();
    result.add(unittestproto.myextensionint);
    result.add(unittestproto.myextensionstring);
    return result;
  }

  public void testparsemultipleextensionranges() throws exception {
    // make sure we can parse a message that contains multiple extensions
    // ranges.
    testfieldorderings source =
      testfieldorderings.newbuilder()
        .setmyint(1)
        .setmystring("foo")
        .setmyfloat(1.0f)
        .setextension(unittestproto.myextensionint, 23)
        .setextension(unittestproto.myextensionstring, "bar")
        .build();
    testfieldorderings dest =
      testfieldorderings.parsefrom(source.tobytestring(),
                                   gettestfieldorderingsregistry());
    assertequals(source, dest);
  }

  public void testparsemultipleextensionrangesdynamic() throws exception {
    // same as above except with dynamicmessage.
    descriptors.descriptor descriptor = testfieldorderings.getdescriptor();
    dynamicmessage source =
      dynamicmessage.newbuilder(testfieldorderings.getdescriptor())
        .setfield(descriptor.findfieldbyname("my_int"), 1l)
        .setfield(descriptor.findfieldbyname("my_string"), "foo")
        .setfield(descriptor.findfieldbyname("my_float"), 1.0f)
        .setfield(unittestproto.myextensionint.getdescriptor(), 23)
        .setfield(unittestproto.myextensionstring.getdescriptor(), "bar")
        .build();
    dynamicmessage dest =
      dynamicmessage.parsefrom(descriptor, source.tobytestring(),
                               gettestfieldorderingsregistry());
    assertequals(source, dest);
  }

  private static final int unknown_type_id = 1550055;
  private static final int type_id_1 =
    testmessagesetextension1.getdescriptor().getextensions().get(0).getnumber();
  private static final int type_id_2 =
    testmessagesetextension2.getdescriptor().getextensions().get(0).getnumber();

  public void testserializemessageseteagerly() throws exception {
    testserializemessagesetwithflag(true);
  }

  public void testserializemessagesetnoteagerly() throws exception {
    testserializemessagesetwithflag(false);
  }

  private void testserializemessagesetwithflag(boolean eagerparsing)
      throws exception {
    extensionregistrylite.seteagerlyparsemessagesets(eagerparsing);
    // set up a testmessageset with two known messages and an unknown one.
    testmessageset messageset =
      testmessageset.newbuilder()
        .setextension(
          testmessagesetextension1.messagesetextension,
          testmessagesetextension1.newbuilder().seti(123).build())
        .setextension(
          testmessagesetextension2.messagesetextension,
          testmessagesetextension2.newbuilder().setstr("foo").build())
        .setunknownfields(
          unknownfieldset.newbuilder()
            .addfield(unknown_type_id,
              unknownfieldset.field.newbuilder()
                .addlengthdelimited(bytestring.copyfromutf8("bar"))
                .build())
            .build())
        .build();

    bytestring data = messageset.tobytestring();

    // parse back using rawmessageset and check the contents.
    rawmessageset raw = rawmessageset.parsefrom(data);

    asserttrue(raw.getunknownfields().asmap().isempty());

    assertequals(3, raw.getitemcount());
    assertequals(type_id_1, raw.getitem(0).gettypeid());
    assertequals(type_id_2, raw.getitem(1).gettypeid());
    assertequals(unknown_type_id, raw.getitem(2).gettypeid());

    testmessagesetextension1 message1 =
      testmessagesetextension1.parsefrom(
        raw.getitem(0).getmessage().tobytearray());
    assertequals(123, message1.geti());

    testmessagesetextension2 message2 =
      testmessagesetextension2.parsefrom(
        raw.getitem(1).getmessage().tobytearray());
    assertequals("foo", message2.getstr());

    assertequals("bar", raw.getitem(2).getmessage().tostringutf8());
  }

  public void testparsemessageseteagerly() throws exception {
    testparsemessagesetwithflag(true);
  }

  public void testparsemessagesetnoteagerly()throws exception {
    testparsemessagesetwithflag(false);
  }

  private void testparsemessagesetwithflag(boolean eagerparsing)
      throws exception {
    extensionregistrylite.seteagerlyparsemessagesets(eagerparsing);
    extensionregistry extensionregistry = extensionregistry.newinstance();
    extensionregistry.add(testmessagesetextension1.messagesetextension);
    extensionregistry.add(testmessagesetextension2.messagesetextension);

    // set up a rawmessageset with two known messages and an unknown one.
    rawmessageset raw =
      rawmessageset.newbuilder()
        .additem(
          rawmessageset.item.newbuilder()
            .settypeid(type_id_1)
            .setmessage(
              testmessagesetextension1.newbuilder()
                .seti(123)
                .build().tobytestring())
            .build())
        .additem(
          rawmessageset.item.newbuilder()
            .settypeid(type_id_2)
            .setmessage(
              testmessagesetextension2.newbuilder()
                .setstr("foo")
                .build().tobytestring())
            .build())
        .additem(
          rawmessageset.item.newbuilder()
            .settypeid(unknown_type_id)
            .setmessage(bytestring.copyfromutf8("bar"))
            .build())
        .build();

    bytestring data = raw.tobytestring();

    // parse as a testmessageset and check the contents.
    testmessageset messageset =
      testmessageset.parsefrom(data, extensionregistry);

    assertequals(123, messageset.getextension(
      testmessagesetextension1.messagesetextension).geti());
    assertequals("foo", messageset.getextension(
      testmessagesetextension2.messagesetextension).getstr());

    // check for unknown field with type length_delimited,
    //   number unknown_type_id, and contents "bar".
    unknownfieldset unknownfields = messageset.getunknownfields();
    assertequals(1, unknownfields.asmap().size());
    asserttrue(unknownfields.hasfield(unknown_type_id));

    unknownfieldset.field field = unknownfields.getfield(unknown_type_id);
    assertequals(1, field.getlengthdelimitedlist().size());
    assertequals("bar", field.getlengthdelimitedlist().get(0).tostringutf8());
  }

  public void testparsemessagesetextensioneagerly() throws exception {
    testparsemessagesetextensionwithflag(true);
  }

  public void testparsemessagesetextensionnoteagerly() throws exception {
    testparsemessagesetextensionwithflag(false);
  }

  private void testparsemessagesetextensionwithflag(boolean eagerparsing)
      throws exception {
    extensionregistrylite.seteagerlyparsemessagesets(eagerparsing);
    extensionregistry extensionregistry = extensionregistry.newinstance();
    extensionregistry.add(testmessagesetextension1.messagesetextension);

    // set up a rawmessageset with a known messages.
    int type_id_1 =
        testmessagesetextension1
            .getdescriptor().getextensions().get(0).getnumber();
    rawmessageset raw =
      rawmessageset.newbuilder()
        .additem(
          rawmessageset.item.newbuilder()
            .settypeid(type_id_1)
            .setmessage(
              testmessagesetextension1.newbuilder()
                .seti(123)
                .build().tobytestring())
            .build())
        .build();

    bytestring data = raw.tobytestring();

    // parse as a testmessageset and check the contents.
    testmessageset messageset =
        testmessageset.parsefrom(data, extensionregistry);
    assertequals(123, messageset.getextension(
        testmessagesetextension1.messagesetextension).geti());
  }

  public void testmergelazymessagesetextensioneagerly() throws exception {
    testmergelazymessagesetextensionwithflag(true);
  }

  public void testmergelazymessagesetextensionnoteagerly() throws exception {
    testmergelazymessagesetextensionwithflag(false);
  }

  private void testmergelazymessagesetextensionwithflag(boolean eagerparsing)
      throws exception {
    extensionregistrylite.seteagerlyparsemessagesets(eagerparsing);
    extensionregistry extensionregistry = extensionregistry.newinstance();
    extensionregistry.add(testmessagesetextension1.messagesetextension);

    // set up a rawmessageset with a known messages.
    int type_id_1 =
        testmessagesetextension1
            .getdescriptor().getextensions().get(0).getnumber();
    rawmessageset raw =
      rawmessageset.newbuilder()
        .additem(
          rawmessageset.item.newbuilder()
            .settypeid(type_id_1)
            .setmessage(
              testmessagesetextension1.newbuilder()
                .seti(123)
                .build().tobytestring())
            .build())
        .build();

    bytestring data = raw.tobytestring();

    // parse as a testmessageset and store value into lazy field
    testmessageset messageset =
        testmessageset.parsefrom(data, extensionregistry);
    // merge lazy field check the contents.
    messageset =
        messageset.tobuilder().mergefrom(data, extensionregistry).build();
    assertequals(123, messageset.getextension(
        testmessagesetextension1.messagesetextension).geti());
  }

  public void testmergemessagesetextensioneagerly() throws exception {
    testmergemessagesetextensionwithflag(true);
  }

  public void testmergemessagesetextensionnoteagerly() throws exception {
    testmergemessagesetextensionwithflag(false);
  }

  private void testmergemessagesetextensionwithflag(boolean eagerparsing)
      throws exception {
    extensionregistrylite.seteagerlyparsemessagesets(eagerparsing);
    extensionregistry extensionregistry = extensionregistry.newinstance();
    extensionregistry.add(testmessagesetextension1.messagesetextension);

    // set up a rawmessageset with a known messages.
    int type_id_1 =
        testmessagesetextension1
            .getdescriptor().getextensions().get(0).getnumber();
    rawmessageset raw =
      rawmessageset.newbuilder()
        .additem(
          rawmessageset.item.newbuilder()
            .settypeid(type_id_1)
            .setmessage(
              testmessagesetextension1.newbuilder()
                .seti(123)
                .build().tobytestring())
            .build())
        .build();

    // serialize rawmessageset unnormally (message value before type id)
    bytestring.codedbuilder out = bytestring.newcodedbuilder(
        raw.getserializedsize());
    codedoutputstream output = out.getcodedoutput();
    list<rawmessageset.item> items = raw.getitemlist();
    for (int i = 0; i < items.size(); i++) {
      rawmessageset.item item = items.get(i);
      output.writetag(1, wireformat.wiretype_start_group);
      output.writebytes(3, item.getmessage());
      output.writeint32(2, item.gettypeid());
      output.writetag(1, wireformat.wiretype_end_group);
    }
    bytestring data = out.build();

    // merge bytes into testmessageset and check the contents.
    testmessageset messageset =
        testmessageset.newbuilder().mergefrom(data, extensionregistry).build();
    assertequals(123, messageset.getextension(
        testmessagesetextension1.messagesetextension).geti());
  }
}
