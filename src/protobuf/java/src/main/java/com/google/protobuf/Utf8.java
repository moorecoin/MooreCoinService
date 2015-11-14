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

/**
 * a set of low-level, high-performance static utility methods related
 * to the utf-8 character encoding.  this class has no dependencies
 * outside of the core jdk libraries.
 *
 * <p>there are several variants of utf-8.  the one implemented by
 * this class is the restricted definition of utf-8 introduced in
 * unicode 3.1, which mandates the rejection of "overlong" byte
 * sequences as well as rejection of 3-byte surrogate codepoint byte
 * sequences.  note that the utf-8 decoder included in oracle's jdk
 * has been modified to also reject "overlong" byte sequences, but (as
 * of 2011) still accepts 3-byte surrogate codepoint byte sequences.
 *
 * <p>the byte sequences considered valid by this class are exactly
 * those that can be roundtrip converted to strings and back to bytes
 * using the utf-8 charset, without loss: <pre> {@code
 * arrays.equals(bytes, new string(bytes, "utf-8").getbytes("utf-8"))
 * }</pre>
 *
 * <p>see the unicode standard,</br>
 * table 3-6. <em>utf-8 bit distribution</em>,</br>
 * table 3-7. <em>well formed utf-8 byte sequences</em>.
 *
 * <p>this class supports decoding of partial byte sequences, so that the
 * bytes in a complete utf-8 byte sequences can be stored in multiple
 * segments.  methods typically return {@link #malformed} if the partial
 * byte sequence is definitely not well-formed, {@link #complete} if it is
 * well-formed in the absence of additional input, or if the byte sequence
 * apparently terminated in the middle of a character, an opaque integer
 * "state" value containing enough information to decode the character when
 * passed to a subsequent invocation of a partial decoding method.
 *
 * @author martinrb@google.com (martin buchholz)
 */
final class utf8 {
  private utf8() {}

  /**
   * state value indicating that the byte sequence is well-formed and
   * complete (no further bytes are needed to complete a character).
   */
  public static final int complete = 0;

  /**
   * state value indicating that the byte sequence is definitely not
   * well-formed.
   */
  public static final int malformed = -1;

  // other state values include the partial bytes of the incomplete
  // character to be decoded in the simplest way: we pack the bytes
  // into the state int in little-endian order.  for example:
  //
  // int state = byte1 ^ (byte2 << 8) ^ (byte3 << 16);
  //
  // such a state is unpacked thus (note the ~ operation for byte2 to
  // undo byte1's sign-extension bits):
  //
  // int byte1 = (byte) state;
  // int byte2 = (byte) ~(state >> 8);
  // int byte3 = (byte) (state >> 16);
  //
  // we cannot store a zero byte in the state because it would be
  // indistinguishable from the absence of a byte.  but we don't need
  // to, because partial bytes must always be negative.  when building
  // a state, we ensure that byte1 is negative and subsequent bytes
  // are valid trailing bytes.

  /**
   * returns {@code true} if the given byte array is a well-formed
   * utf-8 byte sequence.
   *
   * <p>this is a convenience method, equivalent to a call to {@code
   * isvalidutf8(bytes, 0, bytes.length)}.
   */
  public static boolean isvalidutf8(byte[] bytes) {
    return isvalidutf8(bytes, 0, bytes.length);
  }

  /**
   * returns {@code true} if the given byte array slice is a
   * well-formed utf-8 byte sequence.  the range of bytes to be
   * checked extends from index {@code index}, inclusive, to {@code
   * limit}, exclusive.
   *
   * <p>this is a convenience method, equivalent to {@code
   * partialisvalidutf8(bytes, index, limit) == utf8.complete}.
   */
  public static boolean isvalidutf8(byte[] bytes, int index, int limit) {
    return partialisvalidutf8(bytes, index, limit) == complete;
  }

