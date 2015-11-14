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

import com.google.protobuf.abstractmessagelite.builder.limitedinputstream;

import java.io.ioexception;
import java.io.inputstream;
import java.io.outputstream;
import java.util.arraylist;
import java.util.arrays;
import java.util.collections;
import java.util.list;
import java.util.map;
import java.util.treemap;

/**
 * {@code unknownfieldset} is used to keep track of fields which were seen when
 * parsing a protocol message but whose field numbers or types are unrecognized.
 * this most frequently occurs when new fields are added to a message type
 * and then messages containing those fields are read by old software that was
 * compiled before the new types were added.
 *
 * <p>every {@link message} contains an {@code unknownfieldset} (and every
 * {@link message.builder} contains an {@link builder}).
 *
 * <p>most users will never need to use this class.
 *
 * @author kenton@google.com kenton varda
 */
public final class unknownfieldset implements messagelite {
  private unknownfieldset() {}

  /** create a new {@link builder}. */
  public static builder newbuilder() {
    return builder.create();
  }

  /**
   * create a new {@link builder} and initialize it to be a copy
   * of {@code copyfrom}.
   */
  public static builder newbuilder(final unknownfieldset copyfrom) {
    return newbuilder().mergefrom(copyfrom);
  }

  /** get an empty {@code unknownfieldset}. */
  public static unknownfieldset getdefaultinstance() {
    return defaultinstance;
  }
  public unknownfieldset getdefaultinstancefortype() {
    return defaultinstance;
  }
  private static final unknownfieldset defaultinstance =
    new unknownfieldset(collections.<integer, field>emptymap());

  /**
   * construct an {@code unknownfieldset} around the given map.  the map is
   * expected to be immutable.
   */
  private unknownfieldset(final map<integer, field> fields) {
    this.fields = fields;
  }
  private map<integer, field> fields;

  @override
  public boolean equals(final object other) {
    if (this == other) {
      return true;
    }
    return (other instanceof unknownfieldset) &&
           fields.equals(((unknownfieldset) other).fields);
  }

  @override
  public int hashcode() {
    return fields.hashcode();
  }

  /** get a map of fields in the set by number. */
  public map<integer, field> asmap() {
    return fields;
  }

  /** check if the given field number is present in the set. */
  public boolean hasfield(final int number) {
    return fields.containskey(number);
  }

  /**
   * get a field by number.  returns an empty field if not present.  never
   * returns {@code null}.
   */
  public field getfield(final int number) {
    final field result = fields.get(number);
    return (result == null) ? field.getdefaultinstance() : result;
  }

  /** serializes the set and writes it to {@code output}. */
  public void writeto(final codedoutputstream output) throws ioexception {
    for (final map.entry<integer, field> entry : fields.entryset()) {
      entry.getvalue().writeto(entry.getkey(), output);
    }
  }

  /**
   * converts the set to a string in protocol buffer text format. this is
   * just a trivial wrapper around
   * {@link textformat#printtostring(unknownfieldset)}.
   */
  @override
  public string tostring() {
    return textformat.printtostring(this);
  }

  /**
   * serializes the message to a {@code bytestring} and returns it. this is
   * just a trivial wrapper around {@link #writeto(codedoutputstream)}.
   */
  public bytestring tobytestring() {
    try {
      final bytestring.codedbuilder out =
        bytestring.newcodedbuilder(getserializedsize());
      writeto(out.getcodedoutput());
      return out.build();
    } catch (final ioexception e) {
      throw new runtimeexception(
        "serializing to a bytestring threw an ioexception (should " +
        "never happen).", e);
    }
  }

  /**
   * serializes the message to a {@code byte} array and returns it.  this is
   * just a trivial wrapper around {@link #writeto(codedoutputstream)}.
   */
  public byte[] tobytearray() {
    try {
      final byte[] result = new byte[getserializedsize()];
      final codedoutputstream output = codedoutputstream.newinstance(result);
      writeto(output);
      output.checknospaceleft();
      return result;
    } catch (final ioexception e) {
      throw new runtimeexception(
        "serializing to a byte array threw an ioexception " +
        "(should never happen).", e);
    }
  }

