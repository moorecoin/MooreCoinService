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

import java.io.ioexception;
import java.io.inputstream;
import java.util.arraylist;
import java.util.list;

/**
 * reads and decodes protocol message fields.
 *
 * this class contains two kinds of methods:  methods that read specific
 * protocol message constructs and field types (e.g. {@link #readtag()} and
 * {@link #readint32()}) and methods that read low-level values (e.g.
 * {@link #readrawvarint32()} and {@link #readrawbytes}).  if you are reading
 * encoded protocol messages, you should use the former methods, but if you are
 * reading some other format of your own design, use the latter.
 *
 * @author kenton@google.com kenton varda
 */
public final class codedinputstream {
  /**
   * create a new codedinputstream wrapping the given inputstream.
   */
  public static codedinputstream newinstance(final inputstream input) {
    return new codedinputstream(input);
  }

  /**
   * create a new codedinputstream wrapping the given byte array.
   */
  public static codedinputstream newinstance(final byte[] buf) {
    return newinstance(buf, 0, buf.length);
  }

  /**
   * create a new codedinputstream wrapping the given byte array slice.
   */
  public static codedinputstream newinstance(final byte[] buf, final int off,
                                             final int len) {
    codedinputstream result = new codedinputstream(buf, off, len);
    try {
      // some uses of codedinputstream can be more efficient if they know
      // exactly how many bytes are available.  by pushing the end point of the
      // buffer as a limit, we allow them to get this information via
      // getbytesuntillimit().  pushing a limit that we know is at the end of
      // the stream can never hurt, since we can never past that point anyway.
      result.pushlimit(len);
    } catch (invalidprotocolbufferexception ex) {
      // the only reason pushlimit() might throw an exception here is if len
      // is negative. normally pushlimit()'s parameter comes directly off the
      // wire, so it's important to catch exceptions in case of corrupt or
      // malicious data. however, in this case, we expect that len is not a
      // user-supplied value, so we can assume that it being negative indicates
      // a programming error. therefore, throwing an unchecked exception is
      // appropriate.
      throw new illegalargumentexception(ex);
    }
    return result;
  }

  // -----------------------------------------------------------------

  /**
   * attempt to read a field tag, returning zero if we have reached eof.
   * protocol message parsers use this to read tags, since a protocol message
   * may legally end wherever a tag occurs, and zero is not a valid tag number.
   */
  public int readtag() throws ioexception {
    if (isatend()) {
      lasttag = 0;
      return 0;
    }

    lasttag = readrawvarint32();
    if (wireformat.gettagfieldnumber(lasttag) == 0) {
      // if we actually read zero (or any tag number corresponding to field
      // number zero), that's not a valid tag.
      throw invalidprotocolbufferexception.invalidtag();
    }
    return lasttag;
  }

  /**
   * verifies that the last call to readtag() returned the given tag value.
   * this is used to verify that a nested group ended with the correct
   * end tag.
   *
   * @throws invalidprotocolbufferexception {@code value} does not match the
   *                                        last tag.
   */
  public void checklasttagwas(final int value)
                              throws invalidprotocolbufferexception {
    if (lasttag != value) {
      throw invalidprotocolbufferexception.invalidendtag();
    }
  }

  /**
   * reads and discards a single field, given its tag value.
   *
   * @return {@code false} if the tag is an endgroup tag, in which case
   *         nothing is skipped.  otherwise, returns {@code true}.
   */
  public boolean skipfield(final int tag) throws ioexception {
    switch (wireformat.gettagwiretype(tag)) {
      case wireformat.wiretype_varint:
        readint32();
        return true;
      case wireformat.wiretype_fixed64:
        readrawlittleendian64();
        return true;
      case wireformat.wiretype_length_delimited:
        skiprawbytes(readrawvarint32());
        return true;
      case wireformat.wiretype_start_group:
        skipmessage();
        checklasttagwas(
          wireformat.maketag(wireformat.gettagfieldnumber(tag),
                             wireformat.wiretype_end_group));
        return true;
      case wireformat.wiretype_end_group:
        return false;
      case wireformat.wiretype_fixed32:
        readrawlittleendian32();
        return true;
      default:
        throw invalidprotocolbufferexception.invalidwiretype();
    }
  }

