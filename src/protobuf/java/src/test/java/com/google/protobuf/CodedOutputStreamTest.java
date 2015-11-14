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

import protobuf_unittest.unittestproto.sparseenummessage;
import protobuf_unittest.unittestproto.testalltypes;
import protobuf_unittest.unittestproto.testpackedtypes;
import protobuf_unittest.unittestproto.testsparseenum;

import junit.framework.testcase;

import java.io.bytearrayoutputstream;
import java.util.arraylist;
import java.util.list;

/**
 * unit test for {@link codedoutputstream}.
 *
 * @author kenton@google.com kenton varda
 */
public class codedoutputstreamtest extends testcase {
  /**
   * helper to construct a byte array from a bunch of bytes.  the inputs are
   * actually ints so that i can use hex notation and not get stupid errors
   * about precision.
   */
  private byte[] bytes(int... bytesasints) {
    byte[] bytes = new byte[bytesasints.length];
    for (int i = 0; i < bytesasints.length; i++) {
      bytes[i] = (byte) bytesasints[i];
    }
    return bytes;
  }

  /** arrays.aslist() does not work with arrays of primitives.  :( */
  private list<byte> tolist(byte[] bytes) {
    list<byte> result = new arraylist<byte>();
    for (byte b : bytes) {
      result.add(b);
    }
    return result;
  }

  private void assertequalbytes(byte[] a, byte[] b) {
    assertequals(tolist(a), tolist(b));
  }

  /**
   * writes the given value using writerawvarint32() and writerawvarint64() and
   * checks that the result matches the given bytes.
   */
  private void assertwritevarint(byte[] data, long value) throws exception {
    // only do 32-bit write if the value fits in 32 bits.
    if ((value >>> 32) == 0) {
      bytearrayoutputstream rawoutput = new bytearrayoutputstream();
      codedoutputstream output = codedoutputstream.newinstance(rawoutput);
      output.writerawvarint32((int) value);
      output.flush();
      assertequalbytes(data, rawoutput.tobytearray());

      // also try computing size.
      assertequals(data.length,
                   codedoutputstream.computerawvarint32size((int) value));
    }

    {
      bytearrayoutputstream rawoutput = new bytearrayoutputstream();
      codedoutputstream output = codedoutputstream.newinstance(rawoutput);
      output.writerawvarint64(value);
      output.flush();
      assertequalbytes(data, rawoutput.tobytearray());

      // also try computing size.
      assertequals(data.length,
                   codedoutputstream.computerawvarint64size(value));
    }

    // try different block sizes.
    for (int blocksize = 1; blocksize <= 16; blocksize *= 2) {
      // only do 32-bit write if the value fits in 32 bits.
      if ((value >>> 32) == 0) {
        bytearrayoutputstream rawoutput = new bytearrayoutputstream();
        codedoutputstream output =
          codedoutputstream.newinstance(rawoutput, blocksize);
        output.writerawvarint32((int) value);
        output.flush();
        assertequalbytes(data, rawoutput.tobytearray());
      }

      {
        bytearrayoutputstream rawoutput = new bytearrayoutputstream();
        codedoutputstream output =
          codedoutputstream.newinstance(rawoutput, blocksize);
        output.writerawvarint64(value);
        output.flush();
        assertequalbytes(data, rawoutput.tobytearray());
      }
    }
  }