  /**
   * serializes the message and writes it to {@code output}.  this is just a
   * trivial wrapper around {@link #writeto(codedoutputstream)}.
   */
  public void writeto(final outputstream output) throws ioexception {
    final codedoutputstream codedoutput = codedoutputstream.newinstance(output);
    writeto(codedoutput);
    codedoutput.flush();
  }

  public void writedelimitedto(outputstream output) throws ioexception {
    final codedoutputstream codedoutput = codedoutputstream.newinstance(output);
    codedoutput.writerawvarint32(getserializedsize());
    writeto(codedoutput);
    codedoutput.flush();
  }

  /** get the number of bytes required to encode this set. */
  public int getserializedsize() {
    int result = 0;
    for (final map.entry<integer, field> entry : fields.entryset()) {
      result += entry.getvalue().getserializedsize(entry.getkey());
    }
    return result;
  }

  /**
   * serializes the set and writes it to {@code output} using
   * {@code messageset} wire format.
   */
  public void writeasmessagesetto(final codedoutputstream output)
      throws ioexception {
    for (final map.entry<integer, field> entry : fields.entryset()) {
      entry.getvalue().writeasmessagesetextensionto(
        entry.getkey(), output);
    }
  }

  /**
   * get the number of bytes required to encode this set using
   * {@code messageset} wire format.
   */
  public int getserializedsizeasmessageset() {
    int result = 0;
    for (final map.entry<integer, field> entry : fields.entryset()) {
      result += entry.getvalue().getserializedsizeasmessagesetextension(
        entry.getkey());
    }
    return result;
  }

  public boolean isinitialized() {
    // unknownfieldsets do not have required fields, so they are always
    // initialized.
    return true;
  }

  /** parse an {@code unknownfieldset} from the given input stream. */
  public static unknownfieldset parsefrom(final codedinputstream input)
                                          throws ioexception {
    return newbuilder().mergefrom(input).build();
  }

  /** parse {@code data} as an {@code unknownfieldset} and return it. */
  public static unknownfieldset parsefrom(final bytestring data)
      throws invalidprotocolbufferexception {
    return newbuilder().mergefrom(data).build();
  }

  /** parse {@code data} as an {@code unknownfieldset} and return it. */
  public static unknownfieldset parsefrom(final byte[] data)
      throws invalidprotocolbufferexception {
    return newbuilder().mergefrom(data).build();
  }

  /** parse an {@code unknownfieldset} from {@code input} and return it. */
  public static unknownfieldset parsefrom(final inputstream input)
                                          throws ioexception {
    return newbuilder().mergefrom(input).build();
  }

  public builder newbuilderfortype() {
    return newbuilder();
  }

  public builder tobuilder() {
    return newbuilder().mergefrom(this);
  }

  /**
   * builder for {@link unknownfieldset}s.
   *
   * <p>note that this class maintains {@link field.builder}s for all fields
   * in the set.  thus, adding one element to an existing {@link field} does not
   * require making a copy.  this is important for efficient parsing of
   * unknown repeated fields.  however, it implies that {@link field}s cannot
   * be constructed independently, nor can two {@link unknownfieldset}s share
   * the same {@code field} object.
   *
   * <p>use {@link unknownfieldset#newbuilder()} to construct a {@code builder}.
   */
  public static final class builder implements messagelite.builder {
    // this constructor should never be called directly (except from 'create').
    private builder() {}

    private map<integer, field> fields;

    // optimization:  we keep around a builder for the last field that was
    //   modified so that we can efficiently add to it multiple times in a
    //   row (important when parsing an unknown repeated field).
    private int lastfieldnumber;
    private field.builder lastfield;

