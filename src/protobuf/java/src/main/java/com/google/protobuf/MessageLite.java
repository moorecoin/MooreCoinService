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

// todo(kenton):  use generics?  e.g. builder<buildertype extends builder>, then
//   mergefrom*() could return buildertype for better type-safety.

package com.google.protobuf;

import java.io.ioexception;
import java.io.inputstream;
import java.io.outputstream;

/**
 * abstract interface implemented by protocol message objects.
 *
 * <p>this interface is implemented by all protocol message objects.  non-lite
 * messages additionally implement the message interface, which is a subclass
 * of messagelite.  use messagelite instead when you only need the subset of
 * features which it supports -- namely, nothing that uses descriptors or
 * reflection.  you can instruct the protocol compiler to generate classes
 * which implement only messagelite, not the full message interface, by adding
 * the follow line to the .proto file:
 * <pre>
 *   option optimize_for = lite_runtime;
 * </pre>
 *
 * <p>this is particularly useful on resource-constrained systems where the
 * full protocol buffers runtime library is too big.
 *
 * <p>note that on non-constrained systems (e.g. servers) when you need to link
 * in lots of protocol definitions, a better way to reduce total code footprint
 * is to use {@code optimize_for = code_size}.  this will make the generated
 * code smaller while still supporting all the same features (at the expense of
 * speed).  {@code optimize_for = lite_runtime} is best when you only have a
 * small number of message types linked into your binary, in which case the
 * size of the protocol buffers runtime itself is the biggest problem.
 *
 * @author kenton@google.com kenton varda
 */
public interface messagelite extends messageliteorbuilder {


  /**
   * serializes the message and writes it to {@code output}.  this does not
   * flush or close the stream.
   */
  void writeto(codedoutputstream output) throws ioexception;

  /**
   * get the number of bytes required to encode this message.  the result
   * is only computed on the first call and memoized after that.
   */
  int getserializedsize();


  /**
   * gets the parser for a message of the same type as this message.
   */
  parser<? extends messagelite> getparserfortype();

  // -----------------------------------------------------------------
  // convenience methods.

  /**
   * serializes the message to a {@code bytestring} and returns it. this is
   * just a trivial wrapper around
   * {@link #writeto(codedoutputstream)}.
   */
  bytestring tobytestring();

  /**
   * serializes the message to a {@code byte} array and returns it.  this is
   * just a trivial wrapper around
   * {@link #writeto(codedoutputstream)}.
   */
  byte[] tobytearray();

  /**
   * serializes the message and writes it to {@code output}.  this is just a
   * trivial wrapper around {@link #writeto(codedoutputstream)}.  this does
   * not flush or close the stream.
   * <p>
   * note:  protocol buffers are not self-delimiting.  therefore, if you write
   * any more data to the stream after the message, you must somehow ensure
   * that the parser on the receiving end does not interpret this as being
   * part of the protocol message.  this can be done e.g. by writing the size
   * of the message before the data, then making sure to limit the input to
   * that size on the receiving end (e.g. by wrapping the inputstream in one
   * which limits the input).  alternatively, just use
   * {@link #writedelimitedto(outputstream)}.
   */
  void writeto(outputstream output) throws ioexception;

  /**
   * like {@link #writeto(outputstream)}, but writes the size of the message
   * as a varint before writing the data.  this allows more data to be written
   * to the stream after the message without the need to delimit the message
   * data yourself.  use {@link builder#mergedelimitedfrom(inputstream)} (or
   * the static method {@code yourmessagetype.parsedelimitedfrom(inputstream)})
   * to parse messages written by this method.
   */
  void writedelimitedto(outputstream output) throws ioexception;

  // =================================================================
  // builders

  /**
   * constructs a new builder for a message of the same type as this message.
   */
  builder newbuilderfortype();

  /**
   * constructs a builder initialized with the current message.  use this to
   * derive a new message from the current one.
   */
  builder tobuilder();

  /**
   * abstract interface implemented by protocol message builders.
   */
  interface builder extends messageliteorbuilder, cloneable {
    /** resets all fields to their default values. */
    builder clear();

    /**
     * constructs the message based on the state of the builder. subsequent
     * changes to the builder will not affect the returned message.
     * @throws uninitializedmessageexception the message is missing one or more
     *         required fields (i.e. {@link #isinitialized()} returns false).
     *         use {@link #buildpartial()} to bypass this check.
     */
    messagelite build();

    /**
     * like {@link #build()}, but does not throw an exception if the message
     * is missing required fields.  instead, a partial message is returned.
     * subsequent changes to the builder will not affect the returned message.
     */
    messagelite buildpartial();

    /**
     * clones the builder.
     * @see object#clone()
     */
    builder clone();

