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

import static junit.framework.assert.*;

import java.io.unsupportedencodingexception;
import java.util.arraylist;
import java.util.arrays;
import java.util.list;
import java.util.random;
import java.util.logging.logger;
import java.nio.charset.charsetdecoder;
import java.nio.charset.charset;
import java.nio.charset.codingerroraction;
import java.nio.charset.charsetencoder;
import java.nio.charset.coderresult;
import java.nio.bytebuffer;
import java.nio.charbuffer;

/**
 * shared testing code for {@link isvalidutf8test} and
 * {@link isvalidutf8fourbytetest}.
 *
 * @author jonp@google.com (jon perlow)
 * @author martinrb@google.com (martin buchholz)
 */
class isvalidutf8testutil {
  private static logger logger = logger.getlogger(
      isvalidutf8testutil.class.getname());

  // 128 - [chars 0x0000 to 0x007f]
  static long one_byte_roundtrippable_characters = 0x007f - 0x0000 + 1;

  // 128
  static long expected_one_byte_roundtrippable_count =
      one_byte_roundtrippable_characters;

  // 1920 [chars 0x0080 to 0x07ff]
  static long two_byte_roundtrippable_characters = 0x07ff - 0x0080 + 1;

  // 18,304
  static long expected_two_byte_roundtrippable_count =
      // both bytes are one byte characters
      (long) math.pow(expected_one_byte_roundtrippable_count, 2) +
      // the possible number of two byte characters
      two_byte_roundtrippable_characters;

  // 2048
  static long three_byte_surrogates = 2 * 1024;

  // 61,440 [chars 0x0800 to 0xffff, minus surrogates]
  static long three_byte_roundtrippable_characters =
      0xffff - 0x0800 + 1 - three_byte_surrogates;

  // 2,650,112
  static long expected_three_byte_roundtrippable_count =
      // all one byte characters
      (long) math.pow(expected_one_byte_roundtrippable_count, 3) +
      // one two byte character and a one byte character
      2 * two_byte_roundtrippable_characters *
          one_byte_roundtrippable_characters +
       // three byte characters
      three_byte_roundtrippable_characters;

  // 1,048,576 [chars 0x10000l to 0x10ffff]
  static long four_byte_roundtrippable_characters = 0x10ffff - 0x10000l + 1;

  // 289,571,839
  static long expected_four_byte_roundtrippable_count =
      // all one byte characters
      (long) math.pow(expected_one_byte_roundtrippable_count, 4) +
      // one and three byte characters
      2 * three_byte_roundtrippable_characters *
          one_byte_roundtrippable_characters +
      // two two byte characters
      two_byte_roundtrippable_characters * two_byte_roundtrippable_characters +
      // permutations of one and two byte characters
      3 * two_byte_roundtrippable_characters *
          one_byte_roundtrippable_characters *
          one_byte_roundtrippable_characters +
      // four byte characters
      four_byte_roundtrippable_characters;

  static class shard {
    final long index;
    final long start;
    final long lim;
    final long expected;


    public shard(long index, long start, long lim, long expected) {
      asserttrue(start < lim);
      this.index = index;
      this.start = start;
      this.lim = lim;
      this.expected = expected;
    }
  }

  static final long[] four_byte_shards_expected_rountrippables =
      generatefourbyteshardsexpectedrunnables();

  private static long[] generatefourbyteshardsexpectedrunnables() {
    long[] expected = new long[128];

    // 0-63 are all 5300224
    for (int i = 0; i <= 63; i++) {
      expected[i] = 5300224;
    }

    // 97-111 are all 2342912
    for (int i = 97; i <= 111; i++) {
     expected[i] = 2342912;
    }

    // 113-117 are all 1048576
    for (int i = 113; i <= 117; i++) {
      expected[i] = 1048576;
    }

    // one offs
    expected[112] = 786432;
    expected[118] = 786432;
    expected[119] = 1048576;
    expected[120] = 458752;
    expected[121] = 524288;
    expected[122] = 65536;

    // anything not assigned was the default 0.
    return expected;
  }

  static final list<shard> four_byte_shards = generatefourbyteshards(
      128, four_byte_shards_expected_rountrippables);


  private static list<shard> generatefourbyteshards(
      int numshards, long[] expected) {
    assertequals(numshards, expected.length);
    list<shard> shards = new arraylist<shard>(numshards);
    long lim = 1l << 32;
    long increment = lim / numshards;
    asserttrue(lim % numshards == 0);
    for (int i = 0; i < numshards; i++) {
      shards.add(new shard(i,
          increment * i,
          increment * (i + 1),
          expected[i]));
    }
    return shards;
  }

