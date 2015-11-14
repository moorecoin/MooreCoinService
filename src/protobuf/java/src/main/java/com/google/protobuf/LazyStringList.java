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

/**
 * an interface extending {@code list<string>} that also provides access to the
 * items of the list as utf8-encoded bytestring objects. this is used by the
 * protocol buffer implementation to support lazily converting bytes parsed
 * over the wire to string objects until needed and also increases the
 * efficiency of serialization if the string was never requested as the
 * bytestring is already cached.
 * <p>
 * this only adds additional methods that are required for the use in the
 * protocol buffer code in order to be able successfully round trip byte arrays
 * through parsing and serialization without conversion to strings. it's not
 * attempting to support the functionality of say {@code list<bytestring>}, hence
 * why only these two very specific methods are added.
 *
 * @author jonp@google.com (jon perlow)
 */
public interface lazystringlist extends list<string> {

  /**
   * returns the element at the specified position in this list as a bytestring.
   *
   * @param index index of the element to return
   * @return the element at the specified position in this list
   * @throws indexoutofboundsexception if the index is out of range
   *         ({@code index < 0 || index >= size()})
   */
  bytestring getbytestring(int index);

  /**
   * appends the specified element to the end of this list (optional
   * operation).
   *
   * @param element element to be appended to this list
   * @throws unsupportedoperationexception if the <tt>add</tt> operation
   *         is not supported by this list
   */
  void add(bytestring element);

  /**
   * returns an unmodifiable list of the underlying elements, each of
   * which is either a {@code string} or its equivalent utf-8 encoded
   * {@code bytestring}. it is an error for the caller to modify the returned
   * list, and attempting to do so will result in an
   * {@link unsupportedoperationexception}.
   */
  list<?> getunderlyingelements();
}
