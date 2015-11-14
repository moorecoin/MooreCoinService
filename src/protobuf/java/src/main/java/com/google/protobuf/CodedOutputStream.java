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
import java.io.outputstream;
import java.io.unsupportedencodingexception;

/**
 * encodes and writes protocol message fields.
 *
 * <p>this class contains two kinds of methods:  methods that write specific
 * protocol message constructs and field types (e.g. {@link #writetag} and
 * {@link #writeint32}) and methods that write low-level values (e.g.
 * {@link #writerawvarint32} and {@link #writerawbytes}).  if you are
 * writing encoded protocol messages, you should use the former methods, but if
 * you are writing some other format of your own design, use the latter.
 *
 * <p>this class is totally unsynchronized.
 *
 * @author kneton@google.com kenton varda
 */
public final class codedoutputstream {
  private final byte[] buffer;
  private final int limit;
  private int position;

  private final outputstream output;

  /**
   * the buffer size used in {@link #newinstance(outputstream)}.
   */
  public static final int default_buffer_size = 4096;

  /**
   * returns the buffer size to efficiently write datalength bytes to this
   * codedoutputstream. used by abstractmessagelite.
   *
   * @return the buffer size to efficiently write datalength bytes to this
   *         codedoutputstream.
   */
  static int computepreferredbuffersize(int datalength) {
    if (datalength > default_buffer_size) return default_buffer_size;
    return datalength;
  }

  private codedoutputstream(final byte[] buffer, final int offset,
                            final int length) {
    output = null;
    this.buffer = buffer;
    position = offset;
    limit = offset + length;
  }

  private codedoutputstream(final outputstream output, final byte[] buffer) {
    this.output = output;
    this.buffer = buffer;
    position = 0;
    limit = buffer.length;
  }

  /**
   * create a new {@code codedoutputstream} wrapping the given
   * {@code outputstream}.
   */
  public static codedoutputstream newinstance(final outputstream output) {
    return newinstance(output, default_buffer_size);
  }

  /**
   * create a new {@code codedoutputstream} wrapping the given
   * {@code outputstream} with a given buffer size.
   */
  public static codedoutputstream newinstance(final outputstream output,
      final int buffersize) {
    return new codedoutputstream(output, new byte[buffersize]);
  }

  /**
   * create a new {@code codedoutputstream} that writes directly to the given
   * byte array.  if more bytes are written than fit in the array,
   * {@link outofspaceexception} will be thrown.  writing directly to a flat
   * array is faster than writing to an {@code outputstream}.  see also
   * {@link bytestring#newcodedbuilder}.
   */
  public static codedoutputstream newinstance(final byte[] flatarray) {
    return newinstance(flatarray, 0, flatarray.length);
  }

  /**
   * create a new {@code codedoutputstream} that writes directly to the given
   * byte array slice.  if more bytes are written than fit in the slice,
   * {@link outofspaceexception} will be thrown.  writing directly to a flat
   * array is faster than writing to an {@code outputstream}.  see also
   * {@link bytestring#newcodedbuilder}.
   */
  public static codedoutputstream newinstance(final byte[] flatarray,
                                              final int offset,
                                              final int length) {
    return new codedoutputstream(flatarray, offset, length);
  }

  // -----------------------------------------------------------------

  /** write a {@code double} field, including tag, to the stream. */
  public void writedouble(final int fieldnumber, final double value)
                          throws ioexception {
    writetag(fieldnumber, wireformat.wiretype_fixed64);
    writedoublenotag(value);
  }

  /** write a {@code float} field, including tag, to the stream. */
  public void writefloat(final int fieldnumber, final float value)
                         throws ioexception {
    writetag(fieldnumber, wireformat.wiretype_fixed32);
    writefloatnotag(value);
  }

  /** write a {@code uint64} field, including tag, to the stream. */
  public void writeuint64(final int fieldnumber, final long value)
                          throws ioexception {
    writetag(fieldnumber, wireformat.wiretype_varint);
    writeuint64notag(value);
  }

  /** write an {@code int64} field, including tag, to the stream. */
  public void writeint64(final int fieldnumber, final long value)
                         throws ioexception {
    writetag(fieldnumber, wireformat.wiretype_varint);
    writeint64notag(value);
  }

