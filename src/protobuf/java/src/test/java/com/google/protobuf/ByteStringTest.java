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

import com.google.protobuf.bytestring.output;

import junit.framework.testcase;

import java.io.bytearrayinputstream;
import java.io.ioexception;
import java.io.inputstream;
import java.io.outputstream;
import java.io.unsupportedencodingexception;
import java.nio.bytebuffer;
import java.util.arraylist;
import java.util.arrays;
import java.util.iterator;
import java.util.list;
import java.util.nosuchelementexception;
import java.util.random;

/**
 * test methods with implementations in {@link bytestring}, plus do some top-level "integration"
 * tests.
 *
 * @author carlanton@google.com (carl haverl)
 */
public class bytestringtest extends testcase {

  private static final string utf_16 = "utf-16";

  static byte[] gettestbytes(int size, long seed) {
    random random = new random(seed);
    byte[] result = new byte[size];
    random.nextbytes(result);
    return result;
  }

  private byte[] gettestbytes(int size) {
    return gettestbytes(size, 445566l);
  }

  private byte[] gettestbytes() {
    return gettestbytes(1000);
  }

  // compare the entire left array with a subset of the right array.
  private boolean isarrayrange(byte[] left, byte[] right, int rightoffset, int length) {
    boolean stillequal = (left.length == length);
    for (int i = 0; (stillequal && i < length); ++i) {
      stillequal = (left[i] == right[rightoffset + i]);
    }
    return stillequal;
  }

  // returns true only if the given two arrays have identical contents.
  private boolean isarray(byte[] left, byte[] right) {
    return left.length == right.length && isarrayrange(left, right, 0, left.length);
  }

  public void testsubstring_beginindex() {
    byte[] bytes = gettestbytes();
    bytestring substring = bytestring.copyfrom(bytes).substring(500);
    asserttrue("substring must contain the tail of the string",
        isarrayrange(substring.tobytearray(), bytes, 500, bytes.length - 500));
  }

  public void testcopyfrom_bytesoffsetsize() {
    byte[] bytes = gettestbytes();
    bytestring bytestring = bytestring.copyfrom(bytes, 500, 200);
    asserttrue("copyfrom sub-range must contain the expected bytes",
        isarrayrange(bytestring.tobytearray(), bytes, 500, 200));
  }

  public void testcopyfrom_bytes() {
    byte[] bytes = gettestbytes();
    bytestring bytestring = bytestring.copyfrom(bytes);
    asserttrue("copyfrom must contain the expected bytes",
        isarray(bytestring.tobytearray(), bytes));
  }

  public void testcopyfrom_bytebuffersize() {
    byte[] bytes = gettestbytes();
    bytebuffer bytebuffer = bytebuffer.allocate(bytes.length);
    bytebuffer.put(bytes);
    bytebuffer.position(500);
    bytestring bytestring = bytestring.copyfrom(bytebuffer, 200);
    asserttrue("copyfrom bytebuffer sub-range must contain the expected bytes",
        isarrayrange(bytestring.tobytearray(), bytes, 500, 200));
  }

  public void testcopyfrom_bytebuffer() {
    byte[] bytes = gettestbytes();
    bytebuffer bytebuffer = bytebuffer.allocate(bytes.length);
    bytebuffer.put(bytes);
    bytebuffer.position(500);
    bytestring bytestring = bytestring.copyfrom(bytebuffer);
    asserttrue("copyfrom bytebuffer sub-range must contain the expected bytes",
        isarrayrange(bytestring.tobytearray(), bytes, 500, bytes.length - 500));
  }

  public void testcopyfrom_stringencoding() throws unsupportedencodingexception {
    string teststring = "i love unicode \u1234\u5678 characters";
    bytestring bytestring = bytestring.copyfrom(teststring, utf_16);
    byte[] testbytes = teststring.getbytes(utf_16);
    asserttrue("copyfrom string must respect the charset",
        isarrayrange(bytestring.tobytearray(), testbytes, 0, testbytes.length));
  }

  public void testcopyfrom_utf8() throws unsupportedencodingexception {
    string teststring = "i love unicode \u1234\u5678 characters";
    bytestring bytestring = bytestring.copyfromutf8(teststring);
    byte[] testbytes = teststring.getbytes("utf-8");
    asserttrue("copyfromutf8 string must respect the charset",
        isarrayrange(bytestring.tobytearray(), testbytes, 0, testbytes.length));
  }

