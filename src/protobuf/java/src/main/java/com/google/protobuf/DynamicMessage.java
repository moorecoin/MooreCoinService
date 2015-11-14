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

import java.io.inputstream;
import java.io.ioexception;
import java.util.collections;
import java.util.map;

/**
 * an implementation of {@link message} that can represent arbitrary types,
 * given a {@link descriptors.descriptor}.
 *
 * @author kenton@google.com kenton varda
 */
public final class dynamicmessage extends abstractmessage {
  private final descriptor type;
  private final fieldset<fielddescriptor> fields;
  private final unknownfieldset unknownfields;
  private int memoizedsize = -1;

  /**
   * construct a {@code dynamicmessage} using the given {@code fieldset}.
   */
  private dynamicmessage(descriptor type, fieldset<fielddescriptor> fields,
                         unknownfieldset unknownfields) {
    this.type = type;
    this.fields = fields;
    this.unknownfields = unknownfields;
  }

  /**
   * get a {@code dynamicmessage} representing the default instance of the
   * given type.
   */
  public static dynamicmessage getdefaultinstance(descriptor type) {
    return new dynamicmessage(type, fieldset.<fielddescriptor>emptyset(),
                              unknownfieldset.getdefaultinstance());
  }

  /** parse a message of the given type from the given input stream. */
  public static dynamicmessage parsefrom(descriptor type,
                                         codedinputstream input)
                                         throws ioexception {
    return newbuilder(type).mergefrom(input).buildparsed();
  }

  /** parse a message of the given type from the given input stream. */
  public static dynamicmessage parsefrom(
      descriptor type,
      codedinputstream input,
      extensionregistry extensionregistry)
      throws ioexception {
    return newbuilder(type).mergefrom(input, extensionregistry).buildparsed();
  }

  /** parse {@code data} as a message of the given type and return it. */
  public static dynamicmessage parsefrom(descriptor type, bytestring data)
                                         throws invalidprotocolbufferexception {
    return newbuilder(type).mergefrom(data).buildparsed();
  }

  /** parse {@code data} as a message of the given type and return it. */
  public static dynamicmessage parsefrom(descriptor type, bytestring data,
                                         extensionregistry extensionregistry)
                                         throws invalidprotocolbufferexception {
    return newbuilder(type).mergefrom(data, extensionregistry).buildparsed();
  }

  /** parse {@code data} as a message of the given type and return it. */
  public static dynamicmessage parsefrom(descriptor type, byte[] data)
                                         throws invalidprotocolbufferexception {
    return newbuilder(type).mergefrom(data).buildparsed();
  }

  /** parse {@code data} as a message of the given type and return it. */
  public static dynamicmessage parsefrom(descriptor type, byte[] data,
                                         extensionregistry extensionregistry)
                                         throws invalidprotocolbufferexception {
    return newbuilder(type).mergefrom(data, extensionregistry).buildparsed();
  }

  /** parse a message of the given type from {@code input} and return it. */
  public static dynamicmessage parsefrom(descriptor type, inputstream input)
                                         throws ioexception {
    return newbuilder(type).mergefrom(input).buildparsed();
  }

  /** parse a message of the given type from {@code input} and return it. */
  public static dynamicmessage parsefrom(descriptor type, inputstream input,
                                         extensionregistry extensionregistry)
                                         throws ioexception {
    return newbuilder(type).mergefrom(input, extensionregistry).buildparsed();
  }

  /** construct a {@link message.builder} for the given type. */
  public static builder newbuilder(descriptor type) {
    return new builder(type);
  }

  /**
   * construct a {@link message.builder} for a message of the same type as
   * {@code prototype}, and initialize it with {@code prototype}'s contents.
   */
  public static builder newbuilder(message prototype) {
    return new builder(prototype.getdescriptorfortype()).mergefrom(prototype);
  }

  // -----------------------------------------------------------------
  // implementation of message interface.

  public descriptor getdescriptorfortype() {
    return type;
  }

  public dynamicmessage getdefaultinstancefortype() {
    return getdefaultinstance(type);
  }

  public map<fielddescriptor, object> getallfields() {
    return fields.getallfields();
  }

  public boolean hasfield(fielddescriptor field) {
    verifycontainingtype(field);
    return fields.hasfield(field);
  }

