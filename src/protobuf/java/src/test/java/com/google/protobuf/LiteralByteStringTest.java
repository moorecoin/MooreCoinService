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

import junit.framework.testcase;

import java.io.bytearrayoutputstream;
import java.io.ioexception;
import java.io.inputstream;
import java.io.outputstream;
import java.io.unsupportedencodingexception;
import java.nio.bytebuffer;
import java.util.arrays;
import java.util.list;
import java.util.nosuchelementexception;

/**
 * test {@link literalbytestring} by setting up a reference string in {@link #setup()}.
 * this class is designed to be extended for testing extensions of {@link literalbytestring}
 * such as {@link boundedbytestring}, see {@link boundedbytestringtest}.
 *
 * @author carlanton@google.com (carl haverl)
 */
public class literalbytestringtest extends testcase {
  protected static final string utf_8 = "utf-8";

  protected string classundertest;
  protected byte[] referencebytes;
  protected bytestring stringundertest;
  protected int expectedhashcode;

  @override
  protected void setup() throws exception {
    classundertest = "literalbytestring";
    referencebytes = bytestringtest.gettestbytes(1234, 11337766l);
    stringundertest = bytestring.copyfrom(referencebytes);
    expectedhashcode = 331161852;
  }

  public void testexpectedtype() {
    string actualclassname = getactualclassname(stringundertest);
    assertequals(classundertest + " should match type exactly", classundertest, actualclassname);
  }

  protected string getactualclassname(object object) {
    string actualclassname = object.getclass().getname();
    actualclassname = actualclassname.substring(actualclassname.lastindexof('.') + 1);
    return actualclassname;
  }

  public void testbyteat() {
    boolean stillequal = true;
    for (int i = 0; stillequal && i < referencebytes.length; ++i) {
      stillequal = (referencebytes[i] == stringundertest.byteat(i));
    }
    asserttrue(classundertest + " must capture the right bytes", stillequal);
  }

  public void testbyteiterator() {
    boolean stillequal = true;
    bytestring.byteiterator iter = stringundertest.iterator();
    for (int i = 0; stillequal && i < referencebytes.length; ++i) {
      stillequal = (iter.hasnext() && referencebytes[i] == iter.nextbyte());
    }
    asserttrue(classundertest + " must capture the right bytes", stillequal);
    assertfalse(classundertest + " must have exhausted the itertor", iter.hasnext());

    try {
      iter.nextbyte();
      fail("should have thrown an exception.");
    } catch (nosuchelementexception e) {
      // this is success
    }
  }

  public void testbyteiterable() {
    boolean stillequal = true;
    int j = 0;
    for (byte quantum : stringundertest) {
      stillequal = (referencebytes[j] == quantum);
      ++j;
    }
    asserttrue(classundertest + " must capture the right bytes as bytes", stillequal);
    assertequals(classundertest + " iterable character count", referencebytes.length, j);
  }

  public void testsize() {
    assertequals(classundertest + " must have the expected size", referencebytes.length,
        stringundertest.size());
  }

  public void testgettreedepth() {
    assertequals(classundertest + " must have depth 0", 0, stringundertest.gettreedepth());
  }

  public void testisbalanced() {
    asserttrue(classundertest + " is technically balanced", stringundertest.isbalanced());
  }

  public void testcopyto_bytearrayoffsetlength() {
    int destinationoffset = 50;
    int length = 100;
    byte[] destination = new byte[destinationoffset + length];
    int sourceoffset = 213;
    stringundertest.copyto(destination, sourceoffset, destinationoffset, length);
    boolean stillequal = true;
    for (int i = 0; stillequal && i < length; ++i) {
      stillequal = referencebytes[i + sourceoffset] == destination[i + destinationoffset];
    }
    asserttrue(classundertest + ".copyto(4 arg) must give the expected bytes", stillequal);
  }

