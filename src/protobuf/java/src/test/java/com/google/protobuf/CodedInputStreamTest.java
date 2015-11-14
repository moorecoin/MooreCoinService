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
import protobuf_unittest.unittestproto.testrecursivemessage;

import junit.framework.testcase;

import java.io.bytearrayinputstream;
import java.io.filterinputstream;
import java.io.inputstream;
import java.io.ioexception;

/**
 * unit test for {@link codedinputstream}.
 *
 * @author kenton@google.com kenton varda
 */
public class codedinputstreamtest extends testcase {
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

  /**
   * an inputstream which limits the number of bytes it reads at a time.
   * we use this to make sure that codedinputstream doesn't screw up when
   * reading in small blocks.
   */
  private static final class smallblockinputstream extends filterinputstream {
    private final int blocksize;

    public smallblockinputstream(byte[] data, int blocksize) {
      this(new bytearrayinputstream(data), blocksize);
    }

    public smallblockinputstream(inputstream in, int blocksize) {
      super(in);
      this.blocksize = blocksize;
    }

    public int read(byte[] b) throws ioexception {
      return super.read(b, 0, math.min(b.length, blocksize));
    }

    public int read(byte[] b, int off, int len) throws ioexception {
      return super.read(b, off, math.min(len, blocksize));
    }
  }

  /**
   * parses the given bytes using readrawvarint32() and readrawvarint64() and
   * checks that the result matches the given value.
   */
  private void assertreadvarint(byte[] data, long value) throws exception {
    codedinputstream input = codedinputstream.newinstance(data);
    assertequals((int)value, input.readrawvarint32());

    input = codedinputstream.newinstance(data);
    assertequals(value, input.readrawvarint64());
    asserttrue(input.isatend());

    // try different block sizes.
    for (int blocksize = 1; blocksize <= 16; blocksize *= 2) {
      input = codedinputstream.newinstance(
        new smallblockinputstream(data, blocksize));
      assertequals((int)value, input.readrawvarint32());

      input = codedinputstream.newinstance(
        new smallblockinputstream(data, blocksize));
      assertequals(value, input.readrawvarint64());
      asserttrue(input.isatend());
    }

    // try reading direct from an inputstream.  we want to verify that it
    // doesn't read past the end of the input, so we copy to a new, bigger
    // array first.
    byte[] longerdata = new byte[data.length + 1];
    system.arraycopy(data, 0, longerdata, 0, data.length);
    inputstream rawinput = new bytearrayinputstream(longerdata);
    assertequals((int)value, codedinputstream.readrawvarint32(rawinput));
    assertequals(1, rawinput.available());
  }

  /**
   * parses the given bytes using readrawvarint32() and readrawvarint64() and
   * expects them to fail with an invalidprotocolbufferexception whose
   * description matches the given one.
   */
  private void assertreadvarintfailure(
      invalidprotocolbufferexception expected, byte[] data)
      throws exception {
    codedinputstream input = codedinputstream.newinstance(data);
    try {
      input.readrawvarint32();
      fail("should have thrown an exception.");
    } catch (invalidprotocolbufferexception e) {
      assertequals(expected.getmessage(), e.getmessage());
    }

    input = codedinputstream.newinstance(data);
    try {
      input.readrawvarint64();
      fail("should have thrown an exception.");
    } catch (invalidprotocolbufferexception e) {
      assertequals(expected.getmessage(), e.getmessage());
    }

    // make sure we get the same error when reading direct from an inputstream.
    try {
      codedinputstream.readrawvarint32(new bytearrayinputstream(data));
      fail("should have thrown an exception.");
    } catch (invalidprotocolbufferexception e) {
      assertequals(expected.getmessage(), e.getmessage());
    }
  }

