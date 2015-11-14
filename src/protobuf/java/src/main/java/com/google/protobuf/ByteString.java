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

import java.io.bytearrayoutputstream;
import java.io.ioexception;
import java.io.inputstream;
import java.io.outputstream;
import java.io.unsupportedencodingexception;
import java.nio.bytebuffer;
import java.util.arraylist;
import java.util.collection;
import java.util.iterator;
import java.util.list;
import java.util.nosuchelementexception;

/**
 * immutable sequence of bytes.  substring is supported by sharing the reference
 * to the immutable underlying bytes, as with {@link string}.  concatenation is
 * likewise supported without copying (long strings) by building a tree of
 * pieces in {@link ropebytestring}.
 * <p>
 * like {@link string}, the contents of a {@link bytestring} can never be
 * observed to change, not even in the presence of a data race or incorrect
 * api usage in the client code.
 *
 * @author crazybob@google.com bob lee
 * @author kenton@google.com kenton varda
 * @author carlanton@google.com carl haverl
 * @author martinrb@google.com martin buchholz
 */
public abstract class bytestring implements iterable<byte> {

  /**
   * when two strings to be concatenated have a combined length shorter than
   * this, we just copy their bytes on {@link #concat(bytestring)}.
   * the trade-off is copy size versus the overhead of creating tree nodes
   * in {@link ropebytestring}.
   */
  static final int concatenate_by_copy_size = 128;

  /**
   * when copying an inputstream into a bytestring with .readfrom(),
   * the chunks in the underlying rope start at 256 bytes, but double
   * each iteration up to 8192 bytes.
   */
  static final int min_read_from_chunk_size = 0x100;  // 256b
  static final int max_read_from_chunk_size = 0x2000;  // 8k

  /**
   * empty {@code bytestring}.
   */
  public static final bytestring empty = new literalbytestring(new byte[0]);

  // this constructor is here to prevent subclassing outside of this package,
  bytestring() {}

  /**
   * gets the byte at the given index. this method should be used only for
   * random access to individual bytes. to access bytes sequentially, use the
   * {@link byteiterator} returned by {@link #iterator()}, and call {@link
   * #substring(int, int)} first if necessary.
   *
   * @param index index of byte
   * @return the value
   * @throws arrayindexoutofboundsexception {@code index} is < 0 or >= size
   */
  public abstract byte byteat(int index);

  /**
   * return a {@link bytestring.byteiterator} over the bytes in the bytestring.
   * to avoid auto-boxing, you may get the iterator manually and call
   * {@link byteiterator#nextbyte()}.
   *
   * @return the iterator
   */
  public abstract byteiterator iterator();

  /**
   * this interface extends {@code iterator<byte>}, so that we can return an
   * unboxed {@code byte}.
   */
  public interface byteiterator extends iterator<byte> {
    /**
     * an alternative to {@link iterator#next()} that returns an
     * unboxed primitive {@code byte}.
     *
     * @return the next {@code byte} in the iteration
     * @throws nosuchelementexception if the iteration has no more elements
     */
    byte nextbyte();
  }

  /**
   * gets the number of bytes.
   *
   * @return size in bytes
   */
  public abstract int size();

  /**
   * returns {@code true} if the size is {@code 0}, {@code false} otherwise.
   *
   * @return true if this is zero bytes long
   */
  public boolean isempty() {
    return size() == 0;
  }

  // =================================================================
  // bytestring -> substring

  /**
   * return the substring from {@code beginindex}, inclusive, to the end of the
   * string.
   *
   * @param beginindex start at this index
   * @return substring sharing underlying data
   * @throws indexoutofboundsexception if {@code beginindex < 0} or
   *     {@code beginindex > size()}.
   */
  public bytestring substring(int beginindex) {
    return substring(beginindex, size());
  }

  /**
   * return the substring from {@code beginindex}, inclusive, to {@code
   * endindex}, exclusive.
   *
   * @param beginindex start at this index
   * @param endindex   the last character is the one before this index
   * @return substring sharing underlying data
   * @throws indexoutofboundsexception if {@code beginindex < 0},
   *     {@code endindex > size()}, or {@code beginindex > endindex}.
   */
  public abstract bytestring substring(int beginindex, int endindex);

