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

import java.util.abstractlist;
import java.util.randomaccess;
import java.util.list;
import java.util.listiterator;
import java.util.iterator;

/**
 * an implementation of {@link lazystringlist} that wraps another
 * {@link lazystringlist} such that it cannot be modified via the wrapper.
 *
 * @author jonp@google.com (jon perlow)
 */
public class unmodifiablelazystringlist extends abstractlist<string>
    implements lazystringlist, randomaccess {

  private final lazystringlist list;

  public unmodifiablelazystringlist(lazystringlist list) {
    this.list = list;
  }

  @override
  public string get(int index) {
    return list.get(index);
  }

  @override
  public int size() {
    return list.size();
  }

  //@override (java 1.6 override semantics, but we must support 1.5)
  public bytestring getbytestring(int index) {
    return list.getbytestring(index);
  }

  //@override (java 1.6 override semantics, but we must support 1.5)
  public void add(bytestring element) {
    throw new unsupportedoperationexception();
  }

  //@override (java 1.6 override semantics, but we must support 1.5)
  public listiterator<string> listiterator(final int index) {
    return new listiterator<string>() {
      listiterator<string> iter = list.listiterator(index);

      //@override (java 1.6 override semantics, but we must support 1.5)
      public boolean hasnext() {
        return iter.hasnext();
      }

      //@override (java 1.6 override semantics, but we must support 1.5)
      public string next() {
        return iter.next();
      }

      //@override (java 1.6 override semantics, but we must support 1.5)
      public boolean hasprevious() {
        return iter.hasprevious();
      }

      //@override (java 1.6 override semantics, but we must support 1.5)
      public string previous() {
        return iter.previous();
      }

      //@override (java 1.6 override semantics, but we must support 1.5)
      public int nextindex() {
        return iter.nextindex();
      }

      //@override (java 1.6 override semantics, but we must support 1.5)
      public int previousindex() {
        return iter.previousindex();
      }

      //@override (java 1.6 override semantics, but we must support 1.5)
      public void remove() {
        throw new unsupportedoperationexception();
      }

      //@override (java 1.6 override semantics, but we must support 1.5)
      public void set(string o) {
        throw new unsupportedoperationexception();
      }

      //@override (java 1.6 override semantics, but we must support 1.5)
      public void add(string o) {
        throw new unsupportedoperationexception();
      }
    };
  }

  @override
  public iterator<string> iterator() {
    return new iterator<string>() {
      iterator<string> iter = list.iterator();

      //@override (java 1.6 override semantics, but we must support 1.5)
      public boolean hasnext() {
        return iter.hasnext();
      }

      //@override (java 1.6 override semantics, but we must support 1.5)
      public string next() {
        return iter.next();
      }

      //@override (java 1.6 override semantics, but we must support 1.5)
      public void remove() {
        throw new unsupportedoperationexception();
      }
    };
  }

  public list<?> getunderlyingelements() {
    // the returned value is already unmodifiable.
    return list.getunderlyingelements();
  }
}
