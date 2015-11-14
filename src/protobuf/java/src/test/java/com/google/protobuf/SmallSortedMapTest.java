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
import java.util.arrays;
import java.util.hashmap;
import java.util.iterator;
import java.util.list;
import java.util.map;
import java.util.set;
import java.util.treeset;

/**
 * @author darick@google.com darick tong
 */
public class smallsortedmaptest extends testcase {
  // java.util.abstractmap.simpleentry is private in jdk 1.5. we re-implement it
  // here for jdk 1.5 users.
  private static class simpleentry<k, v> implements map.entry<k, v> {
    private final k key;
    private v value;

    simpleentry(k key, v value) {
      this.key = key;
      this.value = value;
    }

    public k getkey() {
      return key;
    }

    public v getvalue() {
      return value;
    }

    public v setvalue(v value) {
      v oldvalue = this.value;
      this.value = value;
      return oldvalue;
    }

    private static boolean eq(object o1, object o2) {
      return o1 == null ? o2 == null : o1.equals(o2);
    }

    @override
    public boolean equals(object o) {
      if (!(o instanceof map.entry))
        return false;
      map.entry e = (map.entry) o;
      return eq(key, e.getkey()) && eq(value, e.getvalue());
    }

    @override
    public int hashcode() {
      return ((key == null) ? 0 : key.hashcode()) ^
          ((value == null) ? 0 : value.hashcode());
    }
  }

  public void testputandgetarrayentriesonly() {
    runputandgettest(3);
  }

  public void testputandgetoverflowentries() {
    runputandgettest(6);
  }

  private void runputandgettest(int numelements) {
    // test with even and odd arraysize
    smallsortedmap<integer, integer> map1 =
        smallsortedmap.newinstancefortest(3);
    smallsortedmap<integer, integer> map2 =
        smallsortedmap.newinstancefortest(4);
    smallsortedmap<integer, integer> map3 =
        smallsortedmap.newinstancefortest(3);
    smallsortedmap<integer, integer> map4 =
        smallsortedmap.newinstancefortest(4);

    // test with puts in ascending order.
    for (int i = 0; i < numelements; i++) {
      assertnull(map1.put(i, i + 1));
      assertnull(map2.put(i, i + 1));
    }
    // test with puts in descending order.
    for (int i = numelements - 1; i >= 0; i--) {
      assertnull(map3.put(i, i + 1));
      assertnull(map4.put(i, i + 1));
    }

    assertequals(math.min(3, numelements), map1.getnumarrayentries());
    assertequals(math.min(4, numelements), map2.getnumarrayentries());
    assertequals(math.min(3, numelements), map3.getnumarrayentries());
    assertequals(math.min(4, numelements), map4.getnumarrayentries());

    list<smallsortedmap<integer, integer>> allmaps =
        new arraylist<smallsortedmap<integer, integer>>();
    allmaps.add(map1);
    allmaps.add(map2);
    allmaps.add(map3);
    allmaps.add(map4);

    for (smallsortedmap<integer, integer> map : allmaps) {
      assertequals(numelements, map.size());
      for (int i = 0; i < numelements; i++) {
        assertequals(new integer(i + 1), map.get(i));
      }
    }

    assertequals(map1, map2);
    assertequals(map2, map3);
    assertequals(map3, map4);
  }

  public void testreplacingput() {
    smallsortedmap<integer, integer> map = smallsortedmap.newinstancefortest(3);
    for (int i = 0; i < 6; i++) {
      assertnull(map.put(i, i + 1));
      assertnull(map.remove(i + 1));
    }
    for (int i = 0; i < 6; i++) {
      assertequals(new integer(i + 1), map.put(i, i + 2));
    }
  }

