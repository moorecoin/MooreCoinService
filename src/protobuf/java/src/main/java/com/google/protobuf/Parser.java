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

import java.io.inputstream;

/**
 * abstract interface for parsing protocol messages.
 *
 * @author liujisi@google.com (pherl liu)
 */
public interface parser<messagetype> {
  /**
   * parses a message of {@code messagetype} from the input.
   *
   * <p>note:  the caller should call
   * {@link codedinputstream#checklasttagwas(int)} after calling this to
   * verify that the last tag seen was the appropriate end-group tag,
   * or zero for eof.
   */
  public messagetype parsefrom(codedinputstream input)
      throws invalidprotocolbufferexception;

  /**
   * like {@link #parsefrom(codedinputstream)}, but also parses extensions.
   * the extensions that you want to be able to parse must be registered in
   * {@code extensionregistry}. extensions not in the registry will be treated
   * as unknown fields.
   */
  public messagetype parsefrom(codedinputstream input,
                               extensionregistrylite extensionregistry)
      throws invalidprotocolbufferexception;

  /**
   * like {@link #parsefrom(codedinputstream)}, but does not throw an
   * exception if the message is missing required fields. instead, a partial
   * message is returned.
   */
  public messagetype parsepartialfrom(codedinputstream input)
      throws invalidprotocolbufferexception;

  /**
   * like {@link #parsefrom(codedinputstream input, extensionregistrylite)},
   * but does not throw an exception if the message is missing required fields.
   * instead, a partial message is returned.
   */
  public messagetype parsepartialfrom(codedinputstream input,
                                      extensionregistrylite extensionregistry)
      throws invalidprotocolbufferexception;

  // ---------------------------------------------------------------
  // convenience methods.

  /**
   * parses {@code data} as a message of {@code messagetype}.
   * this is just a small wrapper around {@link #parsefrom(codedinputstream)}.
   */
  public messagetype parsefrom(bytestring data)
      throws invalidprotocolbufferexception;

  /**
   * parses {@code data} as a message of {@code messagetype}.
   * this is just a small wrapper around
   * {@link #parsefrom(codedinputstream, extensionregistrylite)}.
   */
  public messagetype parsefrom(bytestring data,
                               extensionregistrylite extensionregistry)
      throws invalidprotocolbufferexception;

  /**
   * like {@link #parsefrom(bytestring)}, but does not throw an
   * exception if the message is missing required fields. instead, a partial
   * message is returned.
   */
  public messagetype parsepartialfrom(bytestring data)
      throws invalidprotocolbufferexception;

  /**
   * like {@link #parsefrom(bytestring, extensionregistrylite)},
   * but does not throw an exception if the message is missing required fields.
   * instead, a partial message is returned.
   */
  public messagetype parsepartialfrom(bytestring data,
                                      extensionregistrylite extensionregistry)
      throws invalidprotocolbufferexception;

  /**
   * parses {@code data} as a message of {@code messagetype}.
   * this is just a small wrapper around {@link #parsefrom(codedinputstream)}.
   */
  public messagetype parsefrom(byte[] data, int off, int len)
      throws invalidprotocolbufferexception;

  /**
   * parses {@code data} as a message of {@code messagetype}.
   * this is just a small wrapper around
   * {@link #parsefrom(codedinputstream, extensionregistrylite)}.
   */
  public messagetype parsefrom(byte[] data, int off, int len,
                               extensionregistrylite extensionregistry)
      throws invalidprotocolbufferexception;

  /**
   * parses {@code data} as a message of {@code messagetype}.
   * this is just a small wrapper around {@link #parsefrom(codedinputstream)}.
   */
  public messagetype parsefrom(byte[] data)
      throws invalidprotocolbufferexception;

  /**
   * parses {@code data} as a message of {@code messagetype}.
   * this is just a small wrapper around
   * {@link #parsefrom(codedinputstream, extensionregistrylite)}.
   */
  public messagetype parsefrom(byte[] data,
                               extensionregistrylite extensionregistry)
      throws invalidprotocolbufferexception;