  /**
   * tests if this bytestring starts with the specified prefix.
   * similar to {@link string#startswith(string)}
   *
   * @param prefix the prefix.
   * @return <code>true</code> if the byte sequence represented by the
   *         argument is a prefix of the byte sequence represented by
   *         this string; <code>false</code> otherwise.
   */
  public boolean startswith(bytestring prefix) {
    return size() >= prefix.size() &&
           substring(0, prefix.size()).equals(prefix);
  }

  // =================================================================
  // byte[] -> bytestring

  /**
   * copies the given bytes into a {@code bytestring}.
   *
   * @param bytes source array
   * @param offset offset in source array
   * @param size number of bytes to copy
   * @return new {@code bytestring}
   */
  public static bytestring copyfrom(byte[] bytes, int offset, int size) {
    byte[] copy = new byte[size];
    system.arraycopy(bytes, offset, copy, 0, size);
    return new literalbytestring(copy);
  }

  /**
   * copies the given bytes into a {@code bytestring}.
   *
   * @param bytes to copy
   * @return new {@code bytestring}
   */
  public static bytestring copyfrom(byte[] bytes) {
    return copyfrom(bytes, 0, bytes.length);
  }

  /**
   * copies the next {@code size} bytes from a {@code java.nio.bytebuffer} into
   * a {@code bytestring}.
   *
   * @param bytes source buffer
   * @param size number of bytes to copy
   * @return new {@code bytestring}
   */
  public static bytestring copyfrom(bytebuffer bytes, int size) {
    byte[] copy = new byte[size];
    bytes.get(copy);
    return new literalbytestring(copy);
  }

  /**
   * copies the remaining bytes from a {@code java.nio.bytebuffer} into
   * a {@code bytestring}.
   *
   * @param bytes sourcebuffer
   * @return new {@code bytestring}
   */
  public static bytestring copyfrom(bytebuffer bytes) {
    return copyfrom(bytes, bytes.remaining());
  }

  /**
   * encodes {@code text} into a sequence of bytes using the named charset
   * and returns the result as a {@code bytestring}.
   *
   * @param text source string
   * @param charsetname encoding to use
   * @return new {@code bytestring}
   * @throws unsupportedencodingexception if the encoding isn't found
   */
  public static bytestring copyfrom(string text, string charsetname)
      throws unsupportedencodingexception {
    return new literalbytestring(text.getbytes(charsetname));
  }

  /**
   * encodes {@code text} into a sequence of utf-8 bytes and returns the
   * result as a {@code bytestring}.
   *
   * @param text source string
   * @return new {@code bytestring}
   */
  public static bytestring copyfromutf8(string text) {
    try {
      return new literalbytestring(text.getbytes("utf-8"));
    } catch (unsupportedencodingexception e) {
      throw new runtimeexception("utf-8 not supported?", e);
    }
  }

  // =================================================================
  // inputstream -> bytestring

  /**
   * completely reads the given stream's bytes into a
   * {@code bytestring}, blocking if necessary until all bytes are
   * read through to the end of the stream.
   *
   * <b>performance notes:</b> the returned {@code bytestring} is an
   * immutable tree of byte arrays ("chunks") of the stream data.  the
   * first chunk is small, with subsequent chunks each being double
   * the size, up to 8k.  if the caller knows the precise length of
   * the stream and wishes to avoid all unnecessary copies and
   * allocations, consider using the two-argument version of this
   * method, below.
   *
   * @param streamtodrain the source stream, which is read completely
   *     but not closed.
   * @return a new {@code bytestring} which is made up of chunks of
   *     various sizes, depending on the behavior of the underlying
   *     stream.
   * @throws ioexception ioexception is thrown if there is a problem
   *     reading the underlying stream.
   */
  public static bytestring readfrom(inputstream streamtodrain)
      throws ioexception {
    return readfrom(
        streamtodrain, min_read_from_chunk_size, max_read_from_chunk_size);
  }

