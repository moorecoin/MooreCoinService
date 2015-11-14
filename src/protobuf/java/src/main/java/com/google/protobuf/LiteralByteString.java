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

import java.io.bytearrayinputstream;
import java.io.ioexception;
import java.io.inputstream;
import java.io.outputstream;
import java.io.unsupportedencodingexception;
import java.nio.bytebuffer;
import java.util.arraylist;
import java.util.list;
import java.util.nosuchelementexception;

/**
 * this class implements a {@link com.google.protobuf.bytestring} backed by a
 * single array of bytes, contiguous in memory. it supports substring by
 * pointing to only a sub-range of the underlying byte array, meaning that a
 * substring will reference the full byte-array of the string it's made from,
 * exactly as with {@link string}.
 *
 * @author carlanton@google.com (carl haverl)
 */
class literalbytestring extends bytestring {

  protected final byte[] bytes;

  /**
   * creates a {@code literalbytestring} backed by the given array, without
   * copying.
   *
   * @param bytes array to wrap
   */
  literalbytestring(byte[] bytes) {
    this.bytes = bytes;
  }

  @override
  public byte byteat(int index) {
    // unlike most methods in this class, this one is a direct implementation
    // ignoring the potential offset because we need to do range-checking in the
    // substring case anyway.
    return bytes[index];
  }

  @override
  public int size() {
    return bytes.length;
  }

  // =================================================================
  // bytestring -> substring

  @override
  public bytestring substring(int beginindex, int endindex) {
    if (beginindex < 0) {
      throw new indexoutofboundsexception(
          "beginning index: " + beginindex + " < 0");
    }
    if (endindex > size()) {
      throw new indexoutofboundsexception("end index: " + endindex + " > " +
          size());
    }
    int substringlength = endindex - beginindex;
    if (substringlength < 0) {
      throw new indexoutofboundsexception(
          "beginning index larger than ending index: " + beginindex + ", "
              + endindex);
    }

    bytestring result;
    if (substringlength == 0) {
      result = bytestring.empty;
    } else {
      result = new boundedbytestring(bytes, getoffsetintobytes() + beginindex,
          substringlength);
    }
    return result;
  }

  // =================================================================
  // bytestring -> byte[]

  @override
  protected void copytointernal(byte[] target, int sourceoffset, 
      int targetoffset, int numbertocopy) {
    // optimized form, not for subclasses, since we don't call
    // getoffsetintobytes() or check the 'numbertocopy' parameter.
    system.arraycopy(bytes, sourceoffset, target, targetoffset, numbertocopy);
  }

  @override
  public void copyto(bytebuffer target) {
    target.put(bytes, getoffsetintobytes(), size());  // copies bytes
  }

  @override
  public bytebuffer asreadonlybytebuffer() {
    bytebuffer bytebuffer =
        bytebuffer.wrap(bytes, getoffsetintobytes(), size());
    return bytebuffer.asreadonlybuffer();
  }

  @override
  public list<bytebuffer> asreadonlybytebufferlist() {
    // return the bytebuffer generated by asreadonlybytebuffer() as a singleton
    list<bytebuffer> result = new arraylist<bytebuffer>(1);
    result.add(asreadonlybytebuffer());
    return result;
 }

 @override
  public void writeto(outputstream outputstream) throws ioexception {
    outputstream.write(tobytearray());
  }

  @override
  public string tostring(string charsetname)
      throws unsupportedencodingexception {
    return new string(bytes, getoffsetintobytes(), size(), charsetname);
  }

  // =================================================================
  // utf-8 decoding

  @override
  public boolean isvalidutf8() {
    int offset = getoffsetintobytes();
    return utf8.isvalidutf8(bytes, offset, offset + size());
  }

  @override
  protected int partialisvalidutf8(int state, int offset, int length) {
    int index = getoffsetintobytes() + offset;
    return utf8.partialisvalidutf8(state, bytes, index, index + length);
  }

  // =================================================================
  // equals() and hashcode()