  /**
   * helper to run the loop to test all the permutations for the number of bytes
   * specified.
   *
   * @param numbytes the number of bytes in the byte array
   * @param expectedcount the expected number of roundtrippable permutations
   */
  static void testbytes(int numbytes, long expectedcount)
      throws unsupportedencodingexception {
    testbytes(numbytes, expectedcount, 0, -1);
  }

  /**
   * helper to run the loop to test all the permutations for the number of bytes
   * specified. this overload is useful for debugging to get the loop to start
   * at a certain character.
   *
   * @param numbytes the number of bytes in the byte array
   * @param expectedcount the expected number of roundtrippable permutations
   * @param start the starting bytes encoded as a long as big-endian
   * @param lim the limit of bytes to process encoded as a long as big-endian,
   *     or -1 to mean the max limit for numbytes
   */
  static void testbytes(int numbytes, long expectedcount, long start, long lim)
      throws unsupportedencodingexception {
    random rnd = new random();
    byte[] bytes = new byte[numbytes];

    if (lim == -1) {
      lim = 1l << (numbytes * 8);
    }
    long count = 0;
    long countroundtripped = 0;
    for (long bytechar = start; bytechar < lim; bytechar++) {
      long tmpbytechar = bytechar;
      for (int i = 0; i < numbytes; i++) {
        bytes[bytes.length - i - 1] = (byte) tmpbytechar;
        tmpbytechar = tmpbytechar >> 8;
      }
      bytestring bs = bytestring.copyfrom(bytes);
      boolean isroundtrippable = bs.isvalidutf8();
      string s = new string(bytes, "utf-8");
      byte[] bytesreencoded = s.getbytes("utf-8");
      boolean bytesequal = arrays.equals(bytes, bytesreencoded);

      if (bytesequal != isroundtrippable) {
        outputfailure(bytechar, bytes, bytesreencoded);
      }

      // check agreement with static utf8 methods.
      assertequals(isroundtrippable, utf8.isvalidutf8(bytes));
      assertequals(isroundtrippable, utf8.isvalidutf8(bytes, 0, numbytes));

      // test partial sequences.
      // partition numbytes into three segments (not necessarily non-empty).
      int i = rnd.nextint(numbytes);
      int j = rnd.nextint(numbytes);
      if (j < i) {
        int tmp = i; i = j; j = tmp;
      }
      int state1 = utf8.partialisvalidutf8(utf8.complete, bytes, 0, i);
      int state2 = utf8.partialisvalidutf8(state1, bytes, i, j);
      int state3 = utf8.partialisvalidutf8(state2, bytes, j, numbytes);
      if (isroundtrippable != (state3 == utf8.complete)) {
        system.out.printf("state=%04x %04x %04x i=%d j=%d%n",
                          state1, state2, state3, i, j);
        outputfailure(bytechar, bytes, bytesreencoded);
      }
      assertequals(isroundtrippable, (state3 == utf8.complete));

      // test ropes built out of small partial sequences
      bytestring rope = ropebytestring.newinstancefortest(
          bs.substring(0, i),
          ropebytestring.newinstancefortest(
              bs.substring(i, j),
              bs.substring(j, numbytes)));
      assertsame(ropebytestring.class, rope.getclass());

      bytestring[] bytestrings = { bs, bs.substring(0, numbytes), rope };
      for (bytestring x : bytestrings) {
        assertequals(isroundtrippable,
                     x.isvalidutf8());
        assertequals(state3,
                     x.partialisvalidutf8(utf8.complete, 0, numbytes));

        assertequals(state1,
                     x.partialisvalidutf8(utf8.complete, 0, i));
        assertequals(state1,
                     x.substring(0, i).partialisvalidutf8(utf8.complete, 0, i));
        assertequals(state2,
                     x.partialisvalidutf8(state1, i, j - i));
        assertequals(state2,
                     x.substring(i, j).partialisvalidutf8(state1, 0, j - i));
        assertequals(state3,
                     x.partialisvalidutf8(state2, j, numbytes - j));
        assertequals(state3,
                     x.substring(j, numbytes)
                     .partialisvalidutf8(state2, 0, numbytes - j));
      }

      // bytestring reduplication should not affect its utf-8 validity.
      bytestring ropeadope =
          ropebytestring.newinstancefortest(bs, bs.substring(0, numbytes));
      assertequals(isroundtrippable, ropeadope.isvalidutf8());

      if (isroundtrippable) {
        countroundtripped++;
      }
      count++;
      if (bytechar != 0 && bytechar % 1000000l == 0) {
        logger.info("processed " + (bytechar / 1000000l) +
            " million characters");
      }
    }
    logger.info("round tripped " + countroundtripped + " of " + count);
    assertequals(expectedcount, countroundtripped);
  }