  public void testcopyfrom_iterable() {
    byte[] testbytes = gettestbytes(77777, 113344l);
    final list<bytestring> pieces = makeconcretepieces(testbytes);
    // call copyfrom() on a collection
    bytestring bytestring = bytestring.copyfrom(pieces);
    asserttrue("copyfrom a list must contain the expected bytes",
        isarrayrange(bytestring.tobytearray(), testbytes, 0, testbytes.length));
    // call copyfrom on an iteration that's not a collection
    bytestring bytestringalt = bytestring.copyfrom(new iterable<bytestring>() {
      public iterator<bytestring> iterator() {
        return pieces.iterator();
      }
    });
    assertequals("copyfrom from an iteration must contain the expected bytes",
        bytestring, bytestringalt);
  }

  public void testcopyto_targetoffset() {
    byte[] bytes = gettestbytes();
    bytestring bytestring = bytestring.copyfrom(bytes);
    byte[] target = new byte[bytes.length + 1000];
    bytestring.copyto(target, 400);
    asserttrue("copyfrom bytebuffer sub-range must contain the expected bytes",
        isarrayrange(bytes, target, 400, bytes.length));
  }

  public void testreadfrom_emptystream() throws ioexception {
    bytestring bytestring =
        bytestring.readfrom(new bytearrayinputstream(new byte[0]));
    assertsame("reading an empty stream must result in the empty constant "
        + "byte string", bytestring.empty, bytestring);
  }

  public void testreadfrom_smallstream() throws ioexception {
    assertreadfrom(gettestbytes(10));
  }

  public void testreadfrom_mutating() throws ioexception {
    byte[] capturedarray = null;
    evilinputstream eis = new evilinputstream();
    bytestring bytestring = bytestring.readfrom(eis);

    capturedarray = eis.capturedarray;
    byte[] originalvalue = bytestring.tobytearray();
    for (int x = 0; x < capturedarray.length; ++x) {
      capturedarray[x] = (byte) 0;
    }

    byte[] newvalue = bytestring.tobytearray();
    asserttrue("copyfrom bytebuffer must not grant access to underlying array",
        arrays.equals(originalvalue, newvalue));
  }

  // tests sizes that are near the rope copy-out threshold.
  public void testreadfrom_mediumstream() throws ioexception {
    assertreadfrom(gettestbytes(bytestring.concatenate_by_copy_size - 1));
    assertreadfrom(gettestbytes(bytestring.concatenate_by_copy_size));
    assertreadfrom(gettestbytes(bytestring.concatenate_by_copy_size + 1));
    assertreadfrom(gettestbytes(200));
  }

  // tests sizes that are over multi-segment rope threshold.
  public void testreadfrom_largestream() throws ioexception {
    assertreadfrom(gettestbytes(0x100));
    assertreadfrom(gettestbytes(0x101));
    assertreadfrom(gettestbytes(0x110));
    assertreadfrom(gettestbytes(0x1000));
    assertreadfrom(gettestbytes(0x1001));
    assertreadfrom(gettestbytes(0x1010));
    assertreadfrom(gettestbytes(0x10000));
    assertreadfrom(gettestbytes(0x10001));
    assertreadfrom(gettestbytes(0x10010));
  }

  // tests sizes that are near the read buffer size.
  public void testreadfrom_byteboundaries() throws ioexception {
    final int min = bytestring.min_read_from_chunk_size;
    final int max = bytestring.max_read_from_chunk_size;

    assertreadfrom(gettestbytes(min - 1));
    assertreadfrom(gettestbytes(min));
    assertreadfrom(gettestbytes(min + 1));

    assertreadfrom(gettestbytes(min * 2 - 1));
    assertreadfrom(gettestbytes(min * 2));
    assertreadfrom(gettestbytes(min * 2 + 1));

    assertreadfrom(gettestbytes(min * 4 - 1));
    assertreadfrom(gettestbytes(min * 4));
    assertreadfrom(gettestbytes(min * 4 + 1));

    assertreadfrom(gettestbytes(min * 8 - 1));
    assertreadfrom(gettestbytes(min * 8));
    assertreadfrom(gettestbytes(min * 8 + 1));

    assertreadfrom(gettestbytes(max - 1));
    assertreadfrom(gettestbytes(max));
    assertreadfrom(gettestbytes(max + 1));

    assertreadfrom(gettestbytes(max * 2 - 1));
    assertreadfrom(gettestbytes(max * 2));
    assertreadfrom(gettestbytes(max * 2 + 1));
  }