  /** tests writerawvarint32() and writerawvarint64(). */
  public void testwritevarint() throws exception {
    assertwritevarint(bytes(0x00), 0);
    assertwritevarint(bytes(0x01), 1);
    assertwritevarint(bytes(0x7f), 127);
    // 14882
    assertwritevarint(bytes(0xa2, 0x74), (0x22 << 0) | (0x74 << 7));
    // 2961488830
    assertwritevarint(bytes(0xbe, 0xf7, 0x92, 0x84, 0x0b),
      (0x3e << 0) | (0x77 << 7) | (0x12 << 14) | (0x04 << 21) |
      (0x0bl << 28));

    // 64-bit
    // 7256456126
    assertwritevarint(bytes(0xbe, 0xf7, 0x92, 0x84, 0x1b),
      (0x3e << 0) | (0x77 << 7) | (0x12 << 14) | (0x04 << 21) |
      (0x1bl << 28));
    // 41256202580718336
    assertwritevarint(
      bytes(0x80, 0xe6, 0xeb, 0x9c, 0xc3, 0xc9, 0xa4, 0x49),
      (0x00 << 0) | (0x66 << 7) | (0x6b << 14) | (0x1c << 21) |
      (0x43l << 28) | (0x49l << 35) | (0x24l << 42) | (0x49l << 49));
    // 11964378330978735131
    assertwritevarint(
      bytes(0x9b, 0xa8, 0xf9, 0xc2, 0xbb, 0xd6, 0x80, 0x85, 0xa6, 0x01),
      (0x1b << 0) | (0x28 << 7) | (0x79 << 14) | (0x42 << 21) |
      (0x3bl << 28) | (0x56l << 35) | (0x00l << 42) |
      (0x05l << 49) | (0x26l << 56) | (0x01l << 63));
  }

  /**
   * parses the given bytes using writerawlittleendian32() and checks
   * that the result matches the given value.
   */
  private void assertwritelittleendian32(byte[] data, int value)
                                         throws exception {
    bytearrayoutputstream rawoutput = new bytearrayoutputstream();
    codedoutputstream output = codedoutputstream.newinstance(rawoutput);
    output.writerawlittleendian32(value);
    output.flush();
    assertequalbytes(data, rawoutput.tobytearray());

    // try different block sizes.
    for (int blocksize = 1; blocksize <= 16; blocksize *= 2) {
      rawoutput = new bytearrayoutputstream();
      output = codedoutputstream.newinstance(rawoutput, blocksize);
      output.writerawlittleendian32(value);
      output.flush();
      assertequalbytes(data, rawoutput.tobytearray());
    }
  }

  /**
   * parses the given bytes using writerawlittleendian64() and checks
   * that the result matches the given value.
   */
  private void assertwritelittleendian64(byte[] data, long value)
                                         throws exception {
    bytearrayoutputstream rawoutput = new bytearrayoutputstream();
    codedoutputstream output = codedoutputstream.newinstance(rawoutput);
    output.writerawlittleendian64(value);
    output.flush();
    assertequalbytes(data, rawoutput.tobytearray());

    // try different block sizes.
    for (int blocksize = 1; blocksize <= 16; blocksize *= 2) {
      rawoutput = new bytearrayoutputstream();
      output = codedoutputstream.newinstance(rawoutput, blocksize);
      output.writerawlittleendian64(value);
      output.flush();
      assertequalbytes(data, rawoutput.tobytearray());
    }
  }

  /** tests writerawlittleendian32() and writerawlittleendian64(). */
  public void testwritelittleendian() throws exception {
    assertwritelittleendian32(bytes(0x78, 0x56, 0x34, 0x12), 0x12345678);
    assertwritelittleendian32(bytes(0xf0, 0xde, 0xbc, 0x9a), 0x9abcdef0);

    assertwritelittleendian64(
      bytes(0xf0, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12),
      0x123456789abcdef0l);
    assertwritelittleendian64(
      bytes(0x78, 0x56, 0x34, 0x12, 0xf0, 0xde, 0xbc, 0x9a),
      0x9abcdef012345678l);
  }

