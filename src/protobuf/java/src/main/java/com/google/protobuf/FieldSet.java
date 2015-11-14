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

import com.google.protobuf.lazyfield.lazyiterator;

import java.io.ioexception;
import java.util.arraylist;
import java.util.collections;
import java.util.iterator;
import java.util.list;
import java.util.map;

/**
 * a class which represents an arbitrary set of fields of some message type.
 * this is used to implement {@link dynamicmessage}, and also to represent
 * extensions in {@link generatedmessage}.  this class is package-private,
 * since outside users should probably be using {@link dynamicmessage}.
 *
 * @author kenton@google.com kenton varda
 */
final class fieldset<fielddescriptortype extends
      fieldset.fielddescriptorlite<fielddescriptortype>> {
  /**
   * interface for a fielddescriptor or lite extension descriptor.  this
   * prevents fieldset from depending on {@link descriptors.fielddescriptor}.
   */
  public interface fielddescriptorlite<t extends fielddescriptorlite<t>>
      extends comparable<t> {
    int getnumber();
    wireformat.fieldtype getlitetype();
    wireformat.javatype getlitejavatype();
    boolean isrepeated();
    boolean ispacked();
    internal.enumlitemap<?> getenumtype();

    // if getlitejavatype() == message, this merges a message object of the
    // type into a builder of the type.  returns {@code to}.
    messagelite.builder internalmergefrom(
        messagelite.builder to, messagelite from);
  }

  private final smallsortedmap<fielddescriptortype, object> fields;
  private boolean isimmutable;
  private boolean haslazyfield = false;

  /** construct a new fieldset. */
  private fieldset() {
    this.fields = smallsortedmap.newfieldmap(16);
  }

  /**
   * construct an empty fieldset.  this is only used to initialize
   * default_instance.
   */
  private fieldset(final boolean dummy) {
    this.fields = smallsortedmap.newfieldmap(0);
    makeimmutable();
  }

  /** construct a new fieldset. */
  public static <t extends fieldset.fielddescriptorlite<t>>
      fieldset<t> newfieldset() {
    return new fieldset<t>();
  }

  /** get an immutable empty fieldset. */
  @suppresswarnings("unchecked")
  public static <t extends fieldset.fielddescriptorlite<t>>
      fieldset<t> emptyset() {
    return default_instance;
  }
  @suppresswarnings("rawtypes")
  private static final fieldset default_instance = new fieldset(true);

  /** make this fieldset immutable from this point forward. */
  @suppresswarnings("unchecked")
  public void makeimmutable() {
    if (isimmutable) {
      return;
    }
    fields.makeimmutable();
    isimmutable = true;
  }

  /**
   * returns whether the fieldset is immutable. this is true if it is the
   * {@link #emptyset} or if {@link #makeimmutable} were called.
   *
   * @return whether the fieldset is immutable.
   */
  public boolean isimmutable() {
    return isimmutable;
  }

  /**
   * clones the fieldset. the returned fieldset will be mutable even if the
   * original fieldset was immutable.
   *
   * @return the newly cloned fieldset
   */
  @override
  public fieldset<fielddescriptortype> clone() {
    // we can't just call fields.clone because list objects in the map
    // should not be shared.
    fieldset<fielddescriptortype> clone = fieldset.newfieldset();
    for (int i = 0; i < fields.getnumarrayentries(); i++) {
      map.entry<fielddescriptortype, object> entry = fields.getarrayentryat(i);
      fielddescriptortype descriptor = entry.getkey();
      clone.setfield(descriptor, entry.getvalue());
    }
    for (map.entry<fielddescriptortype, object> entry :
             fields.getoverflowentries()) {
      fielddescriptortype descriptor = entry.getkey();
      clone.setfield(descriptor, entry.getvalue());
    }
    clone.haslazyfield = haslazyfield;
    return clone;
  }

  // =================================================================

  /** see {@link message.builder#clear()}. */
  public void clear() {
    fields.clear();
    haslazyfield = false;
  }

  /**
   * get a simple map containing all the fields.
   */
  public map<fielddescriptortype, object> getallfields() {
    if (haslazyfield) {
      smallsortedmap<fielddescriptortype, object> result =
          smallsortedmap.newfieldmap(16);
      for (int i = 0; i < fields.getnumarrayentries(); i++) {
        clonefieldentry(result, fields.getarrayentryat(i));
      }
      for (map.entry<fielddescriptortype, object> entry :
          fields.getoverflowentries()) {
        clonefieldentry(result, entry);
      }
      if (fields.isimmutable()) {
        result.makeimmutable();
      }
      return result;
    }
    return fields.isimmutable() ? fields : collections.unmodifiablemap(fields);
  }

  private void clonefieldentry(map<fielddescriptortype, object> map,
      map.entry<fielddescriptortype, object> entry) {
    fielddescriptortype key = entry.getkey();
    object value = entry.getvalue();
    if (value instanceof lazyfield) {
      map.put(key, ((lazyfield) value).getvalue());
    } else {
      map.put(key, value);
    }
  }

  /**
   * get an iterator to the field map. this iterator should not be leaked out
   * of the protobuf library as it is not protected from mutation when fields
   * is not immutable.
   */
  public iterator<map.entry<fielddescriptortype, object>> iterator() {
    if (haslazyfield) {
      return new lazyiterator<fielddescriptortype>(
          fields.entryset().iterator());
    }
    return fields.entryset().iterator();
  }

  /**
   * useful for implementing
   * {@link message#hasfield(descriptors.fielddescriptor)}.
   */
  public boolean hasfield(final fielddescriptortype descriptor) {
    if (descriptor.isrepeated()) {
      throw new illegalargumentexception(
        "hasfield() can only be called on non-repeated fields.");
    }

    return fields.get(descriptor) != null;
  }

  /**
   * useful for implementing
   * {@link message#getfield(descriptors.fielddescriptor)}.  this method
   * returns {@code null} if the field is not set; in this case it is up
   * to the caller to fetch the field's default value.
   */
  public object getfield(final fielddescriptortype descriptor) {
    object o = fields.get(descriptor);
    if (o instanceof lazyfield) {
      return ((lazyfield) o).getvalue();
    }
    return o;
  }

  /**
   * useful for implementing
   * {@link message.builder#setfield(descriptors.fielddescriptor,object)}.
   */
  @suppresswarnings({"unchecked", "rawtypes"})
  public void setfield(final fielddescriptortype descriptor,
                       object value) {
    if (descriptor.isrepeated()) {
      if (!(value instanceof list)) {
        throw new illegalargumentexception(
          "wrong object type used with protocol message reflection.");
      }

      // wrap the contents in a new list so that the caller cannot change
      // the list's contents after setting it.
      final list newlist = new arraylist();
      newlist.addall((list) value);
      for (final object element : newlist) {
        verifytype(descriptor.getlitetype(), element);
      }
      value = newlist;
    } else {
      verifytype(descriptor.getlitetype(), value);
    }

    if (value instanceof lazyfield) {
      haslazyfield = true;
    }
    fields.put(descriptor, value);
  }

  /**
   * useful for implementing
   * {@link message.builder#clearfield(descriptors.fielddescriptor)}.
   */
  public void clearfield(final fielddescriptortype descriptor) {
    fields.remove(descriptor);
    if (fields.isempty()) {
      haslazyfield = false;
    }
  }

  /**
   * useful for implementing
   * {@link message#getrepeatedfieldcount(descriptors.fielddescriptor)}.
   */
  public int getrepeatedfieldcount(final fielddescriptortype descriptor) {
    if (!descriptor.isrepeated()) {
      throw new illegalargumentexception(
        "getrepeatedfield() can only be called on repeated fields.");
    }

    final object value = getfield(descriptor);
    if (value == null) {
      return 0;
    } else {
      return ((list<?>) value).size();
    }
  }

  /**
   * useful for implementing
   * {@link message#getrepeatedfield(descriptors.fielddescriptor,int)}.
   */
  public object getrepeatedfield(final fielddescriptortype descriptor,
                                 final int index) {
    if (!descriptor.isrepeated()) {
      throw new illegalargumentexception(
        "getrepeatedfield() can only be called on repeated fields.");
    }

    final object value = getfield(descriptor);

    if (value == null) {
      throw new indexoutofboundsexception();
    } else {
      return ((list<?>) value).get(index);
    }
  }

  /**
   * useful for implementing
   * {@link message.builder#setrepeatedfield(descriptors.fielddescriptor,int,object)}.
   */
  @suppresswarnings("unchecked")
  public void setrepeatedfield(final fielddescriptortype descriptor,
                               final int index,
                               final object value) {
    if (!descriptor.isrepeated()) {
      throw new illegalargumentexception(
        "getrepeatedfield() can only be called on repeated fields.");
    }

    final object list = getfield(descriptor);
    if (list == null) {
      throw new indexoutofboundsexception();
    }

    verifytype(descriptor.getlitetype(), value);
    ((list<object>) list).set(index, value);
  }

  /**
   * useful for implementing
   * {@link message.builder#addrepeatedfield(descriptors.fielddescriptor,object)}.
   */
  @suppresswarnings("unchecked")
  public void addrepeatedfield(final fielddescriptortype descriptor,
                               final object value) {
    if (!descriptor.isrepeated()) {
      throw new illegalargumentexception(
        "addrepeatedfield() can only be called on repeated fields.");
    }

    verifytype(descriptor.getlitetype(), value);

    final object existingvalue = getfield(descriptor);
    list<object> list;
    if (existingvalue == null) {
      list = new arraylist<object>();
      fields.put(descriptor, list);
    } else {
      list = (list<object>) existingvalue;
    }

    list.add(value);
  }

  /**
   * verifies that the given object is of the correct type to be a valid
   * value for the given field.  (for repeated fields, this checks if the
   * object is the right type to be one element of the field.)
   *
   * @throws illegalargumentexception the value is not of the right type.
   */
  private static void verifytype(final wireformat.fieldtype type,
                                 final object value) {
    if (value == null) {
      throw new nullpointerexception();
    }

    boolean isvalid = false;
    switch (type.getjavatype()) {
      case int:          isvalid = value instanceof integer   ; break;
      case long:         isvalid = value instanceof long      ; break;
      case float:        isvalid = value instanceof float     ; break;
      case double:       isvalid = value instanceof double    ; break;
      case boolean:      isvalid = value instanceof boolean   ; break;
      case string:       isvalid = value instanceof string    ; break;
      case byte_string:  isvalid = value instanceof bytestring; break;
      case enum:
        // todo(kenton):  caller must do type checking here, i guess.
        isvalid = value instanceof internal.enumlite;
        break;
      case message:
        // todo(kenton):  caller must do type checking here, i guess.
        isvalid =
            (value instanceof messagelite) || (value instanceof lazyfield);
        break;
    }

    if (!isvalid) {
      // todo(kenton):  when chaining calls to setfield(), it can be hard to
      //   tell from the stack trace which exact call failed, since the whole
      //   chain is considered one line of code.  it would be nice to print
      //   more information here, e.g. naming the field.  we used to do that.
      //   but we can't now that fieldset doesn't use descriptors.  maybe this
      //   isn't a big deal, though, since it would only really apply when using
      //   reflection and generally people don't chain reflection setters.
      throw new illegalargumentexception(
        "wrong object type used with protocol message reflection.");
    }
  }

  // =================================================================
  // parsing and serialization

  /**
   * see {@link message#isinitialized()}.  note:  since {@code fieldset}
   * itself does not have any way of knowing about required fields that
   * aren't actually present in the set, it is up to the caller to check
   * that all required fields are present.
   */
  public boolean isinitialized() {
    for (int i = 0; i < fields.getnumarrayentries(); i++) {
      if (!isinitialized(fields.getarrayentryat(i))) {
        return false;
      }
    }
    for (final map.entry<fielddescriptortype, object> entry :
             fields.getoverflowentries()) {
      if (!isinitialized(entry)) {
        return false;
      }
    }
    return true;
  }

  @suppresswarnings("unchecked")
  private boolean isinitialized(
      final map.entry<fielddescriptortype, object> entry) {
    final fielddescriptortype descriptor = entry.getkey();
    if (descriptor.getlitejavatype() == wireformat.javatype.message) {
      if (descriptor.isrepeated()) {
        for (final messagelite element:
                 (list<messagelite>) entry.getvalue()) {
          if (!element.isinitialized()) {
            return false;
          }
        }
      } else {
        object value = entry.getvalue();
        if (value instanceof messagelite) {
          if (!((messagelite) value).isinitialized()) {
            return false;
          }
        } else if (value instanceof lazyfield) {
          return true;
        } else {
          throw new illegalargumentexception(
              "wrong object type used with protocol message reflection.");
        }
      }
    }
    return true;
  }

  /**
   * given a field type, return the wire type.
   *
   * @returns one of the {@code wiretype_} constants defined in
   *          {@link wireformat}.
   */
  static int getwireformatforfieldtype(final wireformat.fieldtype type,
                                       boolean ispacked) {
    if (ispacked) {
      return wireformat.wiretype_length_delimited;
    } else {
      return type.getwiretype();
    }
  }

  /**
   * like {@link message.builder#mergefrom(message)}, but merges from another 
   * {@link fieldset}.
   */
  public void mergefrom(final fieldset<fielddescriptortype> other) {
    for (int i = 0; i < other.fields.getnumarrayentries(); i++) {
      mergefromfield(other.fields.getarrayentryat(i));
    }
    for (final map.entry<fielddescriptortype, object> entry :
             other.fields.getoverflowentries()) {
      mergefromfield(entry);
    }
  }

  @suppresswarnings({"unchecked", "rawtypes"})
  private void mergefromfield(
      final map.entry<fielddescriptortype, object> entry) {
    final fielddescriptortype descriptor = entry.getkey();
    object othervalue = entry.getvalue();
    if (othervalue instanceof lazyfield) {
      othervalue = ((lazyfield) othervalue).getvalue();
    }

    if (descriptor.isrepeated()) {
      object value = getfield(descriptor);
      if (value == null) {
        // our list is empty, but we still need to make a defensive copy of
        // the other list since we don't know if the other fieldset is still
        // mutable.
        fields.put(descriptor, new arraylist((list) othervalue));
      } else {
        // concatenate the lists.
        ((list) value).addall((list) othervalue);
      }
    } else if (descriptor.getlitejavatype() == wireformat.javatype.message) {
      object value = getfield(descriptor);
      if (value == null) {
        fields.put(descriptor, othervalue);
      } else {
        // merge the messages.
        fields.put(
            descriptor,
            descriptor.internalmergefrom(
                ((messagelite) value).tobuilder(), (messagelite) othervalue)
            .build());
      }
    } else {
      fields.put(descriptor, othervalue);
    }
  }

  // todo(kenton):  move static parsing and serialization methods into some
  //   other class.  probably wireformat.

  /**
   * read a field of any primitive type from a codedinputstream.  enums,
   * groups, and embedded messages are not handled by this method.
   *
   * @param input the stream from which to read.
   * @param type declared type of the field.
   * @return an object representing the field's value, of the exact
   *         type which would be returned by
   *         {@link message#getfield(descriptors.fielddescriptor)} for
   *         this field.
   */
  public static object readprimitivefield(
      codedinputstream input,
      final wireformat.fieldtype type) throws ioexception {
    switch (type) {
      case double  : return input.readdouble  ();
      case float   : return input.readfloat   ();
      case int64   : return input.readint64   ();
      case uint64  : return input.readuint64  ();
      case int32   : return input.readint32   ();
      case fixed64 : return input.readfixed64 ();
      case fixed32 : return input.readfixed32 ();
      case bool    : return input.readbool    ();
      case string  : return input.readstring  ();
      case bytes   : return input.readbytes   ();
      case uint32  : return input.readuint32  ();
      case sfixed32: return input.readsfixed32();
      case sfixed64: return input.readsfixed64();
      case sint32  : return input.readsint32  ();
      case sint64  : return input.readsint64  ();

      case group:
        throw new illegalargumentexception(
          "readprimitivefield() cannot handle nested groups.");
      case message:
        throw new illegalargumentexception(
          "readprimitivefield() cannot handle embedded messages.");
      case enum:
        // we don't handle enums because we don't know what to do if the
        // value is not recognized.
        throw new illegalargumentexception(
          "readprimitivefield() cannot handle enums.");
    }

    throw new runtimeexception(
      "there is no way to get here, but the compiler thinks otherwise.");
  }

  /** see {@link message#writeto(codedoutputstream)}. */
  public void writeto(final codedoutputstream output)
                      throws ioexception {
    for (int i = 0; i < fields.getnumarrayentries(); i++) {
      final map.entry<fielddescriptortype, object> entry =
          fields.getarrayentryat(i);
      writefield(entry.getkey(), entry.getvalue(), output);
    }
    for (final map.entry<fielddescriptortype, object> entry :
         fields.getoverflowentries()) {
      writefield(entry.getkey(), entry.getvalue(), output);
    }
  }

  /**
   * like {@link #writeto} but uses messageset wire format.
   */
  public void writemessagesetto(final codedoutputstream output)
                                throws ioexception {
    for (int i = 0; i < fields.getnumarrayentries(); i++) {
      writemessagesetto(fields.getarrayentryat(i), output);
    }
    for (final map.entry<fielddescriptortype, object> entry :
             fields.getoverflowentries()) {
      writemessagesetto(entry, output);
    }
  }

  private void writemessagesetto(
      final map.entry<fielddescriptortype, object> entry,
      final codedoutputstream output) throws ioexception {
    final fielddescriptortype descriptor = entry.getkey();
    if (descriptor.getlitejavatype() == wireformat.javatype.message &&
        !descriptor.isrepeated() && !descriptor.ispacked()) {
      output.writemessagesetextension(entry.getkey().getnumber(),
                                      (messagelite) entry.getvalue());
    } else {
      writefield(descriptor, entry.getvalue(), output);
    }
  }

  /**
   * write a single tag-value pair to the stream.
   *
   * @param output the output stream.
   * @param type   the field's type.
   * @param number the field's number.
   * @param value  object representing the field's value.  must be of the exact
   *               type which would be returned by
   *               {@link message#getfield(descriptors.fielddescriptor)} for
   *               this field.
   */
  private static void writeelement(final codedoutputstream output,
                                   final wireformat.fieldtype type,
                                   final int number,
                                   final object value) throws ioexception {
    // special case for groups, which need a start and end tag; other fields
    // can just use writetag() and writefieldnotag().
    if (type == wireformat.fieldtype.group) {
      output.writegroup(number, (messagelite) value);
    } else {
      output.writetag(number, getwireformatforfieldtype(type, false));
      writeelementnotag(output, type, value);
    }
  }

  /**
   * write a field of arbitrary type, without its tag, to the stream.
   *
   * @param output the output stream.
   * @param type the field's type.
   * @param value  object representing the field's value.  must be of the exact
   *               type which would be returned by
   *               {@link message#getfield(descriptors.fielddescriptor)} for
   *               this field.
   */
  private static void writeelementnotag(
      final codedoutputstream output,
      final wireformat.fieldtype type,
      final object value) throws ioexception {
    switch (type) {
      case double  : output.writedoublenotag  ((double     ) value); break;
      case float   : output.writefloatnotag   ((float      ) value); break;
      case int64   : output.writeint64notag   ((long       ) value); break;
      case uint64  : output.writeuint64notag  ((long       ) value); break;
      case int32   : output.writeint32notag   ((integer    ) value); break;
      case fixed64 : output.writefixed64notag ((long       ) value); break;
      case fixed32 : output.writefixed32notag ((integer    ) value); break;
      case bool    : output.writeboolnotag    ((boolean    ) value); break;
      case string  : output.writestringnotag  ((string     ) value); break;
      case group   : output.writegroupnotag   ((messagelite) value); break;
      case message : output.writemessagenotag ((messagelite) value); break;
      case bytes   : output.writebytesnotag   ((bytestring ) value); break;
      case uint32  : output.writeuint32notag  ((integer    ) value); break;
      case sfixed32: output.writesfixed32notag((integer    ) value); break;
      case sfixed64: output.writesfixed64notag((long       ) value); break;
      case sint32  : output.writesint32notag  ((integer    ) value); break;
      case sint64  : output.writesint64notag  ((long       ) value); break;

      case enum:
        output.writeenumnotag(((internal.enumlite) value).getnumber());
        break;
    }
  }

  /** write a single field. */
  public static void writefield(final fielddescriptorlite<?> descriptor,
                                final object value,
                                final codedoutputstream output)
                                throws ioexception {
    wireformat.fieldtype type = descriptor.getlitetype();
    int number = descriptor.getnumber();
    if (descriptor.isrepeated()) {
      final list<?> valuelist = (list<?>)value;
      if (descriptor.ispacked()) {
        output.writetag(number, wireformat.wiretype_length_delimited);
        // compute the total data size so the length can be written.
        int datasize = 0;
        for (final object element : valuelist) {
          datasize += computeelementsizenotag(type, element);
        }
        output.writerawvarint32(datasize);
        // write the data itself, without any tags.
        for (final object element : valuelist) {
          writeelementnotag(output, type, element);
        }
      } else {
        for (final object element : valuelist) {
          writeelement(output, type, number, element);
        }
      }
    } else {
      if (value instanceof lazyfield) {
        writeelement(output, type, number, ((lazyfield) value).getvalue());
      } else {
        writeelement(output, type, number, value);
      }
    }
  }

  /**
   * see {@link message#getserializedsize()}.  it's up to the caller to cache
   * the resulting size if desired.
   */
  public int getserializedsize() {
    int size = 0;
    for (int i = 0; i < fields.getnumarrayentries(); i++) {
      final map.entry<fielddescriptortype, object> entry =
          fields.getarrayentryat(i);
      size += computefieldsize(entry.getkey(), entry.getvalue());
    }
    for (final map.entry<fielddescriptortype, object> entry :
         fields.getoverflowentries()) {
      size += computefieldsize(entry.getkey(), entry.getvalue());
    }
    return size;
  }

  /**
   * like {@link #getserializedsize} but uses messageset wire format.
   */
  public int getmessagesetserializedsize() {
    int size = 0;
    for (int i = 0; i < fields.getnumarrayentries(); i++) {
      size += getmessagesetserializedsize(fields.getarrayentryat(i));
    }
    for (final map.entry<fielddescriptortype, object> entry :
             fields.getoverflowentries()) {
      size += getmessagesetserializedsize(entry);
    }
    return size;
  }

  private int getmessagesetserializedsize(
      final map.entry<fielddescriptortype, object> entry) {
    final fielddescriptortype descriptor = entry.getkey();
    object value = entry.getvalue();
    if (descriptor.getlitejavatype() == wireformat.javatype.message
        && !descriptor.isrepeated() && !descriptor.ispacked()) {
      if (value instanceof lazyfield) {
        return codedoutputstream.computelazyfieldmessagesetextensionsize(
            entry.getkey().getnumber(), (lazyfield) value);
      } else {
        return codedoutputstream.computemessagesetextensionsize(
            entry.getkey().getnumber(), (messagelite) value);
      }
    } else {
      return computefieldsize(descriptor, value);
    }
  }

  /**
   * compute the number of bytes that would be needed to encode a
   * single tag/value pair of arbitrary type.
   *
   * @param type   the field's type.
   * @param number the field's number.
   * @param value  object representing the field's value.  must be of the exact
   *               type which would be returned by
   *               {@link message#getfield(descriptors.fielddescriptor)} for
   *               this field.
   */
  private static int computeelementsize(
      final wireformat.fieldtype type,
      final int number, final object value) {
    int tagsize = codedoutputstream.computetagsize(number);
    if (type == wireformat.fieldtype.group) {
      tagsize *= 2;
    }
    return tagsize + computeelementsizenotag(type, value);
  }

  /**
   * compute the number of bytes that would be needed to encode a
   * particular value of arbitrary type, excluding tag.
   *
   * @param type   the field's type.
   * @param value  object representing the field's value.  must be of the exact
   *               type which would be returned by
   *               {@link message#getfield(descriptors.fielddescriptor)} for
   *               this field.
   */
  private static int computeelementsizenotag(
      final wireformat.fieldtype type, final object value) {
    switch (type) {
      // note:  minor violation of 80-char limit rule here because this would
      //   actually be harder to read if we wrapped the lines.
      case double  : return codedoutputstream.computedoublesizenotag  ((double     )value);
      case float   : return codedoutputstream.computefloatsizenotag   ((float      )value);
      case int64   : return codedoutputstream.computeint64sizenotag   ((long       )value);
      case uint64  : return codedoutputstream.computeuint64sizenotag  ((long       )value);
      case int32   : return codedoutputstream.computeint32sizenotag   ((integer    )value);
      case fixed64 : return codedoutputstream.computefixed64sizenotag ((long       )value);
      case fixed32 : return codedoutputstream.computefixed32sizenotag ((integer    )value);
      case bool    : return codedoutputstream.computeboolsizenotag    ((boolean    )value);
      case string  : return codedoutputstream.computestringsizenotag  ((string     )value);
      case group   : return codedoutputstream.computegroupsizenotag   ((messagelite)value);
      case bytes   : return codedoutputstream.computebytessizenotag   ((bytestring )value);
      case uint32  : return codedoutputstream.computeuint32sizenotag  ((integer    )value);
      case sfixed32: return codedoutputstream.computesfixed32sizenotag((integer    )value);
      case sfixed64: return codedoutputstream.computesfixed64sizenotag((long       )value);
      case sint32  : return codedoutputstream.computesint32sizenotag  ((integer    )value);
      case sint64  : return codedoutputstream.computesint64sizenotag  ((long       )value);

      case message:
        if (value instanceof lazyfield) {
          return codedoutputstream.computelazyfieldsizenotag((lazyfield) value);
        } else {
          return codedoutputstream.computemessagesizenotag((messagelite) value);
        }

      case enum:
        return codedoutputstream.computeenumsizenotag(
            ((internal.enumlite) value).getnumber());
    }

    throw new runtimeexception(
      "there is no way to get here, but the compiler thinks otherwise.");
  }

  /**
   * compute the number of bytes needed to encode a particular field.
   */
  public static int computefieldsize(final fielddescriptorlite<?> descriptor,
                                     final object value) {
    wireformat.fieldtype type = descriptor.getlitetype();
    int number = descriptor.getnumber();
    if (descriptor.isrepeated()) {
      if (descriptor.ispacked()) {
        int datasize = 0;
        for (final object element : (list<?>)value) {
          datasize += computeelementsizenotag(type, element);
        }
        return datasize +
            codedoutputstream.computetagsize(number) +
            codedoutputstream.computerawvarint32size(datasize);
      } else {
        int size = 0;
        for (final object element : (list<?>)value) {
          size += computeelementsize(type, number, element);
        }
        return size;
      }
    } else {
      return computeelementsize(type, number, value);
    }
  }
}