  public object getfield(fielddescriptor field) {
    verifycontainingtype(field);
    object result = fields.getfield(field);
    if (result == null) {
      if (field.isrepeated()) {
        result = collections.emptylist();
      } else if (field.getjavatype() == fielddescriptor.javatype.message) {
        result = getdefaultinstance(field.getmessagetype());
      } else {
        result = field.getdefaultvalue();
      }
    }
    return result;
  }

  public int getrepeatedfieldcount(fielddescriptor field) {
    verifycontainingtype(field);
    return fields.getrepeatedfieldcount(field);
  }

  public object getrepeatedfield(fielddescriptor field, int index) {
    verifycontainingtype(field);
    return fields.getrepeatedfield(field, index);
  }

  public unknownfieldset getunknownfields() {
    return unknownfields;
  }

  private static boolean isinitialized(descriptor type,
                                       fieldset<fielddescriptor> fields) {
    // check that all required fields are present.
    for (final fielddescriptor field : type.getfields()) {
      if (field.isrequired()) {
        if (!fields.hasfield(field)) {
          return false;
        }
      }
    }

    // check that embedded messages are initialized.
    return fields.isinitialized();
  }

  @override
  public boolean isinitialized() {
    return isinitialized(type, fields);
  }

  @override
  public void writeto(codedoutputstream output) throws ioexception {
    if (type.getoptions().getmessagesetwireformat()) {
      fields.writemessagesetto(output);
      unknownfields.writeasmessagesetto(output);
    } else {
      fields.writeto(output);
      unknownfields.writeto(output);
    }
  }

  @override
  public int getserializedsize() {
    int size = memoizedsize;
    if (size != -1) return size;

    if (type.getoptions().getmessagesetwireformat()) {
      size = fields.getmessagesetserializedsize();
      size += unknownfields.getserializedsizeasmessageset();
    } else {
      size = fields.getserializedsize();
      size += unknownfields.getserializedsize();
    }

    memoizedsize = size;
    return size;
  }

  public builder newbuilderfortype() {
    return new builder(type);
  }

  public builder tobuilder() {
    return newbuilderfortype().mergefrom(this);
  }

  public parser<dynamicmessage> getparserfortype() {
    return new abstractparser<dynamicmessage>() {
      public dynamicmessage parsepartialfrom(
          codedinputstream input,
          extensionregistrylite extensionregistry)
          throws invalidprotocolbufferexception {
        builder builder = newbuilder(type);
        try {
          builder.mergefrom(input, extensionregistry);
        } catch (invalidprotocolbufferexception e) {
          throw e.setunfinishedmessage(builder.buildpartial());
        } catch (ioexception e) {
          throw new invalidprotocolbufferexception(e.getmessage())
              .setunfinishedmessage(builder.buildpartial());
        }
        return builder.buildpartial();
      }
    };
  }

  /** verifies that the field is a field of this message. */
  private void verifycontainingtype(fielddescriptor field) {
    if (field.getcontainingtype() != type) {
      throw new illegalargumentexception(
        "fielddescriptor does not match message type.");
    }
  }

  // =================================================================

  /**
   * builder for {@link dynamicmessage}s.
   */
  public static final class builder extends abstractmessage.builder<builder> {
    private final descriptor type;
    private fieldset<fielddescriptor> fields;
    private unknownfieldset unknownfields;

    /** construct a {@code builder} for the given type. */
    private builder(descriptor type) {
      this.type = type;
      this.fields = fieldset.newfieldset();
      this.unknownfields = unknownfieldset.getdefaultinstance();
    }

    // ---------------------------------------------------------------
    // implementation of message.builder interface.

    @override
    public builder clear() {
      if (fields.isimmutable()) {
        fields = fieldset.newfieldset();
      } else {
        fields.clear();
      }
      unknownfields = unknownfieldset.getdefaultinstance();
      return this;
    }

    @override
    public builder mergefrom(message other) {
      if (other instanceof dynamicmessage) {
        // this should be somewhat faster than calling super.mergefrom().
        dynamicmessage otherdynamicmessage = (dynamicmessage) other;
        if (otherdynamicmessage.type != type) {
          throw new illegalargumentexception(
            "mergefrom(message) can only merge messages of the same type.");
        }
        ensureismutable();
        fields.mergefrom(otherdynamicmessage.fields);
        mergeunknownfields(otherdynamicmessage.unknownfields);
        return this;
      } else {
        return super.mergefrom(other);
      }
    }