  /** write an {@code int32} field, including tag, to the stream. */
  public void writeint32(final int fieldnumber, final int value)
                         throws ioexception {
    writetag(fieldnumber, wireformat.wiretype_varint);
    writeint32notag(value);
  }

  /** write a {@code fixed64} field, including tag, to the stream. */
  public void writefixed64(final int fieldnumber, final long value)
                           throws ioexception {
    writetag(fieldnumber, wireformat.wiretype_fixed64);
    writefixed64notag(value);
  }

  /** write a {@code fixed32} field, including tag, to the stream. */
  public void writefixed32(final int fieldnumber, final int value)
                           throws ioexception {
    writetag(fieldnumber, wireformat.wiretype_fixed32);
    writefixed32notag(value);
  }

  /** write a {@code bool} field, including tag, to the stream. */
  public void writebool(final int fieldnumber, final boolean value)
                        throws ioexception {
    writetag(fieldnumber, wireformat.wiretype_varint);
    writeboolnotag(value);
  }

  /** write a {@code string} field, including tag, to the stream. */
  public void writestring(final int fieldnumber, final string value)
                          throws ioexception {
    writetag(fieldnumber, wireformat.wiretype_length_delimited);
    writestringnotag(value);
  }

  /** write a {@code group} field, including tag, to the stream. */
  public void writegroup(final int fieldnumber, final messagelite value)
                         throws ioexception {
    writetag(fieldnumber, wireformat.wiretype_start_group);
    writegroupnotag(value);
    writetag(fieldnumber, wireformat.wiretype_end_group);
  }

  /**
   * write a group represented by an {@link unknownfieldset}.
   *
   * @deprecated unknownfieldset now implements messagelite, so you can just
   *             call {@link #writegroup}.
   */
  @deprecated
  public void writeunknowngroup(final int fieldnumber,
                                final messagelite value)
                                throws ioexception {
    writegroup(fieldnumber, value);
  }

  /** write an embedded message field, including tag, to the stream. */
  public void writemessage(final int fieldnumber, final messagelite value)
                           throws ioexception {
    writetag(fieldnumber, wireformat.wiretype_length_delimited);
    writemessagenotag(value);
  }

  /** write a {@code bytes} field, including tag, to the stream. */
  public void writebytes(final int fieldnumber, final bytestring value)
                         throws ioexception {
    writetag(fieldnumber, wireformat.wiretype_length_delimited);
    writebytesnotag(value);
  }

  /** write a {@code uint32} field, including tag, to the stream. */
  public void writeuint32(final int fieldnumber, final int value)
                          throws ioexception {
    writetag(fieldnumber, wireformat.wiretype_varint);
    writeuint32notag(value);
  }

  /**
   * write an enum field, including tag, to the stream.  caller is responsible
   * for converting the enum value to its numeric value.
   */
  public void writeenum(final int fieldnumber, final int value)
                        throws ioexception {
    writetag(fieldnumber, wireformat.wiretype_varint);
    writeenumnotag(value);
  }

  /** write an {@code sfixed32} field, including tag, to the stream. */
  public void writesfixed32(final int fieldnumber, final int value)
                            throws ioexception {
    writetag(fieldnumber, wireformat.wiretype_fixed32);
    writesfixed32notag(value);
  }

  /** write an {@code sfixed64} field, including tag, to the stream. */
  public void writesfixed64(final int fieldnumber, final long value)
                            throws ioexception {
    writetag(fieldnumber, wireformat.wiretype_fixed64);
    writesfixed64notag(value);
  }

  /** write an {@code sint32} field, including tag, to the stream. */
  public void writesint32(final int fieldnumber, final int value)
                          throws ioexception {
    writetag(fieldnumber, wireformat.wiretype_varint);
    writesint32notag(value);
  }

  /** write an {@code sint64} field, including tag, to the stream. */
  public void writesint64(final int fieldnumber, final long value)
                          throws ioexception {
    writetag(fieldnumber, wireformat.wiretype_varint);
    writesint64notag(value);
  }

  /**
   * write a messageset extension field to the stream.  for historical reasons,
   * the wire format differs from normal fields.
   */
  public void writemessagesetextension(final int fieldnumber,
                                       final messagelite value)
                                       throws ioexception {
    writetag(wireformat.message_set_item, wireformat.wiretype_start_group);
    writeuint32(wireformat.message_set_type_id, fieldnumber);
    writemessage(wireformat.message_set_message, value);
    writetag(wireformat.message_set_item, wireformat.wiretype_end_group);
  }