  // tests that ioexceptions propagate through bytestring.readfrom().
  public void testreadfrom_ioexceptions() {
    try {
      bytestring.readfrom(new failstream());
      fail("readfrom must throw the underlying ioexception");

    } catch (ioexception e) {
      assertequals("readfrom must throw the expected exception",
                   "synthetic failure", e.getmessage());
    }
  }

  // tests that bytestring.readfrom works with streams that don't
  // always fill their buffers.
  public void testreadfrom_reluctantstream() throws ioexception {
    final byte[] data = gettestbytes(0x1000);

    bytestring bytestring = bytestring.readfrom(new reluctantstream(data));
    asserttrue("readfrom byte stream must contain the expected bytes",
        isarray(bytestring.tobytearray(), data));

    // same test as above, but with some specific chunk sizes.
    assertreadfromreluctantstream(data, 100);
    assertreadfromreluctantstream(data, 248);
    assertreadfromreluctantstream(data, 249);
    assertreadfromreluctantstream(data, 250);
    assertreadfromreluctantstream(data, 251);
    assertreadfromreluctantstream(data, 0x1000);
    assertreadfromreluctantstream(data, 0x1001);
  }

  // fails unless bytestring.readfrom reads the bytes correctly from a
  // reluctant stream with the given chunksize parameter.
  private void assertreadfromreluctantstream(byte[] bytes, int chunksize)
      throws ioexception {
    bytestring b = bytestring.readfrom(new reluctantstream(bytes), chunksize);
    asserttrue("readfrom byte stream must contain the expected bytes",
        isarray(b.tobytearray(), bytes));
  }

  // tests that bytestring.readfrom works with streams that implement
  // available().
  public void testreadfrom_available() throws ioexception {
    final byte[] data = gettestbytes(0x1001);

    bytestring bytestring = bytestring.readfrom(new availablestream(data));
    asserttrue("readfrom byte stream must contain the expected bytes",
        isarray(bytestring.tobytearray(), data));
  }

  // fails unless bytestring.readfrom reads the bytes correctly.
  private void assertreadfrom(byte[] bytes) throws ioexception {
    bytestring bytestring =
        bytestring.readfrom(new bytearrayinputstream(bytes));
    asserttrue("readfrom byte stream must contain the expected bytes",
        isarray(bytestring.tobytearray(), bytes));
  }

  // a stream that fails when read.
  private static final class failstream extends inputstream {
    @override public int read() throws ioexception {
      throw new ioexception("synthetic failure");
    }
  }

  // a stream that simulates blocking by only producing 250 characters
  // per call to read(byte[]).
  private static class reluctantstream extends inputstream {
    protected final byte[] data;
    protected int pos = 0;

    public reluctantstream(byte[] data) {
      this.data = data;
    }

    @override public int read() {
      if (pos == data.length) {
        return -1;
      } else {
        return data[pos++];
      }
    }

    @override public int read(byte[] buf) {
      return read(buf, 0, buf.length);
    }

    @override public int read(byte[] buf, int offset, int size) {
      if (pos == data.length) {
        return -1;
      }
      int count = math.min(math.min(size, data.length - pos), 250);
      system.arraycopy(data, pos, buf, offset, count);
      pos += count;
      return count;
    }
  }

  // same as above, but also implements available().
  private static final class availablestream extends reluctantstream {
    public availablestream(byte[] data) {
      super(data);
    }

    @override public int available() {
      return math.min(250, data.length - pos);
    }
  }

  // a stream which exposes the byte array passed into read(byte[], int, int).
  private static class evilinputstream extends inputstream {
    public byte[] capturedarray = null;

    @override
    public int read(byte[] buf, int off, int len) {
      if (capturedarray != null) {
        return -1;
      } else {
        capturedarray = buf;
        for (int x = 0; x < len; ++x) {
          buf[x] = (byte) x;
        }
        return len;
      }
    }