    private static builder create() {
      builder builder = new builder();
      builder.reinitialize();
      return builder;
    }

    /**
     * get a field builder for the given field number which includes any
     * values that already exist.
     */
    private field.builder getfieldbuilder(final int number) {
      if (lastfield != null) {
        if (number == lastfieldnumber) {
          return lastfield;
        }
        // note:  addfield() will reset lastfield and lastfieldnumber.
        addfield(lastfieldnumber, lastfield.build());
      }
      if (number == 0) {
        return null;
      } else {
        final field existing = fields.get(number);
        lastfieldnumber = number;
        lastfield = field.newbuilder();
        if (existing != null) {
          lastfield.mergefrom(existing);
        }
        return lastfield;
      }
    }

    /**
     * build the {@link unknownfieldset} and return it.
     *
     * <p>once {@code build()} has been called, the {@code builder} will no
     * longer be usable.  calling any method after {@code build()} will result
     * in undefined behavior and can cause a {@code nullpointerexception} to be
     * thrown.
     */
    public unknownfieldset build() {
      getfieldbuilder(0);  // force lastfield to be built.
      final unknownfieldset result;
      if (fields.isempty()) {
        result = getdefaultinstance();
      } else {
        result = new unknownfieldset(collections.unmodifiablemap(fields));
      }
      fields = null;
      return result;
    }

    public unknownfieldset buildpartial() {
      // no required fields, so this is the same as build().
      return build();
    }

    @override
    public builder clone() {
      getfieldbuilder(0);  // force lastfield to be built.
      return unknownfieldset.newbuilder().mergefrom(
          new unknownfieldset(fields));
    }

    public unknownfieldset getdefaultinstancefortype() {
      return unknownfieldset.getdefaultinstance();
    }

    private void reinitialize() {
      fields = collections.emptymap();
      lastfieldnumber = 0;
      lastfield = null;
    }

    /** reset the builder to an empty set. */
    public builder clear() {
      reinitialize();
      return this;
    }

    /**
     * merge the fields from {@code other} into this set.  if a field number
     * exists in both sets, {@code other}'s values for that field will be
     * appended to the values in this set.
     */
    public builder mergefrom(final unknownfieldset other) {
      if (other != getdefaultinstance()) {
        for (final map.entry<integer, field> entry : other.fields.entryset()) {
          mergefield(entry.getkey(), entry.getvalue());
        }
      }
      return this;
    }

    /**
     * add a field to the {@code unknownfieldset}.  if a field with the same
     * number already exists, the two are merged.
     */
    public builder mergefield(final int number, final field field) {
      if (number == 0) {
        throw new illegalargumentexception("zero is not a valid field number.");
      }
      if (hasfield(number)) {
        getfieldbuilder(number).mergefrom(field);
      } else {
        // optimization:  we could call getfieldbuilder(number).mergefrom(field)
        // in this case, but that would create a copy of the field object.
        // we'd rather reuse the one passed to us, so call addfield() instead.
        addfield(number, field);
      }
      return this;
    }

    /**
     * convenience method for merging a new field containing a single varint
     * value.  this is used in particular when an unknown enum value is
     * encountered.
     */
    public builder mergevarintfield(final int number, final int value) {
      if (number == 0) {
        throw new illegalargumentexception("zero is not a valid field number.");
      }
      getfieldbuilder(number).addvarint(value);
      return this;
    }

    /** check if the given field number is present in the set. */
    public boolean hasfield(final int number) {
      if (number == 0) {
        throw new illegalargumentexception("zero is not a valid field number.");
      }
      return number == lastfieldnumber || fields.containskey(number);
    }

    /**
     * add a field to the {@code unknownfieldset}.  if a field with the same
     * number already exists, it is removed.
     */
    public builder addfield(final int number, final field field) {
      if (number == 0) {
        throw new illegalargumentexception("zero is not a valid field number.");
      }
      if (lastfield != null && lastfieldnumber == number) {
        // discard this.
        lastfield = null;
        lastfieldnumber = 0;
      }
      if (fields.isempty()) {
        fields = new treemap<integer,field>();
      }
      fields.put(number, field);
      return this;
    }

