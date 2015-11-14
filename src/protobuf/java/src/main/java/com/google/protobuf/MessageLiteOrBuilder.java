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

/**
 * base interface for methods common to {@link messagelite}
 * and {@link messagelite.builder} to provide type equivalency.
 *
 * @author jonp@google.com (jon perlow)
 */
public interface messageliteorbuilder {
  /**
   * get an instance of the type with no fields set. because no fields are set,
   * all getters for singular fields will return default values and repeated
   * fields will appear empty.
   * this may or may not be a singleton.  this differs from the
   * {@code getdefaultinstance()} method of generated message classes in that
   * this method is an abstract method of the {@code messagelite} interface
   * whereas {@code getdefaultinstance()} is a static method of a specific
   * class.  they return the same thing.
   */
  messagelite getdefaultinstancefortype();

  /**
   * returns true if all required fields in the message and all embedded
   * messages are set, false otherwise.
   *
   * <p>see also: {@link messageorbuilder#getinitializationerrorstring()}
   */
  boolean isinitialized();

}