  /**
   * reads and discards an entire message.  this will read either until eof
   * or until an endgroup tag, whichever comes first.
   */
  public void skipmessage() throws ioexception {
    while (true) {
      final int tag = readtag();
      if (tag == 0 || !skipfield(tag)) {
        return;
      }
    }
  }

  // -----------------------------------------------------------------

  /** read a {@code double} field value from the stream. */
  public double readdouble() throws ioexception {
    return double.longbitstodouble(readrawlittleendian64());
  }

  /** read a {@code float} field value from the stream. */
  public float readfloat() throws ioexception {
    return float.intbitstofloat(readrawlittleendian32());
  }

  /** read a {@code uint64} field value from the stream. */
  public long readuint64() throws ioexception {
    return readrawvarint64();
  }

  /** read an {@code int64} field value from the stream. */
  public long readint64() throws ioexception {
    return readrawvarint64();
  }

  /** read an {@code int32} field value from the stream. */
  public int readint32() throws ioexception {
    return readrawvarint32();
  }

  /** read a {@code fixed64} field value from the stream. */
  public long readfixed64() throws ioexception {
    return readrawlittleendian64();
  }

  /** read a {@code fixed32} field value from the stream. */
  public int readfixed32() throws ioexception {
    return readrawlittleendian32();
  }

  /** read a {@code bool} field value from the stream. */
  public boolean readbool() throws ioexception {
    return readrawvarint32() != 0;
  }

  /** read a {@code string} field value from the stream. */
  public string readstring() throws ioexception {
    final int size = readrawvarint32();
    if (size <= (buffersize - bufferpos) && size > 0) {
      // fast path:  we already have the bytes in a contiguous buffer, so
      //   just copy directly from it.
      final string result = new string(buffer, bufferpos, size, "utf-8");
      bufferpos += size;
      return result;
    } else {
      // slow path:  build a byte array first then copy it.
      return new string(readrawbytes(size), "utf-8");
    }
  }

  /** read a {@code group} field value from the stream. */
  public void readgroup(final int fieldnumber,
                        final messagelite.builder builder,
                        final extensionregistrylite extensionregistry)
      throws ioexception {
    if (recursiondepth >= recursionlimit) {
      throw invalidprotocolbufferexception.recursionlimitexceeded();
    }
    ++recursiondepth;
    builder.mergefrom(this, extensionregistry);
    checklasttagwas(
      wireformat.maketag(fieldnumber, wireformat.wiretype_end_group));
    --recursiondepth;
  }

  /** read a {@code group} field value from the stream. */
  public <t extends messagelite> t readgroup(
      final int fieldnumber,
      final parser<t> parser,
      final extensionregistrylite extensionregistry)
      throws ioexception {
    if (recursiondepth >= recursionlimit) {
      throw invalidprotocolbufferexception.recursionlimitexceeded();
    }
    ++recursiondepth;
    t result = parser.parsepartialfrom(this, extensionregistry);
    checklasttagwas(
      wireformat.maketag(fieldnumber, wireformat.wiretype_end_group));
    --recursiondepth;
    return result;
  }

  /**
   * reads a {@code group} field value from the stream and merges it into the
   * given {@link unknownfieldset}.
   *
   * @deprecated unknownfieldset.builder now implements messagelite.builder, so
   *             you can just call {@link #readgroup}.
   */
  @deprecated
  public void readunknowngroup(final int fieldnumber,
                               final messagelite.builder builder)
      throws ioexception {
    // we know that unknownfieldset will ignore any extensionregistry so it
    // is safe to pass null here.  (we can't call
    // extensionregistry.getemptyregistry() because that would make this
    // class depend on extensionregistry, which is not part of the lite
    // library.)
    readgroup(fieldnumber, builder, null);
  }