    /**
     * get all present {@code field}s as an immutable {@code map}.  if more
     * fields are added, the changes may or may not be reflected in this map.
     */
    public map<integer, field> asmap() {
      getfieldbuilder(0);  // force lastfield to be built.
      return collections.unmodifiablemap(fields);
    }

    /**
     * parse an entire message from {@code input} and merge its fields into
     * this set.
     */
    public builder mergefrom(final codedinputstream input) throws ioexception {
      while (true) {
        final int tag = input.readtag();
        if (tag == 0 || !mergefieldfrom(tag, input)) {
          break;
        }
      }
      return this;
    }

    /**
     * parse a single field from {@code input} and merge it into this set.
     * @param tag the field's tag number, which was already parsed.
     * @return {@code false} if the tag is an end group tag.
     */
    public boolean mergefieldfrom(final int tag, final codedinputstream input)
                                  throws ioexception {
      final int number = wireformat.gettagfieldnumber(tag);
      switch (wireformat.gettagwiretype(tag)) {
        case wireformat.wiretype_varint:
          getfieldbuilder(number).addvarint(input.readint64());
          return true;
        case wireformat.wiretype_fixed64:
          getfieldbuilder(number).addfixed64(input.readfixed64());
          return true;
        case wireformat.wiretype_length_delimited:
          getfieldbuilder(number).addlengthdelimited(input.readbytes());
          return true;
        case wireformat.wiretype_start_group:
          final builder subbuilder = newbuilder();
          input.readgroup(number, subbuilder,
                          extensionregistry.getemptyregistry());
          getfieldbuilder(number).addgroup(subbuilder.build());
          return true;
        case wireformat.wiretype_end_group:
          return false;
        case wireformat.wiretype_fixed32:
          getfieldbuilder(number).addfixed32(input.readfixed32());
          return true;
        default:
          throw invalidprotocolbufferexception.invalidwiretype();
      }
    }

    /**
     * parse {@code data} as an {@code unknownfieldset} and merge it with the
     * set being built.  this is just a small wrapper around
     * {@link #mergefrom(codedinputstream)}.
     */
    public builder mergefrom(final bytestring data)
        throws invalidprotocolbufferexception {
      try {
        final codedinputstream input = data.newcodedinput();
        mergefrom(input);
        input.checklasttagwas(0);
        return this;
      } catch (final invalidprotocolbufferexception e) {
        throw e;
      } catch (final ioexception e) {
        throw new runtimeexception(
          "reading from a bytestring threw an ioexception (should " +
          "never happen).", e);
      }
    }

    /**
     * parse {@code data} as an {@code unknownfieldset} and merge it with the
     * set being built.  this is just a small wrapper around
     * {@link #mergefrom(codedinputstream)}.
     */
    public builder mergefrom(final byte[] data)
        throws invalidprotocolbufferexception {
      try {
        final codedinputstream input = codedinputstream.newinstance(data);
        mergefrom(input);
        input.checklasttagwas(0);
        return this;
      } catch (final invalidprotocolbufferexception e) {
        throw e;
      } catch (final ioexception e) {
        throw new runtimeexception(
          "reading from a byte array threw an ioexception (should " +
          "never happen).", e);
      }
    }

    /**
     * parse an {@code unknownfieldset} from {@code input} and merge it with the
     * set being built.  this is just a small wrapper around
     * {@link #mergefrom(codedinputstream)}.
     */
    public builder mergefrom(final inputstream input) throws ioexception {
      final codedinputstream codedinput = codedinputstream.newinstance(input);
      mergefrom(codedinput);
      codedinput.checklasttagwas(0);
      return this;
    }