    /**
     * parses a message of this type from the input and merges it with this
     * message.
     *
     * <p>warning:  this does not verify that all required fields are present in
     * the input message.  if you call {@link #build()} without setting all
     * required fields, it will throw an {@link uninitializedmessageexception},
     * which is a {@code runtimeexception} and thus might not be caught.  there
     * are a few good ways to deal with this:
     * <ul>
     *   <li>call {@link #isinitialized()} to verify that all required fields
     *       are set before building.
     *   <li>use {@code buildpartial()} to build, which ignores missing
     *       required fields.
     * </ul>
     *
     * <p>note:  the caller should call
     * {@link codedinputstream#checklasttagwas(int)} after calling this to
     * verify that the last tag seen was the appropriate end-group tag,
     * or zero for eof.
     */
    builder mergefrom(codedinputstream input) throws ioexception;

    /**
     * like {@link builder#mergefrom(codedinputstream)}, but also
     * parses extensions.  the extensions that you want to be able to parse
     * must be registered in {@code extensionregistry}.  extensions not in
     * the registry will be treated as unknown fields.
     */
    builder mergefrom(codedinputstream input,
                      extensionregistrylite extensionregistry)
                      throws ioexception;

    // ---------------------------------------------------------------
    // convenience methods.

    /**
     * parse {@code data} as a message of this type and merge it with the
     * message being built.  this is just a small wrapper around
     * {@link #mergefrom(codedinputstream)}.
     *
     * @return this
     */
    builder mergefrom(bytestring data) throws invalidprotocolbufferexception;

    /**
     * parse {@code data} as a message of this type and merge it with the
     * message being built.  this is just a small wrapper around
     * {@link #mergefrom(codedinputstream,extensionregistrylite)}.
     *
     * @return this
     */
    builder mergefrom(bytestring data,
                      extensionregistrylite extensionregistry)
                      throws invalidprotocolbufferexception;

    /**
     * parse {@code data} as a message of this type and merge it with the
     * message being built.  this is just a small wrapper around
     * {@link #mergefrom(codedinputstream)}.
     *
     * @return this
     */
    builder mergefrom(byte[] data) throws invalidprotocolbufferexception;

    /**
     * parse {@code data} as a message of this type and merge it with the
     * message being built.  this is just a small wrapper around
     * {@link #mergefrom(codedinputstream)}.
     *
     * @return this
     */
    builder mergefrom(byte[] data, int off, int len)
                      throws invalidprotocolbufferexception;

    /**
     * parse {@code data} as a message of this type and merge it with the
     * message being built.  this is just a small wrapper around
     * {@link #mergefrom(codedinputstream,extensionregistrylite)}.
     *
     * @return this
     */
    builder mergefrom(byte[] data,
                      extensionregistrylite extensionregistry)
                      throws invalidprotocolbufferexception;

    /**
     * parse {@code data} as a message of this type and merge it with the
     * message being built.  this is just a small wrapper around
     * {@link #mergefrom(codedinputstream,extensionregistrylite)}.
     *
     * @return this
     */
    builder mergefrom(byte[] data, int off, int len,
                      extensionregistrylite extensionregistry)
                      throws invalidprotocolbufferexception;

    /**
     * parse a message of this type from {@code input} and merge it with the
     * message being built.  this is just a small wrapper around
     * {@link #mergefrom(codedinputstream)}.  note that this method always
     * reads the <i>entire</i> input (unless it throws an exception).  if you
     * want it to stop earlier, you will need to wrap your input in some
     * wrapper stream that limits reading.  or, use
     * {@link messagelite#writedelimitedto(outputstream)} to write your message
     * and {@link #mergedelimitedfrom(inputstream)} to read it.
     * <p>
     * despite usually reading the entire input, this does not close the stream.
     *
     * @return this
     */
    builder mergefrom(inputstream input) throws ioexception;

    /**
     * parse a message of this type from {@code input} and merge it with the
     * message being built.  this is just a small wrapper around
     * {@link #mergefrom(codedinputstream,extensionregistrylite)}.
     *
     * @return this
     */
    builder mergefrom(inputstream input,
                      extensionregistrylite extensionregistry)
                      throws ioexception;

    /**
     * like {@link #mergefrom(inputstream)}, but does not read until eof.
     * instead, the size of the message (encoded as a varint) is read first,
     * then the message data.  use
     * {@link messagelite#writedelimitedto(outputstream)} to write messages in
     * this format.
     *
     * @return true if successful, or false if the stream is at eof when the
     *         method starts.  any other error (including reaching eof during
     *         parsing) will cause an exception to be thrown.
     */
    boolean mergedelimitedfrom(inputstream input)
                               throws ioexception;

    /**
     * like {@link #mergedelimitedfrom(inputstream)} but supporting extensions.
     */
    boolean mergedelimitedfrom(inputstream input,
                               extensionregistrylite extensionregistry)
                               throws ioexception;
  }
}
