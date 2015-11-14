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

import junit.framework.testcase;

import java.io.ioexception;

/**
 * tests to make sure the lazy conversion of utf8-encoded byte arrays to
 * strings works correctly.
 *
 * @author jonp@google.com (jon perlow)
 */
public class lazystringendtoendtest extends testcase {

  private static bytestring test_all_types_serialized_with_illegal_utf8 =
      bytestring.copyfrom(new byte[] {
          114, 4, -1, 0, -1, 0, -30, 2, 4, -1,
          0, -1, 0, -30, 2, 4, -1, 0, -1, 0, });

  private bytestring encodedtestalltypes;

  @override
  protected void setup() throws exception {
    super.setup();
    this.encodedtestalltypes = unittestproto.testalltypes.newbuilder()
        .setoptionalstring("foo")
        .addrepeatedstring("bar")
        .addrepeatedstring("baz")
        .build()
        .tobytestring();
  }

  /**
   * tests that an invalid utf8 string will roundtrip through a parse
   * and serialization.
   */
  public void testparseandserialize() throws invalidprotocolbufferexception {
    unittestproto.testalltypes tv2 = unittestproto.testalltypes.parsefrom(
        test_all_types_serialized_with_illegal_utf8);
    bytestring bytes = tv2.tobytestring();
    assertequals(test_all_types_serialized_with_illegal_utf8, bytes);

    tv2.getoptionalstring();
    bytes = tv2.tobytestring();
    assertequals(test_all_types_serialized_with_illegal_utf8, bytes);
  }

  public void testparseandwrite() throws ioexception {
    unittestproto.testalltypes tv2 = unittestproto.testalltypes.parsefrom(
        test_all_types_serialized_with_illegal_utf8);
    byte[] sink = new byte[test_all_types_serialized_with_illegal_utf8.size()];
    codedoutputstream outputstream = codedoutputstream.newinstance(sink);
    tv2.writeto(outputstream);
    outputstream.flush();
    assertequals(
        test_all_types_serialized_with_illegal_utf8,
        bytestring.copyfrom(sink));
  }

  public void testcaching() {
    string a = "a";
    string b = "b";
    string c = "c";
    unittestproto.testalltypes proto = unittestproto.testalltypes.newbuilder()
        .setoptionalstring(a)
        .addrepeatedstring(b)
        .addrepeatedstring(c)
        .build();

    // string should be the one we passed it.
    assertsame(a, proto.getoptionalstring());
    assertsame(b, proto.getrepeatedstring(0));
    assertsame(c, proto.getrepeatedstring(1));


    // there's no way to directly observe that the bytestring is cached
    // correctly on serialization, but we can observe that it had to recompute
    // the string after serialization.
    proto.tobytestring();
    string aprime = proto.getoptionalstring();
    assertnotsame(a, aprime);
    assertequals(a, aprime);
    string bprime = proto.getrepeatedstring(0);
    assertnotsame(b, bprime);
    assertequals(b, bprime);
    string cprime = proto.getrepeatedstring(1);
    assertnotsame(c, cprime);
    assertequals(c, cprime);

    // and now the string should stay cached.
    assertsame(aprime, proto.getoptionalstring());
    assertsame(bprime, proto.getrepeatedstring(0));
    assertsame(cprime, proto.getrepeatedstring(1));
  }

  public void testnostringcachingifonlybytesaccessed() throws exception {
    unittestproto.testalltypes proto =
        unittestproto.testalltypes.parsefrom(encodedtestalltypes);
    bytestring optional = proto.getoptionalstringbytes();
    assertsame(optional, proto.getoptionalstringbytes());
    assertsame(optional, proto.tobuilder().getoptionalstringbytes());

    bytestring repeated0 = proto.getrepeatedstringbytes(0);
    bytestring repeated1 = proto.getrepeatedstringbytes(1);
    assertsame(repeated0, proto.getrepeatedstringbytes(0));
    assertsame(repeated1, proto.getrepeatedstringbytes(1));
    assertsame(repeated0, proto.tobuilder().getrepeatedstringbytes(0));
    assertsame(repeated1, proto.tobuilder().getrepeatedstringbytes(1));
  }
}
