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

import java.util.arraylist;
import java.util.list;

/**
 * tests for {@link lazystringarraylist}.
 *
 * @author jonp@google.com (jon perlow)
 */
public class lazystringarraylisttest extends testcase {

  private static string string_a = "a";
  private static string string_b = "b";
  private static string string_c = "c";

  private static bytestring byte_string_a = bytestring.copyfromutf8("a");
  private static bytestring byte_string_b = bytestring.copyfromutf8("b");
  private static bytestring byte_string_c = bytestring.copyfromutf8("c");

  public void testjuststrings() {
    lazystringarraylist list = new lazystringarraylist();
    list.add(string_a);
    list.add(string_b);
    list.add(string_c);

    assertequals(3, list.size());
    assertsame(string_a, list.get(0));
    assertsame(string_b, list.get(1));
    assertsame(string_c, list.get(2));

    list.set(1, string_c);
    assertsame(string_c, list.get(1));

    list.remove(1);
    assertsame(string_a, list.get(0));
    assertsame(string_c, list.get(1));
  }

  public void testjustbytestring() {
    lazystringarraylist list = new lazystringarraylist();
    list.add(byte_string_a);
    list.add(byte_string_b);
    list.add(byte_string_c);

    assertequals(3, list.size());
    assertsame(byte_string_a, list.getbytestring(0));
    assertsame(byte_string_b, list.getbytestring(1));
    assertsame(byte_string_c, list.getbytestring(2));

    list.remove(1);
    assertsame(byte_string_a, list.getbytestring(0));
    assertsame(byte_string_c, list.getbytestring(1));
  }

  public void testconversionbackandforth() {
    lazystringarraylist list = new lazystringarraylist();
    list.add(string_a);
    list.add(byte_string_b);
    list.add(byte_string_c);

    // string a should be the same because it was originally a string
    assertsame(string_a, list.get(0));

    // string b and c should be different because the string has to be computed
    // from the bytestring
    string bprime = list.get(1);
    assertnotsame(string_b, bprime);
    assertequals(string_b, bprime);
    string cprime = list.get(2);
    assertnotsame(string_c, cprime);
    assertequals(string_c, cprime);

    // string c and c should stay the same once cached.
    assertsame(bprime, list.get(1));
    assertsame(cprime, list.get(2));

    // bytestring needs to be computed from string for both a and b
    bytestring aprimebytestring = list.getbytestring(0);
    assertequals(byte_string_a, aprimebytestring);
    bytestring bprimebytestring = list.getbytestring(1);
    assertnotsame(byte_string_b, bprimebytestring);
    assertequals(byte_string_b, list.getbytestring(1));

    // once cached, bytestring should stay cached.
    assertsame(aprimebytestring, list.getbytestring(0));
    assertsame(bprimebytestring, list.getbytestring(1));
  }

  public void testcopyconstructorcopiesbyreference() {
    lazystringarraylist list1 = new lazystringarraylist();
    list1.add(string_a);
    list1.add(byte_string_b);
    list1.add(byte_string_c);

    lazystringarraylist list2 = new lazystringarraylist(list1);
    assertequals(3, list2.size());
    assertsame(string_a, list2.get(0));
    assertsame(byte_string_b, list2.getbytestring(1));
    assertsame(byte_string_c, list2.getbytestring(2));
  }

  public void testlistcopyconstructor() {
    list<string> list1 = new arraylist<string>();
    list1.add(string_a);
    list1.add(string_b);
    list1.add(string_c);

    lazystringarraylist list2 = new lazystringarraylist(list1);
    assertequals(3, list2.size());
    assertsame(string_a, list2.get(0));
    assertsame(string_b, list2.get(1));
    assertsame(string_c, list2.get(2));
  }

  public void testaddallcopiesbyreferenceifpossible() {
    lazystringarraylist list1 = new lazystringarraylist();
    list1.add(string_a);
    list1.add(byte_string_b);
    list1.add(byte_string_c);

    lazystringarraylist list2 = new lazystringarraylist();
    list2.addall(list1);

    assertequals(3, list2.size());
    assertsame(string_a, list2.get(0));
    assertsame(byte_string_b, list2.getbytestring(1));
    assertsame(byte_string_c, list2.getbytestring(2));
  }
}
