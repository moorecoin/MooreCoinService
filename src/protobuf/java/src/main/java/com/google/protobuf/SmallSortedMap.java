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

import java.util.abstractmap;
import java.util.abstractset;
import java.util.arraylist;
import java.util.collections;
import java.util.iterator;
import java.util.treemap;
import java.util.list;
import java.util.map;
import java.util.nosuchelementexception;
import java.util.set;
import java.util.sortedmap;

/**
 * a custom map implementation from fielddescriptor to object optimized to
 * minimize the number of memory allocations for instances with a small number
 * of mappings. the implementation stores the first {@code k} mappings in an
 * array for a configurable value of {@code k}, allowing direct access to the
 * corresponding {@code entry}s without the need to create an iterator. the
 * remaining entries are stored in an overflow map. iteration over the entries
 * in the map should be done as follows:
 *
 * <pre>   {@code
 * for (int i = 0; i < fieldmap.getnumarrayentries(); i++) {
 *   process(fieldmap.getarrayentryat(i));
 * }
 * for (map.entry<k, v> entry : fieldmap.getoverflowentries()) {
 *   process(entry);
 * }
 * }</pre>
 *
 * the resulting iteration is in order of ascending field tag number. the
 * object returned by {@link #entryset()} adheres to the same contract but is
 * less efficient as it necessarily involves creating an object for iteration.
 * <p>
 * the tradeoff for this memory efficiency is that the worst case running time
 * of the {@code put()} operation is {@code o(k + lg n)}, which happens when
 * entries are added in descending order. {@code k} should be chosen such that
 * it covers enough common cases without adversely affecting larger maps. in
 * practice, the worst case scenario does not happen for extensions because
 * extension fields are serialized and deserialized in order of ascending tag
 * number, but the worst case scenario can happen for dynamicmessages.
 * <p>
 * the running time for all other operations is similar to that of
 * {@code treemap}.
 * <p>
 * instances are not thread-safe until {@link #makeimmutable()} is called,
 * after which any modifying operation will result in an
 * {@link unsupportedoperationexception}.
 *
 * @author darick@google.com darick tong
 */
// this class is final for all intents and purposes because the constructor is
// private. however, the fielddescriptor-specific logic is encapsulated in
// a subclass to aid testability of the core logic.
class smallsortedmap<k extends comparable<k>, v> extends abstractmap<k, v> {

  /**
   * creates a new instance for mapping fielddescriptors to their values.
   * the {@link #makeimmutable()} implementation will convert the list values
   * of any repeated fields to unmodifiable lists.
   *
   * @param arraysize the size of the entry array containing the
   *        lexicographically smallest mappings.
   */
  static <fielddescriptortype extends
      fieldset.fielddescriptorlite<fielddescriptortype>>
      smallsortedmap<fielddescriptortype, object> newfieldmap(int arraysize) {
    return new smallsortedmap<fielddescriptortype, object>(arraysize) {
      @override
      @suppresswarnings("unchecked")
      public void makeimmutable() {
        if (!isimmutable()) {
          for (int i = 0; i < getnumarrayentries(); i++) {
            final map.entry<fielddescriptortype, object> entry =
                getarrayentryat(i);
            if (entry.getkey().isrepeated()) {
              final list value = (list) entry.getvalue();
              entry.setvalue(collections.unmodifiablelist(value));
            }
          }
          for (map.entry<fielddescriptortype, object> entry :
                   getoverflowentries()) {
            if (entry.getkey().isrepeated()) {
              final list value = (list) entry.getvalue();
              entry.setvalue(collections.unmodifiablelist(value));
            }
          }
        }
        super.makeimmutable();
      }
    };
  }

  /**
   * creates a new instance for testing.
   *
   * @param arraysize the size of the entry array containing the
   *        lexicographically smallest mappings.
   */
  static <k extends comparable<k>, v> smallsortedmap<k, v> newinstancefortest(
      int arraysize) {
    return new smallsortedmap<k, v>(arraysize);
  }

  private final int maxarraysize;
  // the "entry array" is actually a list because generic arrays are not
  // allowed. arraylist also nicely handles the entry shifting on inserts and
  // removes.
  private list<entry> entrylist;
  private map<k, v> overflowentries;
  private boolean isimmutable;
  // the entryset is a stateless view of the map. it's initialized the first
  // time it is requested and reused henceforth.
  private volatile entryset lazyentryset;