    public dynamicmessage build() {
      if (!isinitialized()) {
        throw newuninitializedmessageexception(
          new dynamicmessage(type, fields, unknownfields));
      }
      return buildpartial();
    }

    /**
     * helper for dynamicmessage.parsefrom() methods to call.  throws
     * {@link invalidprotocolbufferexception} instead of
     * {@link uninitializedmessageexception}.
     */
    private dynamicmessage buildparsed() throws invalidprotocolbufferexception {
      if (!isinitialized()) {
        throw newuninitializedmessageexception(
            new dynamicmessage(type, fields, unknownfields))
          .asinvalidprotocolbufferexception();
      }
      return buildpartial();
    }

    public dynamicmessage buildpartial() {
      fields.makeimmutable();
      dynamicmessage result =
        new dynamicmessage(type, fields, unknownfields);
      return result;
    }

    @override
    public builder clone() {
      builder result = new builder(type);
      result.fields.mergefrom(fields);
      result.mergeunknownfields(unknownfields);
      return result;
    }

    public boolean isinitialized() {
      return dynamicmessage.isinitialized(type, fields);
    }

    public descriptor getdescriptorfortype() {
      return type;
    }

    public dynamicmessage getdefaultinstancefortype() {
      return getdefaultinstance(type);
    }

    public map<fielddescriptor, object> getallfields() {
      return fields.getallfields();
    }

    public builder newbuilderforfield(fielddescriptor field) {
      verifycontainingtype(field);

      if (field.getjavatype() != fielddescriptor.javatype.message) {
        throw new illegalargumentexception(
          "newbuilderforfield is only valid for fields with message type.");
      }

      return new builder(field.getmessagetype());
    }

    public boolean hasfield(fielddescriptor field) {
      verifycontainingtype(field);
      return fields.hasfield(field);
    }

    public object getfield(fielddescriptor field) {
      verifycontainingtype(field);
      object result = fields.getfield(field);
      if (result == null) {
        if (field.getjavatype() == fielddescriptor.javatype.message) {
          result = getdefaultinstance(field.getmessagetype());
        } else {
          result = field.getdefaultvalue();
        }
      }
      return result;
    }

    public builder setfield(fielddescriptor field, object value) {
      verifycontainingtype(field);
      ensureismutable();
      fields.setfield(field, value);
      return this;
    }

    public builder clearfield(fielddescriptor field) {
      verifycontainingtype(field);
      ensureismutable();
      fields.clearfield(field);
      return this;
    }

    public int getrepeatedfieldcount(fielddescriptor field) {
      verifycontainingtype(field);
      return fields.getrepeatedfieldcount(field);
    }

    public object getrepeatedfield(fielddescriptor field, int index) {
      verifycontainingtype(field);
      return fields.getrepeatedfield(field, index);
    }

    public builder setrepeatedfield(fielddescriptor field,
                                    int index, object value) {
      verifycontainingtype(field);
      ensureismutable();
      fields.setrepeatedfield(field, index, value);
      return this;
    }

    public builder addrepeatedfield(fielddescriptor field, object value) {
      verifycontainingtype(field);
      ensureismutable();
      fields.addrepeatedfield(field, value);
      return this;
    }

    public unknownfieldset getunknownfields() {
      return unknownfields;
    }

    public builder setunknownfields(unknownfieldset unknownfields) {
      this.unknownfields = unknownfields;
      return this;
    }

    @override
    public builder mergeunknownfields(unknownfieldset unknownfields) {
      this.unknownfields =
        unknownfieldset.newbuilder(this.unknownfields)
                       .mergefrom(unknownfields)
                       .build();
      return this;
    }

    /** verifies that the field is a field of this message. */
    private void verifycontainingtype(fielddescriptor field) {
      if (field.getcontainingtype() != type) {
        throw new illegalargumentexception(
          "fielddescriptor does not match message type.");
      }
    }

    private void ensureismutable() {
      if (fields.isimmutable()) {
        fields = fields.clone();
      }
    }

    @override
    public com.google.protobuf.message.builder getfieldbuilder(fielddescriptor field) {
      // todo(xiangl): need implementation for dynamic message
      throw new unsupportedoperationexception(
        "getfieldbuilder() called on a dynamic message type.");
    }
  }
}
