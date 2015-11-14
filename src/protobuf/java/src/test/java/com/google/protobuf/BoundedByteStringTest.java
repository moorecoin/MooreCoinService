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

/**
 * this class tests {@link boundedbytestring}, which extends {@link literalbytestring},
 * by inheriting the tests from {@link literalbytestringtest}.  the only method which
 * is strange enough that it needs to be overridden here is {@link #testtostring()}.
 *
 * @author carlanton@google.com (carl haverl)
 */
public class boundedbytestringtest extends literalbytestringtest {

  @override
  protected void setup() throws exception {
    classundertest = "boundedbytestring";
    byte[] sourcebytes = bytestringtest.gettestbytes(2341, 11337766l);
    int from = 100;
    int to = sourcebytes.length - 100;
    stringundertest = bytestring.copyfrom(sourcebytes).substring(from, to);
    referencebytes = new byte[to - from];
    system.arraycopy(sourcebytes, from, referencebytes, 0, to - from);
    expectedhashcode = 727575887;
  }

  @override
  public void testtostring() throws unsupportedencodingexception {
    string teststring = "i love unicode \u1234\u5678 characters";
    literalbytestring unicode = new literalbytestring(teststring.getbytes(utf_8));
    bytestring chopped = unicode.substring(2, unicode.size() - 6);
    assertequals(classundertest + ".substring() must have the expected type",
        classundertest, getactualclassname(chopped));

    string roundtripstring = chopped.tostring(utf_8);
    assertequals(classundertest + " unicode bytes must match",
        teststring.substring(2, teststring.length() - 6), roundtripstring);
  }
}