  /** tests readrawvarint32() and readrawvarint64(). */
  public void testreadvarint() throws exception {
    assertreadvarint(bytes(0x00), 0);
    assertreadvarint(bytes(0x01), 1);
    assertreadvarint(bytes(0x7f), 127);
    // 14882
    assertreadvarint(bytes(0xa2, 0x74), (0x22 << 0) | (0x74 << 7));
    // 2961488830
    assertreadvarint(bytes(0xbe, 0xf7, 0x92, 0x84, 0x0b),
      (0x3e << 0) | (0x77 << 7) | (0x12 << 14) | (0x04 << 21) |
      (0x0bl << 28));

    // 64-bit
    // 7256456126
    assertreadvarint(bytes(0xbe, 0xf7, 0x92, 0x84, 0x1b),
      (0x3e << 0) | (0x77 << 7) | (0x12 << 14) | (0x04 << 21) |
      (0x1bl << 28));
    // 41256202580718336
    assertreadvarint(
      bytes(0x80, 0xe6, 0xeb, 0x9c, 0xc3, 0xc9, 0xa4, 0x49),
      (0x00 << 0) | (0x66 << 7) | (0x6b << 14) | (0x1c << 21) |
      (0x43l << 28) | (0x49l << 35) | (0x24l << 42) | (0x49l << 49));
    // 11964378330978735131
    assertreadvarint(
      bytes(0x9b, 0xa8, 0xf9, 0xc2, 0xbb, 0xd6, 0x80, 0x85, 0xa6, 0x01),
      (0x1b << 0) | (0x28 << 7) | (0x79 << 14) | (0x42 << 21) |
      (0x3bl << 28) | (0x56l << 35) | (0x00l << 42) |
      (0x05l << 49) | (0x26l << 56) | (0x01l << 63));

    // failures
    assertreadvarintfailure(
      invalidprotocolbufferexception.malformedvarint(),
      bytes(0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
            0x00));
    assertreadvarintfailure(
      invalidprotocolbufferexception.truncatedmessage(),
      bytes(0x80));
  }

  /**
   * parses the given bytes using readrawlittleendian32() and checks
   * that the result matches the given value.
   */
  private void assertreadlittleendian32(byte[] data, int value)
                                        throws exception {
    codedinputstream input = codedinputstream.newinstance(data);
    assertequals(value, input.readrawlittleendian32());
    asserttrue(input.isatend());

    // try different block sizes.
    for (int blocksize = 1; blocksize <= 16; blocksize *= 2) {
      input = codedinputstream.newinstance(
        new smallblockinputstream(data, blocksize));
      assertequals(value, input.readrawlittleendian32());
      asserttrue(input.isatend());
    }
  }

  /**
   * parses the given bytes using readrawlittleendian64() and checks
   * that the result matches the given value.
   */
  private void assertreadlittleendian64(byte[] data, long value)
                                        throws exception {
    codedinputstream input = codedinputstream.newinstance(data);
    assertequals(value, input.readrawlittleendian64());
    asserttrue(input.isatend());

    // try different block sizes.
    for (int blocksize = 1; blocksize <= 16; blocksize *= 2) {
      input = codedinputstream.newinstance(
        new smallblockinputstream(data, blocksize));
      assertequals(value, input.readrawlittleendian64());
      asserttrue(input.isatend());
    }
  }

  /** tests readrawlittleendian32() and readrawlittleendian64(). */
  public void testreadlittleendian() throws exception {
    assertreadlittleendian32(bytes(0x78, 0x56, 0x34, 0x12), 0x12345678);
    assertreadlittleendian32(bytes(0xf0, 0xde, 0xbc, 0x9a), 0x9abcdef0);

    assertreadlittleendian64(
      bytes(0xf0, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12),
      0x123456789abcdef0l);
    assertreadlittleendian64(
      bytes(0x78, 0x56, 0x34, 0x12, 0xf0, 0xde, 0xbc, 0x9a),
      0x9abcdef012345678l);
  }

  /** test decodezigzag32() and decodezigzag64(). */
  public void testdecodezigzag() throws exception {
    assertequals( 0, codedinputstream.decodezigzag32(0));
    assertequals(-1, codedinputstream.decodezigzag32(1));
    assertequals( 1, codedinputstream.decodezigzag32(2));
    assertequals(-2, codedinputstream.decodezigzag32(3));
    assertequals(0x3fffffff, codedinputstream.decodezigzag32(0x7ffffffe));
    assertequals(0xc0000000, codedinputstream.decodezigzag32(0x7fffffff));
    assertequals(0x7fffffff, codedinputstream.decodezigzag32(0xfffffffe));
    assertequals(0x80000000, codedinputstream.decodezigzag32(0xffffffff));

    assertequals( 0, codedinputstream.decodezigzag64(0));
    assertequals(-1, codedinputstream.decodezigzag64(1));
    assertequals( 1, codedinputstream.decodezigzag64(2));
    assertequals(-2, codedinputstream.decodezigzag64(3));
    assertequals(0x000000003fffffffl,
                 codedinputstream.decodezigzag64(0x000000007ffffffel));
    assertequals(0xffffffffc0000000l,
                 codedinputstream.decodezigzag64(0x000000007fffffffl));
    assertequals(0x000000007fffffffl,
                 codedinputstream.decodezigzag64(0x00000000fffffffel));
    assertequals(0xffffffff80000000l,
                 codedinputstream.decodezigzag64(0x00000000ffffffffl));
    assertequals(0x7fffffffffffffffl,
                 codedinputstream.decodezigzag64(0xfffffffffffffffel));
    assertequals(0x8000000000000000l,
                 codedinputstream.decodezigzag64(0xffffffffffffffffl));
  }