  /**
   * @code arraysize size of the array in which the lexicographically smallest
   *       mappings are stored. (i.e. the {@code k} referred to in the class
   *       documentation).
   */
  private smallsortedmap(int arraysize) {
    this.maxarraysize = arraysize;
    this.entrylist = collections.emptylist();
    this.overflowentries = collections.emptymap();
  }

  /** make this map immutable from this point forward. */
  public void makeimmutable() {
    if (!isimmutable) {
      // note: there's no need to wrap the entrylist in an unmodifiablelist
      // because none of the list's accessors are exposed. the iterator() of
      // overflowentries, on the other hand, is exposed so it must be made
      // unmodifiable.
      overflowentries = overflowentries.isempty() ?
          collections.<k, v>emptymap() :
          collections.unmodifiablemap(overflowentries);
      isimmutable = true;
    }
  }

  /** @return whether {@link #makeimmutable()} has been called. */
  public boolean isimmutable() {
    return isimmutable;
  }

  /** @return the number of entries in the entry array. */
  public int getnumarrayentries() {
    return entrylist.size();
  }

  /** @return the array entry at the given {@code index}. */
  public map.entry<k, v> getarrayentryat(int index) {
    return entrylist.get(index);
  }

  /** @return there number of overflow entries. */
  public int getnumoverflowentries() {
    return overflowentries.size();
  }

  /** @return an iterable over the overflow entries. */
  public iterable<map.entry<k, v>> getoverflowentries() {
    return overflowentries.isempty() ?
        emptyset.<map.entry<k, v>>iterable() :
        overflowentries.entryset();
  }

  @override
  public int size() {
    return entrylist.size() + overflowentries.size();
  }

  /**
   * the implementation throws a {@code classcastexception} if o is not an
   * object of type {@code k}.
   *
   * {@inheritdoc}
   */
  @override
  public boolean containskey(object o) {
    @suppresswarnings("unchecked")
    final k key = (k) o;
    return binarysearchinarray(key) >= 0 || overflowentries.containskey(key);
  }

  /**
   * the implementation throws a {@code classcastexception} if o is not an
   * object of type {@code k}.
   *
   * {@inheritdoc}
   */
  @override
  public v get(object o) {
    @suppresswarnings("unchecked")
    final k key = (k) o;
    final int index = binarysearchinarray(key);
    if (index >= 0) {
      return entrylist.get(index).getvalue();
    }
    return overflowentries.get(key);
  }

  @override
  public v put(k key, v value) {
    checkmutable();
    final int index = binarysearchinarray(key);
    if (index >= 0) {
      // replace existing array entry.
      return entrylist.get(index).setvalue(value);
    }
    ensureentryarraymutable();
    final int insertionpoint = -(index + 1);
    if (insertionpoint >= maxarraysize) {
      // put directly in overflow.
      return getoverflowentriesmutable().put(key, value);
    }
    // insert new entry in array.
    if (entrylist.size() == maxarraysize) {
      // shift the last array entry into overflow.
      final entry lastentryinarray = entrylist.remove(maxarraysize - 1);
      getoverflowentriesmutable().put(lastentryinarray.getkey(),
                                      lastentryinarray.getvalue());
    }
    entrylist.add(insertionpoint, new entry(key, value));
    return null;
  }

  @override
  public void clear() {
    checkmutable();
    if (!entrylist.isempty()) {
      entrylist.clear();
    }
    if (!overflowentries.isempty()) {
      overflowentries.clear();
    }
  }

  /**
   * the implementation throws a {@code classcastexception} if o is not an
   * object of type {@code k}.
   *
   * {@inheritdoc}
   */
  @override
  public v remove(object o) {
    checkmutable();
    @suppresswarnings("unchecked")
    final k key = (k) o;
    final int index = binarysearchinarray(key);
    if (index >= 0) {
      return removearrayentryat(index);
    }
    // overflowentries might be collections.unmodifiablemap(), so only
    // call remove() if it is non-empty.
    if (overflowentries.isempty()) {
      return null;
    } else {
      return overflowentries.remove(key);
    }
  }

