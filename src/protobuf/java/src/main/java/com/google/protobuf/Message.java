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
import java.util.map;

/**
 * abstract interface implemented by protocol message objects.
 * <p>
 * see also {@link messagelite}, which defines most of the methods that typical
 * users care about.  {@link message} adds to it methods that are not available
 * in the "lite" runtime.  the biggest added features are introspection and
 * reflection -- i.e., getting descriptors for the message type and accessing
 * the field values dynamically.
 *
 * @author kenton@google.com kenton varda
 */
public interface message extends messagelite, messageorbuilder {

  // (from messagelite, re-declared here only for return type covariance.)
  parser<? extends message> getparserfortype();

  // -----------------------------------------------------------------
  // comparison and hashing

  /**
   * compares the specified object with this message for equality.  returns
   * {@code true} if the given object is a message of the same type (as
   * defined by {@code getdescriptorfortype()}) and has identical values for
   * all of its fields.  subclasses must implement this; inheriting
   * {@code object.equals()} is incorrect.
   *
   * @param other object to be compared for equality with this message
   * @return {@code true} if the specified object is equal to this message
   */
  @override
  boolean equals(object other);

  /**
   * returns the hash code value for this message.  the hash code of a message
   * should mix the message's type (object identity of the descriptor) with its
   * contents (known and unknown field values).  subclasses must implement this;
   * inheriting {@code object.hashcode()} is incorrect.
   *
   * @return the hash code value for this message
   * @see map#hashcode()
   */
  @override
  int hashcode();

  // -----------------------------------------------------------------
  // convenience methods.

  /**
   * converts the message to a string in protocol buffer text format. this is
   * just a trivial wrapper around {@link
   * textformat#printtostring(messageorbuilder)}.
   */
  @override
  string tostring();

  // =================================================================
  // builders

  // (from messagelite, re-declared here only for return type covariance.)
  builder newbuilderfortype();
  builder tobuilder();

  /**
   * abstract interface implemented by protocol message builders.
   */
  interface builder extends messagelite.builder, messageorbuilder {
    // (from messagelite.builder, re-declared here only for return type
    // covariance.)
    builder clear();

    /**
     * merge {@code other} into the message being built.  {@code other} must
     * have the exact same type as {@code this} (i.e.
     * {@code getdescriptorfortype() == other.getdescriptorfortype()}).
     *
     * merging occurs as follows.  for each field:<br>
     * * for singular primitive fields, if the field is set in {@code other},
     *   then {@code other}'s value overwrites the value in this message.<br>
     * * for singular message fields, if the field is set in {@code other},
     *   it is merged into the corresponding sub-message of this message
     *   using the same merging rules.<br>
     * * for repeated fields, the elements in {@code other} are concatenated
     *   with the elements in this message.
     *
     * this is equivalent to the {@code message::mergefrom} method in c++.
     */
    builder mergefrom(message other);

    // (from messagelite.builder, re-declared here only for return type
    // covariance.)
    message build();
    message buildpartial();
    builder clone();
    builder mergefrom(codedinputstream input) throws ioexception;
    builder mergefrom(codedinputstream input,
                      extensionregistrylite extensionregistry)
                      throws ioexception;

    /**
     * get the message's type's descriptor.
     * see {@link message#getdescriptorfortype()}.
     */
    descriptors.descriptor getdescriptorfortype();

    /**
     * create a builder for messages of the appropriate type for the given
     * field.  messages built with this can then be passed to setfield(),
     * setrepeatedfield(), or addrepeatedfield().
     */
    builder newbuilderforfield(descriptors.fielddescriptor field);

    /**
     * get a nested builder instance for the given field.
     * <p>
     * normally, we hold a reference to the immutable message object for the
     * message type field. some implementations(the generated message builders),
     * however, can also hold a reference to the builder object (a nested
     * builder) for the field.
     * <p>
     * if the field is already backed up by a nested builder, the nested builder
     * will be returned. otherwise, a new field builder will be created and
     * returned. the original message field (if exist) will be merged into the
     * field builder, which will then be nested into its parent builder.
     * <p>
     * note: implementations that do not support nested builders will throw
     * <code>unsupportedexception</code>.
     */
    builder getfieldbuilder(descriptors.fielddescriptor field);

    /**
     * sets a field to the given value.  the value must be of the correct type
     * for this field, i.e. the same type that
     * {@link message#getfield(descriptors.fielddescriptor)} would return.
     */
    builder setfield(descriptors.fielddescriptor field, object value);

    /**
     * clears the field.  this is exactly equivalent to calling the generated
     * "clear" accessor method corresponding to the field.
     */
    builder clearfield(descriptors.fielddescriptor field);

    /**
     * sets an element of a repeated field to the given value.  the value must
     * be of the correct type for this field, i.e. the same type that
     * {@link message#getrepeatedfield(descriptors.fielddescriptor,int)} would
     * return.
     * @throws illegalargumentexception the field is not a repeated field, or
     *           {@code field.getcontainingtype() != getdescriptorfortype()}.
     */
    builder setrepeatedfield(descriptors.fielddescriptor field,
                             int index, object value);

    /**
     * like {@code setrepeatedfield}, but appends the value as a new element.
     * @throws illegalargumentexception the field is not a repeated field, or
     *           {@code field.getcontainingtype() != getdescriptorfortype()}.
     */
    builder addrepeatedfield(descriptors.fielddescriptor field, object value);

    /** set the {@link unknownfieldset} for this message. */
    builder setunknownfields(unknownfieldset unknownfields);

    /**
     * merge some unknown fields into the {@link unknownfieldset} for this
     * message.
     */
    builder mergeunknownfields(unknownfieldset unknownfields);

    // ---------------------------------------------------------------
    // convenience methods.

    // (from messagelite.builder, re-declared here only for return type
    // covariance.)
    builder mergefrom(bytestring data) throws invalidprotocolbufferexception;
    builder mergefrom(bytestring data,
                      extensionregistrylite extensionregistry)
                      throws invalidprotocolbufferexception;
    builder mergefrom(byte[] data) throws invalidprotocolbufferexception;
    builder mergefrom(byte[] data, int off, int len)
                      throws invalidprotocolbufferexception;
    builder mergefrom(byte[] data,
                      extensionregistrylite extensionregistry)
                      throws invalidprotocolbufferexception;
    builder mergefrom(byte[] data, int off, int len,
                      extensionregistrylite extensionregistry)
                      throws invalidprotocolbufferexception;
    builder mergefrom(inputstream input) throws ioexception;
    builder mergefrom(inputstream input,
                      extensionregistrylite extensionregistry)
                      throws ioexception;
    boolean mergedelimitedfrom(inputstream input)
                               throws ioexception;
    boolean mergedelimitedfrom(inputstream input,
                               extensionregistrylite extensionregistry)
                               throws ioexception;
  }
}