  /**
   * completely reads the given stream's bytes into a
   * {@code bytestring}, blocking if necessary until all bytes are
   * read through to the end of the stream.
   *
   * <b>performance notes:</b> the returned {@code bytestring} is an
   * immutable tree of byte arrays ("chunks") of the stream data.  the
   * chunksize parameter sets the size of these byte arrays. in
   * particular, if the chunksize is precisely the same as the length
   * of the stream, unnecessary allocations and copies will be
   * avoided. otherwise, the chunks will be of the given size, except
   * for the last chunk, which will be resized (via a reallocation and
   * copy) to contain the remainder of the stream.
   *
   * @param streamtodrain the source stream, which is read completely
   *     but not closed.
   * @param chunksize the size of the chunks in which to read the
   *     stream.
   * @return a new {@code bytestring} which is made up of chunks of
   *     the given size.
   * @throws ioexception ioexception is thrown if there is a problem
   *     reading the underlying stream.
   */
  public static bytestring readfrom(inputstream streamtodrain, int chunksize)
      throws ioexception {
    return readfrom(streamtodrain, chunksize, chunksize);
  }

  // helper method that takes the chunk size range as a parameter.
  public static bytestring readfrom(inputstream streamtodrain, int minchunksize,
      int maxchunksize) throws ioexception {
    collection<bytestring> results = new arraylist<bytestring>();

    // copy the inbound bytes into a list of chunks; the chunk size
    // grows exponentially to support both short and long streams.
    int chunksize = minchunksize;
    while (true) {
      bytestring chunk = readchunk(streamtodrain, chunksize);
      if (chunk == null) {
        break;
      }
      results.add(chunk);
      chunksize = math.min(chunksize * 2, maxchunksize);
    }

    return bytestring.copyfrom(results);
  }

  /**
   * blocks until a chunk of the given size can be made from the
   * stream, or eof is reached.  calls read() repeatedly in case the
   * given stream implementation doesn't completely fill the given
   * buffer in one read() call.
   *
   * @return a chunk of the desired size, or else a chunk as large as
   * was available when end of stream was reached. returns null if the
   * given stream had no more data in it.
   */
  private static bytestring readchunk(inputstream in, final int chunksize)
      throws ioexception {
      final byte[] buf = new byte[chunksize];
      int bytesread = 0;
      while (bytesread < chunksize) {
        final int count = in.read(buf, bytesread, chunksize - bytesread);
        if (count == -1) {
          break;
        }
        bytesread += count;
      }

      if (bytesread == 0) {
        return null;
      } else {
        return bytestring.copyfrom(buf, 0, bytesread);
      }
  }

  // =================================================================
  // multiple bytestrings -> one bytestring

  /**
   * concatenate the given {@code bytestring} to this one. short concatenations,
   * of total size smaller than {@link bytestring#concatenate_by_copy_size}, are
   * produced by copying the underlying bytes (as per rope.java, <a
   * href="http://www.cs.ubc.ca/local/reading/proceedings/spe91-95/spe/vol25/issue12/spe986.pdf">
   * bap95 </a>. in general, the concatenate involves no copying.
   *
   * @param other string to concatenate
   * @return a new {@code bytestring} instance
   */
  public bytestring concat(bytestring other) {
    int thissize = size();
    int othersize = other.size();
    if ((long) thissize + othersize >= integer.max_value) {
      throw new illegalargumentexception("bytestring would be too long: " +
                                         thissize + "+" + othersize);
    }

    return ropebytestring.concatenate(this, other);
  }

  /**
   * concatenates all byte strings in the iterable and returns the result.
   * this is designed to run in o(list size), not o(total bytes).
   *
   * <p>the returned {@code bytestring} is not necessarily a unique object.
   * if the list is empty, the returned object is the singleton empty
   * {@code bytestring}.  if the list has only one element, that
   * {@code bytestring} will be returned without copying.
   *
   * @param bytestrings strings to be concatenated
   * @return new {@code bytestring}
   */
  public static bytestring copyfrom(iterable<bytestring> bytestrings) {
    collection<bytestring> collection;
    if (!(bytestrings instanceof collection)) {
      collection = new arraylist<bytestring>();
      for (bytestring bytestring : bytestrings) {
        collection.add(bytestring);
      }
    } else {
      collection = (collection<bytestring>) bytestrings;
    }
    bytestring result;
    if (collection.isempty()) {
      result = empty;
    } else {
      result = balancedconcat(collection.iterator(), collection.size());
    }
    return result;
  }

  // internal function used by copyfrom(iterable<bytestring>).
  // create a balanced concatenation of the next "length" elements from the
  // iterable.
  private static bytestring balancedconcat(iterator<bytestring> iterator,
      int length) {
    assert length >= 1;
    bytestring result;
    if (length == 1) {
      result = iterator.next();
    } else {
      int halflength = length >>> 1;
      bytestring left = balancedconcat(iterator, halflength);
      bytestring right = balancedconcat(iterator, length - halflength);
      result = left.concat(right);
    }
    return result;
  }

