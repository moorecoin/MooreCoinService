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
import java.io.objectstreamexception;
import java.io.serializable;
import java.lang.reflect.invocationtargetexception;
import java.lang.reflect.method;
import java.util.collections;
import java.util.iterator;
import java.util.list;
import java.util.map;

/**
 * lite version of {@link generatedmessage}.
 *
 * @author kenton@google.com kenton varda
 */
public abstract class generatedmessagelite extends abstractmessagelite
    implements serializable {
  private static final long serialversionuid = 1l;

  protected generatedmessagelite() {
  }

  protected generatedmessagelite(builder builder) {
  }

  public parser<? extends messagelite> getparserfortype() {
    throw new unsupportedoperationexception(
        "this is supposed to be overridden by subclasses.");
  }

  /**
   * called by subclasses to parse an unknown field.
   * @return {@code true} unless the tag is an end-group tag.
   */
  protected boolean parseunknownfield(
      codedinputstream input,
      extensionregistrylite extensionregistry,
      int tag) throws ioexception {
    return input.skipfield(tag);
  }

  /**
   * used by parsing constructors in generated classes.
   */
  protected void makeextensionsimmutable() {
    // noop for messages without extensions.
  }

  @suppresswarnings("unchecked")
  public abstract static class builder<messagetype extends generatedmessagelite,
                                       buildertype extends builder>
      extends abstractmessagelite.builder<buildertype> {
    protected builder() {}

    //@override (java 1.6 override semantics, but we must support 1.5)
    public buildertype clear() {
      return (buildertype) this;
    }

    // this is implemented here only to work around an apparent bug in the
    // java compiler and/or build system.  see bug #1898463.  the mere presence
    // of this dummy clone() implementation makes it go away.
    @override
    public buildertype clone() {
      throw new unsupportedoperationexception(
          "this is supposed to be overridden by subclasses.");
    }

    /** all subclasses implement this. */
    public abstract buildertype mergefrom(messagetype message);

    // defined here for return type covariance.
    public abstract messagetype getdefaultinstancefortype();

    /**
     * called by subclasses to parse an unknown field.
     * @return {@code true} unless the tag is an end-group tag.
     */
    protected boolean parseunknownfield(
        codedinputstream input,
        extensionregistrylite extensionregistry,
        int tag) throws ioexception {
      return input.skipfield(tag);
    }
  }

  // =================================================================
  // extensions-related stuff

  /**
   * lite equivalent of {@link com.google.protobuf.generatedmessage.extendablemessageorbuilder}.
   */
  public interface extendablemessageorbuilder<
      messagetype extends extendablemessage> extends messageliteorbuilder {

    /** check if a singular extension is present. */
    <type> boolean hasextension(
        generatedextension<messagetype, type> extension);

    /** get the number of elements in a repeated extension. */
    <type> int getextensioncount(
        generatedextension<messagetype, list<type>> extension);

    /** get the value of an extension. */
    <type> type getextension(generatedextension<messagetype, type> extension);

    /** get one element of a repeated extension. */
    <type> type getextension(
        generatedextension<messagetype, list<type>> extension,
        int index);
  }

  /**
   * lite equivalent of {@link generatedmessage.extendablemessage}.
   */
  public abstract static class extendablemessage<
        messagetype extends extendablemessage<messagetype>>
      extends generatedmessagelite
      implements extendablemessageorbuilder<messagetype> {

    private final fieldset<extensiondescriptor> extensions;

    protected extendablemessage() {
      this.extensions = fieldset.newfieldset();
    }

    protected extendablemessage(extendablebuilder<messagetype, ?> builder) {
      this.extensions = builder.buildextensions();
    }

    private void verifyextensioncontainingtype(
        final generatedextension<messagetype, ?> extension) {
      if (extension.getcontainingtypedefaultinstance() !=
          getdefaultinstancefortype()) {
        // this can only happen if someone uses unchecked operations.
        throw new illegalargumentexception(
          "this extension is for a different message type.  please make " +
          "sure that you are not suppressing any generics type warnings.");
      }
    }

    /** check if a singular extension is present. */
    //@override (java 1.6 override semantics, but we must support 1.5)
    public final <type> boolean hasextension(
        final generatedextension<messagetype, type> extension) {
      verifyextensioncontainingtype(extension);
      return extensions.hasfield(extension.descriptor);
    }

    /** get the number of elements in a repeated extension. */
    //@override (java 1.6 override semantics, but we must support 1.5)
    public final <type> int getextensioncount(
        final generatedextension<messagetype, list<type>> extension) {
      verifyextensioncontainingtype(extension);
      return extensions.getrepeatedfieldcount(extension.descriptor);
    }

    /** get the value of an extension. */
    //@override (java 1.6 override semantics, but we must support 1.5)
    @suppresswarnings("unchecked")
    public final <type> type getextension(
        final generatedextension<messagetype, type> extension) {
      verifyextensioncontainingtype(extension);
      final object value = extensions.getfield(extension.descriptor);
      if (value == null) {
        return extension.defaultvalue;
      } else {
        return (type) value;
      }
    }

    /** get one element of a repeated extension. */
    //@override (java 1.6 override semantics, but we must support 1.5)
    @suppresswarnings("unchecked")
    public final <type> type getextension(
        final generatedextension<messagetype, list<type>> extension,
        final int index) {
      verifyextensioncontainingtype(extension);
      return (type) extensions.getrepeatedfield(extension.descriptor, index);
    }

    /** called by subclasses to check if all extensions are initialized. */
    protected boolean extensionsareinitialized() {
      return extensions.isinitialized();
    }

    /**
     * called by subclasses to parse an unknown field or an extension.
     * @return {@code true} unless the tag is an end-group tag.
     */
    @override
    protected boolean parseunknownfield(
        codedinputstream input,
        extensionregistrylite extensionregistry,
        int tag) throws ioexception {
      return generatedmessagelite.parseunknownfield(
          extensions,
          getdefaultinstancefortype(),
          input,
          extensionregistry,
          tag);
    }

    /**
     * used by parsing constructors in generated classes.
     */
    @override
    protected void makeextensionsimmutable() {
      extensions.makeimmutable();
    }

    /**
     * used by subclasses to serialize extensions.  extension ranges may be
     * interleaved with field numbers, but we must write them in canonical
     * (sorted by field number) order.  extensionwriter helps us write
     * individual ranges of extensions at once.
     */
    protected class extensionwriter {
      // imagine how much simpler this code would be if java iterators had
      // a way to get the next element without advancing the iterator.

      private final iterator<map.entry<extensiondescriptor, object>> iter =
            extensions.iterator();
      private map.entry<extensiondescriptor, object> next;
      private final boolean messagesetwireformat;

      private extensionwriter(boolean messagesetwireformat) {
        if (iter.hasnext()) {
          next = iter.next();
        }
        this.messagesetwireformat = messagesetwireformat;
      }

      public void writeuntil(final int end, final codedoutputstream output)
                             throws ioexception {
        while (next != null && next.getkey().getnumber() < end) {
          extensiondescriptor extension = next.getkey();
          if (messagesetwireformat && extension.getlitejavatype() ==
                  wireformat.javatype.message &&
              !extension.isrepeated()) {
            output.writemessagesetextension(extension.getnumber(),
                                            (messagelite) next.getvalue());
          } else {
            fieldset.writefield(extension, next.getvalue(), output);
          }
          if (iter.hasnext()) {
            next = iter.next();
          } else {
            next = null;
          }
        }
      }
    }

    protected extensionwriter newextensionwriter() {
      return new extensionwriter(false);
    }
    protected extensionwriter newmessagesetextensionwriter() {
      return new extensionwriter(true);
    }

    /** called by subclasses to compute the size of extensions. */
    protected int extensionsserializedsize() {
      return extensions.getserializedsize();
    }
    protected int extensionsserializedsizeasmessageset() {
      return extensions.getmessagesetserializedsize();
    }
  }

  /**
   * lite equivalent of {@link generatedmessage.extendablebuilder}.
   */
  @suppresswarnings("unchecked")
  public abstract static class extendablebuilder<
        messagetype extends extendablemessage<messagetype>,
        buildertype extends extendablebuilder<messagetype, buildertype>>
      extends builder<messagetype, buildertype>
      implements extendablemessageorbuilder<messagetype> {
    protected extendablebuilder() {}

    private fieldset<extensiondescriptor> extensions = fieldset.emptyset();
    private boolean extensionsismutable;

    @override
    public buildertype clear() {
      extensions.clear();
      extensionsismutable = false;
      return super.clear();
    }

    private void ensureextensionsismutable() {
      if (!extensionsismutable) {
        extensions = extensions.clone();
        extensionsismutable = true;
      }
    }

    /**
     * called by the build code path to create a copy of the extensions for
     * building the message.
     */
    private fieldset<extensiondescriptor> buildextensions() {
      extensions.makeimmutable();
      extensionsismutable = false;
      return extensions;
    }

    private void verifyextensioncontainingtype(
        final generatedextension<messagetype, ?> extension) {
      if (extension.getcontainingtypedefaultinstance() !=
          getdefaultinstancefortype()) {
        // this can only happen if someone uses unchecked operations.
        throw new illegalargumentexception(
          "this extension is for a different message type.  please make " +
          "sure that you are not suppressing any generics type warnings.");
      }
    }

    /** check if a singular extension is present. */
    //@override (java 1.6 override semantics, but we must support 1.5)
    public final <type> boolean hasextension(
        final generatedextension<messagetype, type> extension) {
      verifyextensioncontainingtype(extension);
      return extensions.hasfield(extension.descriptor);
    }

    /** get the number of elements in a repeated extension. */
    //@override (java 1.6 override semantics, but we must support 1.5)
    public final <type> int getextensioncount(
        final generatedextension<messagetype, list<type>> extension) {
      verifyextensioncontainingtype(extension);
      return extensions.getrepeatedfieldcount(extension.descriptor);
    }

    /** get the value of an extension. */
    //@override (java 1.6 override semantics, but we must support 1.5)
    @suppresswarnings("unchecked")
    public final <type> type getextension(
        final generatedextension<messagetype, type> extension) {
      verifyextensioncontainingtype(extension);
      final object value = extensions.getfield(extension.descriptor);
      if (value == null) {
        return extension.defaultvalue;
      } else {
        return (type) value;
      }
    }

    /** get one element of a repeated extension. */
    @suppresswarnings("unchecked")
    //@override (java 1.6 override semantics, but we must support 1.5)
    public final <type> type getextension(
        final generatedextension<messagetype, list<type>> extension,
        final int index) {
      verifyextensioncontainingtype(extension);
      return (type) extensions.getrepeatedfield(extension.descriptor, index);
    }

    // this is implemented here only to work around an apparent bug in the
    // java compiler and/or build system.  see bug #1898463.  the mere presence
    // of this dummy clone() implementation makes it go away.
    @override
    public buildertype clone() {
      throw new unsupportedoperationexception(
          "this is supposed to be overridden by subclasses.");
    }

    /** set the value of an extension. */
    public final <type> buildertype setextension(
        final generatedextension<messagetype, type> extension,
        final type value) {
      verifyextensioncontainingtype(extension);
      ensureextensionsismutable();
      extensions.setfield(extension.descriptor, value);
      return (buildertype) this;
    }

    /** set the value of one element of a repeated extension. */
    public final <type> buildertype setextension(
        final generatedextension<messagetype, list<type>> extension,
        final int index, final type value) {
      verifyextensioncontainingtype(extension);
      ensureextensionsismutable();
      extensions.setrepeatedfield(extension.descriptor, index, value);
      return (buildertype) this;
    }

    /** append a value to a repeated extension. */
    public final <type> buildertype addextension(
        final generatedextension<messagetype, list<type>> extension,
        final type value) {
      verifyextensioncontainingtype(extension);
      ensureextensionsismutable();
      extensions.addrepeatedfield(extension.descriptor, value);
      return (buildertype) this;
    }

    /** clear an extension. */
    public final <type> buildertype clearextension(
        final generatedextension<messagetype, ?> extension) {
      verifyextensioncontainingtype(extension);
      ensureextensionsismutable();
      extensions.clearfield(extension.descriptor);
      return (buildertype) this;
    }

    /** called by subclasses to check if all extensions are initialized. */
    protected boolean extensionsareinitialized() {
      return extensions.isinitialized();
    }

    /**
     * called by subclasses to parse an unknown field or an extension.
     * @return {@code true} unless the tag is an end-group tag.
     */
    @override
    protected boolean parseunknownfield(
        codedinputstream input,
        extensionregistrylite extensionregistry,
        int tag) throws ioexception {
      ensureextensionsismutable();
      return generatedmessagelite.parseunknownfield(
          extensions,
          getdefaultinstancefortype(),
          input,
          extensionregistry,
          tag);
    }

    protected final void mergeextensionfields(final messagetype other) {
      ensureextensionsismutable();
      extensions.mergefrom(((extendablemessage) other).extensions);
    }
  }

  // -----------------------------------------------------------------

  /**
   * parse an unknown field or an extension.
   * @return {@code true} unless the tag is an end-group tag.
   */
  private static <messagetype extends messagelite>
      boolean parseunknownfield(
          fieldset<extensiondescriptor> extensions,
          messagetype defaultinstance,
          codedinputstream input,
          extensionregistrylite extensionregistry,
          int tag) throws ioexception {
    int wiretype = wireformat.gettagwiretype(tag);
    int fieldnumber = wireformat.gettagfieldnumber(tag);

    generatedextension<messagetype, ?> extension =
      extensionregistry.findliteextensionbynumber(
          defaultinstance, fieldnumber);

    boolean unknown = false;
    boolean packed = false;
    if (extension == null) {
      unknown = true;  // unknown field.
    } else if (wiretype == fieldset.getwireformatforfieldtype(
                 extension.descriptor.getlitetype(),
                 false  /* ispacked */)) {
      packed = false;  // normal, unpacked value.
    } else if (extension.descriptor.isrepeated &&
               extension.descriptor.type.ispackable() &&
               wiretype == fieldset.getwireformatforfieldtype(
                 extension.descriptor.getlitetype(),
                 true  /* ispacked */)) {
      packed = true;  // packed value.
    } else {
      unknown = true;  // wrong wire type.
    }

    if (unknown) {  // unknown field or wrong wire type.  skip.
      return input.skipfield(tag);
    }

    if (packed) {
      int length = input.readrawvarint32();
      int limit = input.pushlimit(length);
      if (extension.descriptor.getlitetype() == wireformat.fieldtype.enum) {
        while (input.getbytesuntillimit() > 0) {
          int rawvalue = input.readenum();
          object value =
              extension.descriptor.getenumtype().findvaluebynumber(rawvalue);
          if (value == null) {
            // if the number isn't recognized as a valid value for this
            // enum, drop it (don't even add it to unknownfields).
            return true;
          }
          extensions.addrepeatedfield(extension.descriptor, value);
        }
      } else {
        while (input.getbytesuntillimit() > 0) {
          object value =
            fieldset.readprimitivefield(input,
                                        extension.descriptor.getlitetype());
          extensions.addrepeatedfield(extension.descriptor, value);
        }
      }
      input.poplimit(limit);
    } else {
      object value;
      switch (extension.descriptor.getlitejavatype()) {
        case message: {
          messagelite.builder subbuilder = null;
          if (!extension.descriptor.isrepeated()) {
            messagelite existingvalue =
                (messagelite) extensions.getfield(extension.descriptor);
            if (existingvalue != null) {
              subbuilder = existingvalue.tobuilder();
            }
          }
          if (subbuilder == null) {
            subbuilder = extension.messagedefaultinstance.newbuilderfortype();
          }
          if (extension.descriptor.getlitetype() ==
              wireformat.fieldtype.group) {
            input.readgroup(extension.getnumber(),
                            subbuilder, extensionregistry);
          } else {
            input.readmessage(subbuilder, extensionregistry);
          }
          value = subbuilder.build();
          break;
        }
        case enum:
          int rawvalue = input.readenum();
          value = extension.descriptor.getenumtype()
                           .findvaluebynumber(rawvalue);
          // if the number isn't recognized as a valid value for this enum,
          // drop it.
          if (value == null) {
            return true;
          }
          break;
        default:
          value = fieldset.readprimitivefield(input,
              extension.descriptor.getlitetype());
          break;
      }

      if (extension.descriptor.isrepeated()) {
        extensions.addrepeatedfield(extension.descriptor, value);
      } else {
        extensions.setfield(extension.descriptor, value);
      }
    }

    return true;
  }

  // -----------------------------------------------------------------

  /** for use by generated code only. */
  public static <containingtype extends messagelite, type>
      generatedextension<containingtype, type>
          newsingulargeneratedextension(
              final containingtype containingtypedefaultinstance,
              final type defaultvalue,
              final messagelite messagedefaultinstance,
              final internal.enumlitemap<?> enumtypemap,
              final int number,
              final wireformat.fieldtype type) {
    return new generatedextension<containingtype, type>(
        containingtypedefaultinstance,
        defaultvalue,
        messagedefaultinstance,
        new extensiondescriptor(enumtypemap, number, type,
                                false /* isrepeated */,
                                false /* ispacked */));
  }

  /** for use by generated code only. */
  public static <containingtype extends messagelite, type>
      generatedextension<containingtype, type>
          newrepeatedgeneratedextension(
              final containingtype containingtypedefaultinstance,
              final messagelite messagedefaultinstance,
              final internal.enumlitemap<?> enumtypemap,
              final int number,
              final wireformat.fieldtype type,
              final boolean ispacked) {
    @suppresswarnings("unchecked")  // subclasses ensure type is a list
    type emptylist = (type) collections.emptylist();
    return new generatedextension<containingtype, type>(
        containingtypedefaultinstance,
        emptylist,
        messagedefaultinstance,
        new extensiondescriptor(
            enumtypemap, number, type, true /* isrepeated */, ispacked));
  }

  private static final class extensiondescriptor
      implements fieldset.fielddescriptorlite<
        extensiondescriptor> {
    private extensiondescriptor(
        final internal.enumlitemap<?> enumtypemap,
        final int number,
        final wireformat.fieldtype type,
        final boolean isrepeated,
        final boolean ispacked) {
      this.enumtypemap = enumtypemap;
      this.number = number;
      this.type = type;
      this.isrepeated = isrepeated;
      this.ispacked = ispacked;
    }

    private final internal.enumlitemap<?> enumtypemap;
    private final int number;
    private final wireformat.fieldtype type;
    private final boolean isrepeated;
    private final boolean ispacked;

    public int getnumber() {
      return number;
    }

    public wireformat.fieldtype getlitetype() {
      return type;
    }

    public wireformat.javatype getlitejavatype() {
      return type.getjavatype();
    }

    public boolean isrepeated() {
      return isrepeated;
    }

    public boolean ispacked() {
      return ispacked;
    }

    public internal.enumlitemap<?> getenumtype() {
      return enumtypemap;
    }

    @suppresswarnings("unchecked")
    public messagelite.builder internalmergefrom(
        messagelite.builder to, messagelite from) {
      return ((builder) to).mergefrom((generatedmessagelite) from);
    }

    public int compareto(extensiondescriptor other) {
      return number - other.number;
    }
  }

  /**
   * lite equivalent to {@link generatedmessage.generatedextension}.
   *
   * users should ignore the contents of this class and only use objects of
   * this type as parameters to extension accessors and extensionregistry.add().
   */
  public static final class generatedextension<
      containingtype extends messagelite, type> {

    private generatedextension(
        final containingtype containingtypedefaultinstance,
        final type defaultvalue,
        final messagelite messagedefaultinstance,
        final extensiondescriptor descriptor) {
      // defensive checks to verify the correct initialization order of
      // generatedextensions and their related generatedmessages.
      if (containingtypedefaultinstance == null) {
        throw new illegalargumentexception(
            "null containingtypedefaultinstance");
      }
      if (descriptor.getlitetype() == wireformat.fieldtype.message &&
          messagedefaultinstance == null) {
        throw new illegalargumentexception(
            "null messagedefaultinstance");
      }
      this.containingtypedefaultinstance = containingtypedefaultinstance;
      this.defaultvalue = defaultvalue;
      this.messagedefaultinstance = messagedefaultinstance;
      this.descriptor = descriptor;
    }

    private final containingtype containingtypedefaultinstance;
    private final type defaultvalue;
    private final messagelite messagedefaultinstance;
    private final extensiondescriptor descriptor;

    /**
     * default instance of the type being extended, used to identify that type.
     */
    public containingtype getcontainingtypedefaultinstance() {
      return containingtypedefaultinstance;
    }

    /** get the field number. */
    public int getnumber() {
      return descriptor.getnumber();
    }

    /**
     * if the extension is an embedded message, this is the default instance of
     * that type.
     */
    public messagelite getmessagedefaultinstance() {
      return messagedefaultinstance;
    }
  }

  /**
   * a serialized (serializable) form of the generated message.  stores the
   * message as a class name and a byte array.
   */
  static final class serializedform implements serializable {
    private static final long serialversionuid = 0l;

    private string messageclassname;
    private byte[] asbytes;

    /**
     * creates the serialized form by calling {@link com.google.protobuf.messagelite#tobytearray}.
     * @param regularform the message to serialize
     */
    serializedform(messagelite regularform) {
      messageclassname = regularform.getclass().getname();
      asbytes = regularform.tobytearray();
    }

    /**
     * when read from an objectinputstream, this method converts this object
     * back to the regular form.  part of java's serialization magic.
     * @return a generatedmessage of the type that was serialized
     */
    @suppresswarnings("unchecked")
    protected object readresolve() throws objectstreamexception {
      try {
        class messageclass = class.forname(messageclassname);
        method newbuilder = messageclass.getmethod("newbuilder");
        messagelite.builder builder =
            (messagelite.builder) newbuilder.invoke(null);
        builder.mergefrom(asbytes);
        return builder.buildpartial();
      } catch (classnotfoundexception e) {
        throw new runtimeexception("unable to find proto buffer class", e);
      } catch (nosuchmethodexception e) {
        throw new runtimeexception("unable to find newbuilder method", e);
      } catch (illegalaccessexception e) {
        throw new runtimeexception("unable to call newbuilder method", e);
      } catch (invocationtargetexception e) {
        throw new runtimeexception("error calling newbuilder", e.getcause());
      } catch (invalidprotocolbufferexception e) {
        throw new runtimeexception("unable to understand proto buffer", e);
      }
    }
  }

  /**
   * replaces this object in the output stream with a serialized form.
   * part of java's serialization magic.  generated sub-classes must override
   * this method by calling {@code return super.writereplace();}
   * @return a serializedform of this message
   */
  protected object writereplace() throws objectstreamexception {
    return new serializedform(this);
  }
}