    public boolean mergedelimitedfrom(inputstream input)
        throws ioexception {
      final int firstbyte = input.read();
      if (firstbyte == -1) {
        return false;
      }
      final int size = codedinputstream.readrawvarint32(firstbyte, input);
      final inputstream limitedinput = new limitedinputstream(input, size);
      mergefrom(limitedinput);
      return true;
    }

    public boolean mergedelimitedfrom(
        inputstream input,
        extensionregistrylite extensionregistry) throws ioexception {
      // unknownfieldset has no extensions.
      return mergedelimitedfrom(input);
    }

    public builder mergefrom(
        codedinputstream input,
        extensionregistrylite extensionregistry) throws ioexception {
      // unknownfieldset has no extensions.
      return mergefrom(input);
    }

    public builder mergefrom(
        bytestring data,
        extensionregistrylite extensionregistry)
        throws invalidprotocolbufferexception {
      // unknownfieldset has no extensions.
      return mergefrom(data);
    }

    public builder mergefrom(byte[] data, int off, int len)
        throws invalidprotocolbufferexception {
      try {
        final codedinputstream input =
            codedinputstream.newinstance(data, off, len);
        mergefrom(input);
        input.checklasttagwas(0);
        return this;
      } catch (invalidprotocolbufferexception e) {
        throw e;
      } catch (ioexception e) {
        throw new runtimeexception(
          "reading from a byte array threw an ioexception (should " +
          "never happen).", e);
      }
    }

    public builder mergefrom(
        byte[] data,
        extensionregistrylite extensionregistry)
        throws invalidprotocolbufferexception {
      // unknownfieldset has no extensions.
      return mergefrom(data);
    }

    public builder mergefrom(
        byte[] data, int off, int len,
        extensionregistrylite extensionregistry)
        throws invalidprotocolbufferexception {
      // unknownfieldset has no extensions.
      return mergefrom(data, off, len);
    }

    public builder mergefrom(
        inputstream input,
        extensionregistrylite extensionregistry) throws ioexception {
      // unknownfieldset has no extensions.
      return mergefrom(input);
    }

    public boolean isinitialized() {
      // unknownfieldsets do not have required fields, so they are always
      // initialized.
      return true;
    }
  }

  /**
   * represents a single field in an {@code unknownfieldset}.
   *
   * <p>a {@code field} consists of five lists of values.  the lists correspond
   * to the five "wire types" used in the protocol buffer binary format.
   * the wire type of each field can be determined from the encoded form alone,
   * without knowing the field's declared type.  so, we are able to parse
   * unknown values at least this far and separate them.  normally, only one
   * of the five lists will contain any values, since it is impossible to
   * define a valid message type that declares two different types for the
   * same field number.  however, the code is designed to allow for the case
   * where the same unknown field number is encountered using multiple different
   * wire types.
   *
   * <p>{@code field} is an immutable class.  to construct one, you must use a
   * {@link builder}.
   *
   * @see unknownfieldset
   */
  public static final class field {
    private field() {}

    /** construct a new {@link builder}. */
    public static builder newbuilder() {
      return builder.create();
    }

    /**
     * construct a new {@link builder} and initialize it to a copy of
     * {@code copyfrom}.
     */
    public static builder newbuilder(final field copyfrom) {
      return newbuilder().mergefrom(copyfrom);
    }

    /** get an empty {@code field}. */
    public static field getdefaultinstance() {
      return fielddefaultinstance;
    }
    private static final field fielddefaultinstance = newbuilder().build();

    /** get the list of varint values for this field. */
    public list<long> getvarintlist()               { return varint;          }

    /** get the list of fixed32 values for this field. */
    public list<integer> getfixed32list()           { return fixed32;         }

    /** get the list of fixed64 values for this field. */
    public list<long> getfixed64list()              { return fixed64;         }

    /** get the list of length-delimited values for this field. */
    public list<bytestring> getlengthdelimitedlist() { return lengthdelimited; }