  // =================================================================
  // bytestring -> byte[]

  /**
   * copies bytes into a buffer at the given offset.
   *
   * @param target buffer to copy into
   * @param offset in the target buffer
   * @throws indexoutofboundsexception if the offset is negative or too large
   */
  public void copyto(byte[] target, int offset) {
    copyto(target, 0, offset, size());
  }

  /**
   * copies bytes into a buffer.
   *
   * @param target       buffer to copy into
   * @param sourceoffset offset within these bytes
   * @param targetoffset offset within the target buffer
   * @param numbertocopy number of bytes to copy
   * @throws indexoutofboundsexception if an offset or size is negative or too
   *     large
   */
  public void copyto(byte[] target, int sourceoffset, int targetoffset,
      int numbertocopy) {
    if (sourceoffset < 0) {
      throw new indexoutofboundsexception("source offset < 0: " + sourceoffset);
    }
    if (targetoffset < 0) {
      throw new indexoutofboundsexception("target offset < 0: " + targetoffset);
    }
    if (numbertocopy < 0) {
      throw new indexoutofboundsexception("length < 0: " + numbertocopy);
    }
    if (sourceoffset + numbertocopy > size()) {
      throw new indexoutofboundsexception(
          "source end offset < 0: " + (sourceoffset + numbertocopy));
    }
    if (targetoffset + numbertocopy > target.length) {
      throw new indexoutofboundsexception(
          "target end offset < 0: " + (targetoffset + numbertocopy));
    }
    if (numbertocopy > 0) {
      copytointernal(target, sourceoffset, targetoffset, numbertocopy);
    }
  }

  /**
   * internal (package private) implementation of
   * @link{#copyto(byte[],int,int,int}.
   * it assumes that all error checking has already been performed and that 
   * @code{numbertocopy > 0}.
   */
  protected abstract void copytointernal(byte[] target, int sourceoffset,
      int targetoffset, int numbertocopy);

  /**
   * copies bytes into a bytebuffer.
   *
   * @param target bytebuffer to copy into.
   * @throws java.nio.readonlybufferexception if the {@code target} is read-only
   * @throws java.nio.bufferoverflowexception if the {@code target}'s
   *     remaining() space is not large enough to hold the data.
   */
  public abstract void copyto(bytebuffer target);

  /**
   * copies bytes to a {@code byte[]}.
   *
   * @return copied bytes
   */
  public byte[] tobytearray() {
    int size = size();
    byte[] result = new byte[size];
    copytointernal(result, 0, 0, size);
    return result;
  }

  /**
   * writes the complete contents of this byte string to
   * the specified output stream argument.
   *
   * @param  out  the output stream to which to write the data.
   * @throws ioexception  if an i/o error occurs.
   */
  public abstract void writeto(outputstream out) throws ioexception;

  /**
   * constructs a read-only {@code java.nio.bytebuffer} whose content
   * is equal to the contents of this byte string.
   * the result uses the same backing array as the byte string, if possible.
   *
   * @return wrapped bytes
   */
  public abstract bytebuffer asreadonlybytebuffer();

  /**
   * constructs a list of read-only {@code java.nio.bytebuffer} objects
   * such that the concatenation of their contents is equal to the contents
   * of this byte string.  the result uses the same backing arrays as the
   * byte string.
   * <p>
   * by returning a list, implementations of this method may be able to avoid
   * copying even when there are multiple backing arrays.
   * 
   * @return a list of wrapped bytes
   */
  public abstract list<bytebuffer> asreadonlybytebufferlist();

  /**
   * constructs a new {@code string} by decoding the bytes using the
   * specified charset.
   *
   * @param charsetname encode using this charset
   * @return new string
   * @throws unsupportedencodingexception if charset isn't recognized
   */
  public abstract string tostring(string charsetname)
      throws unsupportedencodingexception;

  // =================================================================
  // utf-8 decoding

  /**
   * constructs a new {@code string} by decoding the bytes as utf-8.
   *
   * @return new string using utf-8 encoding
   */
  public string tostringutf8() {
    try {
      return tostring("utf-8");
    } catch (unsupportedencodingexception e) {
      throw new runtimeexception("utf-8 not supported?", e);
    }
  }