  /**
   * write an unparsed messageset extension field to the stream.  for
   * historical reasons, the wire format differs from normal fields.
   */
  public void writerawmessagesetextension(final int fieldnumber,
                                          final bytestring value)
                                          throws ioexception {
    writetag(wireformat.message_set_item, wireformat.wiretype_start_group);
    writeuint32(wireformat.message_set_type_id, fieldnumber);
    writebytes(wireformat.message_set_message, value);
    writetag(wireformat.message_set_item, wireformat.wiretype_end_group);
  }

  // -----------------------------------------------------------------

  /** write a {@code double} field to the stream. */
  public void writedoublenotag(final double value) throws ioexception {
    writerawlittleendian64(double.doubletorawlongbits(value));
  }

  /** write a {@code float} field to the stream. */
  public void writefloatnotag(final float value) throws ioexception {
    writerawlittleendian32(float.floattorawintbits(value));
  }

  /** write a {@code uint64} field to the stream. */
  public void writeuint64notag(final long value) throws ioexception {
    writerawvarint64(value);
  }

  /** write an {@code int64} field to the stream. */
  public void writeint64notag(final long value) throws ioexception {
    writerawvarint64(value);
  }

  /** write an {@code int32} field to the stream. */
  public void writeint32notag(final int value) throws ioexception {
    if (value >= 0) {
      writerawvarint32(value);
    } else {
      // must sign-extend.
      writerawvarint64(value);
    }
  }

  /** write a {@code fixed64} field to the stream. */
  public void writefixed64notag(final long value) throws ioexception {
    writerawlittleendian64(value);
  }

  /** write a {@code fixed32} field to the stream. */
  public void writefixed32notag(final int value) throws ioexception {
    writerawlittleendian32(value);
  }

  /** write a {@code bool} field to the stream. */
  public void writeboolnotag(final boolean value) throws ioexception {
    writerawbyte(value ? 1 : 0);
  }

  /** write a {@code string} field to the stream. */
  public void writestringnotag(final string value) throws ioexception {
    // unfortunately there does not appear to be any way to tell java to encode
    // utf-8 directly into our buffer, so we have to let it create its own byte
    // array and then copy.
    final byte[] bytes = value.getbytes("utf-8");
    writerawvarint32(bytes.length);
    writerawbytes(bytes);
  }

  /** write a {@code group} field to the stream. */
  public void writegroupnotag(final messagelite value) throws ioexception {
    value.writeto(this);
  }

  /**
   * write a group represented by an {@link unknownfieldset}.
   *
   * @deprecated unknownfieldset now implements messagelite, so you can just
   *             call {@link #writegroupnotag}.
   */
  @deprecated
  public void writeunknowngroupnotag(final messagelite value)
      throws ioexception {
    writegroupnotag(value);
  }

  /** write an embedded message field to the stream. */
  public void writemessagenotag(final messagelite value) throws ioexception {
    writerawvarint32(value.getserializedsize());
    value.writeto(this);
  }

  /** write a {@code bytes} field to the stream. */
  public void writebytesnotag(final bytestring value) throws ioexception {
    writerawvarint32(value.size());
    writerawbytes(value);
  }

  /** write a {@code uint32} field to the stream. */
  public void writeuint32notag(final int value) throws ioexception {
    writerawvarint32(value);
  }

  /**
   * write an enum field to the stream.  caller is responsible
   * for converting the enum value to its numeric value.
   */
  public void writeenumnotag(final int value) throws ioexception {
    writeint32notag(value);
  }

  /** write an {@code sfixed32} field to the stream. */
  public void writesfixed32notag(final int value) throws ioexception {
    writerawlittleendian32(value);
  }

  /** write an {@code sfixed64} field to the stream. */
  public void writesfixed64notag(final long value) throws ioexception {
    writerawlittleendian64(value);
  }

  /** write an {@code sint32} field to the stream. */
  public void writesint32notag(final int value) throws ioexception {
    writerawvarint32(encodezigzag32(value));
  }