    @override
    public int read() {
      // purposefully do nothing.
      return -1;
    }
  }
  
  // a stream which exposes the byte array passed into write(byte[], int, int).
  private static class eviloutputstream extends outputstream {
    public byte[] capturedarray = null;

    @override
    public void write(byte[] buf, int off, int len) {
      if (capturedarray == null) {
        capturedarray = buf;
      }
    }

    @override
    public void write(int ignored) {
      // purposefully do nothing.
    }
  }

  public void testtostringutf8() throws unsupportedencodingexception {
    string teststring = "i love unicode \u1234\u5678 characters";
    byte[] testbytes = teststring.getbytes("utf-8");
    bytestring bytestring = bytestring.copyfrom(testbytes);
    assertequals("copytostringutf8 must respect the charset",
        teststring, bytestring.tostringutf8());
  }

  public void testnewoutput_initialcapacity() throws ioexception {
    byte[] bytes = gettestbytes();
    bytestring.output output = bytestring.newoutput(bytes.length + 100);
    output.write(bytes);
    bytestring bytestring = output.tobytestring();
    asserttrue(
        "string built from newoutput(int) must contain the expected bytes",
        isarrayrange(bytes, bytestring.tobytearray(), 0, bytes.length));
  }

  // test newoutput() using a variety of buffer sizes and a variety of (fixed)
  // write sizes
  public void testnewoutput_arraywrite() throws ioexception {
    byte[] bytes = gettestbytes();
    int length = bytes.length;
    int[] buffersizes = {128, 256, length / 2, length - 1, length, length + 1,
                         2 * length, 3 * length};
    int[] writesizes = {1, 4, 5, 7, 23, bytes.length};

    for (int buffersize : buffersizes) {
      for (int writesize : writesizes) {
        // test writing the entire output writesize bytes at a time.
        bytestring.output output = bytestring.newoutput(buffersize);
        for (int i = 0; i < length; i += writesize) {
          output.write(bytes, i, math.min(writesize, length - i));
        }
        bytestring bytestring = output.tobytestring();
        asserttrue("string built from newoutput() must contain the expected bytes",
            isarrayrange(bytes, bytestring.tobytearray(), 0, bytes.length));
      }
    }
  }

  // test newoutput() using a variety of buffer sizes, but writing all the
  // characters using write(byte);
  public void testnewoutput_writechar() throws ioexception {
    byte[] bytes = gettestbytes();
    int length = bytes.length;
    int[] buffersizes = {0, 1, 128, 256, length / 2,
                         length - 1, length, length + 1,
                         2 * length, 3 * length};
    for (int buffersize : buffersizes) {
      bytestring.output output = bytestring.newoutput(buffersize);
      for (byte bytevalue : bytes) {
        output.write(bytevalue);
      }
      bytestring bytestring = output.tobytestring();
      asserttrue("string built from newoutput() must contain the expected bytes",
          isarrayrange(bytes, bytestring.tobytearray(), 0, bytes.length));
    }
  }

  // test newoutput() in which we write the bytes using a variety of methods
  // and sizes, and in which we repeatedly call tobytestring() in the middle.
  public void testnewoutput_mixed() throws ioexception {
    random rng = new random(1);
    byte[] bytes = gettestbytes();
    int length = bytes.length;
    int[] buffersizes = {0, 1, 128, 256, length / 2,
                         length - 1, length, length + 1,
                         2 * length, 3 * length};

    for (int buffersize : buffersizes) {
      // test writing the entire output using a mixture of write sizes and
      // methods;
      bytestring.output output = bytestring.newoutput(buffersize);
      int position = 0;
      while (position < bytes.length) {
        if (rng.nextboolean()) {
          int count = 1 + rng.nextint(bytes.length - position);
          output.write(bytes, position, count);
          position += count;
        } else {
          output.write(bytes[position]);
          position++;
        }
        assertequals("size() returns the right value", position, output.size());
        asserttrue("newoutput() substring must have correct bytes",
            isarrayrange(output.tobytestring().tobytearray(),
                bytes, 0, position));
      }
      bytestring bytestring = output.tobytestring();
      asserttrue("string built from newoutput() must contain the expected bytes",
          isarrayrange(bytes, bytestring.tobytearray(), 0, bytes.length));
    }
  }
  