  /**
   * tells whether this {@code bytestring} represents a well-formed utf-8
   * byte sequence, such that the original bytes can be converted to a
   * string object and then round tripped back to bytes without loss.
   *
   * <p>more precisely, returns {@code true} whenever: <pre> {@code
   * arrays.equals(bytestring.tobytearray(),
   *     new string(bytestring.tobytearray(), "utf-8").getbytes("utf-8"))
   * }</pre>
   *
   * <p>this method returns {@code false} for "overlong" byte sequences,
   * as well as for 3-byte sequences that would map to a surrogate
   * character, in accordance with the restricted definition of utf-8
   * introduced in unicode 3.1.  note that the utf-8 decoder included in
   * oracle's jdk has been modified to also reject "overlong" byte
   * sequences, but (as of 2011) still accepts 3-byte surrogate
   * character byte sequences.
   *
   * <p>see the unicode standard,</br>
   * table 3-6. <em>utf-8 bit distribution</em>,</br>
   * table 3-7. <em>well formed utf-8 byte sequences</em>.
   *
   * @return whether the bytes in this {@code bytestring} are a
   * well-formed utf-8 byte sequence
   */
  public abstract boolean isvalidutf8();

  /**
   * tells whether the given byte sequence is a well-formed, malformed, or
   * incomplete utf-8 byte sequence.  this method accepts and returns a partial
   * state result, allowing the bytes for a complete utf-8 byte sequence to be
   * composed from multiple {@code bytestring} segments.
   *
   * @param state either {@code 0} (if this is the initial decoding operation)
   *     or the value returned from a call to a partial decoding method for the
   *     previous bytes
   * @param offset offset of the first byte to check
   * @param length number of bytes to check
   *
   * @return {@code -1} if the partial byte sequence is definitely malformed,
   * {@code 0} if it is well-formed (no additional input needed), or, if the
   * byte sequence is "incomplete", i.e. apparently terminated in the middle of
   * a character, an opaque integer "state" value containing enough information
   * to decode the character when passed to a subsequent invocation of a
   * partial decoding method.
   */
  protected abstract int partialisvalidutf8(int state, int offset, int length);

  // =================================================================
  // equals() and hashcode()

  @override
  public abstract boolean equals(object o);

  /**
   * return a non-zero hashcode depending only on the sequence of bytes
   * in this bytestring.
   *
   * @return hashcode value for this object
   */
  @override
  public abstract int hashcode();

  // =================================================================
  // input stream

  /**
   * creates an {@code inputstream} which can be used to read the bytes.
   * <p>
   * the {@link inputstream} returned by this method is guaranteed to be
   * completely non-blocking.  the method {@link inputstream#available()}
   * returns the number of bytes remaining in the stream. the methods
   * {@link inputstream#read(byte[]), {@link inputstream#read(byte[],int,int)}
   * and {@link inputstream#skip(long)} will read/skip as many bytes as are
   * available.
   * <p>
   * the methods in the returned {@link inputstream} might <b>not</b> be
   * thread safe.
   *
   * @return an input stream that returns the bytes of this byte string.
   */
  public abstract inputstream newinput();

  /**
   * creates a {@link codedinputstream} which can be used to read the bytes.
   * using this is often more efficient than creating a {@link codedinputstream}
   * that wraps the result of {@link #newinput()}.
   *
   * @return stream based on wrapped data
   */
  public abstract codedinputstream newcodedinput();

  // =================================================================
  // output stream

  /**
   * creates a new {@link output} with the given initial capacity. call {@link
   * output#tobytestring()} to create the {@code bytestring} instance.
   * <p>
   * a {@link bytestring.output} offers the same functionality as a
   * {@link bytearrayoutputstream}, except that it returns a {@link bytestring}
   * rather than a {@code byte} array.
   *
   * @param initialcapacity estimate of number of bytes to be written
   * @return {@code outputstream} for building a {@code bytestring}
   */
  public static output newoutput(int initialcapacity) {
    return new output(initialcapacity);
  }

  /**
   * creates a new {@link output}. call {@link output#tobytestring()} to create
   * the {@code bytestring} instance.
   * <p>
   * a {@link bytestring.output} offers the same functionality as a
   * {@link bytearrayoutputstream}, except that it returns a {@link bytestring}
   * rather than a {@code byte array}.
   *
   * @return {@code outputstream} for building a {@code bytestring}
   */
  public static output newoutput() {
    return new output(concatenate_by_copy_size);
  }