  /** write an {@code sint64} field to the stream. */
  public void writesint64notag(final long value) throws ioexception {
    writerawvarint64(encodezigzag64(value));
  }

  // =================================================================

  /**
   * compute the number of bytes that would be needed to encode a
   * {@code double} field, including tag.
   */
  public static int computedoublesize(final int fieldnumber,
                                      final double value) {
    return computetagsize(fieldnumber) + computedoublesizenotag(value);
  }

  /**
   * compute the number of bytes that would be needed to encode a
   * {@code float} field, including tag.
   */
  public static int computefloatsize(final int fieldnumber, final float value) {
    return computetagsize(fieldnumber) + computefloatsizenotag(value);
  }

  /**
   * compute the number of bytes that would be needed to encode a
   * {@code uint64} field, including tag.
   */
  public static int computeuint64size(final int fieldnumber, final long value) {
    return computetagsize(fieldnumber) + computeuint64sizenotag(value);
  }

  /**
   * compute the number of bytes that would be needed to encode an
   * {@code int64} field, including tag.
   */
  public static int computeint64size(final int fieldnumber, final long value) {
    return computetagsize(fieldnumber) + computeint64sizenotag(value);
  }

  /**
   * compute the number of bytes that would be needed to encode an
   * {@code int32} field, including tag.
   */
  public static int computeint32size(final int fieldnumber, final int value) {
    return computetagsize(fieldnumber) + computeint32sizenotag(value);
  }

  /**
   * compute the number of bytes that would be needed to encode a
   * {@code fixed64} field, including tag.
   */
  public static int computefixed64size(final int fieldnumber,
                                       final long value) {
    return computetagsize(fieldnumber) + computefixed64sizenotag(value);
  }

  /**
   * compute the number of bytes that would be needed to encode a
   * {@code fixed32} field, including tag.
   */
  public static int computefixed32size(final int fieldnumber,
                                       final int value) {
    return computetagsize(fieldnumber) + computefixed32sizenotag(value);
  }

  /**
   * compute the number of bytes that would be needed to encode a
   * {@code bool} field, including tag.
   */
  public static int computeboolsize(final int fieldnumber,
                                    final boolean value) {
    return computetagsize(fieldnumber) + computeboolsizenotag(value);
  }

  /**
   * compute the number of bytes that would be needed to encode a
   * {@code string} field, including tag.
   */
  public static int computestringsize(final int fieldnumber,
                                      final string value) {
    return computetagsize(fieldnumber) + computestringsizenotag(value);
  }

  /**
   * compute the number of bytes that would be needed to encode a
   * {@code group} field, including tag.
   */
  public static int computegroupsize(final int fieldnumber,
                                     final messagelite value) {
    return computetagsize(fieldnumber) * 2 + computegroupsizenotag(value);
  }

  /**
   * compute the number of bytes that would be needed to encode a
   * {@code group} field represented by an {@code unknownfieldset}, including
   * tag.
   *
   * @deprecated unknownfieldset now implements messagelite, so you can just
   *             call {@link #computegroupsize}.
   */
  @deprecated
  public static int computeunknowngroupsize(final int fieldnumber,
                                            final messagelite value) {
    return computegroupsize(fieldnumber, value);
  }

  /**
   * compute the number of bytes that would be needed to encode an
   * embedded message field, including tag.
   */
  public static int computemessagesize(final int fieldnumber,
                                       final messagelite value) {
    return computetagsize(fieldnumber) + computemessagesizenotag(value);
  }

  /**
   * compute the number of bytes that would be needed to encode a
   * {@code bytes} field, including tag.
   */
  public static int computebytessize(final int fieldnumber,
                                     final bytestring value) {
    return computetagsize(fieldnumber) + computebytessizenotag(value);
  }

  /**
   * compute the number of bytes that would be needed to encode an
   * embedded message in lazy field, including tag.
   */
  public static int computelazyfieldsize(final int fieldnumber,
                                         final lazyfield value) {
    return computetagsize(fieldnumber) + computelazyfieldsizenotag(value);
  }

  /**
   * compute the number of bytes that would be needed to encode a
   * {@code uint32} field, including tag.
   */
  public static int computeuint32size(final int fieldnumber, final int value) {
    return computetagsize(fieldnumber) + computeuint32sizenotag(value);
  }