  /** test encodezigzag32() and encodezigzag64(). */
  public void testencodezigzag() throws exception {
    assertequals(0, codedoutputstream.encodezigzag32( 0));
    assertequals(1, codedoutputstream.encodezigzag32(-1));
    assertequals(2, codedoutputstream.encodezigzag32( 1));
    assertequals(3, codedoutputstream.encodezigzag32(-2));
    assertequals(0x7ffffffe, codedoutputstream.encodezigzag32(0x3fffffff));
    assertequals(0x7fffffff, codedoutputstream.encodezigzag32(0xc0000000));
    assertequals(0xfffffffe, codedoutputstream.encodezigzag32(0x7fffffff));
    assertequals(0xffffffff, codedoutputstream.encodezigzag32(0x80000000));

    assertequals(0, codedoutputstream.encodezigzag64( 0));
    assertequals(1, codedoutputstream.encodezigzag64(-1));
    assertequals(2, codedoutputstream.encodezigzag64( 1));
    assertequals(3, codedoutputstream.encodezigzag64(-2));
    assertequals(0x000000007ffffffel,
                 codedoutputstream.encodezigzag64(0x000000003fffffffl));
    assertequals(0x000000007fffffffl,
                 codedoutputstream.encodezigzag64(0xffffffffc0000000l));
    assertequals(0x00000000fffffffel,
                 codedoutputstream.encodezigzag64(0x000000007fffffffl));
    assertequals(0x00000000ffffffffl,
                 codedoutputstream.encodezigzag64(0xffffffff80000000l));
    assertequals(0xfffffffffffffffel,
                 codedoutputstream.encodezigzag64(0x7fffffffffffffffl));
    assertequals(0xffffffffffffffffl,
                 codedoutputstream.encodezigzag64(0x8000000000000000l));

    // some easier-to-verify round-trip tests.  the inputs (other than 0, 1, -1)
    // were chosen semi-randomly via keyboard bashing.
    assertequals(0,
      codedoutputstream.encodezigzag32(codedinputstream.decodezigzag32(0)));
    assertequals(1,
      codedoutputstream.encodezigzag32(codedinputstream.decodezigzag32(1)));
    assertequals(-1,
      codedoutputstream.encodezigzag32(codedinputstream.decodezigzag32(-1)));
    assertequals(14927,
      codedoutputstream.encodezigzag32(codedinputstream.decodezigzag32(14927)));
    assertequals(-3612,
      codedoutputstream.encodezigzag32(codedinputstream.decodezigzag32(-3612)));

    assertequals(0,
      codedoutputstream.encodezigzag64(codedinputstream.decodezigzag64(0)));
    assertequals(1,
      codedoutputstream.encodezigzag64(codedinputstream.decodezigzag64(1)));
    assertequals(-1,
      codedoutputstream.encodezigzag64(codedinputstream.decodezigzag64(-1)));
    assertequals(14927,
      codedoutputstream.encodezigzag64(codedinputstream.decodezigzag64(14927)));
    assertequals(-3612,
      codedoutputstream.encodezigzag64(codedinputstream.decodezigzag64(-3612)));

    assertequals(856912304801416l,
      codedoutputstream.encodezigzag64(
        codedinputstream.decodezigzag64(
          856912304801416l)));
    assertequals(-75123905439571256l,
      codedoutputstream.encodezigzag64(
        codedinputstream.decodezigzag64(
          -75123905439571256l)));
  }

  /** tests writing a whole message with every field type. */
  public void testwritewholemessage() throws exception {
    testalltypes message = testutil.getallset();

    byte[] rawbytes = message.tobytearray();
    assertequalbytes(testutil.getgoldenmessage().tobytearray(), rawbytes);

    // try different block sizes.
    for (int blocksize = 1; blocksize < 256; blocksize *= 2) {
      bytearrayoutputstream rawoutput = new bytearrayoutputstream();
      codedoutputstream output =
        codedoutputstream.newinstance(rawoutput, blocksize);
      message.writeto(output);
      output.flush();
      assertequalbytes(rawbytes, rawoutput.tobytearray());
    }
  }

  /** tests writing a whole message with every packed field type. ensures the
   * wire format of packed fields is compatible with c++. */
  public void testwritewholepackedfieldsmessage() throws exception {
    testpackedtypes message = testutil.getpackedset();

    byte[] rawbytes = message.tobytearray();
    assertequalbytes(testutil.getgoldenpackedfieldsmessage().tobytearray(),
                     rawbytes);
  }

  /** test writing a message containing a negative enum value. this used to
   * fail because the size was not properly computed as a sign-extended varint.
   */
  public void testwritemessagewithnegativeenumvalue() throws exception {
    sparseenummessage message = sparseenummessage.newbuilder()
        .setsparseenum(testsparseenum.sparse_e) .build();
    asserttrue(message.getsparseenum().getnumber() < 0);
    byte[] rawbytes = message.tobytearray();
    sparseenummessage message2 = sparseenummessage.parsefrom(rawbytes);
    assertequals(testsparseenum.sparse_e, message2.getsparseenum());
  }
}