  /**
   * tells whether the given byte array slice is a well-formed,
   * malformed, or incomplete utf-8 byte sequence.  the range of bytes
   * to be checked extends from index {@code index}, inclusive, to
   * {@code limit}, exclusive.
   *
   * @param state either {@link utf8#complete} (if this is the initial decoding
   * operation) or the value returned from a call to a partial decoding method
   * for the previous bytes
   *
   * @return {@link #malformed} if the partial byte sequence is
   * definitely not well-formed, {@link #complete} if it is well-formed
   * (no additional input needed), or if the byte sequence is
   * "incomplete", i.e. apparently terminated in the middle of a character,
   * an opaque integer "state" value containing enough information to
   * decode the character when passed to a subsequent invocation of a
   * partial decoding method.
   */
  public static int partialisvalidutf8(
      int state, byte[] bytes, int index, int limit) {
    if (state != complete) {
      // the previous decoding operation was incomplete (or malformed).
      // we look for a well-formed sequence consisting of bytes from
      // the previous decoding operation (stored in state) together
      // with bytes from the array slice.
      //
      // we expect such "straddler characters" to be rare.

      if (index >= limit) {  // no bytes? no progress.
        return state;
      }
      int byte1 = (byte) state;
      // byte1 is never ascii.
      if (byte1 < (byte) 0xe0) {
        // two-byte form

        // simultaneously checks for illegal trailing-byte in
        // leading position and overlong 2-byte form.
        if (byte1 < (byte) 0xc2 ||
            // byte2 trailing-byte test
            bytes[index++] > (byte) 0xbf) {
          return malformed;
        }
      } else if (byte1 < (byte) 0xf0) {
        // three-byte form

        // get byte2 from saved state or array
        int byte2 = (byte) ~(state >> 8);
        if (byte2 == 0) {
          byte2 = bytes[index++];
          if (index >= limit) {
            return incompletestatefor(byte1, byte2);
          }
        }
        if (byte2 > (byte) 0xbf ||
            // overlong? 5 most significant bits must not all be zero
            (byte1 == (byte) 0xe0 && byte2 < (byte) 0xa0) ||
            // illegal surrogate codepoint?
            (byte1 == (byte) 0xed && byte2 >= (byte) 0xa0) ||
            // byte3 trailing-byte test
            bytes[index++] > (byte) 0xbf) {
          return malformed;
        }
      } else {
        // four-byte form

        // get byte2 and byte3 from saved state or array
        int byte2 = (byte) ~(state >> 8);
        int byte3 = 0;
        if (byte2 == 0) {
          byte2 = bytes[index++];
          if (index >= limit) {
            return incompletestatefor(byte1, byte2);
          }
        } else {
          byte3 = (byte) (state >> 16);
        }
        if (byte3 == 0) {
          byte3 = bytes[index++];
          if (index >= limit) {
            return incompletestatefor(byte1, byte2, byte3);
          }
        }

        // if we were called with state == malformed, then byte1 is 0xff,
        // which never occurs in well-formed utf-8, and so we will return
        // malformed again below.

        if (byte2 > (byte) 0xbf ||
            // check that 1 <= plane <= 16.  tricky optimized form of:
            // if (byte1 > (byte) 0xf4 ||
            //     byte1 == (byte) 0xf0 && byte2 < (byte) 0x90 ||
            //     byte1 == (byte) 0xf4 && byte2 > (byte) 0x8f)
            (((byte1 << 28) + (byte2 - (byte) 0x90)) >> 30) != 0 ||
            // byte3 trailing-byte test
            byte3 > (byte) 0xbf ||
            // byte4 trailing-byte test
             bytes[index++] > (byte) 0xbf) {
          return malformed;
        }
      }
    }

    return partialisvalidutf8(bytes, index, limit);
  }