  /**
   * compute the number of bytes that would be needed to encode an
   * enum field, including tag.  caller is responsible for converting the
   * enum value to its numeric value.
   */
  public static int computeenumsize(final int fieldnumber, final int value) {
    return computetagsize(fieldnumber) + computeenumsizenotag(value);
  }

  /**
   * compute the number of bytes that would be needed to encode an
   * {@code sfixed32} field, including tag.
   */
  public static int computesfixed32size(final int fieldnumber,
                                        final int value) {
    return computetagsize(fieldnumber) + computesfixed32sizenotag(value);
  }

  /**
   * compute the number of bytes that would be needed to encode an
   * {@code sfixed64} field, including tag.
   */
  public static int computesfixed64size(final int fieldnumber,
                                        final long value) {
    return computetagsize(fieldnumber) + computesfixed64sizenotag(value);
  }

  /**
   * compute the number of bytes that would be needed to encode an
   * {@code sint32} field, including tag.
   */
  public static int computesint32size(final int fieldnumber, final int value) {
    return computetagsize(fieldnumber) + computesint32sizenotag(value);
  }

  /**
   * compute the number of bytes that would be needed to encode an
   * {@code sint64} field, including tag.
   */
  public static int computesint64size(final int fieldnumber, final long value) {
    return computetagsize(fieldnumber) + computesint64sizenotag(value);
  }

  /**
   * compute the number of bytes that would be needed to encode a
   * messageset extension to the stream.  for historical reasons,
   * the wire format differs from normal fields.
   */
  public static int computemessagesetextensionsize(
      final int fieldnumber, final messagelite value) {
    return computetagsize(wireformat.message_set_item) * 2 +
           computeuint32size(wireformat.message_set_type_id, fieldnumber) +
           computemessagesize(wireformat.message_set_message, value);
  }

  /**
   * compute the number of bytes that would be needed to encode an
   * unparsed messageset extension field to the stream.  for
   * historical reasons, the wire format differs from normal fields.
   */
  public static int computerawmessagesetextensionsize(
      final int fieldnumber, final bytestring value) {
    return computetagsize(wireformat.message_set_item) * 2 +
           computeuint32size(wireformat.message_set_type_id, fieldnumber) +
           computebytessize(wireformat.message_set_message, value);
  }

  /**
   * compute the number of bytes that would be needed to encode an
   * lazily parsed messageset extension field to the stream.  for
   * historical reasons, the wire format differs from normal fields.
   */
  public static int computelazyfieldmessagesetextensionsize(
      final int fieldnumber, final lazyfield value) {
    return computetagsize(wireformat.message_set_item) * 2 +
           computeuint32size(wireformat.message_set_type_id, fieldnumber) +
           computelazyfieldsize(wireformat.message_set_message, value);
  }
  
  // -----------------------------------------------------------------

  /**
   * compute the number of bytes that would be needed to encode a
   * {@code double} field, including tag.
   */
  public static int computedoublesizenotag(final double value) {
    return little_endian_64_size;
  }

  /**
   * compute the number of bytes that would be needed to encode a
   * {@code float} field, including tag.
   */
  public static int computefloatsizenotag(final float value) {
    return little_endian_32_size;
  }

  /**
   * compute the number of bytes that would be needed to encode a
   * {@code uint64} field, including tag.
   */
  public static int computeuint64sizenotag(final long value) {
    return computerawvarint64size(value);
  }

  /**
   * compute the number of bytes that would be needed to encode an
   * {@code int64} field, including tag.
   */
  public static int computeint64sizenotag(final long value) {
    return computerawvarint64size(value);
  }

  /**
   * compute the number of bytes that would be needed to encode an
   * {@code int32} field, including tag.
   */
  public static int computeint32sizenotag(final int value) {
    if (value >= 0) {
      return computerawvarint32size(value);
    } else {
      // must sign-extend.
      return 10;
    }
  }

  /**
   * compute the number of bytes that would be needed to encode a
   * {@code fixed64} field.
   */
  public static int computefixed64sizenotag(final long value) {
    return little_endian_64_size;
  }

  /**
   * compute the number of bytes that would be needed to encode a
   * {@code fixed32} field.
   */
  public static int computefixed32sizenotag(final int value) {
    return little_endian_32_size;
  }