  private v removearrayentryat(int index) {
    checkmutable();
    final v removed = entrylist.remove(index).getvalue();
    if (!overflowentries.isempty()) {
      // shift the first entry in the overflow to be the last entry in the
      // array.
      final iterator<map.entry<k, v>> iterator =
          getoverflowentriesmutable().entryset().iterator();
      entrylist.add(new entry(iterator.next()));
      iterator.remove();
    }
    return removed;
  }

  /**
   * @param key the key to find in the entry array.
   * @return the returned integer position follows the same semantics as the
   *     value returned by {@link java.util.arrays#binarysearch()}.
   */
  private int binarysearchinarray(k key) {
    int left = 0;
    int right = entrylist.size() - 1;

    // optimization: for the common case in which entries are added in
    // ascending tag order, check the largest element in the array before
    // doing a full binary search.
    if (right >= 0) {
      int cmp = key.compareto(entrylist.get(right).getkey());
      if (cmp > 0) {
        return -(right + 2);  // insert point is after "right".
      } else if (cmp == 0) {
        return right;
      }
    }

    while (left <= right) {
      int mid = (left + right) / 2;
      int cmp = key.compareto(entrylist.get(mid).getkey());
      if (cmp < 0) {
        right = mid - 1;
      } else if (cmp > 0) {
        left = mid + 1;
      } else {
        return mid;
      }
    }
    return -(left + 1);
  }

  /**
   * similar to the abstractmap implementation of {@code keyset()} and
   * {@code values()}, the entry set is created the first time this method is
   * called, and returned in response to all subsequent calls.
   *
   * {@inheritdoc}
   */
  @override
  public set<map.entry<k, v>> entryset() {
    if (lazyentryset == null) {
      lazyentryset = new entryset();
    }
    return lazyentryset;
  }

  /**
   * @throws unsupportedoperationexception if {@link #makeimmutable()} has
   *         has been called.
   */
  private void checkmutable() {
    if (isimmutable) {
      throw new unsupportedoperationexception();
    }
  }

  /**
   * @return a {@link sortedmap} to which overflow entries mappings can be
   *         added or removed.
   * @throws unsupportedoperationexception if {@link #makeimmutable()} has been
   *         called.
   */
  @suppresswarnings("unchecked")
  private sortedmap<k, v> getoverflowentriesmutable() {
    checkmutable();
    if (overflowentries.isempty() && !(overflowentries instanceof treemap)) {
      overflowentries = new treemap<k, v>();
    }
    return (sortedmap<k, v>) overflowentries;
  }

  /**
   * lazily creates the entry list. any code that adds to the list must first
   * call this method.
   */
  private void ensureentryarraymutable() {
    checkmutable();
    if (entrylist.isempty() && !(entrylist instanceof arraylist)) {
      entrylist = new arraylist<entry>(maxarraysize);
    }
  }

  /**
   * entry implementation that implements comparable in order to support
   * binary search within the entry array. also checks mutability in
   * {@link #setvalue()}.
   */
  private class entry implements map.entry<k, v>, comparable<entry> {

    private final k key;
    private v value;

    entry(map.entry<k, v> copy) {
      this(copy.getkey(), copy.getvalue());
    }

    entry(k key, v value) {
      this.key = key;
      this.value = value;
    }

    //@override (java 1.6 override semantics, but we must support 1.5)
    public k getkey() {
      return key;
    }

    //@override (java 1.6 override semantics, but we must support 1.5)
    public v getvalue() {
      return value;
    }

    //@override (java 1.6 override semantics, but we must support 1.5)
    public int compareto(entry other) {
      return getkey().compareto(other.getkey());
    }

    //@override (java 1.6 override semantics, but we must support 1.5)
    public v setvalue(v newvalue) {
      checkmutable();
      final v oldvalue = this.value;
      this.value = newvalue;
      return oldvalue;
    }

    @override
    public boolean equals(object o) {
      if (o == this) {
        return true;
      }
      if (!(o instanceof map.entry)) {
        return false;
      }
      @suppresswarnings("unchecked")
      map.entry<?, ?> other = (map.entry<?, ?>) o;
      return equals(key, other.getkey()) && equals(value, other.getvalue());
    }

    @override
    public int hashcode() {
      return (key == null ? 0 : key.hashcode()) ^
          (value == null ? 0 : value.hashcode());
    }

