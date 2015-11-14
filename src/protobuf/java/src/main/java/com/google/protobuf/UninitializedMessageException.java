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

import java.util.collections;
import java.util.list;

/**
 * thrown when attempting to build a protocol message that is missing required
 * fields.  this is a {@code runtimeexception} because it normally represents
 * a programming error:  it happens when some code which constructs a message
 * fails to set all the fields.  {@code parsefrom()} methods <b>do not</b>
 * throw this; they throw an {@link invalidprotocolbufferexception} if
 * required fields are missing, because it is not a programming error to
 * receive an incomplete message.  in other words,
 * {@code uninitializedmessageexception} should never be thrown by correct
 * code, but {@code invalidprotocolbufferexception} might be.
 *
 * @author kenton@google.com kenton varda
 */
public class uninitializedmessageexception extends runtimeexception {
  private static final long serialversionuid = -7466929953374883507l;

  public uninitializedmessageexception(final messagelite message) {
    super("message was missing required fields.  (lite runtime could not " +
          "determine which fields were missing).");
    missingfields = null;
  }

  public uninitializedmessageexception(final list<string> missingfields) {
    super(builddescription(missingfields));
    this.missingfields = missingfields;
  }

  private final list<string> missingfields;

  /**
   * get a list of human-readable names of required fields missing from this
   * message.  each name is a full path to a field, e.g. "foo.bar[5].baz".
   * returns null if the lite runtime was used, since it lacks the ability to
   * find missing fields.
   */
  public list<string> getmissingfields() {
    return collections.unmodifiablelist(missingfields);
  }

  /**
   * converts this exception to an {@link invalidprotocolbufferexception}.
   * when a parsed message is missing required fields, this should be thrown
   * instead of {@code uninitializedmessageexception}.
   */
  public invalidprotocolbufferexception asinvalidprotocolbufferexception() {
    return new invalidprotocolbufferexception(getmessage());
  }

  /** construct the description string for this exception. */
  private static string builddescription(final list<string> missingfields) {
    final stringbuilder description =
      new stringbuilder("message missing required fields: ");
    boolean first = true;
    for (final string field : missingfields) {
      if (first) {
        first = false;
      } else {
        description.append(", ");
      }
      description.append(field);
    }
    return description.tostring();
  }
}
