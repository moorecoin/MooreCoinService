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

import com.google.protobuf.unittestlite;
import com.google.protobuf.unittestlite.testalltypeslite;
import com.google.protobuf.unittestlite.testallextensionslite;
import com.google.protobuf.unittestlite.testnestedextensionlite;

import junit.framework.testcase;

import java.io.bytearrayinputstream;
import java.io.bytearrayoutputstream;
import java.io.objectinputstream;
import java.io.objectoutputstream;

/**
 * test lite runtime.
 *
 * @author kenton@google.com kenton varda
 */
public class litetest extends testcase {
  public void setup() throws exception {
    // test that nested extensions are initialized correctly even if the outer
    // class has not been accessed directly.  this was once a bug with lite
    // messages.
    //
    // we put this in setup() rather than in its own test method because we
    // need to make sure it runs before any actual tests.
    asserttrue(testnestedextensionlite.nestedextension != null);
  }

  public void testlite() throws exception {
    // since lite messages are a subset of regular messages, we can mostly
    // assume that the functionality of lite messages is already thoroughly
    // tested by the regular tests.  all this test really verifies is that
    // a proto with optimize_for = lite_runtime compiles correctly when
    // linked only against the lite library.  that is all tested at compile
    // time, leaving not much to do in this method.  let's just do some random
    // stuff to make sure the lite message is actually here and usable.

    testalltypeslite message =
      testalltypeslite.newbuilder()
                      .setoptionalint32(123)
                      .addrepeatedstring("hello")
                      .setoptionalnestedmessage(
                          testalltypeslite.nestedmessage.newbuilder().setbb(7))
                      .build();

    bytestring data = message.tobytestring();

    testalltypeslite message2 = testalltypeslite.parsefrom(data);

    assertequals(123, message2.getoptionalint32());
    assertequals(1, message2.getrepeatedstringcount());
    assertequals("hello", message2.getrepeatedstring(0));
    assertequals(7, message2.getoptionalnestedmessage().getbb());
  }

  public void testliteextensions() throws exception {
    // todo(kenton):  unlike other features of the lite library, extensions are
    //   implemented completely differently from the regular library.  we
    //   should probably test them more thoroughly.

    testallextensionslite message =
      testallextensionslite.newbuilder()
        .setextension(unittestlite.optionalint32extensionlite, 123)
        .addextension(unittestlite.repeatedstringextensionlite, "hello")
        .setextension(unittestlite.optionalnestedenumextensionlite,
            testalltypeslite.nestedenum.baz)
        .setextension(unittestlite.optionalnestedmessageextensionlite,
            testalltypeslite.nestedmessage.newbuilder().setbb(7).build())
        .build();

    // test copying a message, since coping extensions actually does use a
    // different code path between lite and regular libraries, and as of this
    // writing, parsing hasn't been implemented yet.
    testallextensionslite message2 = message.tobuilder().build();

    assertequals(123, (int) message2.getextension(
        unittestlite.optionalint32extensionlite));
    assertequals(1, message2.getextensioncount(
        unittestlite.repeatedstringextensionlite));
    assertequals(1, message2.getextension(
        unittestlite.repeatedstringextensionlite).size());
    assertequals("hello", message2.getextension(
        unittestlite.repeatedstringextensionlite, 0));
    assertequals(testalltypeslite.nestedenum.baz, message2.getextension(
        unittestlite.optionalnestedenumextensionlite));
    assertequals(7, message2.getextension(
        unittestlite.optionalnestedmessageextensionlite).getbb());
  }

  public void testserialize() throws exception {
    bytearrayoutputstream baos = new bytearrayoutputstream();
    testalltypeslite expected =
      testalltypeslite.newbuilder()
                      .setoptionalint32(123)
                      .addrepeatedstring("hello")
                      .setoptionalnestedmessage(
                          testalltypeslite.nestedmessage.newbuilder().setbb(7))
                      .build();
    objectoutputstream out = new objectoutputstream(baos);
    try {
      out.writeobject(expected);
    } finally {
      out.close();
    }
    bytearrayinputstream bais = new bytearrayinputstream(baos.tobytearray());
    objectinputstream in = new objectinputstream(bais);
    testalltypeslite actual = (testalltypeslite) in.readobject();
    assertequals(expected.getoptionalint32(), actual.getoptionalint32());
    assertequals(expected.getrepeatedstringcount(),
        actual.getrepeatedstringcount());
    assertequals(expected.getrepeatedstring(0),
        actual.getrepeatedstring(0));
    assertequals(expected.getoptionalnestedmessage().getbb(),
        actual.getoptionalnestedmessage().getbb());
  }
}