  /**
   * outputs to a {@code bytestring} instance. call {@link #tobytestring()} to
   * create the {@code bytestring} instance.
   */
  public static final class output extends outputstream {
    // implementation note.
    // the public methods of this class must be synchronized.  bytestrings
    // are guaranteed to be immutable.  without some sort of locking, it could
    // be possible for one thread to call tobytesring(), while another thread
    // is still modifying the underlying byte array.

    private static final byte[] empty_byte_array = new byte[0];
    // argument passed by user, indicating initial capacity.
    private final int initialcapacity;
    // bytestrings to be concatenated to create the result
    private final arraylist<bytestring> flushedbuffers;
    // total number of bytes in the bytestrings of flushedbuffers
    private int flushedbufferstotalbytes;
    // current buffer to which we are writing
    private byte[] buffer;
    // location in buffer[] to which we write the next byte.
    private int bufferpos;

    /**
     * creates a new bytestring output stream with the specified
     * initial capacity.
     *
     * @param initialcapacity  the initial capacity of the output stream.
     */
    output(int initialcapacity) {
      if (initialcapacity < 0) {
        throw new illegalargumentexception("buffer size < 0");
      }
      this.initialcapacity = initialcapacity;
      this.flushedbuffers = new arraylist<bytestring>();
      this.buffer = new byte[initialcapacity];
    }

    @override
    public synchronized void write(int b) {
      if (bufferpos == buffer.length) {
        flushfullbuffer(1);
      }
      buffer[bufferpos++] = (byte)b;
    }

    @override
    public synchronized void write(byte[] b, int offset, int length)  {
      if (length <= buffer.length - bufferpos) {
        // the bytes can fit into the current buffer.
        system.arraycopy(b, offset, buffer, bufferpos, length);
        bufferpos += length;
      } else {
        // use up the current buffer
        int copysize  = buffer.length - bufferpos;
        system.arraycopy(b, offset, buffer, bufferpos, copysize);
        offset += copysize;
        length -= copysize;
        // flush the buffer, and get a new buffer at least big enough to cover
        // what we still need to output
        flushfullbuffer(length);
        system.arraycopy(b, offset, buffer, 0 /* count */, length);
        bufferpos = length;
      }
    }

    /**
     * creates a byte string. its size is the current size of this output
     * stream and its output has been copied to it.
     *
     * @return  the current contents of this output stream, as a byte string.
     */
    public synchronized bytestring tobytestring() {
      flushlastbuffer();
      return bytestring.copyfrom(flushedbuffers);
    }
    
    /**
     * implement java.util.arrays.copyof() for jdk 1.5.
     */
    private byte[] copyarray(byte[] buffer, int length) {
      byte[] result = new byte[length];
      system.arraycopy(buffer, 0, result, 0, math.min(buffer.length, length));
      return result;
    }

    /**
     * writes the complete contents of this byte array output stream to
     * the specified output stream argument.
     *
     * @param out the output stream to which to write the data.
     * @throws ioexception  if an i/o error occurs.
     */
    public void writeto(outputstream out) throws ioexception {
      bytestring[] cachedflushbuffers;
      byte[] cachedbuffer;
      int cachedbufferpos;
      synchronized (this) {
        // copy the information we need into local variables so as to hold
        // the lock for as short a time as possible.
        cachedflushbuffers =
            flushedbuffers.toarray(new bytestring[flushedbuffers.size()]);
        cachedbuffer = buffer;
        cachedbufferpos = bufferpos;
      }
      for (bytestring bytestring : cachedflushbuffers) {
        bytestring.writeto(out);
      }

      out.write(copyarray(cachedbuffer, cachedbufferpos));
    }

    /**
     * returns the current size of the output stream.
     *
     * @return  the current size of the output stream
     */
    public synchronized int size() {
      return flushedbufferstotalbytes + bufferpos;
    }

    /**
     * resets this stream, so that all currently accumulated output in the
     * output stream is discarded. the output stream can be used again,
     * reusing the already allocated buffer space.
     */
    public synchronized void reset() {
      flushedbuffers.clear();
      flushedbufferstotalbytes = 0;
      bufferpos = 0;
    }