  /**
   * compute the number of bytes that would be needed to encode a
   * {@code bool} field.
   */
  public static int computeboolsizenotag(final boolean value) {
    return 1;
  }

  /**
   * compute the number of bytes that would be needed to encode a
   * {@code string} field.
   */
  public static int computestringsizenotag(final string value) {
    try {
      final byte[] bytes = value.getbytes("utf-8");
      return computerawvarint32size(bytes.length) +
             bytes.length;
    } catch (unsupportedencodingexception e) {
      throw new runtimeexception("utf-8 not supported.", e);
    }
  }

  /**
   * compute the number of bytes that would be needed to encode a
   * {@code group} field.
   */
  public static int computegroupsizenotag(final messagelite value) {
    return value.getserializedsize();
  }

  /**
   * compute the number of bytes that would be needed to encode a
   * {@code group} field represented by an {@code unknownfieldset}, including
   * tag.
   *
   * @deprecated unknownfieldset now implements messagelite, so you can just
   *             call {@link #computeunknowngroupsizenotag}.
   */
  @deprecated
  public static int computeunknowngroupsizenotag(final messagelite value) {
    return computegroupsizenotag(value);
  }

  /**
   * compute the number of bytes that would be needed to encode an embedded
   * message field.
   */
  public static int computemessagesizenotag(final messagelite value) {
    final int size = value.getserializedsize();
    return computerawvarint32size(size) + size;
  }

  /**
   * compute the number of bytes that would be needed to encode an embedded
   * message stored in lazy field.
   */
  public static int computelazyfieldsizenotag(final lazyfield value) {
    final int size = value.getserializedsize();
    return computerawvarint32size(size) + size;
  }

  /**
   * compute the number of bytes that would be needed to encode a
   * {@code bytes} field.
   */
  public static int computebytessizenotag(final bytestring value) {
    return computerawvarint32size(value.size()) +
           value.size();
  }

  /**
   * compute the number of bytes that would be needed to encode a
   * {@code uint32} field.
   */
  public static int computeuint32sizenotag(final int value) {
    return computerawvarint32size(value);
  }

  /**
   * compute the number of bytes that would be needed to encode an enum field.
   * caller is responsible for converting the enum value to its numeric value.
   */
  public static int computeenumsizenotag(final int value) {
    return computeint32sizenotag(value);
  }

  /**
   * compute the number of bytes that would be needed to encode an
   * {@code sfixed32} field.
   */
  public static int computesfixed32sizenotag(final int value) {
    return little_endian_32_size;
  }

  /**
   * compute the number of bytes that would be needed to encode an
   * {@code sfixed64} field.
   */
  public static int computesfixed64sizenotag(final long value) {
    return little_endian_64_size;
  }

  /**
   * compute the number of bytes that would be needed to encode an
   * {@code sint32} field.
   */
  public static int computesint32sizenotag(final int value) {
    return computerawvarint32size(encodezigzag32(value));
  }

  /**
   * compute the number of bytes that would be needed to encode an
   * {@code sint64} field.
   */
  public static int computesint64sizenotag(final long value) {
    return computerawvarint64size(encodezigzag64(value));
  }

  // =================================================================

  /**
   * internal helper that writes the current buffer to the output. the
   * buffer position is reset to its initial value when this returns.
   */
  private void refreshbuffer() throws ioexception {
    if (output == null) {
      // we're writing to a single buffer.
      throw new outofspaceexception();
    }

    // since we have an output stream, this is our buffer
    // and buffer offset == 0
    output.write(buffer, 0, position);
    position = 0;
  }

  /**
   * flushes the stream and forces any buffered bytes to be written.  this
   * does not flush the underlying outputstream.
   */
  public void flush() throws ioexception {
    if (output != null) {
      refreshbuffer();
    }
  }

  /**
   * if writing to a flat array, return the space left in the array.
   * otherwise, throws {@code unsupportedoperationexception}.
   */
  public int spaceleft() {
    if (output == null) {
      return limit - position;
    } else {
      throw new unsupportedoperationexception(
        "spaceleft() can only be called on codedoutputstreams that are " +
        "writing to a flat array.");
    }
  }

