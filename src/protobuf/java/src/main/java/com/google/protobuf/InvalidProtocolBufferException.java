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

import java.io.ioexception;

/**
 * thrown when a protocol message being parsed is invalid in some way,
 * e.g. it contains a malformed varint or a negative byte length.
 *
 * @author kenton@google.com kenton varda
 */
public class invalidprotocolbufferexception extends ioexception {
  private static final long serialversionuid = -1616151763072450476l;
  private messagelite unfinishedmessage = null;

  public invalidprotocolbufferexception(final string description) {
    super(description);
  }

  /**
   * attaches an unfinished message to the exception to support best-effort
   * parsing in {@code parser} interface.
   *
   * @return this
   */
  public invalidprotocolbufferexception setunfinishedmessage(
      messagelite unfinishedmessage) {
    this.unfinishedmessage = unfinishedmessage;
    return this;
  }

  /**
   * returns the unfinished message attached to the exception, or null if
   * no message is attached.
   */
  public messagelite getunfinishedmessage() {
    return unfinishedmessage;
  }

  static invalidprotocolbufferexception truncatedmessage() {
    return new invalidprotocolbufferexception(
      "while parsing a protocol message, the input ended unexpectedly " +
      "in the middle of a field.  this could mean either than the " +
      "input has been truncated or that an embedded message " +
      "misreported its own length.");
  }

  static invalidprotocolbufferexception negativesize() {
    return new invalidprotocolbufferexception(
      "codedinputstream encountered an embedded string or message " +
      "which claimed to have negative size.");
  }

  static invalidprotocolbufferexception malformedvarint() {
    return new invalidprotocolbufferexception(
      "codedinputstream encountered a malformed varint.");
  }

  static invalidprotocolbufferexception invalidtag() {
    return new invalidprotocolbufferexception(
      "protocol message contained an invalid tag (zero).");
  }

  static invalidprotocolbufferexception invalidendtag() {
    return new invalidprotocolbufferexception(
      "protocol message end-group tag did not match expected tag.");
  }

  static invalidprotocolbufferexception invalidwiretype() {
    return new invalidprotocolbufferexception(
      "protocol message tag had invalid wire type.");
  }

  static invalidprotocolbufferexception recursionlimitexceeded() {
    return new invalidprotocolbufferexception(
      "protocol message had too many levels of nesting.  may be malicious.  " +
      "use codedinputstream.setrecursionlimit() to increase the depth limit.");
  }

  static invalidprotocolbufferexception sizelimitexceeded() {
    return new invalidprotocolbufferexception(
      "protocol message was too large.  may be malicious.  " +
      "use codedinputstream.setsizelimit() to increase the size limit.");
  }
}