  /** tests reading and parsing a whole message with every field type. */
  public void testreadwholemessage() throws exception {
    testalltypes message = testutil.getallset();

    byte[] rawbytes = message.tobytearray();
    assertequals(rawbytes.length, message.getserializedsize());

    testalltypes message2 = testalltypes.parsefrom(rawbytes);
    testutil.assertallfieldsset(message2);

    // try different block sizes.
    for (int blocksize = 1; blocksize < 256; blocksize *= 2) {
      message2 = testalltypes.parsefrom(
        new smallblockinputstream(rawbytes, blocksize));
      testutil.assertallfieldsset(message2);
    }
  }

  /** tests skipfield(). */
  public void testskipwholemessage() throws exception {
    testalltypes message = testutil.getallset();
    byte[] rawbytes = message.tobytearray();

    // create two parallel inputs.  parse one as unknown fields while using
    // skipfield() to skip each field on the other.  expect the same tags.
    codedinputstream input1 = codedinputstream.newinstance(rawbytes);
    codedinputstream input2 = codedinputstream.newinstance(rawbytes);
    unknownfieldset.builder unknownfields = unknownfieldset.newbuilder();

    while (true) {
      int tag = input1.readtag();
      assertequals(tag, input2.readtag());
      if (tag == 0) {
        break;
      }
      unknownfields.mergefieldfrom(tag, input1);
      input2.skipfield(tag);
    }
  }

  /**
   * test that a bug in skiprawbytes() has been fixed:  if the skip skips
   * exactly up to a limit, this should not break things.
   */
  public void testskiprawbytesbug() throws exception {
    byte[] rawbytes = new byte[] { 1, 2 };
    codedinputstream input = codedinputstream.newinstance(rawbytes);

    int limit = input.pushlimit(1);
    input.skiprawbytes(1);
    input.poplimit(limit);
    assertequals(2, input.readrawbyte());
  }

  /**
   * test that a bug in skiprawbytes() has been fixed:  if the skip skips
   * past the end of a buffer with a limit that has been set past the end of
   * that buffer, this should not break things.
   */
  public void testskiprawbytespastendofbufferwithlimit() throws exception {
    byte[] rawbytes = new byte[] { 1, 2, 3, 4, 5 };
    codedinputstream input = codedinputstream.newinstance(
        new smallblockinputstream(rawbytes, 3));

    int limit = input.pushlimit(4);
    // in order to expose the bug we need to read at least one byte to prime the
    // buffer inside the codedinputstream.
    assertequals(1, input.readrawbyte());
    // skip to the end of the limit.
    input.skiprawbytes(3);
    asserttrue(input.isatend());
    input.poplimit(limit);
    assertequals(5, input.readrawbyte());
  }

  public void testreadhugeblob() throws exception {
    // allocate and initialize a 1mb blob.
    byte[] blob = new byte[1 << 20];
    for (int i = 0; i < blob.length; i++) {
      blob[i] = (byte)i;
    }

    // make a message containing it.
    testalltypes.builder builder = testalltypes.newbuilder();
    testutil.setallfields(builder);
    builder.setoptionalbytes(bytestring.copyfrom(blob));
    testalltypes message = builder.build();

    // serialize and parse it.  make sure to parse from an inputstream, not
    // directly from a bytestring, so that codedinputstream uses buffered
    // reading.
    testalltypes message2 =
      testalltypes.parsefrom(message.tobytestring().newinput());

    assertequals(message.getoptionalbytes(), message2.getoptionalbytes());

    // make sure all the other fields were parsed correctly.
    testalltypes message3 = testalltypes.newbuilder(message2)
      .setoptionalbytes(testutil.getallset().getoptionalbytes())
      .build();
    testutil.assertallfieldsset(message3);
  }

  public void testreadmaliciouslylargeblob() throws exception {
    bytestring.output rawoutput = bytestring.newoutput();
    codedoutputstream output = codedoutputstream.newinstance(rawoutput);

    int tag = wireformat.maketag(1, wireformat.wiretype_length_delimited);
    output.writerawvarint32(tag);
    output.writerawvarint32(0x7fffffff);
    output.writerawbytes(new byte[32]);  // pad with a few random bytes.
    output.flush();

    codedinputstream input = rawoutput.tobytestring().newcodedinput();
    assertequals(tag, input.readtag());

    try {
      input.readbytes();
      fail("should have thrown an exception!");
    } catch (invalidprotocolbufferexception e) {
      // success.
    }
  }