  /**
   * tells whether the given byte array slice is a well-formed,
   * malformed, or incomplete utf-8 byte sequence.  the range of bytes
   * to be checked extends from index {@code index}, inclusive, to
   * {@code limit}, exclusive.
   *
   * <p>this is a convenience method, equivalent to a call to {@code
   * partialisvalidutf8(utf8.complete, bytes, index, limit)}.
   *
   * @return {@link #malformed} if the partial byte sequence is
   * definitely not well-formed, {@link #complete} if it is well-formed
   * (no additional input needed), or if the byte sequence is
   * "incomplete", i.e. apparently terminated in the middle of a character,
   * an opaque integer "state" value containing enough information to
   * decode the character when passed to a subsequent invocation of a
   * partial decoding method.
   */
  public static int partialisvalidutf8(
      byte[] bytes, int index, int limit) {
    // optimize for 100% ascii.
    // hotspot loves small simple top-level loops like this.
    while (index < limit && bytes[index] >= 0) {
      index++;
    }

    return (index >= limit) ? complete :
        partialisvalidutf8nonascii(bytes, index, limit);
  }

  private static int partialisvalidutf8nonascii(
      byte[] bytes, int index, int limit) {
    for (;;) {
      int byte1, byte2;

      // optimize for interior runs of ascii bytes.
      do {
        if (index >= limit) {
          return complete;
        }
      } while ((byte1 = bytes[index++]) >= 0);

      if (byte1 < (byte) 0xe0) {
        // two-byte form

        if (index >= limit) {
          return byte1;
        }

        // simultaneously checks for illegal trailing-byte in
        // leading position and overlong 2-byte form.
        if (byte1 < (byte) 0xc2 ||
            bytes[index++] > (byte) 0xbf) {
          return malformed;
        }
      } else if (byte1 < (byte) 0xf0) {
        // three-byte form

        if (index >= limit - 1) { // incomplete sequence
          return incompletestatefor(bytes, index, limit);
        }
        if ((byte2 = bytes[index++]) > (byte) 0xbf ||
            // overlong? 5 most significant bits must not all be zero
            (byte1 == (byte) 0xe0 && byte2 < (byte) 0xa0) ||
            // check for illegal surrogate codepoints
            (byte1 == (byte) 0xed && byte2 >= (byte) 0xa0) ||
            // byte3 trailing-byte test
            bytes[index++] > (byte) 0xbf) {
          return malformed;
        }
      } else {
        // four-byte form

        if (index >= limit - 2) {  // incomplete sequence
          return incompletestatefor(bytes, index, limit);
        }
        if ((byte2 = bytes[index++]) > (byte) 0xbf ||
            // check that 1 <= plane <= 16.  tricky optimized form of:
            // if (byte1 > (byte) 0xf4 ||
            //     byte1 == (byte) 0xf0 && byte2 < (byte) 0x90 ||
            //     byte1 == (byte) 0xf4 && byte2 > (byte) 0x8f)
            (((byte1 << 28) + (byte2 - (byte) 0x90)) >> 30) != 0 ||
            // byte3 trailing-byte test
            bytes[index++] > (byte) 0xbf ||
            // byte4 trailing-byte test
            bytes[index++] > (byte) 0xbf) {
          return malformed;
        }
      }
    }
  }

  private static int incompletestatefor(int byte1) {
    return (byte1 > (byte) 0xf4) ?
        malformed : byte1;
  }

  private static int incompletestatefor(int byte1, int byte2) {
    return (byte1 > (byte) 0xf4 ||
            byte2 > (byte) 0xbf) ?
        malformed : byte1 ^ (byte2 << 8);
  }

  private static int incompletestatefor(int byte1, int byte2, int byte3) {
    return (byte1 > (byte) 0xf4 ||
            byte2 > (byte) 0xbf ||
            byte3 > (byte) 0xbf) ?
        malformed : byte1 ^ (byte2 << 8) ^ (byte3 << 16);
  }

  private static int incompletestatefor(byte[] bytes, int index, int limit) {
    int byte1 = bytes[index - 1];
    switch (limit - index) {
      case 0: return incompletestatefor(byte1);
      case 1: return incompletestatefor(byte1, bytes[index]);
      case 2: return incompletestatefor(byte1, bytes[index], bytes[index + 1]);
      default: throw new assertionerror();
    }
  }
}