  /** read an embedded message field value from the stream. */
  public void readmessage(final messagelite.builder builder,
                          final extensionregistrylite extensionregistry)
      throws ioexception {
    final int length = readrawvarint32();
    if (recursiondepth >= recursionlimit) {
      throw invalidprotocolbufferexception.recursionlimitexceeded();
    }
    final int oldlimit = pushlimit(length);
    ++recursiondepth;
    builder.mergefrom(this, extensionregistry);
    checklasttagwas(0);
    --recursiondepth;
    poplimit(oldlimit);
  }

  /** read an embedded message field value from the stream. */
  public <t extends messagelite> t readmessage(
      final parser<t> parser,
      final extensionregistrylite extensionregistry)
      throws ioexception {
    int length = readrawvarint32();
    if (recursiondepth >= recursionlimit) {
      throw invalidprotocolbufferexception.recursionlimitexceeded();
    }
    final int oldlimit = pushlimit(length);
    ++recursiondepth;
    t result = parser.parsepartialfrom(this, extensionregistry);
    checklasttagwas(0);
    --recursiondepth;
    poplimit(oldlimit);
    return result;
  }

  /** read a {@code bytes} field value from the stream. */
  public bytestring readbytes() throws ioexception {
    final int size = readrawvarint32();
    if (size == 0) {
      return bytestring.empty;
    } else if (size <= (buffersize - bufferpos) && size > 0) {
      // fast path:  we already have the bytes in a contiguous buffer, so
      //   just copy directly from it.
      final bytestring result = bytestring.copyfrom(buffer, bufferpos, size);
      bufferpos += size;
      return result;
    } else {
      // slow path:  build a byte array first then copy it.
      return bytestring.copyfrom(readrawbytes(size));
    }
  }

  /** read a {@code uint32} field value from the stream. */
  public int readuint32() throws ioexception {
    return readrawvarint32();
  }

  /**
   * read an enum field value from the stream.  caller is responsible
   * for converting the numeric value to an actual enum.
   */
  public int readenum() throws ioexception {
    return readrawvarint32();
  }

  /** read an {@code sfixed32} field value from the stream. */
  public int readsfixed32() throws ioexception {
    return readrawlittleendian32();
  }

  /** read an {@code sfixed64} field value from the stream. */
  public long readsfixed64() throws ioexception {
    return readrawlittleendian64();
  }

  /** read an {@code sint32} field value from the stream. */
  public int readsint32() throws ioexception {
    return decodezigzag32(readrawvarint32());
  }

  /** read an {@code sint64} field value from the stream. */
  public long readsint64() throws ioexception {
    return decodezigzag64(readrawvarint64());
  }

  // =================================================================

  /**
   * read a raw varint from the stream.  if larger than 32 bits, discard the
   * upper bits.
   */
  public int readrawvarint32() throws ioexception {
    byte tmp = readrawbyte();
    if (tmp >= 0) {
      return tmp;
    }
    int result = tmp & 0x7f;
    if ((tmp = readrawbyte()) >= 0) {
      result |= tmp << 7;
    } else {
      result |= (tmp & 0x7f) << 7;
      if ((tmp = readrawbyte()) >= 0) {
        result |= tmp << 14;
      } else {
        result |= (tmp & 0x7f) << 14;
        if ((tmp = readrawbyte()) >= 0) {
          result |= tmp << 21;
        } else {
          result |= (tmp & 0x7f) << 21;
          result |= (tmp = readrawbyte()) << 28;
          if (tmp < 0) {
            // discard upper 32 bits.
            for (int i = 0; i < 5; i++) {
              if (readrawbyte() >= 0) {
                return result;
              }
            }
            throw invalidprotocolbufferexception.malformedvarint();
          }
        }
      }
    }
    return result;
  }

  /**
   * reads a varint from the input one byte at a time, so that it does not
   * read any bytes after the end of the varint.  if you simply wrapped the
   * stream in a codedinputstream and used {@link #readrawvarint32(inputstream)}
   * then you would probably end up reading past the end of the varint since
   * codedinputstream buffers its input.
   */
  static int readrawvarint32(final inputstream input) throws ioexception {
    final int firstbyte = input.read();
    if (firstbyte == -1) {
      throw invalidprotocolbufferexception.truncatedmessage();
    }
    return readrawvarint32(firstbyte, input);
  }