  public void testcopyto_bytearrayoffsetlengtherrors() {
    int destinationoffset = 50;
    int length = 100;
    byte[] destination = new byte[destinationoffset + length];

    try {
      // copy one too many bytes
      stringundertest.copyto(destination, stringundertest.size() + 1 - length,
          destinationoffset, length);
      fail("should have thrown an exception when copying too many bytes of a "
          + classundertest);
    } catch (indexoutofboundsexception expected) {
      // this is success
    }

    try {
      // copy with illegal negative sourceoffset
      stringundertest.copyto(destination, -1, destinationoffset, length);
      fail("should have thrown an exception when given a negative sourceoffset in "
          + classundertest);
    } catch (indexoutofboundsexception expected) {
      // this is success
    }

    try {
      // copy with illegal negative destinationoffset
      stringundertest.copyto(destination, 0, -1, length);
      fail("should have thrown an exception when given a negative destinationoffset in "
          + classundertest);
    } catch (indexoutofboundsexception expected) {
      // this is success
    }

    try {
      // copy with illegal negative size
      stringundertest.copyto(destination, 0, 0, -1);
      fail("should have thrown an exception when given a negative size in "
          + classundertest);
    } catch (indexoutofboundsexception expected) {
      // this is success
    }

    try {
      // copy with illegal too-large sourceoffset
      stringundertest.copyto(destination, 2 * stringundertest.size(), 0, length);
      fail("should have thrown an exception when the destinationoffset is too large in "
          + classundertest);
    } catch (indexoutofboundsexception expected) {
      // this is success
    }

    try {
      // copy with illegal too-large destinationoffset
      stringundertest.copyto(destination, 0, 2 * destination.length, length);
      fail("should have thrown an exception when the destinationoffset is too large in "
          + classundertest);
    } catch (indexoutofboundsexception expected) {
      // this is success
    }
  }

  public void testcopyto_bytebuffer() {
    bytebuffer mybuffer = bytebuffer.allocate(referencebytes.length);
    stringundertest.copyto(mybuffer);
    asserttrue(classundertest + ".copyto(bytebuffer) must give back the same bytes",
        arrays.equals(referencebytes, mybuffer.array()));
  }

  public void testasreadonlybytebuffer() {
    bytebuffer bytebuffer = stringundertest.asreadonlybytebuffer();
    byte[] roundtripbytes = new byte[referencebytes.length];
    asserttrue(bytebuffer.remaining() == referencebytes.length);
    asserttrue(bytebuffer.isreadonly());
    bytebuffer.get(roundtripbytes);
    asserttrue(classundertest + ".asreadonlybytebuffer() must give back the same bytes",
        arrays.equals(referencebytes, roundtripbytes));
  }

  public void testasreadonlybytebufferlist() {
    list<bytebuffer> bytebuffers = stringundertest.asreadonlybytebufferlist();
    int bytesseen = 0;
    byte[] roundtripbytes = new byte[referencebytes.length];
    for (bytebuffer bytebuffer : bytebuffers) {
      int thislength = bytebuffer.remaining();
      asserttrue(bytebuffer.isreadonly());
      asserttrue(bytesseen + thislength <= referencebytes.length);
      bytebuffer.get(roundtripbytes, bytesseen, thislength);
      bytesseen += thislength;
    }
    asserttrue(bytesseen == referencebytes.length);
    asserttrue(classundertest + ".asreadonlybytebuffertest() must give back the same bytes",
        arrays.equals(referencebytes, roundtripbytes));
  }

  public void testtobytearray() {
    byte[] roundtripbytes = stringundertest.tobytearray();
    asserttrue(classundertest + ".tobytearray() must give back the same bytes",
        arrays.equals(referencebytes, roundtripbytes));
  }

  public void testwriteto() throws ioexception {
    bytearrayoutputstream bos = new bytearrayoutputstream();
    stringundertest.writeto(bos);
    byte[] roundtripbytes = bos.tobytearray();
    asserttrue(classundertest + ".writeto() must give back the same bytes",
        arrays.equals(referencebytes, roundtripbytes));
  }
  
  public void testwriteto_mutating() throws ioexception {
    outputstream os = new outputstream() {
      @override
      public void write(byte[] b, int off, int len) {
        for (int x = 0; x < len; ++x) {
          b[off + x] = (byte) 0;
        }
      }

      @override
      public void write(int b) {
        // purposefully left blank.
      }
    };

    stringundertest.writeto(os);
    byte[] newbytes = stringundertest.tobytearray();
    asserttrue(classundertest + ".writeto() must not grant access to underlying array",
        arrays.equals(referencebytes, newbytes));
  }

  public void testnewoutput() throws ioexception {
    bytearrayoutputstream bos = new bytearrayoutputstream();
    bytestring.output output = bytestring.newoutput();
    stringundertest.writeto(output);
    assertequals("output size returns correct result",
        output.size(), stringundertest.size());
    output.writeto(bos);
    asserttrue("output.writeto() must give back the same bytes",
        arrays.equals(referencebytes, bos.tobytearray()));

    // write the output stream to itself! this should cause it to double
    output.writeto(output);
    assertequals("writing an output stream to itself is successful",
        stringundertest.concat(stringundertest), output.tobytestring());

    output.reset();
    assertequals("output.reset() resets the output", 0, output.size());
    assertequals("output.reset() resets the output",
        bytestring.empty, output.tobytestring());
    
  }