  /**
   * like {@link #parsefrom(byte[], int, int)}, but does not throw an
   * exception if the message is missing required fields. instead, a partial
   * message is returned.
   */
  public messagetype parsepartialfrom(byte[] data, int off, int len)
      throws invalidprotocolbufferexception;

  /**
   * like {@link #parsefrom(bytestring, extensionregistrylite)},
   * but does not throw an exception if the message is missing required fields.
   * instead, a partial message is returned.
   */
  public messagetype parsepartialfrom(byte[] data, int off, int len,
                                      extensionregistrylite extensionregistry)
      throws invalidprotocolbufferexception;

  /**
   * like {@link #parsefrom(byte[])}, but does not throw an
   * exception if the message is missing required fields. instead, a partial
   * message is returned.
   */
  public messagetype parsepartialfrom(byte[] data)
      throws invalidprotocolbufferexception;

  /**
   * like {@link #parsefrom(byte[], extensionregistrylite)},
   * but does not throw an exception if the message is missing required fields.
   * instead, a partial message is returned.
   */
  public messagetype parsepartialfrom(byte[] data,
                                      extensionregistrylite extensionregistry)
      throws invalidprotocolbufferexception;

  /**
   * parse a message of {@code messagetype} from {@code input}.
   * this is just a small wrapper around {@link #parsefrom(codedinputstream)}.
   * note that this method always reads the <i>entire</i> input (unless it
   * throws an exception).  if you want it to stop earlier, you will need to
   * wrap your input in some wrapper stream that limits reading.  or, use
   * {@link messagelite#writedelimitedto(java.io.outputstream)} to write your
   * message and {@link #parsedelimitedfrom(inputstream)} to read it.
   * <p>
   * despite usually reading the entire input, this does not close the stream.
   */
  public messagetype parsefrom(inputstream input)
      throws invalidprotocolbufferexception;

  /**
   * parses a message of {@code messagetype} from {@code input}.
   * this is just a small wrapper around
   * {@link #parsefrom(codedinputstream, extensionregistrylite)}.
   */
  public messagetype parsefrom(inputstream input,
                               extensionregistrylite extensionregistry)
      throws invalidprotocolbufferexception;

  /**
   * like {@link #parsefrom(inputstream)}, but does not throw an
   * exception if the message is missing required fields. instead, a partial
   * message is returned.
   */
  public messagetype parsepartialfrom(inputstream input)
      throws invalidprotocolbufferexception;

  /**
   * like {@link #parsefrom(inputstream, extensionregistrylite)},
   * but does not throw an exception if the message is missing required fields.
   * instead, a partial message is returned.
   */
  public messagetype parsepartialfrom(inputstream input,
                                      extensionregistrylite extensionregistry)
      throws invalidprotocolbufferexception;

  /**
   * like {@link #parsefrom(inputstream)}, but does not read util eof.
   * instead, the size of message (encoded as a varint) is read first,
   * then the message data. use
   * {@link messagelite#writedelimitedto(java.io.outputstream)} to write
   * messages in this format.
   *
   * @return true if successful, or false if the stream is at eof when the
   *         method starts. any other error (including reaching eof during
   *         parsing) will cause an exception to be thrown.
   */
  public messagetype parsedelimitedfrom(inputstream input)
      throws invalidprotocolbufferexception;

  /**
   * like {@link #parsedelimitedfrom(inputstream)} but supporting extensions.
   */
  public messagetype parsedelimitedfrom(inputstream input,
                                        extensionregistrylite extensionregistry)
      throws invalidprotocolbufferexception;

  /**
   * like {@link #parsedelimitedfrom(inputstream)}, but does not throw an
   * exception if the message is missing required fields. instead, a partial
   * message is returned.
   */
  public messagetype parsepartialdelimitedfrom(inputstream input)
      throws invalidprotocolbufferexception;

  /**
   * like {@link #parsedelimitedfrom(inputstream, extensionregistrylite)},
   * but does not throw an exception if the message is missing required fields.
   * instead, a partial message is returned.
   */
  public messagetype parsepartialdelimitedfrom(
      inputstream input,
      extensionregistrylite extensionregistry)
      throws invalidprotocolbufferexception;
}