  /**
   * like {@link #readrawvarint32(inputstream)}, but expects that the caller
   * has already read one byte.  this allows the caller to determine if eof
   * has been reached before attempting to read.
   */
  public static int readrawvarint32(
      final int firstbyte, final inputstream input) throws ioexception {
    if ((firstbyte & 0x80) == 0) {
      return firstbyte;
    }

    int result = firstbyte & 0x7f;
    int offset = 7;
    for (; offset < 32; offset += 7) {
      final int b = input.read();
      if (b == -1) {
        throw invalidprotocolbufferexception.truncatedmessage();
      }
      result |= (b & 0x7f) << offset;
      if ((b & 0x80) == 0) {
        return result;
      }
    }
    // keep reading up to 64 bits.
    for (; offset < 64; offset += 7) {
      final int b = input.read();
      if (b == -1) {
        throw invalidprotocolbufferexception.truncatedmessage();
      }
      if ((b & 0x80) == 0) {
        return result;
      }
    }
    throw invalidprotocolbufferexception.malformedvarint();
  }

  /** read a raw varint from the stream. */
  public long readrawvarint64() throws ioexception {
    int shift = 0;
    long result = 0;
    while (shift < 64) {
      final byte b = readrawbyte();
      result |= (long)(b & 0x7f) << shift;
      if ((b & 0x80) == 0) {
        return result;
      }
      shift += 7;
    }
    throw invalidprotocolbufferexception.malformedvarint();
  }

  /** read a 32-bit little-endian integer from the stream. */
  public int readrawlittleendian32() throws ioexception {
    final byte b1 = readrawbyte();
    final byte b2 = readrawbyte();
    final byte b3 = readrawbyte();
    final byte b4 = readrawbyte();
    return (((int)b1 & 0xff)      ) |
           (((int)b2 & 0xff) <<  8) |
           (((int)b3 & 0xff) << 16) |
           (((int)b4 & 0xff) << 24);
  }

  /** read a 64-bit little-endian integer from the stream. */
  public long readrawlittleendian64() throws ioexception {
    final byte b1 = readrawbyte();
    final byte b2 = readrawbyte();
    final byte b3 = readrawbyte();
    final byte b4 = readrawbyte();
    final byte b5 = readrawbyte();
    final byte b6 = readrawbyte();
    final byte b7 = readrawbyte();
    final byte b8 = readrawbyte();
    return (((long)b1 & 0xff)      ) |
           (((long)b2 & 0xff) <<  8) |
           (((long)b3 & 0xff) << 16) |
           (((long)b4 & 0xff) << 24) |
           (((long)b5 & 0xff) << 32) |
           (((long)b6 & 0xff) << 40) |
           (((long)b7 & 0xff) << 48) |
           (((long)b8 & 0xff) << 56);
  }

  /**
   * decode a zigzag-encoded 32-bit value.  zigzag encodes signed integers
   * into values that can be efficiently encoded with varint.  (otherwise,
   * negative values must be sign-extended to 64 bits to be varint encoded,
   * thus always taking 10 bytes on the wire.)
   *
   * @param n an unsigned 32-bit integer, stored in a signed int because
   *          java has no explicit unsigned support.
   * @return a signed 32-bit integer.
   */
  public static int decodezigzag32(final int n) {
    return (n >>> 1) ^ -(n & 1);
  }

  /**
   * decode a zigzag-encoded 64-bit value.  zigzag encodes signed integers
   * into values that can be efficiently encoded with varint.  (otherwise,
   * negative values must be sign-extended to 64 bits to be varint encoded,
   * thus always taking 10 bytes on the wire.)
   *
   * @param n an unsigned 64-bit integer, stored in a signed int because
   *          java has no explicit unsigned support.
   * @return a signed 64-bit integer.
   */
  public static long decodezigzag64(final long n) {
    return (n >>> 1) ^ -(n & 1);
  }

  // -----------------------------------------------------------------

  private final byte[] buffer;
  private int buffersize;
  private int buffersizeafterlimit;
  private int bufferpos;
  private final inputstream input;
  private int lasttag;

