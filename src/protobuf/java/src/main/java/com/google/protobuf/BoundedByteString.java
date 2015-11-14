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

import java.util.nosuchelementexception;

/**
 * this class is used to represent the substring of a {@link bytestring} over a
 * single byte array. in terms of the public api of {@link bytestring}, you end
 * up here by calling {@link bytestring#copyfrom(byte[])} followed by {@link
 * bytestring#substring(int, int)}.
 *
 * <p>this class contains most of the overhead involved in creating a substring
 * from a {@link literalbytestring}.  the overhead involves some range-checking
 * and two extra fields.
 *
 * @author carlanton@google.com (carl haverl)
 */
class boundedbytestring extends literalbytestring {

  private final int bytesoffset;
  private final int byteslength;

  /**
   * creates a {@code boundedbytestring} backed by the sub-range of given array,
   * without copying.
   *
   * @param bytes  array to wrap
   * @param offset index to first byte to use in bytes
   * @param length number of bytes to use from bytes
   * @throws illegalargumentexception if {@code offset < 0}, {@code length < 0},
   *                                  or if {@code offset + length >
   *                                  bytes.length}.
   */
  boundedbytestring(byte[] bytes, int offset, int length) {
    super(bytes);
    if (offset < 0) {
      throw new illegalargumentexception("offset too small: " + offset);
    }
    if (length < 0) {
      throw new illegalargumentexception("length too small: " + offset);
    }
    if ((long) offset + length > bytes.length) {
      throw new illegalargumentexception(
          "offset+length too large: " + offset + "+" + length);
    }

    this.bytesoffset = offset;
    this.byteslength = length;
  }

  /**
   * gets the byte at the given index.
   * throws {@link arrayindexoutofboundsexception}
   * for backwards-compatibility reasons although it would more properly be
   * {@link indexoutofboundsexception}.
   *
   * @param index index of byte
   * @return the value
   * @throws arrayindexoutofboundsexception {@code index} is < 0 or >= size
   */
  @override
  public byte byteat(int index) {
    // we must check the index ourselves as we cannot rely on java array index
    // checking for substrings.
    if (index < 0) {
      throw new arrayindexoutofboundsexception("index too small: " + index);
    }
    if (index >= size()) {
      throw new arrayindexoutofboundsexception(
          "index too large: " + index + ", " + size());
    }

    return bytes[bytesoffset + index];
  }

  @override
  public int size() {
    return byteslength;
  }

  @override
  protected int getoffsetintobytes() {
    return bytesoffset;
  }

  // =================================================================
  // bytestring -> byte[]

  @override
  protected void copytointernal(byte[] target, int sourceoffset, 
      int targetoffset, int numbertocopy) {
    system.arraycopy(bytes, getoffsetintobytes() + sourceoffset, target,
        targetoffset, numbertocopy);
  }

  // =================================================================
  // byteiterator

  @override
  public byteiterator iterator() {
    return new boundedbyteiterator();
  }

  private class boundedbyteiterator implements byteiterator {

    private int position;
    private final int limit;

    private boundedbyteiterator() {
      position = getoffsetintobytes();
      limit = position + size();
    }

    public boolean hasnext() {
      return (position < limit);
    }

    public byte next() {
      // boxing calls byte.valueof(byte), which does not instantiate.
      return nextbyte();
    }

    public byte nextbyte() {
      if (position >= limit) {
        throw new nosuchelementexception();
      }
      return bytes[position++];
    }

    public void remove() {
      throw new unsupportedoperationexception();
    }
  }
}