    @override
    public string tostring() {
      return key + "=" + value;
    }

    /** equals() that handles null values. */
    private boolean equals(object o1, object o2) {
      return o1 == null ? o2 == null : o1.equals(o2);
    }
  }

  /**
   * stateless view of the entries in the field map.
   */
  private class entryset extends abstractset<map.entry<k, v>> {

    @override
    public iterator<map.entry<k, v>> iterator() {
      return new entryiterator();
    }

    @override
    public int size() {
      return smallsortedmap.this.size();
    }

    /**
     * throws a {@link classcastexception} if o is not of the expected type.
     *
     * {@inheritdoc}
     */
    @override
    public boolean contains(object o) {
      @suppresswarnings("unchecked")
      final map.entry<k, v> entry = (map.entry<k, v>) o;
      final v existing = get(entry.getkey());
      final v value = entry.getvalue();
      return existing == value ||
          (existing != null && existing.equals(value));
    }

    @override
    public boolean add(map.entry<k, v> entry) {
      if (!contains(entry)) {
        put(entry.getkey(), entry.getvalue());
        return true;
      }
      return false;
    }

    /**
     * throws a {@link classcastexception} if o is not of the expected type.
     *
     * {@inheritdoc}
     */
    @override
    public boolean remove(object o) {
      @suppresswarnings("unchecked")
      final map.entry<k, v> entry = (map.entry<k, v>) o;
      if (contains(entry)) {
        smallsortedmap.this.remove(entry.getkey());
        return true;
      }
      return false;
    }

    @override
    public void clear() {
      smallsortedmap.this.clear();
    }
  }

  /**
   * iterator implementation that switches from the entry array to the overflow
   * entries appropriately.
   */
  private class entryiterator implements iterator<map.entry<k, v>> {

    private int pos = -1;
    private boolean nextcalledbeforeremove;
    private iterator<map.entry<k, v>> lazyoverflowiterator;

    //@override (java 1.6 override semantics, but we must support 1.5)
    public boolean hasnext() {
      return (pos + 1) < entrylist.size() ||
          getoverflowiterator().hasnext();
    }

    //@override (java 1.6 override semantics, but we must support 1.5)
    public map.entry<k, v> next() {
      nextcalledbeforeremove = true;
      // always increment pos so that we know whether the last returned value
      // was from the array or from overflow.
      if (++pos < entrylist.size()) {
        return entrylist.get(pos);
      }
      return getoverflowiterator().next();
    }

    //@override (java 1.6 override semantics, but we must support 1.5)
    public void remove() {
      if (!nextcalledbeforeremove) {
        throw new illegalstateexception("remove() was called before next()");
      }
      nextcalledbeforeremove = false;
      checkmutable();

      if (pos < entrylist.size()) {
        removearrayentryat(pos--);
      } else {
        getoverflowiterator().remove();
      }
    }

    /**
     * it is important to create the overflow iterator only after the array
     * entries have been iterated over because the overflow entry set changes
     * when the client calls remove() on the array entries, which invalidates
     * any existing iterators.
     */
    private iterator<map.entry<k, v>> getoverflowiterator() {
      if (lazyoverflowiterator == null) {
        lazyoverflowiterator = overflowentries.entryset().iterator();
      }
      return lazyoverflowiterator;
    }
  }

  /**
   * helper class that holds immutable instances of an iterable/iterator that
   * we return when the overflow entries is empty. this eliminates the creation
   * of an iterator object when there is nothing to iterate over.
   */
  private static class emptyset {

    private static final iterator<object> iterator = new iterator<object>() {
      //@override (java 1.6 override semantics, but we must support 1.5)
      public boolean hasnext() {
        return false;
      }
      //@override (java 1.6 override semantics, but we must support 1.5)
      public object next() {
        throw new nosuchelementexception();
      }
      //@override (java 1.6 override semantics, but we must support 1.5)
      public void remove() {
        throw new unsupportedoperationexception();
      }
    };

    private static final iterable<object> iterable = new iterable<object>() {
      //@override (java 1.6 override semantics, but we must support 1.5)
      public iterator<object> iterator() {
        return iterator;
      }
    };

    @suppresswarnings("unchecked")
    static <t> iterable<t> iterable() {
      return (iterable<t>) iterable;
    }
  }
}