    /**
     * get the list of embedded group values for this field.  these are
     * represented using {@link unknownfieldset}s rather than {@link message}s
     * since the group's type is presumably unknown.
     */
    public list<unknownfieldset> getgrouplist()      { return group;           }

    @override
    public boolean equals(final object other) {
      if (this == other) {
        return true;
      }
      if (!(other instanceof field)) {
        return false;
      }
      return arrays.equals(getidentityarray(),
          ((field) other).getidentityarray());
    }

    @override
    public int hashcode() {
      return arrays.hashcode(getidentityarray());
    }

    /**
     * returns the array of objects to be used to uniquely identify this
     * {@link field} instance.
     */
    private object[] getidentityarray() {
      return new object[] {
          varint,
          fixed32,
          fixed64,
          lengthdelimited,
          group};
    }

    /**
     * serializes the field, including field number, and writes it to
     * {@code output}.
     */
    public void writeto(final int fieldnumber, final codedoutputstream output)
                        throws ioexception {
      for (final long value : varint) {
        output.writeuint64(fieldnumber, value);
      }
      for (final int value : fixed32) {
        output.writefixed32(fieldnumber, value);
      }
      for (final long value : fixed64) {
        output.writefixed64(fieldnumber, value);
      }
      for (final bytestring value : lengthdelimited) {
        output.writebytes(fieldnumber, value);
      }
      for (final unknownfieldset value : group) {
        output.writegroup(fieldnumber, value);
      }
    }

    /**
     * get the number of bytes required to encode this field, including field
     * number.
     */
    public int getserializedsize(final int fieldnumber) {
      int result = 0;
      for (final long value : varint) {
        result += codedoutputstream.computeuint64size(fieldnumber, value);
      }
      for (final int value : fixed32) {
        result += codedoutputstream.computefixed32size(fieldnumber, value);
      }
      for (final long value : fixed64) {
        result += codedoutputstream.computefixed64size(fieldnumber, value);
      }
      for (final bytestring value : lengthdelimited) {
        result += codedoutputstream.computebytessize(fieldnumber, value);
      }
      for (final unknownfieldset value : group) {
        result += codedoutputstream.computegroupsize(fieldnumber, value);
      }
      return result;
    }

    /**
     * serializes the field, including field number, and writes it to
     * {@code output}, using {@code messageset} wire format.
     */
    public void writeasmessagesetextensionto(
        final int fieldnumber,
        final codedoutputstream output)
        throws ioexception {
      for (final bytestring value : lengthdelimited) {
        output.writerawmessagesetextension(fieldnumber, value);
      }
    }

    /**
     * get the number of bytes required to encode this field, including field
     * number, using {@code messageset} wire format.
     */
    public int getserializedsizeasmessagesetextension(final int fieldnumber) {
      int result = 0;
      for (final bytestring value : lengthdelimited) {
        result += codedoutputstream.computerawmessagesetextensionsize(
          fieldnumber, value);
      }
      return result;
    }

    private list<long> varint;
    private list<integer> fixed32;
    private list<long> fixed64;
    private list<bytestring> lengthdelimited;
    private list<unknownfieldset> group;

    /**
     * used to build a {@link field} within an {@link unknownfieldset}.
     *
     * <p>use {@link field#newbuilder()} to construct a {@code builder}.
     */
    public static final class builder {
      // this constructor should never be called directly (except from 'create').
      private builder() {}

      private static builder create() {
        builder builder = new builder();
        builder.result = new field();
        return builder;
      }

      private field result;