  /**
   * verifies that {@link #spaceleft()} returns zero.  it's common to create
   * a byte array that is exactly big enough to hold a message, then write to
   * it with a {@code codedoutputstream}.  calling {@code checknospaceleft()}
   * after writing verifies that the message was actually as big as expected,
   * which can help catch bugs.
   */
  public void checknospaceleft() {
    if (spaceleft() != 0) {
      throw new illegalstateexception(
        "did not write as much data as expected.");
    }
  }

  /**
   * if you create a codedoutputstream around a simple flat array, you must
   * not attempt to write more bytes than the array has space.  otherwise,
   * this exception will be thrown.
   */
  public static class outofspaceexception extends ioexception {
    private static final long serialversionuid = -6947486886997889499l;

    outofspaceexception() {
      super("codedoutputstream was writing to a flat byte array and ran " +
            "out of space.");
    }
  }

  /** write a single byte. */
  public void writerawbyte(final byte value) throws ioexception {
    if (position == limit) {
      refreshbuffer();
    }

    buffer[position++] = value;
  }

  /** write a single byte, represented by an integer value. */
  public void writerawbyte(final int value) throws ioexception {
    writerawbyte((byte) value);
  }

  /** write a byte string. */
  public void writerawbytes(final bytestring value) throws ioexception {
    writerawbytes(value, 0, value.size());
  }

  /** write an array of bytes. */
  public void writerawbytes(final byte[] value) throws ioexception {
    writerawbytes(value, 0, value.length);
  }

  /** write part of an array of bytes. */
  public void writerawbytes(final byte[] value, int offset, int length)
                            throws ioexception {
    if (limit - position >= length) {
      // we have room in the current buffer.
      system.arraycopy(value, offset, buffer, position, length);
      position += length;
    } else {
      // write extends past current buffer.  fill the rest of this buffer and
      // flush.
      final int byteswritten = limit - position;
      system.arraycopy(value, offset, buffer, position, byteswritten);
      offset += byteswritten;
      length -= byteswritten;
      position = limit;
      refreshbuffer();

      // now deal with the rest.
      // since we have an output stream, this is our buffer
      // and buffer offset == 0
      if (length <= limit) {
        // fits in new buffer.
        system.arraycopy(value, offset, buffer, 0, length);
        position = length;
      } else {
        // write is very big.  let's do it all at once.
        output.write(value, offset, length);
      }
    }
  }

  /** write part of a byte string. */
  public void writerawbytes(final bytestring value, int offset, int length)
                            throws ioexception {
    if (limit - position >= length) {
      // we have room in the current buffer.
      value.copyto(buffer, offset, position, length);
      position += length;
    } else {
      // write extends past current buffer.  fill the rest of this buffer and
      // flush.
      final int byteswritten = limit - position;
      value.copyto(buffer, offset, position, byteswritten);
      offset += byteswritten;
      length -= byteswritten;
      position = limit;
      refreshbuffer();

      // now deal with the rest.
      // since we have an output stream, this is our buffer
      // and buffer offset == 0
      if (length <= limit) {
        // fits in new buffer.
        value.copyto(buffer, offset, 0, length);
        position = length;
      } else {
        // write is very big, but we can't do it all at once without allocating
        // an a copy of the byte array since bytestring does not give us access
        // to the underlying bytes. use the inputstream interface on the
        // bytestring and our buffer to copy between the two.
        inputstream inputstreamfrom = value.newinput();
        if (offset != inputstreamfrom.skip(offset)) {
          throw new illegalstateexception("skip failed? should never happen.");
        }
        // use the buffer as the temporary buffer to avoid allocating memory.
        while (length > 0) {
          int bytestoread = math.min(length, limit);
          int bytesread = inputstreamfrom.read(buffer, 0, bytestoread);
          if (bytesread != bytestoread) {
            throw new illegalstateexception("read failed? should never happen");
          }
          output.write(buffer, 0, bytesread);
          length -= bytesread;
        }
      }
    }
  }

  /** encode and write a tag. */
  public void writetag(final int fieldnumber, final int wiretype)
                       throws ioexception {
    writerawvarint32(wireformat.maketag(fieldnumber, wiretype));
  }

  /** compute the number of bytes that would be needed to encode a tag. */
  public static int computetagsize(final int fieldnumber) {
    return computerawvarint32size(wireformat.maketag(fieldnumber, 0));
  }