    @override
    public string tostring() {
      return string.format("<bytestring.output@%s size=%d>",
          integer.tohexstring(system.identityhashcode(this)), size());
    }

    /**
     * internal function used by writers.  the current buffer is full, and the
     * writer needs a new buffer whose size is at least the specified minimum
     * size.
     */
    private void flushfullbuffer(int minsize)  {
      flushedbuffers.add(new literalbytestring(buffer));
      flushedbufferstotalbytes += buffer.length;
      // we want to increase our total capacity by 50%, but as a minimum,
      // the new buffer should also at least be >= minsize and
      // >= initial capacity.
      int newsize = math.max(initialcapacity,
          math.max(minsize, flushedbufferstotalbytes >>> 1));
      buffer = new byte[newsize];
      bufferpos = 0;
    }

    /**
     * internal function used by {@link #tobytestring()}. the current buffer may
     * or may not be full, but it needs to be flushed.
     */
    private void flushlastbuffer()  {
      if (bufferpos < buffer.length) {
        if (bufferpos > 0) {
          byte[] buffercopy = copyarray(buffer, bufferpos);
          flushedbuffers.add(new literalbytestring(buffercopy));
        }
        // we reuse this buffer for further writes.
      } else {
        // buffer is completely full.  huzzah.
        flushedbuffers.add(new literalbytestring(buffer));
        // 99% of the time, we're not going to use this outputstream again.
        // we set buffer to an empty byte stream so that we're handling this
        // case without wasting space.  in the rare case that more writes
        // *do* occur, this empty buffer will be flushed and an appropriately
        // sized new buffer will be created.
        buffer = empty_byte_array;
      }
      flushedbufferstotalbytes += bufferpos;
      bufferpos = 0;
    }
  }

  /**
   * constructs a new {@code bytestring} builder, which allows you to
   * efficiently construct a {@code bytestring} by writing to a {@link
   * codedoutputstream}. using this is much more efficient than calling {@code
   * newoutput()} and wrapping that in a {@code codedoutputstream}.
   *
   * <p>this is package-private because it's a somewhat confusing interface.
   * users can call {@link message#tobytestring()} instead of calling this
   * directly.
   *
   * @param size the target byte size of the {@code bytestring}.  you must write
   *     exactly this many bytes before building the result.
   * @return the builder
   */
  static codedbuilder newcodedbuilder(int size) {
    return new codedbuilder(size);
  }

  /** see {@link bytestring#newcodedbuilder(int)}. */
  static final class codedbuilder {
    private final codedoutputstream output;
    private final byte[] buffer;

    private codedbuilder(int size) {
      buffer = new byte[size];
      output = codedoutputstream.newinstance(buffer);
    }

    public bytestring build() {
      output.checknospaceleft();

      // we can be confident that the codedoutputstream will not modify the
      // underlying bytes anymore because it already wrote all of them.  so,
      // no need to make a copy.
      return new literalbytestring(buffer);
    }

    public codedoutputstream getcodedoutput() {
      return output;
    }
  }

  // =================================================================
  // methods {@link ropebytestring} needs on instances, which aren't part of the
  // public api.

  /**
   * return the depth of the tree representing this {@code bytestring}, if any,
   * whose root is this node. if this is a leaf node, return 0.
   *
   * @return tree depth or zero
   */
  protected abstract int gettreedepth();

  /**
   * return {@code true} if this bytestring is literal (a leaf node) or a
   * flat-enough tree in the sense of {@link ropebytestring}.
   *
   * @return true if the tree is flat enough
   */
  protected abstract boolean isbalanced();

  /**
   * return the cached hash code if available.
   *
   * @return value of cached hash code or 0 if not computed yet
   */
  protected abstract int peekcachedhashcode();

  /**
   * compute the hash across the value bytes starting with the given hash, and
   * return the result.  this is used to compute the hash across strings
   * represented as a set of pieces by allowing the hash computation to be
   * continued from piece to piece.
   *
   * @param h starting hash value
   * @param offset offset into this value to start looking at data values
   * @param length number of data values to include in the hash computation
   * @return ending hash value
   */
  protected abstract int partialhash(int h, int offset, int length);

  @override
  public string tostring() {
    return string.format("<bytestring@%s size=%d>",
        integer.tohexstring(system.identityhashcode(this)), size());
  }
}
