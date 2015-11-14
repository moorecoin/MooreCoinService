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

import java.util.iterator;
import java.util.listiterator;

/**
 * tests for {@link unmodifiablelazystringlist}.
 *
 * @author jonp@google.com (jon perlow)
 */
public class unmodifiablelazystringlisttest extends testcase {

  private static string string_a = "a";
  private static string string_b = "b";
  private static string string_c = "c";

  private static bytestring byte_string_a = bytestring.copyfromutf8("a");
  private static bytestring byte_string_b = bytestring.copyfromutf8("b");
  private static bytestring byte_string_c = bytestring.copyfromutf8("c");

  public void testreadonlymethods() {
    lazystringarraylist rawlist = createsamplelist();
    unmodifiablelazystringlist list = new unmodifiablelazystringlist(rawlist);
    assertequals(3, list.size());
    assertsame(string_a, list.get(0));
    assertsame(string_b, list.get(1));
    assertsame(string_c, list.get(2));
    assertequals(byte_string_a, list.getbytestring(0));
    assertequals(byte_string_b, list.getbytestring(1));
    assertequals(byte_string_c, list.getbytestring(2));
  }

  public void testmodifymethods() {
    lazystringarraylist rawlist = createsamplelist();
    unmodifiablelazystringlist list = new unmodifiablelazystringlist(rawlist);

    try {
      list.remove(0);
      fail();
    } catch (unsupportedoperationexception e) {
      // expected
    }
    assertequals(3, list.size());

    try {
      list.add(string_b);
      fail();
    } catch (unsupportedoperationexception e) {
      // expected
    }
    assertequals(3, list.size());

    try {
      list.set(1, string_b);
      fail();
    } catch (unsupportedoperationexception e) {
      // expected
    }
  }

  public void testiterator() {
    lazystringarraylist rawlist = createsamplelist();
    unmodifiablelazystringlist list = new unmodifiablelazystringlist(rawlist);

    iterator<string> iter = list.iterator();
    int count = 0;
    while (iter.hasnext()) {
      iter.next();
      count++;
      try {
        iter.remove();
        fail();
      } catch (unsupportedoperationexception e) {
        // expected
      }
    }
    assertequals(3, count);

  }

  public void testlistiterator() {
    lazystringarraylist rawlist = createsamplelist();
    unmodifiablelazystringlist list = new unmodifiablelazystringlist(rawlist);

    listiterator<string> iter = list.listiterator();
    int count = 0;
    while (iter.hasnext()) {
      iter.next();
      count++;
      try {
        iter.remove();
        fail();
      } catch (unsupportedoperationexception e) {
        // expected
      }
      try {
        iter.set("bar");
        fail();
      } catch (unsupportedoperationexception e) {
        // expected
      }
      try {
        iter.add("bar");
        fail();
      } catch (unsupportedoperationexception e) {
        // expected
      }
    }
    assertequals(3, count);

  }

  private lazystringarraylist createsamplelist() {
    lazystringarraylist rawlist = new lazystringarraylist();
    rawlist.add(string_a);
    rawlist.add(string_b);
    rawlist.add(string_c);
    return rawlist;
  }
}