  private testrecursivemessage makerecursivemessage(int depth) {
    if (depth == 0) {
      return testrecursivemessage.newbuilder().seti(5).build();
    } else {
      return testrecursivemessage.newbuilder()
        .seta(makerecursivemessage(depth - 1)).build();
    }
  }

  private void assertmessagedepth(testrecursivemessage message, int depth) {
    if (depth == 0) {
      assertfalse(message.hasa());
      assertequals(5, message.geti());
    } else {
      asserttrue(message.hasa());
      assertmessagedepth(message.geta(), depth - 1);
    }
  }

  public void testmaliciousrecursion() throws exception {
    bytestring data64 = makerecursivemessage(64).tobytestring();
    bytestring data65 = makerecursivemessage(65).tobytestring();

    assertmessagedepth(testrecursivemessage.parsefrom(data64), 64);

    try {
      testrecursivemessage.parsefrom(data65);
      fail("should have thrown an exception!");
    } catch (invalidprotocolbufferexception e) {
      // success.
    }

    codedinputstream input = data64.newcodedinput();
    input.setrecursionlimit(8);
    try {
      testrecursivemessage.parsefrom(input);
      fail("should have thrown an exception!");
    } catch (invalidprotocolbufferexception e) {
      // success.
    }
  }

  public void testsizelimit() throws exception {
    codedinputstream input = codedinputstream.newinstance(
      testutil.getallset().tobytestring().newinput());
    input.setsizelimit(16);

    try {
      testalltypes.parsefrom(input);
      fail("should have thrown an exception!");
    } catch (invalidprotocolbufferexception e) {
      // success.
    }
  }

  public void testresetsizecounter() throws exception {
    codedinputstream input = codedinputstream.newinstance(
        new smallblockinputstream(new byte[256], 8));
    input.setsizelimit(16);
    input.readrawbytes(16);
    assertequals(16, input.gettotalbytesread());

    try {
      input.readrawbyte();
      fail("should have thrown an exception!");
    } catch (invalidprotocolbufferexception e) {
      // success.
    }

    input.resetsizecounter();
    assertequals(0, input.gettotalbytesread());
    input.readrawbyte();  // no exception thrown.
    input.resetsizecounter();
    assertequals(0, input.gettotalbytesread());

    try {
      input.readrawbytes(16);  // hits limit again.
      fail("should have thrown an exception!");
    } catch (invalidprotocolbufferexception e) {
      // success.
    }
  }

  /**
   * tests that if we read an string that contains invalid utf-8, no exception
   * is thrown.  instead, the invalid bytes are replaced with the unicode
   * "replacement character" u+fffd.
   */
  public void testreadinvalidutf8() throws exception {
    bytestring.output rawoutput = bytestring.newoutput();
    codedoutputstream output = codedoutputstream.newinstance(rawoutput);

    int tag = wireformat.maketag(1, wireformat.wiretype_length_delimited);
    output.writerawvarint32(tag);
    output.writerawvarint32(1);
    output.writerawbytes(new byte[] { (byte)0x80 });
    output.flush();

    codedinputstream input = rawoutput.tobytestring().newcodedinput();
    assertequals(tag, input.readtag());
    string text = input.readstring();
    assertequals(0xfffd, text.charat(0));
  }

  public void testreadfromslice() throws exception {
    byte[] bytes = bytes(0, 1, 2, 3, 4, 5, 6, 7, 8, 9);
    codedinputstream in = codedinputstream.newinstance(bytes, 3, 5);
    assertequals(0, in.gettotalbytesread());
    for (int i = 3; i < 8; i++) {
      assertequals(i, in.readrawbyte());
      assertequals(i-2, in.gettotalbytesread());
    }
    // eof
    assertequals(0, in.readtag());
    assertequals(5, in.gettotalbytesread());
  }

  public void testinvalidtag() throws exception {
    // any tag number which corresponds to field number zero is invalid and
    // should throw invalidprotocolbufferexception.
    for (int i = 0; i < 8; i++) {
      try {
        codedinputstream.newinstance(bytes(i)).readtag();
        fail("should have thrown an exception.");
      } catch (invalidprotocolbufferexception e) {
        assertequals(invalidprotocolbufferexception.invalidtag().getmessage(),
                     e.getmessage());
      }
    }
  }
}