  /**
   * the total number of bytes read before the current buffer.  the total
   * bytes read up to the current position can be computed as
   * {@code totalbytesretired + bufferpos}.  this value may be negative if
   * reading started in the middle of the current buffer (e.g. if the
   * constructor that takes a byte array and an offset was used).
   */
  private int totalbytesretired;

  /** the absolute position of the end of the current message. */
  private int currentlimit = integer.max_value;

  /** see setrecursionlimit() */
  private int recursiondepth;
  private int recursionlimit = default_recursion_limit;

  /** see setsizelimit() */
  private int sizelimit = default_size_limit;

  private static final int default_recursion_limit = 64;
  private static final int default_size_limit = 64 << 20;  // 64mb
  private static final int buffer_size = 4096;

  private codedinputstream(final byte[] buffer, final int off, final int len) {
    this.buffer = buffer;
    buffersize = off + len;
    bufferpos = off;
    totalbytesretired = -off;
    input = null;
  }

  private codedinputstream(final inputstream input) {
    buffer = new byte[buffer_size];
    buffersize = 0;
    bufferpos = 0;
    totalbytesretired = 0;
    this.input = input;
  }

  /**
   * set the maximum message recursion depth.  in order to prevent malicious
   * messages from causing stack overflows, {@code codedinputstream} limits
   * how deeply messages may be nested.  the default limit is 64.
   *
   * @return the old limit.
   */
  public int setrecursionlimit(final int limit) {
    if (limit < 0) {
      throw new illegalargumentexception(
        "recursion limit cannot be negative: " + limit);
    }
    final int oldlimit = recursionlimit;
    recursionlimit = limit;
    return oldlimit;
  }

  /**
   * set the maximum message size.  in order to prevent malicious
   * messages from exhausting memory or causing integer overflows,
   * {@code codedinputstream} limits how large a message may be.
   * the default limit is 64mb.  you should set this limit as small
   * as you can without harming your app's functionality.  note that
   * size limits only apply when reading from an {@code inputstream}, not
   * when constructed around a raw byte array (nor with
   * {@link bytestring#newcodedinput}).
   * <p>
   * if you want to read several messages from a single codedinputstream, you
   * could call {@link #resetsizecounter()} after each one to avoid hitting the
   * size limit.
   *
   * @return the old limit.
   */
  public int setsizelimit(final int limit) {
    if (limit < 0) {
      throw new illegalargumentexception(
        "size limit cannot be negative: " + limit);
    }
    final int oldlimit = sizelimit;
    sizelimit = limit;
    return oldlimit;
  }

  /**
   * resets the current size counter to zero (see {@link #setsizelimit(int)}).
   */
  public void resetsizecounter() {
    totalbytesretired = -bufferpos;
  }

  /**
   * sets {@code currentlimit} to (current position) + {@code bytelimit}.  this
   * is called when descending into a length-delimited embedded message.
   *
   * <p>note that {@code pushlimit()} does not affect how many bytes the
   * {@code codedinputstream} reads from an underlying {@code inputstream} when
   * refreshing its buffer.  if you need to prevent reading past a certain
   * point in the underlying {@code inputstream} (e.g. because you expect it to
   * contain more data after the end of the message which you need to handle
   * differently) then you must place a wrapper around your {@code inputstream}
   * which limits the amount of data that can be read from it.
   *
   * @return the old limit.
   */
  public int pushlimit(int bytelimit) throws invalidprotocolbufferexception {
    if (bytelimit < 0) {
      throw invalidprotocolbufferexception.negativesize();
    }
    bytelimit += totalbytesretired + bufferpos;
    final int oldlimit = currentlimit;
    if (bytelimit > oldlimit) {
      throw invalidprotocolbufferexception.truncatedmessage();
    }
    currentlimit = bytelimit;

    recomputebuffersizeafterlimit();

    return oldlimit;
  }