  /**
   * encode and write a varint.  {@code value} is treated as
   * unsigned, so it won't be sign-extended if negative.
   */
  public void writerawvarint32(int value) throws ioexception {
    while (true) {
      if ((value & ~0x7f) == 0) {
        writerawbyte(value);
        return;
      } else {
        writerawbyte((value & 0x7f) | 0x80);
        value >>>= 7;
      }
    }
  }

  /**
   * compute the number of bytes that would be needed to encode a varint.
   * {@code value} is treated as unsigned, so it won't be sign-extended if
   * negative.
   */
  public static int computerawvarint32size(final int value) {
    if ((value & (0xffffffff <<  7)) == 0) return 1;
    if ((value & (0xffffffff << 14)) == 0) return 2;
    if ((value & (0xffffffff << 21)) == 0) return 3;
    if ((value & (0xffffffff << 28)) == 0) return 4;
    return 5;
  }

  /** encode and write a varint. */
  public void writerawvarint64(long value) throws ioexception {
    while (true) {
      if ((value & ~0x7fl) == 0) {
        writerawbyte((int)value);
        return;
      } else {
        writerawbyte(((int)value & 0x7f) | 0x80);
        value >>>= 7;
      }
    }
  }

  /** compute the number of bytes that would be needed to encode a varint. */
  public static int computerawvarint64size(final long value) {
    if ((value & (0xffffffffffffffffl <<  7)) == 0) return 1;
    if ((value & (0xffffffffffffffffl << 14)) == 0) return 2;
    if ((value & (0xffffffffffffffffl << 21)) == 0) return 3;
    if ((value & (0xffffffffffffffffl << 28)) == 0) return 4;
    if ((value & (0xffffffffffffffffl << 35)) == 0) return 5;
    if ((value & (0xffffffffffffffffl << 42)) == 0) return 6;
    if ((value & (0xffffffffffffffffl << 49)) == 0) return 7;
    if ((value & (0xffffffffffffffffl << 56)) == 0) return 8;
    if ((value & (0xffffffffffffffffl << 63)) == 0) return 9;
    return 10;
  }

  /** write a little-endian 32-bit integer. */
  public void writerawlittleendian32(final int value) throws ioexception {
    writerawbyte((value      ) & 0xff);
    writerawbyte((value >>  8) & 0xff);
    writerawbyte((value >> 16) & 0xff);
    writerawbyte((value >> 24) & 0xff);
  }

  public static final int little_endian_32_size = 4;

  /** write a little-endian 64-bit integer. */
  public void writerawlittleendian64(final long value) throws ioexception {
    writerawbyte((int)(value      ) & 0xff);
    writerawbyte((int)(value >>  8) & 0xff);
    writerawbyte((int)(value >> 16) & 0xff);
    writerawbyte((int)(value >> 24) & 0xff);
    writerawbyte((int)(value >> 32) & 0xff);
    writerawbyte((int)(value >> 40) & 0xff);
    writerawbyte((int)(value >> 48) & 0xff);
    writerawbyte((int)(value >> 56) & 0xff);
  }

  public static final int little_endian_64_size = 8;

  /**
   * encode a zigzag-encoded 32-bit value.  zigzag encodes signed integers
   * into values that can be efficiently encoded with varint.  (otherwise,
   * negative values must be sign-extended to 64 bits to be varint encoded,
   * thus always taking 10 bytes on the wire.)
   *
   * @param n a signed 32-bit integer.
   * @return an unsigned 32-bit integer, stored in a signed int because
   *         java has no explicit unsigned support.
   */
  public static int encodezigzag32(final int n) {
    // note:  the right-shift must be arithmetic
    return (n << 1) ^ (n >> 31);
  }

  /**
   * encode a zigzag-encoded 64-bit value.  zigzag encodes signed integers
   * into values that can be efficiently encoded with varint.  (otherwise,
   * negative values must be sign-extended to 64 bits to be varint encoded,
   * thus always taking 10 bytes on the wire.)
   *
   * @param n a signed 64-bit integer.
   * @return an unsigned 64-bit integer, stored in a signed int because
   *         java has no explicit unsigned support.
   */
  public static long encodezigzag64(final long n) {
    // note:  the right-shift must be arithmetic
    return (n << 1) ^ (n >> 63);
  }
}