  public void testnewoutputempty() throws ioexception {
    // make sure newoutput() correctly builds empty byte strings
    bytestring bytestring = bytestring.newoutput().tobytestring();
    assertequals(bytestring.empty, bytestring);
  }
  
  public void testnewoutput_mutating() throws ioexception {
    output os = bytestring.newoutput(5);
    os.write(new byte[] {1, 2, 3, 4, 5});
    eviloutputstream eos = new eviloutputstream();
    os.writeto(eos);
    byte[] capturedarray = eos.capturedarray;
    bytestring bytestring = os.tobytestring();
    byte[] oldvalue = bytestring.tobytearray();
    arrays.fill(capturedarray, (byte) 0);
    byte[] newvalue = bytestring.tobytearray();
    asserttrue("output must not provide access to the underlying byte array",
        arrays.equals(oldvalue, newvalue));
  }

  public void testnewcodedbuilder() throws ioexception {
    byte[] bytes = gettestbytes();
    bytestring.codedbuilder builder = bytestring.newcodedbuilder(bytes.length);
    builder.getcodedoutput().writerawbytes(bytes);
    bytestring bytestring = builder.build();
    asserttrue("string built from newcodedbuilder() must contain the expected bytes",
        isarrayrange(bytes, bytestring.tobytearray(), 0, bytes.length));
  }

  public void testsubstringparity() {
    byte[] bigbytes = gettestbytes(2048 * 1024, 113344l);
    int start = 512 * 1024 - 3333;
    int end   = 512 * 1024 + 7777;
    bytestring concretesubstring = bytestring.copyfrom(bigbytes).substring(start, end);
    boolean ok = true;
    for (int i = start; ok && i < end; ++i) {
      ok = (bigbytes[i] == concretesubstring.byteat(i - start));
    }
    asserttrue("concrete substring didn't capture the right bytes", ok);

    bytestring literalstring = bytestring.copyfrom(bigbytes, start, end - start);
    asserttrue("substring must be equal to literal string",
        concretesubstring.equals(literalstring));
    assertequals("substring must have same hashcode as literal string",
        literalstring.hashcode(), concretesubstring.hashcode());
  }

  public void testcompositesubstring() {
    byte[] referencebytes = gettestbytes(77748, 113344l);

    list<bytestring> pieces = makeconcretepieces(referencebytes);
    bytestring liststring = bytestring.copyfrom(pieces);

    int from = 1000;
    int to = 40000;
    bytestring compositesubstring = liststring.substring(from, to);
    byte[] substringbytes = compositesubstring.tobytearray();
    boolean stillequal = true;
    for (int i = 0; stillequal && i < to - from; ++i) {
      stillequal = referencebytes[from + i] == substringbytes[i];
    }
    asserttrue("substring must return correct bytes", stillequal);

    stillequal = true;
    for (int i = 0; stillequal && i < to - from; ++i) {
      stillequal = referencebytes[from + i] == compositesubstring.byteat(i);
    }
    asserttrue("substring must support byteat() correctly", stillequal);

    bytestring literalsubstring = bytestring.copyfrom(referencebytes, from, to - from);
    asserttrue("composite substring must equal a literal substring over the same bytes",
        compositesubstring.equals(literalsubstring));
    asserttrue("literal substring must equal a composite substring over the same bytes",
        literalsubstring.equals(compositesubstring));

    assertequals("we must get the same hashcodes for composite and literal substrings",
        literalsubstring.hashcode(), compositesubstring.hashcode());

    assertfalse("we can't be equal to a proper substring",
        compositesubstring.equals(literalsubstring.substring(0, literalsubstring.size() - 1)));
  }

  public void testcopyfromlist() {
    byte[] referencebytes = gettestbytes(77748, 113344l);
    bytestring literalstring = bytestring.copyfrom(referencebytes);

    list<bytestring> pieces = makeconcretepieces(referencebytes);
    bytestring liststring = bytestring.copyfrom(pieces);

    asserttrue("composite string must be equal to literal string",
        liststring.equals(literalstring));
    assertequals("composite string must have same hashcode as literal string",
        literalstring.hashcode(), liststring.hashcode());
  }

