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
import com.google.protobuf.descriptors.enumvaluedescriptor;
import com.google.protobuf.descriptors.fielddescriptor;

import java.io.ioexception;
import java.io.objectstreamexception;
import java.io.serializable;
import java.lang.reflect.invocationtargetexception;
import java.lang.reflect.method;
import java.util.arraylist;
import java.util.collections;
import java.util.iterator;
import java.util.list;
import java.util.map;
import java.util.treemap;

/**
 * all generated protocol message classes extend this class.  this class
 * implements most of the message and builder interfaces using java reflection.
 * users can ignore this class and pretend that generated messages implement
 * the message interface directly.
 *
 * @author kenton@google.com kenton varda
 */
public abstract class generatedmessage extends abstractmessage
    implements serializable {
  private static final long serialversionuid = 1l;

  /**
   * for testing. allows a test to disable the optimization that avoids using
   * field builders for nested messages until they are requested. by disabling
   * this optimization, existing tests can be reused to test the field builders.
   */
  protected static boolean alwaysusefieldbuilders = false;

  protected generatedmessage() {
  }

  protected generatedmessage(builder<?> builder) {
  }

  public parser<? extends message> getparserfortype() {
    throw new unsupportedoperationexception(
        "this is supposed to be overridden by subclasses.");
  }

 /**
  * for testing. allows a test to disable the optimization that avoids using
  * field builders for nested messages until they are requested. by disabling
  * this optimization, existing tests can be reused to test the field builders.
  * see {@link repeatedfieldbuilder} and {@link singlefieldbuilder}.
  */
  static void enablealwaysusefieldbuildersfortesting() {
    alwaysusefieldbuilders = true;
  }

  /**
   * get the fieldaccessortable for this type.  we can't have the message
   * class pass this in to the constructor because of bootstrapping trouble
   * with descriptorprotos.
   */
  protected abstract fieldaccessortable internalgetfieldaccessortable();

  //@override (java 1.6 override semantics, but we must support 1.5)
  public descriptor getdescriptorfortype() {
    return internalgetfieldaccessortable().descriptor;
  }

  /** internal helper which returns a mutable map. */
  private map<fielddescriptor, object> getallfieldsmutable() {
    final treemap<fielddescriptor, object> result =
      new treemap<fielddescriptor, object>();
    final descriptor descriptor = internalgetfieldaccessortable().descriptor;
    for (final fielddescriptor field : descriptor.getfields()) {
      if (field.isrepeated()) {
        final list<?> value = (list<?>) getfield(field);
        if (!value.isempty()) {
          result.put(field, value);
        }
      } else {
        if (hasfield(field)) {
          result.put(field, getfield(field));
        }
      }
    }
    return result;
  }

  @override
  public boolean isinitialized() {
    for (final fielddescriptor field : getdescriptorfortype().getfields()) {
      // check that all required fields are present.
      if (field.isrequired()) {
        if (!hasfield(field)) {
          return false;
        }
      }
      // check that embedded messages are initialized.
      if (field.getjavatype() == fielddescriptor.javatype.message) {
        if (field.isrepeated()) {
          @suppresswarnings("unchecked") final
          list<message> messagelist = (list<message>) getfield(field);
          for (final message element : messagelist) {
            if (!element.isinitialized()) {
              return false;
            }
          }
        } else {
          if (hasfield(field) && !((message) getfield(field)).isinitialized()) {
            return false;
          }
        }
      }
    }

    return true;
  }

  //@override (java 1.6 override semantics, but we must support 1.5)
  public map<fielddescriptor, object> getallfields() {
    return collections.unmodifiablemap(getallfieldsmutable());
  }

  //@override (java 1.6 override semantics, but we must support 1.5)
  public boolean hasfield(final fielddescriptor field) {
    return internalgetfieldaccessortable().getfield(field).has(this);
  }

  //@override (java 1.6 override semantics, but we must support 1.5)
  public object getfield(final fielddescriptor field) {
    return internalgetfieldaccessortable().getfield(field).get(this);
  }

  //@override (java 1.6 override semantics, but we must support 1.5)
  public int getrepeatedfieldcount(final fielddescriptor field) {
    return internalgetfieldaccessortable().getfield(field)
      .getrepeatedcount(this);
  }

  //@override (java 1.6 override semantics, but we must support 1.5)
  public object getrepeatedfield(final fielddescriptor field, final int index) {
    return internalgetfieldaccessortable().getfield(field)
      .getrepeated(this, index);
  }

  //@override (java 1.6 override semantics, but we must support 1.5)
  public unknownfieldset getunknownfields() {
    throw new unsupportedoperationexception(
        "this is supposed to be overridden by subclasses.");
  }

  /**
   * called by subclasses to parse an unknown field.
   * @return {@code true} unless the tag is an end-group tag.
   */
  protected boolean parseunknownfield(
      codedinputstream input,
      unknownfieldset.builder unknownfields,
      extensionregistrylite extensionregistry,
      int tag) throws ioexception {
    return unknownfields.mergefieldfrom(tag, input);
  }

  /**
   * used by parsing constructors in generated classes.
   */
  protected void makeextensionsimmutable() {
    // noop for messages without extensions.
  }

  protected abstract message.builder newbuilderfortype(builderparent parent);

  /**
   * interface for the parent of a builder that allows the builder to
   * communicate invalidations back to the parent for use when using nested
   * builders.
   */
  protected interface builderparent {

    /**
     * a builder becomes dirty whenever a field is modified -- including fields
     * in nested builders -- and becomes clean when build() is called.  thus,
     * when a builder becomes dirty, all its parents become dirty as well, and
     * when it becomes clean, all its children become clean.  the dirtiness
     * state is used to invalidate certain cached values.
     * <br>
     * to this end, a builder calls markasdirty() on its parent whenever it
     * transitions from clean to dirty.  the parent must propagate this call to
     * its own parent, unless it was already dirty, in which case the
     * grandparent must necessarily already be dirty as well.  the parent can
     * only transition back to "clean" after calling build() on all children.
     */
    void markdirty();
  }

  @suppresswarnings("unchecked")
  public abstract static class builder <buildertype extends builder>
      extends abstractmessage.builder<buildertype> {

    private builderparent builderparent;

    private builderparentimpl measparent;

    // indicates that we've built a message and so we are now obligated
    // to dispatch dirty invalidations. see generatedmessage.builderlistener.
    private boolean isclean;

    private unknownfieldset unknownfields =
        unknownfieldset.getdefaultinstance();

    protected builder() {
      this(null);
    }

    protected builder(builderparent builderparent) {
      this.builderparent = builderparent;
    }

    void dispose() {
      builderparent = null;
    }

    /**
     * called by the subclass when a message is built.
     */
    protected void onbuilt() {
      if (builderparent != null) {
        markclean();
      }
    }

    /**
     * called by the subclass or a builder to notify us that a message was
     * built and may be cached and therefore invalidations are needed.
     */
    protected void markclean() {
      this.isclean = true;
    }

    /**
     * gets whether invalidations are needed
     *
     * @return whether invalidations are needed
     */
    protected boolean isclean() {
      return isclean;
    }

    // this is implemented here only to work around an apparent bug in the
    // java compiler and/or build system.  see bug #1898463.  the mere presence
    // of this dummy clone() implementation makes it go away.
    @override
    public buildertype clone() {
      throw new unsupportedoperationexception(
          "this is supposed to be overridden by subclasses.");
    }

    /**
     * called by the initialization and clear code paths to allow subclasses to
     * reset any of their builtin fields back to the initial values.
     */
    public buildertype clear() {
      unknownfields = unknownfieldset.getdefaultinstance();
      onchanged();
      return (buildertype) this;
    }

    /**
     * get the fieldaccessortable for this type.  we can't have the message
     * class pass this in to the constructor because of bootstrapping trouble
     * with descriptorprotos.
     */
    protected abstract fieldaccessortable internalgetfieldaccessortable();

    //@override (java 1.6 override semantics, but we must support 1.5)
    public descriptor getdescriptorfortype() {
      return internalgetfieldaccessortable().descriptor;
    }

    //@override (java 1.6 override semantics, but we must support 1.5)
    public map<fielddescriptor, object> getallfields() {
      return collections.unmodifiablemap(getallfieldsmutable());
    }

    /** internal helper which returns a mutable map. */
    private map<fielddescriptor, object> getallfieldsmutable() {
      final treemap<fielddescriptor, object> result =
        new treemap<fielddescriptor, object>();
      final descriptor descriptor = internalgetfieldaccessortable().descriptor;
      for (final fielddescriptor field : descriptor.getfields()) {
        if (field.isrepeated()) {
          final list value = (list) getfield(field);
          if (!value.isempty()) {
            result.put(field, value);
          }
        } else {
          if (hasfield(field)) {
            result.put(field, getfield(field));
          }
        }
      }
      return result;
    }

    public message.builder newbuilderforfield(
        final fielddescriptor field) {
      return internalgetfieldaccessortable().getfield(field).newbuilder();
    }

    //@override (java 1.6 override semantics, but we must support 1.5)
    public message.builder getfieldbuilder(final fielddescriptor field) {
      return internalgetfieldaccessortable().getfield(field).getbuilder(this);
    }

    //@override (java 1.6 override semantics, but we must support 1.5)
    public boolean hasfield(final fielddescriptor field) {
      return internalgetfieldaccessortable().getfield(field).has(this);
    }

    //@override (java 1.6 override semantics, but we must support 1.5)
    public object getfield(final fielddescriptor field) {
      object object = internalgetfieldaccessortable().getfield(field).get(this);
      if (field.isrepeated()) {
        // the underlying list object is still modifiable at this point.
        // make sure not to expose the modifiable list to the caller.
        return collections.unmodifiablelist((list) object);
      } else {
        return object;
      }
    }

    public buildertype setfield(final fielddescriptor field,
                                final object value) {
      internalgetfieldaccessortable().getfield(field).set(this, value);
      return (buildertype) this;
    }

    //@override (java 1.6 override semantics, but we must support 1.5)
    public buildertype clearfield(final fielddescriptor field) {
      internalgetfieldaccessortable().getfield(field).clear(this);
      return (buildertype) this;
    }

    //@override (java 1.6 override semantics, but we must support 1.5)
    public int getrepeatedfieldcount(final fielddescriptor field) {
      return internalgetfieldaccessortable().getfield(field)
          .getrepeatedcount(this);
    }

    //@override (java 1.6 override semantics, but we must support 1.5)
    public object getrepeatedfield(final fielddescriptor field,
                                   final int index) {
      return internalgetfieldaccessortable().getfield(field)
          .getrepeated(this, index);
    }

    public buildertype setrepeatedfield(final fielddescriptor field,
                                        final int index, final object value) {
      internalgetfieldaccessortable().getfield(field)
        .setrepeated(this, index, value);
      return (buildertype) this;
    }

    public buildertype addrepeatedfield(final fielddescriptor field,
                                        final object value) {
      internalgetfieldaccessortable().getfield(field).addrepeated(this, value);
      return (buildertype) this;
    }

    public final buildertype setunknownfields(
        final unknownfieldset unknownfields) {
      this.unknownfields = unknownfields;
      onchanged();
      return (buildertype) this;
    }

    @override
    public final buildertype mergeunknownfields(
        final unknownfieldset unknownfields) {
      this.unknownfields =
        unknownfieldset.newbuilder(this.unknownfields)
                       .mergefrom(unknownfields)
                       .build();
      onchanged();
      return (buildertype) this;
    }

    //@override (java 1.6 override semantics, but we must support 1.5)
    public boolean isinitialized() {
      for (final fielddescriptor field : getdescriptorfortype().getfields()) {
        // check that all required fields are present.
        if (field.isrequired()) {
          if (!hasfield(field)) {
            return false;
          }
        }
        // check that embedded messages are initialized.
        if (field.getjavatype() == fielddescriptor.javatype.message) {
          if (field.isrepeated()) {
            @suppresswarnings("unchecked") final
            list<message> messagelist = (list<message>) getfield(field);
            for (final message element : messagelist) {
              if (!element.isinitialized()) {
                return false;
              }
            }
          } else {
            if (hasfield(field) &&
                !((message) getfield(field)).isinitialized()) {
              return false;
            }
          }
        }
      }
      return true;
    }

    //@override (java 1.6 override semantics, but we must support 1.5)
    public final unknownfieldset getunknownfields() {
      return unknownfields;
    }

    /**
     * called by subclasses to parse an unknown field.
     * @return {@code true} unless the tag is an end-group tag.
     */
    protected boolean parseunknownfield(
        final codedinputstream input,
        final unknownfieldset.builder unknownfields,
        final extensionregistrylite extensionregistry,
        final int tag) throws ioexception {
      return unknownfields.mergefieldfrom(tag, input);
    }

    /**
     * implementation of {@link builderparent} for giving to our children. this
     * small inner class makes it so we don't publicly expose the builderparent
     * methods.
     */
    private class builderparentimpl implements builderparent {

      //@override (java 1.6 override semantics, but we must support 1.5)
      public void markdirty() {
        onchanged();
      }
    }

    /**
     * gets the {@link builderparent} for giving to our children.
     * @return the builder parent for our children.
     */
    protected builderparent getparentforchildren() {
      if (measparent == null) {
        measparent = new builderparentimpl();
      }
      return measparent;
    }

    /**
     * called when a the builder or one of its nested children has changed
     * and any parent should be notified of its invalidation.
     */
    protected final void onchanged() {
      if (isclean && builderparent != null) {
        builderparent.markdirty();

        // don't keep dispatching invalidations until build is called again.
        isclean = false;
      }
    }
  }

  // =================================================================
  // extensions-related stuff

  public interface extendablemessageorbuilder<
      messagetype extends extendablemessage> extends messageorbuilder {

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
   * generated message classes for message types that contain extension ranges
   * subclass this.
   *
   * <p>this class implements type-safe accessors for extensions.  they
   * implement all the same operations that you can do with normal fields --
   * e.g. "has", "get", and "getcount" -- but for extensions.  the extensions
   * are identified using instances of the class {@link generatedextension};
   * the protocol compiler generates a static instance of this class for every
   * extension in its input.  through the magic of generics, all is made
   * type-safe.
   *
   * <p>for example, imagine you have the {@code .proto} file:
   *
   * <pre>
   * option java_class = "myproto";
   *
   * message foo {
   *   extensions 1000 to max;
   * }
   *
   * extend foo {
   *   optional int32 bar;
   * }
   * </pre>
   *
   * <p>then you might write code like:
   *
   * <pre>
   * myproto.foo foo = getfoo();
   * int i = foo.getextension(myproto.bar);
   * </pre>
   *
   * <p>see also {@link extendablebuilder}.
   */
  public abstract static class extendablemessage<
        messagetype extends extendablemessage>
      extends generatedmessage
      implements extendablemessageorbuilder<messagetype> {

    private final fieldset<fielddescriptor> extensions;

    protected extendablemessage() {
      this.extensions = fieldset.newfieldset();
    }

    protected extendablemessage(
        extendablebuilder<messagetype, ?> builder) {
      super(builder);
      this.extensions = builder.buildextensions();
    }

    private void verifyextensioncontainingtype(
        final generatedextension<messagetype, ?> extension) {
      if (extension.getdescriptor().getcontainingtype() !=
          getdescriptorfortype()) {
        // this can only happen if someone uses unchecked operations.
        throw new illegalargumentexception(
          "extension is for type \"" +
          extension.getdescriptor().getcontainingtype().getfullname() +
          "\" which does not match message type \"" +
          getdescriptorfortype().getfullname() + "\".");
      }
    }

    /** check if a singular extension is present. */
    //@override (java 1.6 override semantics, but we must support 1.5)
    public final <type> boolean hasextension(
        final generatedextension<messagetype, type> extension) {
      verifyextensioncontainingtype(extension);
      return extensions.hasfield(extension.getdescriptor());
    }

    /** get the number of elements in a repeated extension. */
    //@override (java 1.6 override semantics, but we must support 1.5)
    public final <type> int getextensioncount(
        final generatedextension<messagetype, list<type>> extension) {
      verifyextensioncontainingtype(extension);
      final fielddescriptor descriptor = extension.getdescriptor();
      return extensions.getrepeatedfieldcount(descriptor);
    }

    /** get the value of an extension. */
    //@override (java 1.6 override semantics, but we must support 1.5)
    @suppresswarnings("unchecked")
    public final <type> type getextension(
        final generatedextension<messagetype, type> extension) {
      verifyextensioncontainingtype(extension);
      fielddescriptor descriptor = extension.getdescriptor();
      final object value = extensions.getfield(descriptor);
      if (value == null) {
        if (descriptor.isrepeated()) {
          return (type) collections.emptylist();
        } else if (descriptor.getjavatype() ==
                   fielddescriptor.javatype.message) {
          return (type) extension.getmessagedefaultinstance();
        } else {
          return (type) extension.fromreflectiontype(
              descriptor.getdefaultvalue());
        }
      } else {
        return (type) extension.fromreflectiontype(value);
      }
    }

    /** get one element of a repeated extension. */
    //@override (java 1.6 override semantics, but we must support 1.5)
    @suppresswarnings("unchecked")
    public final <type> type getextension(
        final generatedextension<messagetype, list<type>> extension,
        final int index) {
      verifyextensioncontainingtype(extension);
      fielddescriptor descriptor = extension.getdescriptor();
      return (type) extension.singularfromreflectiontype(
          extensions.getrepeatedfield(descriptor, index));
    }

    /** called by subclasses to check if all extensions are initialized. */
    protected boolean extensionsareinitialized() {
      return extensions.isinitialized();
    }

    @override
    public boolean isinitialized() {
      return super.isinitialized() && extensionsareinitialized();
    }

    @override
    protected boolean parseunknownfield(
        codedinputstream input,
        unknownfieldset.builder unknownfields,
        extensionregistrylite extensionregistry,
        int tag) throws ioexception {
      return abstractmessage.builder.mergefieldfrom(
        input, unknownfields, extensionregistry, getdescriptorfortype(),
        null, extensions, tag);
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

      private final iterator<map.entry<fielddescriptor, object>> iter =
        extensions.iterator();
      private map.entry<fielddescriptor, object> next;
      private final boolean messagesetwireformat;

      private extensionwriter(final boolean messagesetwireformat) {
        if (iter.hasnext()) {
          next = iter.next();
        }
        this.messagesetwireformat = messagesetwireformat;
      }

      public void writeuntil(final int end, final codedoutputstream output)
                             throws ioexception {
        while (next != null && next.getkey().getnumber() < end) {
          fielddescriptor descriptor = next.getkey();
          if (messagesetwireformat && descriptor.getlitejavatype() ==
                  wireformat.javatype.message &&
              !descriptor.isrepeated()) {
            if (next instanceof lazyfield.lazyentry<?>) {
              output.writerawmessagesetextension(descriptor.getnumber(),
                  ((lazyfield.lazyentry<?>) next).getfield().tobytestring());
            } else {
              output.writemessagesetextension(descriptor.getnumber(),
                                              (message) next.getvalue());
            }
          } else {
            // todo(xiangl): taken care of following code, it may cause
            // problem when we use lazyfield for normal fields/extensions.
            // due to the optional field can be duplicated at the end of
            // serialized bytes, which will make the serialized size change
            // after lazy field parsed. so when we use lazyfield globally,
            // we need to change the following write method to write cached
            // bytes directly rather than write the parsed message.
            fieldset.writefield(descriptor, next.getvalue(), output);
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

    // ---------------------------------------------------------------
    // reflection

    protected map<fielddescriptor, object> getextensionfields() {
      return extensions.getallfields();
    }

    @override
    public map<fielddescriptor, object> getallfields() {
      final map<fielddescriptor, object> result = super.getallfieldsmutable();
      result.putall(getextensionfields());
      return collections.unmodifiablemap(result);
    }

    @override
    public boolean hasfield(final fielddescriptor field) {
      if (field.isextension()) {
        verifycontainingtype(field);
        return extensions.hasfield(field);
      } else {
        return super.hasfield(field);
      }
    }

    @override
    public object getfield(final fielddescriptor field) {
      if (field.isextension()) {
        verifycontainingtype(field);
        final object value = extensions.getfield(field);
        if (value == null) {
          if (field.getjavatype() == fielddescriptor.javatype.message) {
            // lacking an extensionregistry, we have no way to determine the
            // extension's real type, so we return a dynamicmessage.
            return dynamicmessage.getdefaultinstance(field.getmessagetype());
          } else {
            return field.getdefaultvalue();
          }
        } else {
          return value;
        }
      } else {
        return super.getfield(field);
      }
    }

    @override
    public int getrepeatedfieldcount(final fielddescriptor field) {
      if (field.isextension()) {
        verifycontainingtype(field);
        return extensions.getrepeatedfieldcount(field);
      } else {
        return super.getrepeatedfieldcount(field);
      }
    }

    @override
    public object getrepeatedfield(final fielddescriptor field,
                                   final int index) {
      if (field.isextension()) {
        verifycontainingtype(field);
        return extensions.getrepeatedfield(field, index);
      } else {
        return super.getrepeatedfield(field, index);
      }
    }

    private void verifycontainingtype(final fielddescriptor field) {
      if (field.getcontainingtype() != getdescriptorfortype()) {
        throw new illegalargumentexception(
          "fielddescriptor does not match message type.");
      }
    }
  }

  /**
   * generated message builders for message types that contain extension ranges
   * subclass this.
   *
   * <p>this class implements type-safe accessors for extensions.  they
   * implement all the same operations that you can do with normal fields --
   * e.g. "get", "set", and "add" -- but for extensions.  the extensions are
   * identified using instances of the class {@link generatedextension}; the
   * protocol compiler generates a static instance of this class for every
   * extension in its input.  through the magic of generics, all is made
   * type-safe.
   *
   * <p>for example, imagine you have the {@code .proto} file:
   *
   * <pre>
   * option java_class = "myproto";
   *
   * message foo {
   *   extensions 1000 to max;
   * }
   *
   * extend foo {
   *   optional int32 bar;
   * }
   * </pre>
   *
   * <p>then you might write code like:
   *
   * <pre>
   * myproto.foo foo =
   *   myproto.foo.newbuilder()
   *     .setextension(myproto.bar, 123)
   *     .build();
   * </pre>
   *
   * <p>see also {@link extendablemessage}.
   */
  @suppresswarnings("unchecked")
  public abstract static class extendablebuilder<
        messagetype extends extendablemessage,
        buildertype extends extendablebuilder>
      extends builder<buildertype>
      implements extendablemessageorbuilder<messagetype> {

    private fieldset<fielddescriptor> extensions = fieldset.emptyset();

    protected extendablebuilder() {}

    protected extendablebuilder(
        builderparent parent) {
      super(parent);
    }

    @override
    public buildertype clear() {
      extensions = fieldset.emptyset();
      return super.clear();
    }

    // this is implemented here only to work around an apparent bug in the
    // java compiler and/or build system.  see bug #1898463.  the mere presence
    // of this dummy clone() implementation makes it go away.
    @override
    public buildertype clone() {
      throw new unsupportedoperationexception(
          "this is supposed to be overridden by subclasses.");
    }

    private void ensureextensionsismutable() {
      if (extensions.isimmutable()) {
        extensions = extensions.clone();
      }
    }

    private void verifyextensioncontainingtype(
        final generatedextension<messagetype, ?> extension) {
      if (extension.getdescriptor().getcontainingtype() !=
          getdescriptorfortype()) {
        // this can only happen if someone uses unchecked operations.
        throw new illegalargumentexception(
          "extension is for type \"" +
          extension.getdescriptor().getcontainingtype().getfullname() +
          "\" which does not match message type \"" +
          getdescriptorfortype().getfullname() + "\".");
      }
    }

    /** check if a singular extension is present. */
    //@override (java 1.6 override semantics, but we must support 1.5)
    public final <type> boolean hasextension(
        final generatedextension<messagetype, type> extension) {
      verifyextensioncontainingtype(extension);
      return extensions.hasfield(extension.getdescriptor());
    }

    /** get the number of elements in a repeated extension. */
    //@override (java 1.6 override semantics, but we must support 1.5)
    public final <type> int getextensioncount(
        final generatedextension<messagetype, list<type>> extension) {
      verifyextensioncontainingtype(extension);
      final fielddescriptor descriptor = extension.getdescriptor();
      return extensions.getrepeatedfieldcount(descriptor);
    }

    /** get the value of an extension. */
    //@override (java 1.6 override semantics, but we must support 1.5)
    public final <type> type getextension(
        final generatedextension<messagetype, type> extension) {
      verifyextensioncontainingtype(extension);
      fielddescriptor descriptor = extension.getdescriptor();
      final object value = extensions.getfield(descriptor);
      if (value == null) {
        if (descriptor.isrepeated()) {
          return (type) collections.emptylist();
        } else if (descriptor.getjavatype() ==
                   fielddescriptor.javatype.message) {
          return (type) extension.getmessagedefaultinstance();
        } else {
          return (type) extension.fromreflectiontype(
              descriptor.getdefaultvalue());
        }
      } else {
        return (type) extension.fromreflectiontype(value);
      }
    }

    /** get one element of a repeated extension. */
    //@override (java 1.6 override semantics, but we must support 1.5)
    public final <type> type getextension(
        final generatedextension<messagetype, list<type>> extension,
        final int index) {
      verifyextensioncontainingtype(extension);
      fielddescriptor descriptor = extension.getdescriptor();
      return (type) extension.singularfromreflectiontype(
          extensions.getrepeatedfield(descriptor, index));
    }

    /** set the value of an extension. */
    public final <type> buildertype setextension(
        final generatedextension<messagetype, type> extension,
        final type value) {
      verifyextensioncontainingtype(extension);
      ensureextensionsismutable();
      final fielddescriptor descriptor = extension.getdescriptor();
      extensions.setfield(descriptor, extension.toreflectiontype(value));
      onchanged();
      return (buildertype) this;
    }

    /** set the value of one element of a repeated extension. */
    public final <type> buildertype setextension(
        final generatedextension<messagetype, list<type>> extension,
        final int index, final type value) {
      verifyextensioncontainingtype(extension);
      ensureextensionsismutable();
      final fielddescriptor descriptor = extension.getdescriptor();
      extensions.setrepeatedfield(
        descriptor, index,
        extension.singulartoreflectiontype(value));
      onchanged();
      return (buildertype) this;
    }

    /** append a value to a repeated extension. */
    public final <type> buildertype addextension(
        final generatedextension<messagetype, list<type>> extension,
        final type value) {
      verifyextensioncontainingtype(extension);
      ensureextensionsismutable();
      final fielddescriptor descriptor = extension.getdescriptor();
      extensions.addrepeatedfield(
          descriptor, extension.singulartoreflectiontype(value));
      onchanged();
      return (buildertype) this;
    }

    /** clear an extension. */
    public final <type> buildertype clearextension(
        final generatedextension<messagetype, ?> extension) {
      verifyextensioncontainingtype(extension);
      ensureextensionsismutable();
      extensions.clearfield(extension.getdescriptor());
      onchanged();
      return (buildertype) this;
    }

    /** called by subclasses to check if all extensions are initialized. */
    protected boolean extensionsareinitialized() {
      return extensions.isinitialized();
    }

    /**
     * called by the build code path to create a copy of the extensions for
     * building the message.
     */
    private fieldset<fielddescriptor> buildextensions() {
      extensions.makeimmutable();
      return extensions;
    }

    @override
    public boolean isinitialized() {
      return super.isinitialized() && extensionsareinitialized();
    }

    /**
     * called by subclasses to parse an unknown field or an extension.
     * @return {@code true} unless the tag is an end-group tag.
     */
    @override
    protected boolean parseunknownfield(
        final codedinputstream input,
        final unknownfieldset.builder unknownfields,
        final extensionregistrylite extensionregistry,
        final int tag) throws ioexception {
      return abstractmessage.builder.mergefieldfrom(
        input, unknownfields, extensionregistry, getdescriptorfortype(),
        this, null, tag);
    }

    // ---------------------------------------------------------------
    // reflection

    @override
    public map<fielddescriptor, object> getallfields() {
      final map<fielddescriptor, object> result = super.getallfieldsmutable();
      result.putall(extensions.getallfields());
      return collections.unmodifiablemap(result);
    }

    @override
    public object getfield(final fielddescriptor field) {
      if (field.isextension()) {
        verifycontainingtype(field);
        final object value = extensions.getfield(field);
        if (value == null) {
          if (field.getjavatype() == fielddescriptor.javatype.message) {
            // lacking an extensionregistry, we have no way to determine the
            // extension's real type, so we return a dynamicmessage.
            return dynamicmessage.getdefaultinstance(field.getmessagetype());
          } else {
            return field.getdefaultvalue();
          }
        } else {
          return value;
        }
      } else {
        return super.getfield(field);
      }
    }

    @override
    public int getrepeatedfieldcount(final fielddescriptor field) {
      if (field.isextension()) {
        verifycontainingtype(field);
        return extensions.getrepeatedfieldcount(field);
      } else {
        return super.getrepeatedfieldcount(field);
      }
    }

    @override
    public object getrepeatedfield(final fielddescriptor field,
                                   final int index) {
      if (field.isextension()) {
        verifycontainingtype(field);
        return extensions.getrepeatedfield(field, index);
      } else {
        return super.getrepeatedfield(field, index);
      }
    }

    @override
    public boolean hasfield(final fielddescriptor field) {
      if (field.isextension()) {
        verifycontainingtype(field);
        return extensions.hasfield(field);
      } else {
        return super.hasfield(field);
      }
    }

    @override
    public buildertype setfield(final fielddescriptor field,
                                final object value) {
      if (field.isextension()) {
        verifycontainingtype(field);
        ensureextensionsismutable();
        extensions.setfield(field, value);
        onchanged();
        return (buildertype) this;
      } else {
        return super.setfield(field, value);
      }
    }

    @override
    public buildertype clearfield(final fielddescriptor field) {
      if (field.isextension()) {
        verifycontainingtype(field);
        ensureextensionsismutable();
        extensions.clearfield(field);
        onchanged();
        return (buildertype) this;
      } else {
        return super.clearfield(field);
      }
    }

    @override
    public buildertype setrepeatedfield(final fielddescriptor field,
                                        final int index, final object value) {
      if (field.isextension()) {
        verifycontainingtype(field);
        ensureextensionsismutable();
        extensions.setrepeatedfield(field, index, value);
        onchanged();
        return (buildertype) this;
      } else {
        return super.setrepeatedfield(field, index, value);
      }
    }

    @override
    public buildertype addrepeatedfield(final fielddescriptor field,
                                        final object value) {
      if (field.isextension()) {
        verifycontainingtype(field);
        ensureextensionsismutable();
        extensions.addrepeatedfield(field, value);
        onchanged();
        return (buildertype) this;
      } else {
        return super.addrepeatedfield(field, value);
      }
    }

    protected final void mergeextensionfields(final extendablemessage other) {
      ensureextensionsismutable();
      extensions.mergefrom(other.extensions);
      onchanged();
    }

    private void verifycontainingtype(final fielddescriptor field) {
      if (field.getcontainingtype() != getdescriptorfortype()) {
        throw new illegalargumentexception(
          "fielddescriptor does not match message type.");
      }
    }
  }

  // -----------------------------------------------------------------

  /**
   * gets the descriptor for an extension. the implementation depends on whether
   * the extension is scoped in the top level of a file or scoped in a message.
   */
  private static interface extensiondescriptorretriever {
    fielddescriptor getdescriptor();
  }

  /** for use by generated code only. */
  public static <containingtype extends message, type>
      generatedextension<containingtype, type>
      newmessagescopedgeneratedextension(final message scope,
                                         final int descriptorindex,
                                         final class singulartype,
                                         final message defaultinstance) {
    // for extensions scoped within a message, we use the message to resolve
    // the outer class's descriptor, from which the extension descriptor is
    // obtained.
    return new generatedextension<containingtype, type>(
        new extensiondescriptorretriever() {
          //@override (java 1.6 override semantics, but we must support 1.5)
          public fielddescriptor getdescriptor() {
            return scope.getdescriptorfortype().getextensions()
                .get(descriptorindex);
          }
        },
        singulartype,
        defaultinstance);
  }

  /** for use by generated code only. */
  public static <containingtype extends message, type>
     generatedextension<containingtype, type>
     newfilescopedgeneratedextension(final class singulartype,
                                     final message defaultinstance) {
    // for extensions scoped within a file, we rely on the outer class's
    // static initializer to call internalinit() on the extension when the
    // descriptor is available.
    return new generatedextension<containingtype, type>(
        null,  // extensiondescriptorretriever is initialized in internalinit();
        singulartype,
        defaultinstance);
  }

  /**
   * type used to represent generated extensions.  the protocol compiler
   * generates a static singleton instance of this class for each extension.
   *
   * <p>for example, imagine you have the {@code .proto} file:
   *
   * <pre>
   * option java_class = "myproto";
   *
   * message foo {
   *   extensions 1000 to max;
   * }
   *
   * extend foo {
   *   optional int32 bar;
   * }
   * </pre>
   *
   * <p>then, {@code myproto.foo.bar} has type
   * {@code generatedextension<myproto.foo, integer>}.
   *
   * <p>in general, users should ignore the details of this type, and simply use
   * these static singletons as parameters to the extension accessors defined
   * in {@link extendablemessage} and {@link extendablebuilder}.
   */
  public static final class generatedextension<
      containingtype extends message, type> {
    // todo(kenton):  find ways to avoid using java reflection within this
    //   class.  also try to avoid suppressing unchecked warnings.

    // we can't always initialize the descriptor of a generatedextension when
    // we first construct it due to initialization order difficulties (namely,
    // the descriptor may not have been constructed yet, since it is often
    // constructed by the initializer of a separate module).
    //
    // in the case of nested extensions, we initialize the
    // extensiondescriptorretriever with an instance that uses the scoping
    // message's default instance to retrieve the extension's descriptor.
    //
    // in the case of non-nested extensions, we initialize the
    // extensiondescriptorretriever to null and rely on the outer class's static
    // initializer to call internalinit() after the descriptor has been parsed.
    private generatedextension(extensiondescriptorretriever descriptorretriever,
                               class singulartype,
                               message messagedefaultinstance) {
      if (message.class.isassignablefrom(singulartype) &&
          !singulartype.isinstance(messagedefaultinstance)) {
        throw new illegalargumentexception(
            "bad messagedefaultinstance for " + singulartype.getname());
      }
      this.descriptorretriever = descriptorretriever;
      this.singulartype = singulartype;
      this.messagedefaultinstance = messagedefaultinstance;

      if (protocolmessageenum.class.isassignablefrom(singulartype)) {
        this.enumvalueof = getmethodordie(singulartype, "valueof",
                                          enumvaluedescriptor.class);
        this.enumgetvaluedescriptor =
            getmethodordie(singulartype, "getvaluedescriptor");
      } else {
        this.enumvalueof = null;
        this.enumgetvaluedescriptor = null;
      }
    }

    /** for use by generated code only. */
    public void internalinit(final fielddescriptor descriptor) {
      if (descriptorretriever != null) {
        throw new illegalstateexception("already initialized.");
      }
      descriptorretriever = new extensiondescriptorretriever() {
          //@override (java 1.6 override semantics, but we must support 1.5)
          public fielddescriptor getdescriptor() {
            return descriptor;
          }
        };
    }

    private extensiondescriptorretriever descriptorretriever;
    private final class singulartype;
    private final message messagedefaultinstance;
    private final method enumvalueof;
    private final method enumgetvaluedescriptor;

    public fielddescriptor getdescriptor() {
      if (descriptorretriever == null) {
        throw new illegalstateexception(
            "getdescriptor() called before internalinit()");
      }
      return descriptorretriever.getdescriptor();
    }

    /**
     * if the extension is an embedded message or group, returns the default
     * instance of the message.
     */
    public message getmessagedefaultinstance() {
      return messagedefaultinstance;
    }

    /**
     * convert from the type used by the reflection accessors to the type used
     * by native accessors.  e.g., for enums, the reflection accessors use
     * enumvaluedescriptors but the native accessors use the generated enum
     * type.
     */
    @suppresswarnings("unchecked")
    private object fromreflectiontype(final object value) {
      fielddescriptor descriptor = getdescriptor();
      if (descriptor.isrepeated()) {
        if (descriptor.getjavatype() == fielddescriptor.javatype.message ||
            descriptor.getjavatype() == fielddescriptor.javatype.enum) {
          // must convert the whole list.
          final list result = new arraylist();
          for (final object element : (list) value) {
            result.add(singularfromreflectiontype(element));
          }
          return result;
        } else {
          return value;
        }
      } else {
        return singularfromreflectiontype(value);
      }
    }

    /**
     * like {@link #fromreflectiontype(object)}, but if the type is a repeated
     * type, this converts a single element.
     */
    private object singularfromreflectiontype(final object value) {
      fielddescriptor descriptor = getdescriptor();
      switch (descriptor.getjavatype()) {
        case message:
          if (singulartype.isinstance(value)) {
            return value;
          } else {
            // it seems the copy of the embedded message stored inside the
            // extended message is not of the exact type the user was
            // expecting.  this can happen if a user defines a
            // generatedextension manually and gives it a different type.
            // this should not happen in normal use.  but, to be nice, we'll
            // copy the message to whatever type the caller was expecting.
            return messagedefaultinstance.newbuilderfortype()
                           .mergefrom((message) value).build();
          }
        case enum:
          return invokeordie(enumvalueof, null, (enumvaluedescriptor) value);
        default:
          return value;
      }
    }

    /**
     * convert from the type used by the native accessors to the type used
     * by reflection accessors.  e.g., for enums, the reflection accessors use
     * enumvaluedescriptors but the native accessors use the generated enum
     * type.
     */
    @suppresswarnings("unchecked")
    private object toreflectiontype(final object value) {
      fielddescriptor descriptor = getdescriptor();
      if (descriptor.isrepeated()) {
        if (descriptor.getjavatype() == fielddescriptor.javatype.enum) {
          // must convert the whole list.
          final list result = new arraylist();
          for (final object element : (list) value) {
            result.add(singulartoreflectiontype(element));
          }
          return result;
        } else {
          return value;
        }
      } else {
        return singulartoreflectiontype(value);
      }
    }

    /**
     * like {@link #toreflectiontype(object)}, but if the type is a repeated
     * type, this converts a single element.
     */
    private object singulartoreflectiontype(final object value) {
      fielddescriptor descriptor = getdescriptor();
      switch (descriptor.getjavatype()) {
        case enum:
          return invokeordie(enumgetvaluedescriptor, value);
        default:
          return value;
      }
    }
  }

  // =================================================================

  /** calls class.getmethod and throws a runtimeexception if it fails. */
  @suppresswarnings("unchecked")
  private static method getmethodordie(
      final class clazz, final string name, final class... params) {
    try {
      return clazz.getmethod(name, params);
    } catch (nosuchmethodexception e) {
      throw new runtimeexception(
        "generated message class \"" + clazz.getname() +
        "\" missing method \"" + name + "\".", e);
    }
  }

  /** calls invoke and throws a runtimeexception if it fails. */
  private static object invokeordie(
      final method method, final object object, final object... params) {
    try {
      return method.invoke(object, params);
    } catch (illegalaccessexception e) {
      throw new runtimeexception(
        "couldn't use java reflection to implement protocol message " +
        "reflection.", e);
    } catch (invocationtargetexception e) {
      final throwable cause = e.getcause();
      if (cause instanceof runtimeexception) {
        throw (runtimeexception) cause;
      } else if (cause instanceof error) {
        throw (error) cause;
      } else {
        throw new runtimeexception(
          "unexpected exception thrown by generated accessor method.", cause);
      }
    }
  }

  /**
   * users should ignore this class.  this class provides the implementation
   * with access to the fields of a message object using java reflection.
   */
  public static final class fieldaccessortable {

    /**
     * construct a fieldaccessortable for a particular message class.  only
     * one fieldaccessortable should ever be constructed per class.
     *
     * @param descriptor     the type's descriptor.
     * @param camelcasenames the camelcase names of all fields in the message.
     *                       these are used to derive the accessor method names.
     * @param messageclass   the message type.
     * @param builderclass   the builder type.
     */
    public fieldaccessortable(
        final descriptor descriptor,
        final string[] camelcasenames,
        final class<? extends generatedmessage> messageclass,
        final class<? extends builder> builderclass) {
      this(descriptor, camelcasenames);
      ensurefieldaccessorsinitialized(messageclass, builderclass);
    }

    /**
     * construct a fieldaccessortable for a particular message class without
     * initializing fieldaccessors.
     */
    public fieldaccessortable(
        final descriptor descriptor,
        final string[] camelcasenames) {
      this.descriptor = descriptor;
      this.camelcasenames = camelcasenames;
      fields = new fieldaccessor[descriptor.getfields().size()];
      initialized = false;
    }

    /**
     * ensures the field accessors are initialized. this method is thread-safe.
     *
     * @param messageclass   the message type.
     * @param builderclass   the builder type.
     * @return this
     */
    public fieldaccessortable ensurefieldaccessorsinitialized(
        class<? extends generatedmessage> messageclass,
        class<? extends builder> builderclass) {
      if (initialized) { return this; }
      synchronized (this) {
        if (initialized) { return this; }
        for (int i = 0; i < fields.length; i++) {
          fielddescriptor field = descriptor.getfields().get(i);
          if (field.isrepeated()) {
            if (field.getjavatype() == fielddescriptor.javatype.message) {
              fields[i] = new repeatedmessagefieldaccessor(
                  field, camelcasenames[i], messageclass, builderclass);
            } else if (field.getjavatype() == fielddescriptor.javatype.enum) {
              fields[i] = new repeatedenumfieldaccessor(
                  field, camelcasenames[i], messageclass, builderclass);
            } else {
              fields[i] = new repeatedfieldaccessor(
                  field, camelcasenames[i], messageclass, builderclass);
            }
          } else {
            if (field.getjavatype() == fielddescriptor.javatype.message) {
              fields[i] = new singularmessagefieldaccessor(
                  field, camelcasenames[i], messageclass, builderclass);
            } else if (field.getjavatype() == fielddescriptor.javatype.enum) {
              fields[i] = new singularenumfieldaccessor(
                  field, camelcasenames[i], messageclass, builderclass);
            } else {
              fields[i] = new singularfieldaccessor(
                  field, camelcasenames[i], messageclass, builderclass);
            }
          }
        }
        initialized = true;
        camelcasenames = null;
        return this;
      }
    }

    private final descriptor descriptor;
    private final fieldaccessor[] fields;
    private string[] camelcasenames;
    private volatile boolean initialized;

    /** get the fieldaccessor for a particular field. */
    private fieldaccessor getfield(final fielddescriptor field) {
      if (field.getcontainingtype() != descriptor) {
        throw new illegalargumentexception(
          "fielddescriptor does not match message type.");
      } else if (field.isextension()) {
        // if this type had extensions, it would subclass extendablemessage,
        // which overrides the reflection interface to handle extensions.
        throw new illegalargumentexception(
          "this type does not have extensions.");
      }
      return fields[field.getindex()];
    }

    /**
     * abstract interface that provides access to a single field.  this is
     * implemented differently depending on the field type and cardinality.
     */
    private interface fieldaccessor {
      object get(generatedmessage message);
      object get(generatedmessage.builder builder);
      void set(builder builder, object value);
      object getrepeated(generatedmessage message, int index);
      object getrepeated(generatedmessage.builder builder, int index);
      void setrepeated(builder builder,
                       int index, object value);
      void addrepeated(builder builder, object value);
      boolean has(generatedmessage message);
      boolean has(generatedmessage.builder builder);
      int getrepeatedcount(generatedmessage message);
      int getrepeatedcount(generatedmessage.builder builder);
      void clear(builder builder);
      message.builder newbuilder();
      message.builder getbuilder(generatedmessage.builder builder);
    }

    // ---------------------------------------------------------------

    private static class singularfieldaccessor implements fieldaccessor {
      singularfieldaccessor(
          final fielddescriptor descriptor, final string camelcasename,
          final class<? extends generatedmessage> messageclass,
          final class<? extends builder> builderclass) {
        getmethod = getmethodordie(messageclass, "get" + camelcasename);
        getmethodbuilder = getmethodordie(builderclass, "get" + camelcasename);
        type = getmethod.getreturntype();
        setmethod = getmethodordie(builderclass, "set" + camelcasename, type);
        hasmethod =
            getmethodordie(messageclass, "has" + camelcasename);
        hasmethodbuilder =
            getmethodordie(builderclass, "has" + camelcasename);
        clearmethod = getmethodordie(builderclass, "clear" + camelcasename);
      }

      // note:  we use java reflection to call public methods rather than
      //   access private fields directly as this avoids runtime security
      //   checks.
      protected final class<?> type;
      protected final method getmethod;
      protected final method getmethodbuilder;
      protected final method setmethod;
      protected final method hasmethod;
      protected final method hasmethodbuilder;
      protected final method clearmethod;

      public object get(final generatedmessage message) {
        return invokeordie(getmethod, message);
      }
      public object get(generatedmessage.builder builder) {
        return invokeordie(getmethodbuilder, builder);
      }
      public void set(final builder builder, final object value) {
        invokeordie(setmethod, builder, value);
      }
      public object getrepeated(final generatedmessage message,
                                final int index) {
        throw new unsupportedoperationexception(
          "getrepeatedfield() called on a singular field.");
      }
      public object getrepeated(generatedmessage.builder builder, int index) {
        throw new unsupportedoperationexception(
          "getrepeatedfield() called on a singular field.");
      }
      public void setrepeated(final builder builder,
                              final int index, final object value) {
        throw new unsupportedoperationexception(
          "setrepeatedfield() called on a singular field.");
      }
      public void addrepeated(final builder builder, final object value) {
        throw new unsupportedoperationexception(
          "addrepeatedfield() called on a singular field.");
      }
      public boolean has(final generatedmessage message) {
        return (boolean) invokeordie(hasmethod, message);
      }
      public boolean has(generatedmessage.builder builder) {
        return (boolean) invokeordie(hasmethodbuilder, builder);
      }
      public int getrepeatedcount(final generatedmessage message) {
        throw new unsupportedoperationexception(
          "getrepeatedfieldsize() called on a singular field.");
      }
      public int getrepeatedcount(generatedmessage.builder builder) {
        throw new unsupportedoperationexception(
          "getrepeatedfieldsize() called on a singular field.");
      }
      public void clear(final builder builder) {
        invokeordie(clearmethod, builder);
      }
      public message.builder newbuilder() {
        throw new unsupportedoperationexception(
          "newbuilderforfield() called on a non-message type.");
      }
      public message.builder getbuilder(generatedmessage.builder builder) {
        throw new unsupportedoperationexception(
          "getfieldbuilder() called on a non-message type.");
      }
    }

    private static class repeatedfieldaccessor implements fieldaccessor {
      protected final class type;
      protected final method getmethod;
      protected final method getmethodbuilder;
      protected final method getrepeatedmethod;
      protected final method getrepeatedmethodbuilder;
      protected final method setrepeatedmethod;
      protected final method addrepeatedmethod;
      protected final method getcountmethod;
      protected final method getcountmethodbuilder;
      protected final method clearmethod;

      repeatedfieldaccessor(
          final fielddescriptor descriptor, final string camelcasename,
          final class<? extends generatedmessage> messageclass,
          final class<? extends builder> builderclass) {
        getmethod = getmethodordie(messageclass,
                                   "get" + camelcasename + "list");
        getmethodbuilder = getmethodordie(builderclass,
                                   "get" + camelcasename + "list");
        getrepeatedmethod =
            getmethodordie(messageclass, "get" + camelcasename, integer.type);
        getrepeatedmethodbuilder =
            getmethodordie(builderclass, "get" + camelcasename, integer.type);
        type = getrepeatedmethod.getreturntype();
        setrepeatedmethod =
            getmethodordie(builderclass, "set" + camelcasename,
                           integer.type, type);
        addrepeatedmethod =
            getmethodordie(builderclass, "add" + camelcasename, type);
        getcountmethod =
            getmethodordie(messageclass, "get" + camelcasename + "count");
        getcountmethodbuilder =
            getmethodordie(builderclass, "get" + camelcasename + "count");

        clearmethod = getmethodordie(builderclass, "clear" + camelcasename);
      }

      public object get(final generatedmessage message) {
        return invokeordie(getmethod, message);
      }
      public object get(generatedmessage.builder builder) {
        return invokeordie(getmethodbuilder, builder);
      }
      public void set(final builder builder, final object value) {
        // add all the elements individually.  this serves two purposes:
        // 1) verifies that each element has the correct type.
        // 2) insures that the caller cannot modify the list later on and
        //    have the modifications be reflected in the message.
        clear(builder);
        for (final object element : (list<?>) value) {
          addrepeated(builder, element);
        }
      }
      public object getrepeated(final generatedmessage message,
                                final int index) {
        return invokeordie(getrepeatedmethod, message, index);
      }
      public object getrepeated(generatedmessage.builder builder, int index) {
        return invokeordie(getrepeatedmethodbuilder, builder, index);
      }
      public void setrepeated(final builder builder,
                              final int index, final object value) {
        invokeordie(setrepeatedmethod, builder, index, value);
      }
      public void addrepeated(final builder builder, final object value) {
        invokeordie(addrepeatedmethod, builder, value);
      }
      public boolean has(final generatedmessage message) {
        throw new unsupportedoperationexception(
          "hasfield() called on a repeated field.");
      }
      public boolean has(generatedmessage.builder builder) {
        throw new unsupportedoperationexception(
          "hasfield() called on a repeated field.");
      }
      public int getrepeatedcount(final generatedmessage message) {
        return (integer) invokeordie(getcountmethod, message);
      }
      public int getrepeatedcount(generatedmessage.builder builder) {
        return (integer) invokeordie(getcountmethodbuilder, builder);
      }
      public void clear(final builder builder) {
        invokeordie(clearmethod, builder);
      }
      public message.builder newbuilder() {
        throw new unsupportedoperationexception(
          "newbuilderforfield() called on a non-message type.");
      }
      public message.builder getbuilder(generatedmessage.builder builder) {
        throw new unsupportedoperationexception(
          "getfieldbuilder() called on a non-message type.");
      }
    }

    // ---------------------------------------------------------------

    private static final class singularenumfieldaccessor
        extends singularfieldaccessor {
      singularenumfieldaccessor(
          final fielddescriptor descriptor, final string camelcasename,
          final class<? extends generatedmessage> messageclass,
          final class<? extends builder> builderclass) {
        super(descriptor, camelcasename, messageclass, builderclass);

        valueofmethod = getmethodordie(type, "valueof",
                                       enumvaluedescriptor.class);
        getvaluedescriptormethod =
          getmethodordie(type, "getvaluedescriptor");
      }

      private method valueofmethod;
      private method getvaluedescriptormethod;

      @override
      public object get(final generatedmessage message) {
        return invokeordie(getvaluedescriptormethod, super.get(message));
      }

      @override
      public object get(final generatedmessage.builder builder) {
        return invokeordie(getvaluedescriptormethod, super.get(builder));
      }

      @override
      public void set(final builder builder, final object value) {
        super.set(builder, invokeordie(valueofmethod, null, value));
      }
    }

    private static final class repeatedenumfieldaccessor
        extends repeatedfieldaccessor {
      repeatedenumfieldaccessor(
          final fielddescriptor descriptor, final string camelcasename,
          final class<? extends generatedmessage> messageclass,
          final class<? extends builder> builderclass) {
        super(descriptor, camelcasename, messageclass, builderclass);

        valueofmethod = getmethodordie(type, "valueof",
                                       enumvaluedescriptor.class);
        getvaluedescriptormethod =
          getmethodordie(type, "getvaluedescriptor");
      }

      private final method valueofmethod;
      private final method getvaluedescriptormethod;

      @override
      @suppresswarnings("unchecked")
      public object get(final generatedmessage message) {
        final list newlist = new arraylist();
        for (final object element : (list) super.get(message)) {
          newlist.add(invokeordie(getvaluedescriptormethod, element));
        }
        return collections.unmodifiablelist(newlist);
      }

      @override
      @suppresswarnings("unchecked")
      public object get(final generatedmessage.builder builder) {
        final list newlist = new arraylist();
        for (final object element : (list) super.get(builder)) {
          newlist.add(invokeordie(getvaluedescriptormethod, element));
        }
        return collections.unmodifiablelist(newlist);
      }

      @override
      public object getrepeated(final generatedmessage message,
                                final int index) {
        return invokeordie(getvaluedescriptormethod,
          super.getrepeated(message, index));
      }
      @override
      public object getrepeated(final generatedmessage.builder builder,
                                final int index) {
        return invokeordie(getvaluedescriptormethod,
          super.getrepeated(builder, index));
      }
      @override
      public void setrepeated(final builder builder,
                              final int index, final object value) {
        super.setrepeated(builder, index, invokeordie(valueofmethod, null,
                          value));
      }
      @override
      public void addrepeated(final builder builder, final object value) {
        super.addrepeated(builder, invokeordie(valueofmethod, null, value));
      }
    }

    // ---------------------------------------------------------------

    private static final class singularmessagefieldaccessor
        extends singularfieldaccessor {
      singularmessagefieldaccessor(
          final fielddescriptor descriptor, final string camelcasename,
          final class<? extends generatedmessage> messageclass,
          final class<? extends builder> builderclass) {
        super(descriptor, camelcasename, messageclass, builderclass);

        newbuildermethod = getmethodordie(type, "newbuilder");
        getbuildermethodbuilder =
            getmethodordie(builderclass, "get" + camelcasename + "builder");
      }

      private final method newbuildermethod;
      private final method getbuildermethodbuilder;

      private object coercetype(final object value) {
        if (type.isinstance(value)) {
          return value;
        } else {
          // the value is not the exact right message type.  however, if it
          // is an alternative implementation of the same type -- e.g. a
          // dynamicmessage -- we should accept it.  in this case we can make
          // a copy of the message.
          return ((message.builder) invokeordie(newbuildermethod, null))
                  .mergefrom((message) value).buildpartial();
        }
      }

      @override
      public void set(final builder builder, final object value) {
        super.set(builder, coercetype(value));
      }
      @override
      public message.builder newbuilder() {
        return (message.builder) invokeordie(newbuildermethod, null);
      }
      @override
      public message.builder getbuilder(generatedmessage.builder builder) {
        return (message.builder) invokeordie(getbuildermethodbuilder, builder);
      }
    }

    private static final class repeatedmessagefieldaccessor
        extends repeatedfieldaccessor {
      repeatedmessagefieldaccessor(
          final fielddescriptor descriptor, final string camelcasename,
          final class<? extends generatedmessage> messageclass,
          final class<? extends builder> builderclass) {
        super(descriptor, camelcasename, messageclass, builderclass);

        newbuildermethod = getmethodordie(type, "newbuilder");
      }

      private final method newbuildermethod;

      private object coercetype(final object value) {
        if (type.isinstance(value)) {
          return value;
        } else {
          // the value is not the exact right message type.  however, if it
          // is an alternative implementation of the same type -- e.g. a
          // dynamicmessage -- we should accept it.  in this case we can make
          // a copy of the message.
          return ((message.builder) invokeordie(newbuildermethod, null))
                  .mergefrom((message) value).build();
        }
      }

      @override
      public void setrepeated(final builder builder,
                              final int index, final object value) {
        super.setrepeated(builder, index, coercetype(value));
      }
      @override
      public void addrepeated(final builder builder, final object value) {
        super.addrepeated(builder, coercetype(value));
      }
      @override
      public message.builder newbuilder() {
        return (message.builder) invokeordie(newbuildermethod, null);
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
    return new generatedmessagelite.serializedform(this);
  }
}
