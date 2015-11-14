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
 * this class is used internally by the protocol buffer library and generated
 * message implementations.  it is public only because those generated messages
 * do not reside in the {@code protobuf} package.  others should not use this
 * class directly.
 *
 * this class contains constants and helper functions useful for dealing with
 * the protocol buffer wire format.
 *
 * @author kenton@google.com kenton varda
 */
public final class wireformat {
  // do not allow instantiation.
  private wireformat() {}

  public static final int wiretype_varint           = 0;
  public static final int wiretype_fixed64          = 1;
  public static final int wiretype_length_delimited = 2;
  public static final int wiretype_start_group      = 3;
  public static final int wiretype_end_group        = 4;
  public static final int wiretype_fixed32          = 5;

  static final int tag_type_bits = 3;
  static final int tag_type_mask = (1 << tag_type_bits) - 1;

  /** given a tag value, determines the wire type (the lower 3 bits). */
  static int gettagwiretype(final int tag) {
    return tag & tag_type_mask;
  }

  /** given a tag value, determines the field number (the upper 29 bits). */
  public static int gettagfieldnumber(final int tag) {
    return tag >>> tag_type_bits;
  }

  /** makes a tag value given a field number and wire type. */
  static int maketag(final int fieldnumber, final int wiretype) {
    return (fieldnumber << tag_type_bits) | wiretype;
  }

  /**
   * lite equivalent to {@link descriptors.fielddescriptor.javatype}.  this is
   * only here to support the lite runtime and should not be used by users.
   */
  public enum javatype {
    int(0),
    long(0l),
    float(0f),
    double(0d),
    boolean(false),
    string(""),
    byte_string(bytestring.empty),
    enum(null),
    message(null);

    javatype(final object defaultdefault) {
      this.defaultdefault = defaultdefault;
    }

    /**
     * the default default value for fields of this type, if it's a primitive
     * type.
     */
    object getdefaultdefault() {
      return defaultdefault;
    }

    private final object defaultdefault;
  }

  /**
   * lite equivalent to {@link descriptors.fielddescriptor.type}.  this is
   * only here to support the lite runtime and should not be used by users.
   */
  public enum fieldtype {
    double  (javatype.double     , wiretype_fixed64         ),
    float   (javatype.float      , wiretype_fixed32         ),
    int64   (javatype.long       , wiretype_varint          ),
    uint64  (javatype.long       , wiretype_varint          ),
    int32   (javatype.int        , wiretype_varint          ),
    fixed64 (javatype.long       , wiretype_fixed64         ),
    fixed32 (javatype.int        , wiretype_fixed32         ),
    bool    (javatype.boolean    , wiretype_varint          ),
    string  (javatype.string     , wiretype_length_delimited) {
      public boolean ispackable() { return false; }
    },
    group   (javatype.message    , wiretype_start_group     ) {
      public boolean ispackable() { return false; }
    },
    message (javatype.message    , wiretype_length_delimited) {
      public boolean ispackable() { return false; }
    },
    bytes   (javatype.byte_string, wiretype_length_delimited) {
      public boolean ispackable() { return false; }
    },
    uint32  (javatype.int        , wiretype_varint          ),
    enum    (javatype.enum       , wiretype_varint          ),
    sfixed32(javatype.int        , wiretype_fixed32         ),
    sfixed64(javatype.long       , wiretype_fixed64         ),
    sint32  (javatype.int        , wiretype_varint          ),
    sint64  (javatype.long       , wiretype_varint          );

    fieldtype(final javatype javatype, final int wiretype) {
      this.javatype = javatype;
      this.wiretype = wiretype;
    }

    private final javatype javatype;
    private final int wiretype;

    public javatype getjavatype() { return javatype; }
    public int getwiretype() { return wiretype; }

    public boolean ispackable() { return true; }
  }

  // field numbers for fields in messageset wire format.
  static final int message_set_item    = 1;
  static final int message_set_type_id = 2;
  static final int message_set_message = 3;

  // tag numbers.
  static final int message_set_item_tag =
    maketag(message_set_item, wiretype_start_group);
  static final int message_set_item_end_tag =
    maketag(message_set_item, wiretype_end_group);
  static final int message_set_type_id_tag =
    maketag(message_set_type_id, wiretype_varint);
  static final int message_set_message_tag =
    maketag(message_set_message, wiretype_length_delimited);
}