  public void testconcat() {
    byte[] referencebytes = gettestbytes(77748, 113344l);
    bytestring literalstring = bytestring.copyfrom(referencebytes);

    list<bytestring> pieces = makeconcretepieces(referencebytes);

    iterator<bytestring> iter = pieces.iterator();
    bytestring concatenatedstring = iter.next();
    while (iter.hasnext()) {
      concatenatedstring = concatenatedstring.concat(iter.next());
    }

    asserttrue("concatenated string must be equal to literal string",
        concatenatedstring.equals(literalstring));
    assertequals("concatenated string must have same hashcode as literal string",
        literalstring.hashcode(), concatenatedstring.hashcode());
  }

  /**
   * test the rope implementation can deal with empty nodes, even though we
   * guard against them. see also {@link literalbytestringtest#testconcat_empty()}.
   */
  public void testconcat_empty() {
    byte[] referencebytes = gettestbytes(7748, 113344l);
    bytestring literalstring = bytestring.copyfrom(referencebytes);

    bytestring duo = ropebytestring.newinstancefortest(literalstring, literalstring);
    bytestring temp = ropebytestring.newinstancefortest(
        ropebytestring.newinstancefortest(literalstring, bytestring.empty),
        ropebytestring.newinstancefortest(bytestring.empty, literalstring));
    bytestring quintet = ropebytestring.newinstancefortest(temp, bytestring.empty);

    asserttrue("string with concatenated nulls must equal simple concatenate",
        duo.equals(quintet));
    assertequals("string with concatenated nulls have same hashcode as simple concatenate",
        duo.hashcode(), quintet.hashcode());

    bytestring.byteiterator duoiter = duo.iterator();
    bytestring.byteiterator quintetiter = quintet.iterator();
    boolean stillequal = true;
    while (stillequal && quintetiter.hasnext()) {
      stillequal = (duoiter.nextbyte() == quintetiter.nextbyte());
    }
    asserttrue("we must get the same characters by iterating", stillequal);
    assertfalse("iterator must be exhausted", duoiter.hasnext());
    try {
      duoiter.nextbyte();
      fail("should have thrown an exception.");
    } catch (nosuchelementexception e) {
      // this is success
    }
    try {
      quintetiter.nextbyte();
      fail("should have thrown an exception.");
    } catch (nosuchelementexception e) {
      // this is success
    }

    // test that even if we force empty strings in as rope leaves in this
    // configuration, we always get a (possibly bounded) literalbytestring
    // for a length 1 substring.
    //
    // it is possible, using the testing factory method to create deeply nested
    // trees of empty leaves, to make a string that will fail this test.
    for (int i = 1; i < duo.size(); ++i) {
      asserttrue("substrings of size() < 2 must not be ropebytestrings",
          duo.substring(i - 1, i) instanceof literalbytestring);
    }
    for (int i = 1; i < quintet.size(); ++i) {
      asserttrue("substrings of size() < 2 must not be ropebytestrings",
          quintet.substring(i - 1, i) instanceof literalbytestring);
    }
  }

  public void teststartswith() {
    byte[] bytes = gettestbytes(1000, 1234l);
    bytestring string = bytestring.copyfrom(bytes);
    bytestring prefix = bytestring.copyfrom(bytes, 0, 500);
    bytestring suffix = bytestring.copyfrom(bytes, 400, 600);
    asserttrue(string.startswith(bytestring.empty));
    asserttrue(string.startswith(string));
    asserttrue(string.startswith(prefix));
    assertfalse(string.startswith(suffix));
    assertfalse(prefix.startswith(suffix));
    assertfalse(suffix.startswith(prefix));
    assertfalse(bytestring.empty.startswith(prefix));
    asserttrue(bytestring.empty.startswith(bytestring.empty));
  }

  static list<bytestring> makeconcretepieces(byte[] referencebytes) {
    list<bytestring> pieces = new arraylist<bytestring>();
    // starting length should be small enough that we'll do some concatenating by
    // copying if we just concatenate all these pieces together.
    for (int start = 0, length = 16; start < referencebytes.length; start += length) {
      length = (length << 1) - 1;
      if (start + length > referencebytes.length) {
        length = referencebytes.length - start;
      }
      pieces.add(bytestring.copyfrom(referencebytes, start, length));
    }
    return pieces;
  }
}