  private void recomputebuffersizeafterlimit() {
    buffersize += buffersizeafterlimit;
    final int bufferend = totalbytesretired + buffersize;
    if (bufferend > currentlimit) {
      // limit is in current buffer.
      buffersizeafterlimit = bufferend - currentlimit;
      buffersize -= buffersizeafterlimit;
    } else {
      buffersizeafterlimit = 0;
    }
  }

  /**
   * discards the current limit, returning to the previous limit.
   *
   * @param oldlimit the old limit, as returned by {@code pushlimit}.
   */
  public void poplimit(final int oldlimit) {
    currentlimit = oldlimit;
    recomputebuffersizeafterlimit();
  }

  /**
   * returns the number of bytes to be read before the current limit.
   * if no limit is set, returns -1.
   */
  public int getbytesuntillimit() {
    if (currentlimit == integer.max_value) {
      return -1;
    }

    final int currentabsoluteposition = totalbytesretired + bufferpos;
    return currentlimit - currentabsoluteposition;
  }

  /**
   * returns true if the stream has reached the end of the input.  this is the
   * case if either the end of the underlying input source has been reached or
   * if the stream has reached a limit created using {@link #pushlimit(int)}.
   */
  public boolean isatend() throws ioexception {
    return bufferpos == buffersize && !refillbuffer(false);
  }

  /**
   * the total bytes read up to the current position. calling
   * {@link #resetsizecounter()} resets this value to zero.
   */
  public int gettotalbytesread() {
      return totalbytesretired + bufferpos;
  }

  /**
   * called with {@code this.buffer} is empty to read more bytes from the
   * input.  if {@code mustsucceed} is true, refillbuffer() guarantees that
   * either there will be at least one byte in the buffer when it returns
   * or it will throw an exception.  if {@code mustsucceed} is false,
   * refillbuffer() returns false if no more bytes were available.
   */
  private boolean refillbuffer(final boolean mustsucceed) throws ioexception {
    if (bufferpos < buffersize) {
      throw new illegalstateexception(
        "refillbuffer() called when buffer wasn't empty.");
    }

    if (totalbytesretired + buffersize == currentlimit) {
      // oops, we hit a limit.
      if (mustsucceed) {
        throw invalidprotocolbufferexception.truncatedmessage();
      } else {
        return false;
      }
    }

    totalbytesretired += buffersize;

    bufferpos = 0;
    buffersize = (input == null) ? -1 : input.read(buffer);
    if (buffersize == 0 || buffersize < -1) {
      throw new illegalstateexception(
          "inputstream#read(byte[]) returned invalid result: " + buffersize +
          "\nthe inputstream implementation is buggy.");
    }
    if (buffersize == -1) {
      buffersize = 0;
      if (mustsucceed) {
        throw invalidprotocolbufferexception.truncatedmessage();
      } else {
        return false;
      }
    } else {
      recomputebuffersizeafterlimit();
      final int totalbytesread =
        totalbytesretired + buffersize + buffersizeafterlimit;
      if (totalbytesread > sizelimit || totalbytesread < 0) {
        throw invalidprotocolbufferexception.sizelimitexceeded();
      }
      return true;
    }
  }

  /**
   * read one byte from the input.
   *
   * @throws invalidprotocolbufferexception the end of the stream or the current
   *                                        limit was reached.
   */
  public byte readrawbyte() throws ioexception {
    if (bufferpos == buffersize) {
      refillbuffer(true);
    }
    return buffer[bufferpos++];
  }

