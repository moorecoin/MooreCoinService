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
import java.util.map;

/**
 * base interface for methods common to {@link message} and
 * {@link message.builder} to provide type equivalency.
 *
 * @author jonp@google.com (jon perlow)
 */
public interface messageorbuilder extends messageliteorbuilder {

  // (from messagelite, re-declared here only for return type covariance.)
  //@override (java 1.6 override semantics, but we must support 1.5)
  message getdefaultinstancefortype();

  /**
   * returns a list of field paths (e.g. "foo.bar.baz") of required fields
   * which are not set in this message.  you should call
   * {@link messageliteorbuilder#isinitialized()} first to check if there
   * are any missing fields, as that method is likely to be much faster
   * than this one even when the message is fully-initialized.
   */
  list<string> findinitializationerrors();

  /**
   * returns a comma-delimited list of required fields which are not set
   * in this message object.  you should call
   * {@link messageliteorbuilder#isinitialized()} first to check if there
   * are any missing fields, as that method is likely to be much faster
   * than this one even when the message is fully-initialized.
   */
  string getinitializationerrorstring();

  /**
   * get the message's type's descriptor.  this differs from the
   * {@code getdescriptor()} method of generated message classes in that
   * this method is an abstract method of the {@code message} interface
   * whereas {@code getdescriptor()} is a static method of a specific class.
   * they return the same thing.
   */
  descriptors.descriptor getdescriptorfortype();

  /**
   * returns a collection of all the fields in this message which are set
   * and their corresponding values.  a singular ("required" or "optional")
   * field is set iff hasfield() returns true for that field.  a "repeated"
   * field is set iff getrepeatedfieldsize() is greater than zero.  the
   * values are exactly what would be returned by calling
   * {@link #getfield(descriptors.fielddescriptor)} for each field.  the map
   * is guaranteed to be a sorted map, so iterating over it will return fields
   * in order by field number.
   * <br>
   * if this is for a builder, the returned map may or may not reflect future
   * changes to the builder.  either way, the returned map is itself
   * unmodifiable.
   */
  map<descriptors.fielddescriptor, object> getallfields();

  /**
   * returns true if the given field is set.  this is exactly equivalent to
   * calling the generated "has" accessor method corresponding to the field.
   * @throws illegalargumentexception the field is a repeated field, or
   *           {@code field.getcontainingtype() != getdescriptorfortype()}.
   */
  boolean hasfield(descriptors.fielddescriptor field);

  /**
   * obtains the value of the given field, or the default value if it is
   * not set.  for primitive fields, the boxed primitive value is returned.
   * for enum fields, the enumvaluedescriptor for the value is returned. for
   * embedded message fields, the sub-message is returned.  for repeated
   * fields, a java.util.list is returned.
   */
  object getfield(descriptors.fielddescriptor field);

  /**
   * gets the number of elements of a repeated field.  this is exactly
   * equivalent to calling the generated "count" accessor method corresponding
   * to the field.
   * @throws illegalargumentexception the field is not a repeated field, or
   *           {@code field.getcontainingtype() != getdescriptorfortype()}.
   */
  int getrepeatedfieldcount(descriptors.fielddescriptor field);

  /**
   * gets an element of a repeated field.  for primitive fields, the boxed
   * primitive value is returned.  for enum fields, the enumvaluedescriptor
   * for the value is returned. for embedded message fields, the sub-message
   * is returned.
   * @throws illegalargumentexception the field is not a repeated field, or
   *           {@code field.getcontainingtype() != getdescriptorfortype()}.
   */
  object getrepeatedfield(descriptors.fielddescriptor field, int index);

  /** get the {@link unknownfieldset} for this message. */
  unknownfieldset getunknownfields();
}