  @override
  public boolean equals(object other) {
    if (other == this) {
      return true;
    }
    if (!(other instanceof bytestring)) {
      return false;
    }

    if (size() != ((bytestring) other).size()) {
      return false;
    }
    if (size() == 0) {
      return true;
    }

    if (other instanceof literalbytestring) {
      return equalsrange((literalbytestring) other, 0, size());
    } else if (other instanceof ropebytestring) {
      return other.equals(this);
    } else {
      throw new illegalargumentexception(
          "has a new type of bytestring been created? found "
              + other.getclass());
    }
  }

  /**
   * check equality of the substring of given length of this object starting at
   * zero with another {@code literalbytestring} substring starting at offset.
   *
   * @param other  what to compare a substring in
   * @param offset offset into other
   * @param length number of bytes to compare
   * @return true for equality of substrings, else false.
   */
  boolean equalsrange(literalbytestring other, int offset, int length) {
    if (length > other.size()) {
      throw new illegalargumentexception(
          "length too large: " + length + size());
    }
    if (offset + length > other.size()) {
      throw new illegalargumentexception(
          "ran off end of other: " + offset + ", " + length + ", " +
              other.size());
    }

    byte[] thisbytes = bytes;
    byte[] otherbytes = other.bytes;
    int thislimit = getoffsetintobytes() + length;
    for (int thisindex = getoffsetintobytes(), otherindex =
        other.getoffsetintobytes() + offset;
        (thisindex < thislimit); ++thisindex, ++otherindex) {
      if (thisbytes[thisindex] != otherbytes[otherindex]) {
        return false;
      }
    }
    return true;
  }

  /**
   * cached hash value.  intentionally accessed via a data race, which
   * is safe because of the java memory model's "no out-of-thin-air values"
   * guarantees for ints.
   */
  private int hash = 0;

  /**
   * compute the hashcode using the traditional algorithm from {@link
   * bytestring}.
   *
   * @return hashcode value
   */
  @override
  public int hashcode() {
    int h = hash;

    if (h == 0) {
      int size = size();
      h = partialhash(size, 0, size);
      if (h == 0) {
        h = 1;
      }
      hash = h;
    }
    return h;
  }

  @override
  protected int peekcachedhashcode() {
    return hash;
  }

  @override
  protected int partialhash(int h, int offset, int length) {
    byte[] thisbytes = bytes;
    for (int i = getoffsetintobytes() + offset, limit = i + length; i < limit;
        i++) {
      h = h * 31 + thisbytes[i];
    }
    return h;
  }

  // =================================================================
  // input stream

  @override
  public inputstream newinput() {
    return new bytearrayinputstream(bytes, getoffsetintobytes(),
        size());  // no copy
  }

  @override
  public codedinputstream newcodedinput() {
    // we trust codedinputstream not to modify the bytes, or to give anyone
    // else access to them.
    return codedinputstream
        .newinstance(bytes, getoffsetintobytes(), size());  // no copy
  }

  // =================================================================
  // byteiterator

  @override
  public byteiterator iterator() {
    return new literalbyteiterator();
  }

  private class literalbyteiterator implements byteiterator {
    private int position;
    private final int limit;

    private literalbyteiterator() {
      position = 0;
      limit = size();
    }

    public boolean hasnext() {
      return (position < limit);
    }

    public byte next() {
      // boxing calls byte.valueof(byte), which does not instantiate.
      return nextbyte();
    }

    public byte nextbyte() {
      try {
        return bytes[position++];
      } catch (arrayindexoutofboundsexception e) {
        throw new nosuchelementexception(e.getmessage());
      }
    }

    public void remove() {
      throw new unsupportedoperationexception();
    }
  }

  // =================================================================
  // internal methods

  @override
  protected int gettreedepth() {
    return 0;
  }

  @override
  protected boolean isbalanced() {
    return true;
  }

  /**
   * offset into {@code bytes[]} to use, non-zero for substrings.
   *
   * @return always 0 for this class
   */
  protected int getoffsetintobytes() {
    return 0;
  }
}
