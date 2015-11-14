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

import java.io.unsupportedencodingexception;
import java.util.arrays;
import java.util.iterator;

/**
 * this class tests {@link ropebytestring} by inheriting the tests from
 * {@link literalbytestringtest}.  only a couple of methods are overridden.
 * 
 * <p>a full test of the result of {@link ropebytestring#substring(int, int)} is found in the
 * separate class {@link ropebytestringsubstringtest}.
 *
 * @author carlanton@google.com (carl haverl)
 */
public class ropebytestringtest extends literalbytestringtest {

  @override
  protected void setup() throws exception {
    classundertest = "ropebytestring";
    referencebytes = bytestringtest.gettestbytes(22341, 22337766l);
    iterator<bytestring> iter = bytestringtest.makeconcretepieces(referencebytes).iterator();
    stringundertest = iter.next();
    while (iter.hasnext()) {
      stringundertest = stringundertest.concat(iter.next());
    }
    expectedhashcode = -1214197238;
  }

  @override
  public void testgettreedepth() {
    assertequals(classundertest + " must have the expected tree depth",
        4, stringundertest.gettreedepth());
  }

  public void testbalance() {
    int numberofpieces = 10000;
    int piecesize = 64;
    byte[] testbytes = bytestringtest.gettestbytes(numberofpieces * piecesize, 113377l);

    // build up a big bytestring from smaller pieces to force a rebalance
    bytestring concatenated = bytestring.empty;
    for (int i = 0; i < numberofpieces; ++i) {
      concatenated = concatenated.concat(bytestring.copyfrom(testbytes, i * piecesize, piecesize));
    }

    assertequals(classundertest + " from string must have the expected type",
        classundertest, getactualclassname(concatenated));
    asserttrue(classundertest + " underlying bytes must match after balancing",
        arrays.equals(testbytes, concatenated.tobytearray()));
    bytestring teststring = bytestring.copyfrom(testbytes);
    asserttrue(classundertest + " balanced string must equal flat string",
        concatenated.equals(teststring));
    asserttrue(classundertest + " flat string must equal balanced string",
        teststring.equals(concatenated));
    assertequals(classundertest + " balanced string must have same hash code as flat string",
        teststring.hashcode(), concatenated.hashcode());
  }

  @override
  public void testtostring() throws unsupportedencodingexception {
    string sourcestring = "i love unicode \u1234\u5678 characters";
    bytestring sourcebytestring = bytestring.copyfromutf8(sourcestring);
    int copies = 250;

    // by building the ropebytestring by concatenating, this is actually a fairly strenuous test.
    stringbuilder builder = new stringbuilder(copies * sourcestring.length());
    bytestring unicode = bytestring.empty;
    for (int i = 0; i < copies; ++i) {
      builder.append(sourcestring);
      unicode = ropebytestring.concatenate(unicode, sourcebytestring);
    }
    string teststring = builder.tostring();

    assertequals(classundertest + " from string must have the expected type",
        classundertest, getactualclassname(unicode));
    string roundtripstring = unicode.tostring(utf_8);
    assertequals(classundertest + " unicode bytes must match",
        teststring, roundtripstring);
    bytestring flatstring = bytestring.copyfromutf8(teststring);
    assertequals(classundertest + " string must equal the flat string", flatstring, unicode);
    assertequals(classundertest + " string must must have same hashcode as the flat string",
        flatstring.hashcode(), unicode.hashcode());
  }
}
