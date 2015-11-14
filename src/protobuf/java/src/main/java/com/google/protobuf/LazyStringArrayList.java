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

import java.util.list;
import java.util.abstractlist;
import java.util.arraylist;
import java.util.collection;
import java.util.collections;
import java.util.randomaccess;

/**
 * an implementation of {@link lazystringlist} that wraps an arraylist. each
 * element is either a bytestring or a string. it caches the last one requested
 * which is most likely the one needed next. this minimizes memory usage while
 * satisfying the most common use cases.
 * <p>
 * <strong>note that this implementation is not synchronized.</strong>
 * if multiple threads access an <tt>arraylist</tt> instance concurrently,
 * and at least one of the threads modifies the list structurally, it
 * <i>must</i> be synchronized externally.  (a structural modification is
 * any operation that adds or deletes one or more elements, or explicitly
 * resizes the backing array; merely setting the value of an element is not
 * a structural modification.)  this is typically accomplished by
 * synchronizing on some object that naturally encapsulates the list.
 * <p>
 * if the implementation is accessed via concurrent reads, this is thread safe.
 * conversions are done in a thread safe manner. it's possible that the
 * conversion may happen more than once if two threads attempt to access the
 * same element and the modifications were not visible to each other, but this
 * will not result in any corruption of the list or change in behavior other
 * than performance.
 *
 * @author jonp@google.com (jon perlow)
 */
public class lazystringarraylist extends abstractlist<string>
    implements lazystringlist, randomaccess {

  public final static lazystringlist empty = new unmodifiablelazystringlist(
      new lazystringarraylist());

  private final list<object> list;

  public lazystringarraylist() {
    list = new arraylist<object>();
  }

  public lazystringarraylist(lazystringlist from) {
    list = new arraylist<object>(from.size());
    addall(from);
  }

  public lazystringarraylist(list<string> from) {
    list = new arraylist<object>(from);
  }

  @override
  public string get(int index) {
    object o = list.get(index);
    if (o instanceof string) {
      return (string) o;
    } else {
      bytestring bs = (bytestring) o;
      string s = bs.tostringutf8();
      if (bs.isvalidutf8()) {
        list.set(index, s);
      }
      return s;
    }
  }

  @override
  public int size() {
    return list.size();
  }

  @override
  public string set(int index, string s) {
    object o = list.set(index, s);
    return asstring(o);
  }

  @override
  public void add(int index, string element) {
    list.add(index, element);
    modcount++;
  }

  @override
  public boolean addall(collection<? extends string> c) {
    // the default implementation of abstractcollection.addall(collection)
    // delegates to add(object). this implementation instead delegates to
    // addall(int, collection), which makes a special case for collections
    // which are instances of lazystringlist.
    return addall(size(), c);
  }

  @override
  public boolean addall(int index, collection<? extends string> c) {
    // when copying from another lazystringlist, directly copy the underlying
    // elements rather than forcing each element to be decoded to a string.
    collection<?> collection = c instanceof lazystringlist
        ? ((lazystringlist) c).getunderlyingelements() : c;
    boolean ret = list.addall(index, collection);
    modcount++;
    return ret;
  }

  @override
  public string remove(int index) {
    object o = list.remove(index);
    modcount++;
    return asstring(o);
  }

  public void clear() {
    list.clear();
    modcount++;
  }

  // @override
  public void add(bytestring element) {
    list.add(element);
    modcount++;
  }

  // @override
  public bytestring getbytestring(int index) {
    object o = list.get(index);
    if (o instanceof string) {
      bytestring b = bytestring.copyfromutf8((string) o);
      list.set(index, b);
      return b;
    } else {
      return (bytestring) o;
    }
  }

  private string asstring(object o) {
    if (o instanceof string) {
      return (string) o;
    } else {
      return ((bytestring) o).tostringutf8();
    }
  }

  public list<?> getunderlyingelements() {
    return collections.unmodifiablelist(list);
  }
}