  public void testremove() {
    smallsortedmap<integer, integer> map = smallsortedmap.newinstancefortest(3);
    for (int i = 0; i < 6; i++) {
      assertnull(map.put(i, i + 1));
      assertnull(map.remove(i + 1));
    }

    assertequals(3, map.getnumarrayentries());
    assertequals(3, map.getnumoverflowentries());
    assertequals(6,  map.size());
    assertequals(makesortedkeyset(0, 1, 2, 3, 4, 5), map.keyset());

    assertequals(new integer(2), map.remove(1));
    assertequals(3, map.getnumarrayentries());
    assertequals(2, map.getnumoverflowentries());
    assertequals(5,  map.size());
    assertequals(makesortedkeyset(0, 2, 3, 4, 5), map.keyset());

    assertequals(new integer(5), map.remove(4));
    assertequals(3, map.getnumarrayentries());
    assertequals(1, map.getnumoverflowentries());
    assertequals(4,  map.size());
    assertequals(makesortedkeyset(0, 2, 3, 5), map.keyset());

    assertequals(new integer(4), map.remove(3));
    assertequals(3, map.getnumarrayentries());
    assertequals(0, map.getnumoverflowentries());
    assertequals(3, map.size());
    assertequals(makesortedkeyset(0, 2, 5), map.keyset());

    assertnull(map.remove(3));
    assertequals(3, map.getnumarrayentries());
    assertequals(0, map.getnumoverflowentries());
    assertequals(3, map.size());

    assertequals(new integer(1), map.remove(0));
    assertequals(2, map.getnumarrayentries());
    assertequals(0, map.getnumoverflowentries());
    assertequals(2, map.size());
  }

  public void testclear() {
    smallsortedmap<integer, integer> map = smallsortedmap.newinstancefortest(3);
    for (int i = 0; i < 6; i++) {
      assertnull(map.put(i, i + 1));
    }
    map.clear();
    assertequals(0, map.getnumarrayentries());
    assertequals(0, map.getnumoverflowentries());
    assertequals(0, map.size());
  }

  public void testgetarrayentryandoverflowentries() {
    smallsortedmap<integer, integer> map = smallsortedmap.newinstancefortest(3);
    for (int i = 0; i < 6; i++) {
      assertnull(map.put(i, i + 1));
    }
    assertequals(3, map.getnumarrayentries());
    for (int i = 0; i < 3; i++) {
      map.entry<integer, integer> entry = map.getarrayentryat(i);
      assertequals(new integer(i), entry.getkey());
      assertequals(new integer(i + 1), entry.getvalue());
    }
    iterator<map.entry<integer, integer>> it =
        map.getoverflowentries().iterator();
    for (int i = 3; i < 6; i++) {
      asserttrue(it.hasnext());
      map.entry<integer, integer> entry = it.next();
      assertequals(new integer(i), entry.getkey());
      assertequals(new integer(i + 1), entry.getvalue());
    }
    assertfalse(it.hasnext());
  }

  public void testentrysetcontains() {
    smallsortedmap<integer, integer> map = smallsortedmap.newinstancefortest(3);
    for (int i = 0; i < 6; i++) {
      assertnull(map.put(i, i + 1));
    }
    set<map.entry<integer, integer>> entryset = map.entryset();
    for (int i = 0; i < 6; i++) {
      asserttrue(
          entryset.contains(new simpleentry<integer, integer>(i, i + 1)));
      assertfalse(
          entryset.contains(new simpleentry<integer, integer>(i, i)));
    }
  }

  public void testentrysetadd() {
    smallsortedmap<integer, integer> map = smallsortedmap.newinstancefortest(3);
    set<map.entry<integer, integer>> entryset = map.entryset();
    for (int i = 0; i < 6; i++) {
      map.entry<integer, integer> entry =
          new simpleentry<integer, integer>(i, i + 1);
      asserttrue(entryset.add(entry));
      assertfalse(entryset.add(entry));
    }
    for (int i = 0; i < 6; i++) {
      assertequals(new integer(i + 1), map.get(i));
    }
    assertequals(3, map.getnumarrayentries());
    assertequals(3, map.getnumoverflowentries());
    assertequals(6, map.size());
  }

  public void testentrysetremove() {
    smallsortedmap<integer, integer> map = smallsortedmap.newinstancefortest(3);
    set<map.entry<integer, integer>> entryset = map.entryset();
    for (int i = 0; i < 6; i++) {
      assertnull(map.put(i, i + 1));
    }
    for (int i = 0; i < 6; i++) {
      map.entry<integer, integer> entry =
          new simpleentry<integer, integer>(i, i + 1);
      asserttrue(entryset.remove(entry));
      assertfalse(entryset.remove(entry));
    }
    asserttrue(map.isempty());
    assertequals(0, map.getnumarrayentries());
    assertequals(0, map.getnumoverflowentries());
    assertequals(0, map.size());
  }