  /**
   * variation of {@link #testbytes} that does less allocation using the
   * low-level encoders/decoders directly. checked in because it's useful for
   * debugging when trying to process bytes faster, but since it doesn't use the
   * actual string class, it's possible for incompatibilities to develop
   * (although unlikely).
   *
   * @param numbytes the number of bytes in the byte array
   * @param expectedcount the expected number of roundtrippable permutations
   * @param start the starting bytes encoded as a long as big-endian
   * @param lim the limit of bytes to process encoded as a long as big-endian,
   *     or -1 to mean the max limit for numbytes
   */
  void testbytesusingbytebuffers(
      int numbytes, long expectedcount, long start, long lim)
      throws unsupportedencodingexception {
    charsetdecoder decoder = charset.forname("utf-8").newdecoder()
        .onmalformedinput(codingerroraction.replace)
        .onunmappablecharacter(codingerroraction.replace);
    charsetencoder encoder = charset.forname("utf-8").newencoder()
        .onmalformedinput(codingerroraction.replace)
        .onunmappablecharacter(codingerroraction.replace);
    byte[] bytes = new byte[numbytes];
    int maxchars = (int) (decoder.maxcharsperbyte() * numbytes) + 1;
    char[] charsdecoded =
        new char[(int) (decoder.maxcharsperbyte() * numbytes) + 1];
    int maxbytes = (int) (encoder.maxbytesperchar() * maxchars) + 1;
    byte[] bytesreencoded = new byte[maxbytes];

    bytebuffer bb = bytebuffer.wrap(bytes);
    charbuffer cb = charbuffer.wrap(charsdecoded);
    bytebuffer bbreencoded = bytebuffer.wrap(bytesreencoded);
    if (lim == -1) {
      lim = 1l << (numbytes * 8);
    }
    long count = 0;
    long countroundtripped = 0;
    for (long bytechar = start; bytechar < lim; bytechar++) {
      bb.rewind();
      bb.limit(bytes.length);
      cb.rewind();
      cb.limit(charsdecoded.length);
      bbreencoded.rewind();
      bbreencoded.limit(bytesreencoded.length);
      encoder.reset();
      decoder.reset();
      long tmpbytechar = bytechar;
      for (int i = 0; i < bytes.length; i++) {
        bytes[bytes.length - i - 1] = (byte) tmpbytechar;
        tmpbytechar = tmpbytechar >> 8;
      }
      boolean isroundtrippable = bytestring.copyfrom(bytes).isvalidutf8();
      coderresult result = decoder.decode(bb, cb, true);
      assertfalse(result.iserror());
      result = decoder.flush(cb);
      assertfalse(result.iserror());

      int charlen = cb.position();
      cb.rewind();
      cb.limit(charlen);
      result = encoder.encode(cb, bbreencoded, true);
      assertfalse(result.iserror());
      result = encoder.flush(bbreencoded);
      assertfalse(result.iserror());

      boolean bytesequal = true;
      int byteslen = bbreencoded.position();
      if (byteslen != numbytes) {
        bytesequal = false;
      } else {
        for (int i = 0; i < numbytes; i++) {
          if (bytes[i] != bytesreencoded[i]) {
            bytesequal = false;
            break;
          }
        }
      }
      if (bytesequal != isroundtrippable) {
        outputfailure(bytechar, bytes, bytesreencoded, byteslen);
      }

      count++;
      if (isroundtrippable) {
        countroundtripped++;
      }
      if (bytechar != 0 && bytechar % 1000000 == 0) {
        logger.info("processed " + (bytechar / 1000000) +
            " million characters");
      }
    }
    logger.info("round tripped " + countroundtripped + " of " + count);
    assertequals(expectedcount, countroundtripped);
  }

  private static void outputfailure(long bytechar, byte[] bytes, byte[] after) {
    outputfailure(bytechar, bytes, after, after.length);
  }

  private static void outputfailure(long bytechar, byte[] bytes, byte[] after,
      int len) {
    fail("failure: (" + long.tohexstring(bytechar) + ") " +
        tohexstring(bytes) + " => " + tohexstring(after, len));
  }

  private static string tohexstring(byte[] b) {
    return tohexstring(b, b.length);
  }

  private static string tohexstring(byte[] b, int len) {
    stringbuilder s = new stringbuilder();
    s.append("\"");
    for (int i = 0; i < len; i++) {
      if (i > 0) {
        s.append(" ");
      }
      s.append(string.format("%02x", b[i] & 0xff));
    }
    s.append("\"");
    return s.tostring();
  }

}