      /**
       * build the field.  after {@code build()} has been called, the
       * {@code builder} is no longer usable.  calling any other method will
       * result in undefined behavior and can cause a
       * {@code nullpointerexception} to be thrown.
       */
      public field build() {
        if (result.varint == null) {
          result.varint = collections.emptylist();
        } else {
          result.varint = collections.unmodifiablelist(result.varint);
        }
        if (result.fixed32 == null) {
          result.fixed32 = collections.emptylist();
        } else {
          result.fixed32 = collections.unmodifiablelist(result.fixed32);
        }
        if (result.fixed64 == null) {
          result.fixed64 = collections.emptylist();
        } else {
          result.fixed64 = collections.unmodifiablelist(result.fixed64);
        }
        if (result.lengthdelimited == null) {
          result.lengthdelimited = collections.emptylist();
        } else {
          result.lengthdelimited =
            collections.unmodifiablelist(result.lengthdelimited);
        }
        if (result.group == null) {
          result.group = collections.emptylist();
        } else {
          result.group = collections.unmodifiablelist(result.group);
        }

        final field returnme = result;
        result = null;
        return returnme;
      }

      /** discard the field's contents. */
      public builder clear() {
        result = new field();
        return this;
      }

      /**
       * merge the values in {@code other} into this field.  for each list
       * of values, {@code other}'s values are append to the ones in this
       * field.
       */
      public builder mergefrom(final field other) {
        if (!other.varint.isempty()) {
          if (result.varint == null) {
            result.varint = new arraylist<long>();
          }
          result.varint.addall(other.varint);
        }
        if (!other.fixed32.isempty()) {
          if (result.fixed32 == null) {
            result.fixed32 = new arraylist<integer>();
          }
          result.fixed32.addall(other.fixed32);
        }
        if (!other.fixed64.isempty()) {
          if (result.fixed64 == null) {
            result.fixed64 = new arraylist<long>();
          }
          result.fixed64.addall(other.fixed64);
        }
        if (!other.lengthdelimited.isempty()) {
          if (result.lengthdelimited == null) {
            result.lengthdelimited = new arraylist<bytestring>();
          }
          result.lengthdelimited.addall(other.lengthdelimited);
        }
        if (!other.group.isempty()) {
          if (result.group == null) {
            result.group = new arraylist<unknownfieldset>();
          }
          result.group.addall(other.group);
        }
        return this;
      }

      /** add a varint value. */
      public builder addvarint(final long value) {
        if (result.varint == null) {
          result.varint = new arraylist<long>();
        }
        result.varint.add(value);
        return this;
      }

      /** add a fixed32 value. */
      public builder addfixed32(final int value) {
        if (result.fixed32 == null) {
          result.fixed32 = new arraylist<integer>();
        }
        result.fixed32.add(value);
        return this;
      }

      /** add a fixed64 value. */
      public builder addfixed64(final long value) {
        if (result.fixed64 == null) {
          result.fixed64 = new arraylist<long>();
        }
        result.fixed64.add(value);
        return this;
      }

      /** add a length-delimited value. */
      public builder addlengthdelimited(final bytestring value) {
        if (result.lengthdelimited == null) {
          result.lengthdelimited = new arraylist<bytestring>();
        }
        result.lengthdelimited.add(value);
        return this;
      }

      /** add an embedded group. */
      public builder addgroup(final unknownfieldset value) {
        if (result.group == null) {
          result.group = new arraylist<unknownfieldset>();
        }
        result.group.add(value);
        return this;
      }
    }
  }

  /**
   * parser to implement messagelite interface.
   */
  public static final class parser extends abstractparser<unknownfieldset> {
    public unknownfieldset parsepartialfrom(
        codedinputstream input, extensionregistrylite extensionregistry)
        throws invalidprotocolbufferexception {
      builder builder = newbuilder();
      try {
        builder.mergefrom(input);
      } catch (invalidprotocolbufferexception e) {
        throw e.setunfinishedmessage(builder.buildpartial());
      } catch (ioexception e) {
        throw new invalidprotocolbufferexception(e.getmessage())
            .setunfinishedmessage(builder.buildpartial());
      }
      return builder.buildpartial();
    }
  }

  private static final parser parser = new parser();
  public final parser getparserfortype() {
    return parser;
  }
}