  public void testtostring() throws unsupportedencodingexception {
    string teststring = "i love unicode \u1234\u5678 characters";
    literalbytestring unicode = new literalbytestring(teststring.getbytes(utf_8));
    string roundtripstring = unicode.tostring(utf_8);
    assertequals(classundertest + " unicode must match", teststring, roundtripstring);
  }

  public void testequals() {
    assertequals(classundertest + " must not equal null", false, stringundertest.equals(null));
    assertequals(classundertest + " must equal self", stringundertest, stringundertest);
    assertfalse(classundertest + " must not equal the empty string",
        stringundertest.equals(bytestring.empty));
    assertequals(classundertest + " empty strings must be equal",
        new literalbytestring(new byte[]{}), stringundertest.substring(55, 55));
    assertequals(classundertest + " must equal another string with the same value",
        stringundertest, new literalbytestring(referencebytes));

    byte[] mungedbytes = new byte[referencebytes.length];
    system.arraycopy(referencebytes, 0, mungedbytes, 0, referencebytes.length);
    mungedbytes[mungedbytes.length - 5] ^= 0xff;
    assertfalse(classundertest + " must not equal every string with the same length",
        stringundertest.equals(new literalbytestring(mungedbytes)));
  }

  public void testhashcode() {
    int hash = stringundertest.hashcode();
    assertequals(classundertest + " must have expected hashcode", expectedhashcode, hash);
  }

  public void testpeekcachedhashcode() {
    assertequals(classundertest + ".peekcachedhashcode() should return zero at first", 0,
        stringundertest.peekcachedhashcode());
    stringundertest.hashcode();
    assertequals(classundertest + ".peekcachedhashcode should return zero at first",
        expectedhashcode, stringundertest.peekcachedhashcode());
  }

  public void testpartialhash() {
    // partialhash() is more strenuously tested elsewhere by testing hashes of substrings.
    // this test would fail if the expected hash were 1.  it's not.
    int hash = stringundertest.partialhash(stringundertest.size(), 0, stringundertest.size());
    assertequals(classundertest + ".partialhash() must yield expected hashcode",
        expectedhashcode, hash);
  }

  public void testnewinput() throws ioexception {
    inputstream input = stringundertest.newinput();
    assertequals("inputstream.available() returns correct value",
        stringundertest.size(), input.available());
    boolean stillequal = true;
    for (byte referencebyte : referencebytes) {
      int expectedint = (referencebyte & 0xff);
      stillequal = (expectedint == input.read());
    }
    assertequals("inputstream.available() returns correct value",
        0, input.available());
    asserttrue(classundertest + " must give the same bytes from the inputstream", stillequal);
    assertequals(classundertest + " inputstream must now be exhausted", -1, input.read());
  }

  public void testnewinput_skip() throws ioexception {
    inputstream input = stringundertest.newinput();
    int stringsize = stringundertest.size();
    int nearendindex = stringsize * 2 / 3;
    long skipped1 = input.skip(nearendindex);
    assertequals("inputstream.skip()", skipped1, nearendindex);
    assertequals("inputstream.available()",
        stringsize - skipped1, input.available());
    asserttrue("inputstream.mark() is available", input.marksupported());
    input.mark(0);
    assertequals("inputstream.skip(), read()",
        stringundertest.byteat(nearendindex) & 0xff, input.read());
    assertequals("inputstream.available()",
                 stringsize - skipped1 - 1, input.available());
    long skipped2 = input.skip(stringsize);
    assertequals("inputstream.skip() incomplete",
        skipped2, stringsize - skipped1 - 1);
    assertequals("inputstream.skip(), no more input", 0, input.available());
    assertequals("inputstream.skip(), no more input", -1, input.read());
    input.reset();
    assertequals("inputstream.reset() succeded",
                 stringsize - skipped1, input.available());
    assertequals("inputstream.reset(), read()",
        stringundertest.byteat(nearendindex) & 0xff, input.read());
  }

  public void testnewcodedinput() throws ioexception {
    codedinputstream cis = stringundertest.newcodedinput();
    byte[] roundtripbytes = cis.readrawbytes(referencebytes.length);
    asserttrue(classundertest + " must give the same bytes back from the codedinputstream",
        arrays.equals(referencebytes, roundtripbytes));
    asserttrue(classundertest + " codedinputstream must now be exhausted", cis.isatend());
  }

  /**
   * make sure we keep things simple when concatenating with empty. see also
   * {@link bytestringtest#testconcat_empty()}.
   */
  public void testconcat_empty() {
    assertsame(classundertest + " concatenated with empty must give " + classundertest,
        stringundertest.concat(bytestring.empty), stringundertest);
    assertsame("empty concatenated with " + classundertest + " must give " + classundertest,
        bytestring.empty.concat(stringundertest), stringundertest);
  }
}
