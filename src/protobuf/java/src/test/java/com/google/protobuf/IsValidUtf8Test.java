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

import com.google.protobuf.isvalidutf8testutil.shard;

import junit.framework.testcase;

import java.io.unsupportedencodingexception;

/**
 * tests cases for {@link bytestring#isvalidutf8()}. this includes three
 * brute force tests that actually test every permutation of one byte, two byte,
 * and three byte sequences to ensure that the method produces the right result
 * for every possible byte encoding where "right" means it's consistent with
 * java's utf-8 string encoding/decoding such that the method returns true for
 * any sequence that will round trip when converted to a string and then back to
 * bytes and will return false for any sequence that will not round trip.
 * see also {@link isvalidutf8fourbytetest}. it also includes some
 * other more targeted tests.
 *
 * @author jonp@google.com (jon perlow)
 * @author martinrb@google.com (martin buchholz)
 */
public class isvalidutf8test extends testcase {

  /**
   * tests that round tripping of all two byte permutations work.
   */
  public void testisvalidutf8_1byte() throws unsupportedencodingexception {
    isvalidutf8testutil.testbytes(1,
        isvalidutf8testutil.expected_one_byte_roundtrippable_count);
  }

  /**
   * tests that round tripping of all two byte permutations work.
   */
  public void testisvalidutf8_2bytes() throws unsupportedencodingexception {
    isvalidutf8testutil.testbytes(2,
        isvalidutf8testutil.expected_two_byte_roundtrippable_count);
  }

  /**
   * tests that round tripping of all three byte permutations work.
   */
  public void testisvalidutf8_3bytes() throws unsupportedencodingexception {
    isvalidutf8testutil.testbytes(3,
        isvalidutf8testutil.expected_three_byte_roundtrippable_count);
  }

  /**
   * tests that round tripping of a sample of four byte permutations work.
   * all permutations are prohibitively expensive to test for automated runs;
   * {@link isvalidutf8fourbytetest} is used for full coverage. this method
   * tests specific four-byte cases.
   */
  public void testisvalidutf8_4bytessamples()
      throws unsupportedencodingexception {
    // valid 4 byte.
    assertvalidutf8(0xf0, 0xa4, 0xad, 0xa2);

    // bad trailing bytes
    assertinvalidutf8(0xf0, 0xa4, 0xad, 0x7f);
    assertinvalidutf8(0xf0, 0xa4, 0xad, 0xc0);

    // special cases for byte2
    assertinvalidutf8(0xf0, 0x8f, 0xad, 0xa2);
    assertinvalidutf8(0xf4, 0x90, 0xad, 0xa2);
  }

  /**
   * tests some hard-coded test cases.
   */
  public void testsomesequences() {
    // empty
    asserttrue(asbytes("").isvalidutf8());

    // one-byte characters, including control characters
    asserttrue(asbytes("\u0000abc\u007f").isvalidutf8());

    // two-byte characters
    asserttrue(asbytes("\u00a2\u00a2").isvalidutf8());

    // three-byte characters
    asserttrue(asbytes("\u020ac\u020ac").isvalidutf8());

    // four-byte characters
    asserttrue(asbytes("\u024b62\u024b62").isvalidutf8());

    // mixed string
    asserttrue(
        asbytes("a\u020ac\u00a2b\\u024b62u020acc\u00a2de\u024b62")
        .isvalidutf8());

    // not a valid string
    assertinvalidutf8(-1, 0, -1, 0);
  }

  private byte[] tobytearray(int... bytes) {
    byte[] realbytes = new byte[bytes.length];
    for (int i = 0; i < bytes.length; i++) {
      realbytes[i] = (byte) bytes[i];
    }
    return realbytes;
  }

  private bytestring tobytestring(int... bytes) {
    return bytestring.copyfrom(tobytearray(bytes));
  }

  private void assertvalidutf8(int[] bytes, boolean not) {
    byte[] realbytes = tobytearray(bytes);
    asserttrue(not ^ utf8.isvalidutf8(realbytes));
    asserttrue(not ^ utf8.isvalidutf8(realbytes, 0, bytes.length));
    bytestring lit = bytestring.copyfrom(realbytes);
    bytestring sub = lit.substring(0, bytes.length);
    asserttrue(not ^ lit.isvalidutf8());
    asserttrue(not ^ sub.isvalidutf8());
    bytestring[] ropes = {
      ropebytestring.newinstancefortest(bytestring.empty, lit),
      ropebytestring.newinstancefortest(bytestring.empty, sub),
      ropebytestring.newinstancefortest(lit, bytestring.empty),
      ropebytestring.newinstancefortest(sub, bytestring.empty),
      ropebytestring.newinstancefortest(sub, lit)
    };
    for (bytestring rope : ropes) {
      asserttrue(not ^ rope.isvalidutf8());
    }
  }

  private void assertvalidutf8(int... bytes) {
    assertvalidutf8(bytes, false);
  }

  private void assertinvalidutf8(int... bytes) {
    assertvalidutf8(bytes, true);
  }

  private static bytestring asbytes(string s) {
    return bytestring.copyfromutf8(s);
  }

  public void testshardshaveexpectedroundtrippables() {
    // a sanity check.
    int actual = 0;
    for (shard shard : isvalidutf8testutil.four_byte_shards) {
      actual += shard.expected;
    }
    assertequals(isvalidutf8testutil.expected_four_byte_roundtrippable_count,
        actual);
  }
}