  /**
   * read a fixed size of bytes from the input.
   *
   * @throws invalidprotocolbufferexception the end of the stream or the current
   *                                        limit was reached.
   */
  public byte[] readrawbytes(final int size) throws ioexception {
    if (size < 0) {
      throw invalidprotocolbufferexception.negativesize();
    }

    if (totalbytesretired + bufferpos + size > currentlimit) {
      // read to the end of the stream anyway.
      skiprawbytes(currentlimit - totalbytesretired - bufferpos);
      // then fail.
      throw invalidprotocolbufferexception.truncatedmessage();
    }

    if (size <= buffersize - bufferpos) {
      // we have all the bytes we need already.
      final byte[] bytes = new byte[size];
      system.arraycopy(buffer, bufferpos, bytes, 0, size);
      bufferpos += size;
      return bytes;
    } else if (size < buffer_size) {
      // reading more bytes than are in the buffer, but not an excessive number
      // of bytes.  we can safely allocate the resulting array ahead of time.

      // first copy what we have.
      final byte[] bytes = new byte[size];
      int pos = buffersize - bufferpos;
      system.arraycopy(buffer, bufferpos, bytes, 0, pos);
      bufferpos = buffersize;

      // we want to use refillbuffer() and then copy from the buffer into our
      // byte array rather than reading directly into our byte array because
      // the input may be unbuffered.
      refillbuffer(true);

      while (size - pos > buffersize) {
        system.arraycopy(buffer, 0, bytes, pos, buffersize);
        pos += buffersize;
        bufferpos = buffersize;
        refillbuffer(true);
      }

      system.arraycopy(buffer, 0, bytes, pos, size - pos);
      bufferpos = size - pos;

      return bytes;
    } else {
      // the size is very large.  for security reasons, we can't allocate the
      // entire byte array yet.  the size comes directly from the input, so a
      // maliciously-crafted message could provide a bogus very large size in
      // order to trick the app into allocating a lot of memory.  we avoid this
      // by allocating and reading only a small chunk at a time, so that the
      // malicious message must actually *be* extremely large to cause
      // problems.  meanwhile, we limit the allowed size of a message elsewhere.

      // remember the buffer markers since we'll have to copy the bytes out of
      // it later.
      final int originalbufferpos = bufferpos;
      final int originalbuffersize = buffersize;

      // mark the current buffer consumed.
      totalbytesretired += buffersize;
      bufferpos = 0;
      buffersize = 0;

      // read all the rest of the bytes we need.
      int sizeleft = size - (originalbuffersize - originalbufferpos);
      final list<byte[]> chunks = new arraylist<byte[]>();

      while (sizeleft > 0) {
        final byte[] chunk = new byte[math.min(sizeleft, buffer_size)];
        int pos = 0;
        while (pos < chunk.length) {
          final int n = (input == null) ? -1 :
            input.read(chunk, pos, chunk.length - pos);
          if (n == -1) {
            throw invalidprotocolbufferexception.truncatedmessage();
          }
          totalbytesretired += n;
          pos += n;
        }
        sizeleft -= chunk.length;
        chunks.add(chunk);
      }

      // ok, got everything.  now concatenate it all into one buffer.
      final byte[] bytes = new byte[size];

      // start by copying the leftover bytes from this.buffer.
      int pos = originalbuffersize - originalbufferpos;
      system.arraycopy(buffer, originalbufferpos, bytes, 0, pos);

      // and now all the chunks.
      for (final byte[] chunk : chunks) {
        system.arraycopy(chunk, 0, bytes, pos, chunk.length);
        pos += chunk.length;
      }

      // done.
      return bytes;
    }
  }

  /**
   * reads and discards {@code size} bytes.
   *
   * @throws invalidprotocolbufferexception the end of the stream or the current
   *                                        limit was reached.
   */
  public void skiprawbytes(final int size) throws ioexception {
    if (size < 0) {
      throw invalidprotocolbufferexception.negativesize();
    }

    if (totalbytesretired + bufferpos + size > currentlimit) {
      // read to the end of the stream anyway.
      skiprawbytes(currentlimit - totalbytesretired - bufferpos);
      // then fail.
      throw invalidprotocolbufferexception.truncatedmessage();
    }

    if (size <= buffersize - bufferpos) {
      // we have all the bytes we need already.
      bufferpos += size;
    } else {
      // skipping more bytes than are in the buffer.  first skip what we have.
      int pos = buffersize - bufferpos;
      bufferpos = buffersize;

      // keep refilling the buffer until we get to the point we wanted to skip
      // to.  this has the side effect of ensuring the limits are updated
      // correctly.
      refillbuffer(true);
      while (size - pos > buffersize) {
        pos += buffersize;
        bufferpos = buffersize;
        refillbuffer(true);
      }

      bufferpos = size - pos;
    }
  }
}
