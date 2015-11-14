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

import com.google.protobuf.descriptors.descriptor;
import com.google.protobuf.descriptors.fielddescriptor;
import com.google.protobuf.generatedmessage.extendablebuilder;
import com.google.protobuf.internal.enumlite;

import java.io.ioexception;
import java.io.inputstream;
import java.util.arraylist;
import java.util.list;
import java.util.map;

/**
 * a partial implementation of the {@link message} interface which implements
 * as many methods of that interface as possible in terms of other methods.
 *
 * @author kenton@google.com kenton varda
 */
public abstract class abstractmessage extends abstractmessagelite
                                      implements message {
  @suppresswarnings("unchecked")
  public boolean isinitialized() {
    // check that all required fields are present.
    for (final fielddescriptor field : getdescriptorfortype().getfields()) {
      if (field.isrequired()) {
        if (!hasfield(field)) {
          return false;
        }
      }
    }

    // check that embedded messages are initialized.
    for (final map.entry<fielddescriptor, object> entry :
        getallfields().entryset()) {
      final fielddescriptor field = entry.getkey();
      if (field.getjavatype() == fielddescriptor.javatype.message) {
        if (field.isrepeated()) {
          for (final message element : (list<message>) entry.getvalue()) {
            if (!element.isinitialized()) {
              return false;
            }
          }
        } else {
          if (!((message) entry.getvalue()).isinitialized()) {
            return false;
          }
        }
      }
    }

    return true;
  }

  public list<string> findinitializationerrors() {
    return builder.findmissingfields(this);
  }

  public string getinitializationerrorstring() {
    return delimitwithcommas(findinitializationerrors());
  }

  private static string delimitwithcommas(list<string> parts) {
    stringbuilder result = new stringbuilder();
    for (string part : parts) {
      if (result.length() > 0) {
        result.append(", ");
      }
      result.append(part);
    }
    return result.tostring();
  }

  @override
  public final string tostring() {
    return textformat.printtostring(this);
  }

  public void writeto(final codedoutputstream output) throws ioexception {
    final boolean ismessageset =
        getdescriptorfortype().getoptions().getmessagesetwireformat();

    for (final map.entry<fielddescriptor, object> entry :
        getallfields().entryset()) {
      final fielddescriptor field = entry.getkey();
      final object value = entry.getvalue();
      if (ismessageset && field.isextension() &&
          field.gettype() == fielddescriptor.type.message &&
          !field.isrepeated()) {
        output.writemessagesetextension(field.getnumber(), (message) value);
      } else {
        fieldset.writefield(field, value, output);
      }
    }

    final unknownfieldset unknownfields = getunknownfields();
    if (ismessageset) {
      unknownfields.writeasmessagesetto(output);
    } else {
      unknownfields.writeto(output);
    }
  }

  private int memoizedsize = -1;

  public int getserializedsize() {
    int size = memoizedsize;
    if (size != -1) {
      return size;
    }

    size = 0;
    final boolean ismessageset =
        getdescriptorfortype().getoptions().getmessagesetwireformat();

    for (final map.entry<fielddescriptor, object> entry :
        getallfields().entryset()) {
      final fielddescriptor field = entry.getkey();
      final object value = entry.getvalue();
      if (ismessageset && field.isextension() &&
          field.gettype() == fielddescriptor.type.message &&
          !field.isrepeated()) {
        size += codedoutputstream.computemessagesetextensionsize(
            field.getnumber(), (message) value);
      } else {
        size += fieldset.computefieldsize(field, value);
      }
    }

    final unknownfieldset unknownfields = getunknownfields();
    if (ismessageset) {
      size += unknownfields.getserializedsizeasmessageset();
    } else {
      size += unknownfields.getserializedsize();
    }

    memoizedsize = size;
    return size;
  }

  @override
  public boolean equals(final object other) {
    if (other == this) {
      return true;
    }
    if (!(other instanceof message)) {
      return false;
    }
    final message othermessage = (message) other;
    if (getdescriptorfortype() != othermessage.getdescriptorfortype()) {
      return false;
    }
    return getallfields().equals(othermessage.getallfields()) &&
        getunknownfields().equals(othermessage.getunknownfields());
  }

  @override
  public int hashcode() {
    int hash = 41;
    hash = (19 * hash) + getdescriptorfortype().hashcode();
    hash = hashfields(hash, getallfields());
    hash = (29 * hash) + getunknownfields().hashcode();
    return hash;
  }

  /** get a hash code for given fields and values, using the given seed. */
  @suppresswarnings("unchecked")
  protected int hashfields(int hash, map<fielddescriptor, object> map) {
    for (map.entry<fielddescriptor, object> entry : map.entryset()) {
      fielddescriptor field = entry.getkey();
      object value = entry.getvalue();
      hash = (37 * hash) + field.getnumber();
      if (field.gettype() != fielddescriptor.type.enum){
        hash = (53 * hash) + value.hashcode();
      } else if (field.isrepeated()) {
        list<? extends enumlite> list = (list<? extends enumlite>) value;
        hash = (53 * hash) + hashenumlist(list);
      } else {
        hash = (53 * hash) + hashenum((enumlite) value);
      }
    }
    return hash;
  }

  /**
   * helper method for implementing {@link message#hashcode()}.
   * @see boolean#hashcode()
   */
  protected static int hashlong(long n) {
    return (int) (n ^ (n >>> 32));
  }

  /**
   * helper method for implementing {@link message#hashcode()}.
   * @see boolean#hashcode()
   */
  protected static int hashboolean(boolean b) {
    return b ? 1231 : 1237;
  }

  /**
   * package private helper method for abstractparser to create
   * uninitializedmessageexception with missing field information.
   */
  @override
  uninitializedmessageexception newuninitializedmessageexception() {
    return builder.newuninitializedmessageexception(this);
  }

  /**
   * helper method for implementing {@link message#hashcode()}.
   * <p>
   * this is needed because {@link java.lang.enum#hashcode()} is final, but we
   * need to use the field number as the hash code to ensure compatibility
   * between statically and dynamically generated enum objects.
   */
  protected static int hashenum(enumlite e) {
    return e.getnumber();
  }

  /** helper method for implementing {@link message#hashcode()}. */
  protected static int hashenumlist(list<? extends enumlite> list) {
    int hash = 1;
    for (enumlite e : list) {
      hash = 31 * hash + hashenum(e);
    }
    return hash;
  }

  // =================================================================

  /**
   * a partial implementation of the {@link message.builder} interface which
   * implements as many methods of that interface as possible in terms of
   * other methods.
   */
  @suppresswarnings("unchecked")
  public static abstract class builder<buildertype extends builder>
      extends abstractmessagelite.builder<buildertype>
      implements message.builder {
    // the compiler produces an error if this is not declared explicitly.
    @override
    public abstract buildertype clone();

    public buildertype clear() {
      for (final map.entry<fielddescriptor, object> entry :
           getallfields().entryset()) {
        clearfield(entry.getkey());
      }
      return (buildertype) this;
    }

    public list<string> findinitializationerrors() {
      return findmissingfields(this);
    }

    public string getinitializationerrorstring() {
      return delimitwithcommas(findinitializationerrors());
    }

    public buildertype mergefrom(final message other) {
      if (other.getdescriptorfortype() != getdescriptorfortype()) {
        throw new illegalargumentexception(
          "mergefrom(message) can only merge messages of the same type.");
      }

      // note:  we don't attempt to verify that other's fields have valid
      //   types.  doing so would be a losing battle.  we'd have to verify
      //   all sub-messages as well, and we'd have to make copies of all of
      //   them to insure that they don't change after verification (since
      //   the message interface itself cannot enforce immutability of
      //   implementations).
      // todo(kenton):  provide a function somewhere called makedeepcopy()
      //   which allows people to make secure deep copies of messages.

      for (final map.entry<fielddescriptor, object> entry :
           other.getallfields().entryset()) {
        final fielddescriptor field = entry.getkey();
        if (field.isrepeated()) {
          for (final object element : (list)entry.getvalue()) {
            addrepeatedfield(field, element);
          }
        } else if (field.getjavatype() == fielddescriptor.javatype.message) {
          final message existingvalue = (message)getfield(field);
          if (existingvalue == existingvalue.getdefaultinstancefortype()) {
            setfield(field, entry.getvalue());
          } else {
            setfield(field,
              existingvalue.newbuilderfortype()
                .mergefrom(existingvalue)
                .mergefrom((message)entry.getvalue())
                .build());
          }
        } else {
          setfield(field, entry.getvalue());
        }
      }

      mergeunknownfields(other.getunknownfields());

      return (buildertype) this;
    }

    @override
    public buildertype mergefrom(final codedinputstream input)
                                 throws ioexception {
      return mergefrom(input, extensionregistry.getemptyregistry());
    }

    @override
    public buildertype mergefrom(
        final codedinputstream input,
        final extensionregistrylite extensionregistry)
        throws ioexception {
      final unknownfieldset.builder unknownfields =
        unknownfieldset.newbuilder(getunknownfields());
      while (true) {
        final int tag = input.readtag();
        if (tag == 0) {
          break;
        }

        if (!mergefieldfrom(input, unknownfields, extensionregistry,
                            getdescriptorfortype(), this, null, tag)) {
          // end group tag
          break;
        }
      }
      setunknownfields(unknownfields.build());
      return (buildertype) this;
    }

    /** helper method to handle {@code builder} and {@code extensions}. */
    private static void addrepeatedfield(
        message.builder builder,
        fieldset<fielddescriptor> extensions,
        fielddescriptor field,
        object value) {
      if (builder != null) {
        builder.addrepeatedfield(field, value);
      } else {
        extensions.addrepeatedfield(field, value);
      }
    }

    /** helper method to handle {@code builder} and {@code extensions}. */
    private static void setfield(
        message.builder builder,
        fieldset<fielddescriptor> extensions,
        fielddescriptor field,
        object value) {
      if (builder != null) {
        builder.setfield(field, value);
      } else {
        extensions.setfield(field, value);
      }
    }

    /** helper method to handle {@code builder} and {@code extensions}. */
    private static boolean hasoriginalmessage(
        message.builder builder,
        fieldset<fielddescriptor> extensions,
        fielddescriptor field) {
      if (builder != null) {
        return builder.hasfield(field);
      } else {
        return extensions.hasfield(field);
      }
    }

    /** helper method to handle {@code builder} and {@code extensions}. */
    private static message getoriginalmessage(
        message.builder builder,
        fieldset<fielddescriptor> extensions,
        fielddescriptor field) {
      if (builder != null) {
        return (message) builder.getfield(field);
      } else {
        return (message) extensions.getfield(field);
      }
    }

    /** helper method to handle {@code builder} and {@code extensions}. */
    private static void mergeoriginalmessage(
        message.builder builder,
        fieldset<fielddescriptor> extensions,
        fielddescriptor field,
        message.builder subbuilder) {
      message originalmessage = getoriginalmessage(builder, extensions, field);
      if (originalmessage != null) {
        subbuilder.mergefrom(originalmessage);
      }
    }

    /**
     * like {@link #mergefrom(codedinputstream, extensionregistrylite)}, but
     * parses a single field.
     *
     * when {@code builder} is not null, the method will parse and merge the
     * field into {@code builder}. otherwise, it will try to parse the field
     * into {@code extensions}, when it's called by the parsing constructor in
     * generated classes.
     *
     * package-private because it is used by generatedmessage.extendablemessage.
     * @param tag the tag, which should have already been read.
     * @return {@code true} unless the tag is an end-group tag.
     */
    static boolean mergefieldfrom(
        codedinputstream input,
        unknownfieldset.builder unknownfields,
        extensionregistrylite extensionregistry,
        descriptor type,
        message.builder builder,
        fieldset<fielddescriptor> extensions,
        int tag) throws ioexception {
      if (type.getoptions().getmessagesetwireformat() &&
          tag == wireformat.message_set_item_tag) {
        mergemessagesetextensionfromcodedstream(
            input, unknownfields, extensionregistry, type, builder, extensions);
        return true;
      }

      final int wiretype = wireformat.gettagwiretype(tag);
      final int fieldnumber = wireformat.gettagfieldnumber(tag);

      final fielddescriptor field;
      message defaultinstance = null;

      if (type.isextensionnumber(fieldnumber)) {
        // extensionregistry may be either extensionregistry or
        // extensionregistrylite.  since the type we are parsing is a full
        // message, only a full extensionregistry could possibly contain
        // extensions of it.  otherwise we will treat the registry as if it
        // were empty.
        if (extensionregistry instanceof extensionregistry) {
          final extensionregistry.extensioninfo extension =
            ((extensionregistry) extensionregistry)
              .findextensionbynumber(type, fieldnumber);
          if (extension == null) {
            field = null;
          } else {
            field = extension.descriptor;
            defaultinstance = extension.defaultinstance;
            if (defaultinstance == null &&
                field.getjavatype() == fielddescriptor.javatype.message) {
              throw new illegalstateexception(
                  "message-typed extension lacked default instance: " +
                  field.getfullname());
            }
          }
        } else {
          field = null;
        }
      } else if (builder != null) {
        field = type.findfieldbynumber(fieldnumber);
      } else {
        field = null;
      }

      boolean unknown = false;
      boolean packed = false;
      if (field == null) {
        unknown = true;  // unknown field.
      } else if (wiretype == fieldset.getwireformatforfieldtype(
                   field.getlitetype(),
                   false  /* ispacked */)) {
        packed = false;
      } else if (field.ispackable() &&
                 wiretype == fieldset.getwireformatforfieldtype(
                   field.getlitetype(),
                   true  /* ispacked */)) {
        packed = true;
      } else {
        unknown = true;  // unknown wire type.
      }

      if (unknown) {  // unknown field or wrong wire type.  skip.
        return unknownfields.mergefieldfrom(tag, input);
      }

      if (packed) {
        final int length = input.readrawvarint32();
        final int limit = input.pushlimit(length);
        if (field.getlitetype() == wireformat.fieldtype.enum) {
          while (input.getbytesuntillimit() > 0) {
            final int rawvalue = input.readenum();
            final object value = field.getenumtype().findvaluebynumber(rawvalue);
            if (value == null) {
              // if the number isn't recognized as a valid value for this
              // enum, drop it (don't even add it to unknownfields).
              return true;
            }
            addrepeatedfield(builder, extensions, field, value);
          }
        } else {
          while (input.getbytesuntillimit() > 0) {
            final object value =
              fieldset.readprimitivefield(input, field.getlitetype());
            addrepeatedfield(builder, extensions, field, value);
          }
        }
        input.poplimit(limit);
      } else {
        final object value;
        switch (field.gettype()) {
          case group: {
            final message.builder subbuilder;
            if (defaultinstance != null) {
              subbuilder = defaultinstance.newbuilderfortype();
            } else {
              subbuilder = builder.newbuilderforfield(field);
            }
            if (!field.isrepeated()) {
              mergeoriginalmessage(builder, extensions, field, subbuilder);
            }
            input.readgroup(field.getnumber(), subbuilder, extensionregistry);
            value = subbuilder.buildpartial();
            break;
          }
          case message: {
            final message.builder subbuilder;
            if (defaultinstance != null) {
              subbuilder = defaultinstance.newbuilderfortype();
            } else {
              subbuilder = builder.newbuilderforfield(field);
            }
            if (!field.isrepeated()) {
              mergeoriginalmessage(builder, extensions, field, subbuilder);
            }
            input.readmessage(subbuilder, extensionregistry);
            value = subbuilder.buildpartial();
            break;
          }
          case enum:
            final int rawvalue = input.readenum();
            value = field.getenumtype().findvaluebynumber(rawvalue);
            // if the number isn't recognized as a valid value for this enum,
            // drop it.
            if (value == null) {
              unknownfields.mergevarintfield(fieldnumber, rawvalue);
              return true;
            }
            break;
          default:
            value = fieldset.readprimitivefield(input, field.getlitetype());
            break;
        }

        if (field.isrepeated()) {
          addrepeatedfield(builder, extensions, field, value);
        } else {
          setfield(builder, extensions, field, value);
        }
      }

      return true;
    }

    /**
     * called by {@code #mergefieldfrom()} to parse a messageset extension.
     * if {@code builder} is not null, this method will merge messageset into
     * the builder.  otherwise, it will merge the messageset into {@code
     * extensions}.
     */
    private static void mergemessagesetextensionfromcodedstream(
        codedinputstream input,
        unknownfieldset.builder unknownfields,
        extensionregistrylite extensionregistry,
        descriptor type,
        message.builder builder,
        fieldset<fielddescriptor> extensions) throws ioexception {

      // the wire format for messageset is:
      //   message messageset {
      //     repeated group item = 1 {
      //       required int32 typeid = 2;
      //       required bytes message = 3;
      //     }
      //   }
      // "typeid" is the extension's field number.  the extension can only be
      // a message type, where "message" contains the encoded bytes of that
      // message.
      //
      // in practice, we will probably never see a messageset item in which
      // the message appears before the type id, or where either field does not
      // appear exactly once.  however, in theory such cases are valid, so we
      // should be prepared to accept them.

      int typeid = 0;
      bytestring rawbytes = null; // if we encounter "message" before "typeid"
      extensionregistry.extensioninfo extension = null;

      // read bytes from input, if we get it's type first then parse it eagerly,
      // otherwise we store the raw bytes in a local variable.
      while (true) {
        final int tag = input.readtag();
        if (tag == 0) {
          break;
        }

        if (tag == wireformat.message_set_type_id_tag) {
          typeid = input.readuint32();
          if (typeid != 0) {
            // extensionregistry may be either extensionregistry or
            // extensionregistrylite. since the type we are parsing is a full
            // message, only a full extensionregistry could possibly contain
            // extensions of it. otherwise we will treat the registry as if it
            // were empty.
            if (extensionregistry instanceof extensionregistry) {
              extension = ((extensionregistry) extensionregistry)
                  .findextensionbynumber(type, typeid);
            }
          }

        } else if (tag == wireformat.message_set_message_tag) {
          if (typeid != 0) {
            if (extension != null && extensionregistrylite.iseagerlyparsemessagesets()) {
              // we already know the type, so we can parse directly from the
              // input with no copying.  hooray!
              eagerlymergemessagesetextension(
                  input, extension, extensionregistry, builder, extensions);
              rawbytes = null;
              continue;
            }
          }
          // we haven't seen a type id yet or we want parse message lazily.
          rawbytes = input.readbytes();

        } else { // unknown tag. skip it.
          if (!input.skipfield(tag)) {
            break; // end of group
          }
        }
      }
      input.checklasttagwas(wireformat.message_set_item_end_tag);

      // process the raw bytes.
      if (rawbytes != null && typeid != 0) { // zero is not a valid type id.
        if (extension != null) { // we known the type
          mergemessagesetextensionfrombytes(
              rawbytes, extension, extensionregistry, builder, extensions);
        } else { // we don't know how to parse this. ignore it.
          if (rawbytes != null) {
            unknownfields.mergefield(typeid, unknownfieldset.field.newbuilder()
                .addlengthdelimited(rawbytes).build());
          }
        }
      }
    }

    private static void eagerlymergemessagesetextension(
        codedinputstream input,
        extensionregistry.extensioninfo extension,
        extensionregistrylite extensionregistry,
        message.builder builder,
        fieldset<fielddescriptor> extensions) throws ioexception {

      fielddescriptor field = extension.descriptor;
      message value = null;
      if (hasoriginalmessage(builder, extensions, field)) {
        message originalmessage =
            getoriginalmessage(builder, extensions, field);
        message.builder subbuilder = originalmessage.tobuilder();
        input.readmessage(subbuilder, extensionregistry);
        value = subbuilder.buildpartial();
      } else {
        value = input.readmessage(extension.defaultinstance.getparserfortype(),
          extensionregistry);
      }

      if (builder != null) {
        builder.setfield(field, value);
      } else {
        extensions.setfield(field, value);
      }
    }

    private static void mergemessagesetextensionfrombytes(
        bytestring rawbytes,
        extensionregistry.extensioninfo extension,
        extensionregistrylite extensionregistry,
        message.builder builder,
        fieldset<fielddescriptor> extensions) throws ioexception {

      fielddescriptor field = extension.descriptor;
      boolean hasoriginalvalue = hasoriginalmessage(builder, extensions, field);

      if (hasoriginalvalue || extensionregistrylite.iseagerlyparsemessagesets()) {
        // if the field already exists, we just parse the field.
        message value = null;
        if (hasoriginalvalue) {
          message originalmessage =
              getoriginalmessage(builder, extensions, field);
          message.builder subbuilder= originalmessage.tobuilder();
          subbuilder.mergefrom(rawbytes, extensionregistry);
          value = subbuilder.buildpartial();
        } else {
          value = extension.defaultinstance.getparserfortype()
              .parsepartialfrom(rawbytes, extensionregistry);
        }
        setfield(builder, extensions, field, value);
      } else {
        // use lazyfield to load messageset lazily.
        lazyfield lazyfield = new lazyfield(
            extension.defaultinstance, extensionregistry, rawbytes);
        if (builder != null) {
          // todo(xiangl): it looks like this method can only be invoked by
          // extendablebuilder, but i'm not sure. so i double check the type of
          // builder here. it may be useless and need more investigation.
          if (builder instanceof extendablebuilder) {
            builder.setfield(field, lazyfield);
          } else {
            builder.setfield(field, lazyfield.getvalue());
          }
        } else {
          extensions.setfield(field, lazyfield);
        }
      }
    }

    public buildertype mergeunknownfields(final unknownfieldset unknownfields) {
      setunknownfields(
        unknownfieldset.newbuilder(getunknownfields())
                       .mergefrom(unknownfields)
                       .build());
      return (buildertype) this;
    }

    public message.builder getfieldbuilder(final fielddescriptor field) {
      throw new unsupportedoperationexception(
          "getfieldbuilder() called on an unsupported message type.");
    }

    /**
     * construct an uninitializedmessageexception reporting missing fields in
     * the given message.
     */
    protected static uninitializedmessageexception
        newuninitializedmessageexception(message message) {
      return new uninitializedmessageexception(findmissingfields(message));
    }

    /**
     * populates {@code this.missingfields} with the full "path" of each
     * missing required field in the given message.
     */
    private static list<string> findmissingfields(
        final messageorbuilder message) {
      final list<string> results = new arraylist<string>();
      findmissingfields(message, "", results);
      return results;
    }

    /** recursive helper implementing {@link #findmissingfields(message)}. */
    private static void findmissingfields(final messageorbuilder message,
                                          final string prefix,
                                          final list<string> results) {
      for (final fielddescriptor field :
          message.getdescriptorfortype().getfields()) {
        if (field.isrequired() && !message.hasfield(field)) {
          results.add(prefix + field.getname());
        }
      }

      for (final map.entry<fielddescriptor, object> entry :
           message.getallfields().entryset()) {
        final fielddescriptor field = entry.getkey();
        final object value = entry.getvalue();

        if (field.getjavatype() == fielddescriptor.javatype.message) {
          if (field.isrepeated()) {
            int i = 0;
            for (final object element : (list) value) {
              findmissingfields((messageorbuilder) element,
                                submessageprefix(prefix, field, i++),
                                results);
            }
          } else {
            if (message.hasfield(field)) {
              findmissingfields((messageorbuilder) value,
                                submessageprefix(prefix, field, -1),
                                results);
            }
          }
        }
      }
    }

    private static string submessageprefix(final string prefix,
                                           final fielddescriptor field,
                                           final int index) {
      final stringbuilder result = new stringbuilder(prefix);
      if (field.isextension()) {
        result.append('(')
              .append(field.getfullname())
              .append(')');
      } else {
        result.append(field.getname());
      }
      if (index != -1) {
        result.append('[')
              .append(index)
              .append(']');
      }
      result.append('.');
      return result.tostring();
    }

    // ===============================================================
    // the following definitions seem to be required in order to make javac
    // not produce weird errors like:
    //
    // java/com/google/protobuf/dynamicmessage.java:203: types
    //   com.google.protobuf.abstractmessage.builder<
    //     com.google.protobuf.dynamicmessage.builder> and
    //   com.google.protobuf.abstractmessage.builder<
    //     com.google.protobuf.dynamicmessage.builder> are incompatible; both
    //   define mergefrom(com.google.protobuf.bytestring), but with unrelated
    //   return types.
    //
    // strangely, these lines are only needed if javac is invoked separately
    // on abstractmessage.java and abstractmessagelite.java.  if javac is
    // invoked on both simultaneously, it works.  (or maybe the important
    // point is whether or not dynamicmessage.java is compiled together with
    // abstractmessagelite.java -- not sure.)  i suspect this is a compiler
    // bug.

    @override
    public buildertype mergefrom(final bytestring data)
        throws invalidprotocolbufferexception {
      return super.mergefrom(data);
    }

    @override
    public buildertype mergefrom(
        final bytestring data,
        final extensionregistrylite extensionregistry)
        throws invalidprotocolbufferexception {
      return super.mergefrom(data, extensionregistry);
    }

    @override
    public buildertype mergefrom(final byte[] data)
        throws invalidprotocolbufferexception {
      return super.mergefrom(data);
    }

    @override
    public buildertype mergefrom(
        final byte[] data, final int off, final int len)
        throws invalidprotocolbufferexception {
      return super.mergefrom(data, off, len);
    }

    @override
    public buildertype mergefrom(
        final byte[] data,
        final extensionregistrylite extensionregistry)
        throws invalidprotocolbufferexception {
      return super.mergefrom(data, extensionregistry);
    }

    @override
    public buildertype mergefrom(
        final byte[] data, final int off, final int len,
        final extensionregistrylite extensionregistry)
        throws invalidprotocolbufferexception {
      return super.mergefrom(data, off, len, extensionregistry);
    }

    @override
    public buildertype mergefrom(final inputstream input)
        throws ioexception {
      return super.mergefrom(input);
    }

    @override
    public buildertype mergefrom(
        final inputstream input,
        final extensionregistrylite extensionregistry)
        throws ioexception {
      return super.mergefrom(input, extensionregistry);
    }

    @override
    public boolean mergedelimitedfrom(final inputstream input)
        throws ioexception {
      return super.mergedelimitedfrom(input);
    }

    @override
    public boolean mergedelimitedfrom(
        final inputstream input,
        final extensionregistrylite extensionregistry)
        throws ioexception {
      return super.mergedelimitedfrom(input, extensionregistry);
    }

  }
}