  public void testentrysetclear() {
    smallsortedmap<integer, integer> map = smallsortedmap.newinstancefortest(3);
    for (int i = 0; i < 6; i++) {
      assertnull(map.put(i, i + 1));
    }
    map.entryset().clear();
    asserttrue(map.isempty());
    assertequals(0, map.getnumarrayentries());
    assertequals(0, map.getnumoverflowentries());
    assertequals(0, map.size());
  }

  public void testentrysetiteratornext() {
    smallsortedmap<integer, integer> map = smallsortedmap.newinstancefortest(3);
    for (int i = 0; i < 6; i++) {
      assertnull(map.put(i, i + 1));
    }
    iterator<map.entry<integer, integer>> it = map.entryset().iterator();
    for (int i = 0; i < 6; i++) {
      asserttrue(it.hasnext());
      map.entry<integer, integer> entry = it.next();
      assertequals(new integer(i), entry.getkey());
      assertequals(new integer(i + 1), entry.getvalue());
    }
    assertfalse(it.hasnext());
  }

  public void testentrysetiteratorremove() {
    smallsortedmap<integer, integer> map = smallsortedmap.newinstancefortest(3);
    for (int i = 0; i < 6; i++) {
      assertnull(map.put(i, i + 1));
    }
    iterator<map.entry<integer, integer>> it = map.entryset().iterator();
    for (int i = 0; i < 6; i++) {
      asserttrue(map.containskey(i));
      it.next();
      it.remove();
      assertfalse(map.containskey(i));
      assertequals(6 - i - 1, map.size());
    }
  }

  public void testmapentrymodification() {
    smallsortedmap<integer, integer> map = smallsortedmap.newinstancefortest(3);
    for (int i = 0; i < 6; i++) {
      assertnull(map.put(i, i + 1));
    }
    iterator<map.entry<integer, integer>> it = map.entryset().iterator();
    for (int i = 0; i < 6; i++) {
      map.entry<integer, integer> entry = it.next();
      entry.setvalue(i + 23);
    }
    for (int i = 0; i < 6; i++) {
      assertequals(new integer(i + 23), map.get(i));
    }
  }

  public void testmakeimmutable() {
    smallsortedmap<integer, integer> map = smallsortedmap.newinstancefortest(3);
    for (int i = 0; i < 6; i++) {
      assertnull(map.put(i, i + 1));
    }
    map.makeimmutable();
    assertequals(new integer(1), map.get(0));
    assertequals(6, map.size());

    try {
      map.put(23, 23);
      fail("expected unsupportedoperationexception");
    } catch (unsupportedoperationexception expected) {
    }

    map<integer, integer> other = new hashmap<integer, integer>();
    other.put(23, 23);
    try {
      map.putall(other);
      fail("expected unsupportedoperationexception");
    } catch (unsupportedoperationexception expected) {
    }

    try {
      map.remove(0);
      fail("expected unsupportedoperationexception");
    } catch (unsupportedoperationexception expected) {
    }

    try {
      map.clear();
      fail("expected unsupportedoperationexception");
    } catch (unsupportedoperationexception expected) {
    }

    set<map.entry<integer, integer>> entryset = map.entryset();
    try {
      entryset.clear();
      fail("expected unsupportedoperationexception");
    } catch (unsupportedoperationexception expected) {
    }

    iterator<map.entry<integer, integer>> it = entryset.iterator();
    while (it.hasnext()) {
      map.entry<integer, integer> entry = it.next();
      try {
        entry.setvalue(0);
        fail("expected unsupportedoperationexception");
      } catch (unsupportedoperationexception expected) {
      }
      try {
        it.remove();
        fail("expected unsupportedoperationexception");
      } catch (unsupportedoperationexception expected) {
      }
    }

    set<integer> keyset = map.keyset();
    try {
      keyset.clear();
      fail("expected unsupportedoperationexception");
    } catch (unsupportedoperationexception expected) {
    }

    iterator<integer> keys = keyset.iterator();
    while (keys.hasnext()) {
      integer key = keys.next();
      try {
        keyset.remove(key);
        fail("expected unsupportedoperationexception");
      } catch (unsupportedoperationexception expected) {
      }
      try {
        keys.remove();
        fail("expected unsupportedoperationexception");
      } catch (unsupportedoperationexception expected) {
      }
    }
  }

  private set<integer> makesortedkeyset(integer... keys) {
    return new treeset<integer>(arrays.<integer>aslist(keys));
  }
}
