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

import com.google.protobuf.descriptorprotos.*;

import java.util.arrays;
import java.util.collections;
import java.util.hashmap;
import java.util.hashset;
import java.util.list;
import java.util.map;
import java.util.set;
import java.io.unsupportedencodingexception;

/**
 * contains a collection of classes which describe protocol message types.
 *
 * every message type has a {@link descriptor}, which lists all
 * its fields and other information about a type.  you can get a message
 * type's descriptor by calling {@code messagetype.getdescriptor()}, or
 * (given a message object of the type) {@code message.getdescriptorfortype()}.
 * furthermore, each message is associated with a {@link filedescriptor} for
 * a relevant {@code .proto} file. you can obtain it by calling
 * {@code descriptor.getfile()}. a {@link filedescriptor} contains descriptors
 * for all the messages defined in that file, and file descriptors for all the
 * imported {@code .proto} files.
 *
 * descriptors are built from descriptorprotos, as defined in
 * {@code google/protobuf/descriptor.proto}.
 *
 * @author kenton@google.com kenton varda
 */
public final class descriptors {
  /**
   * describes a {@code .proto} file, including everything defined within.
   * that includes, in particular, descriptors for all the messages and
   * file descriptors for all other imported {@code .proto} files
   * (dependencies).
   */
  public static final class filedescriptor {
    /** convert the descriptor to its protocol message representation. */
    public filedescriptorproto toproto() { return proto; }

    /** get the file name. */
    public string getname() { return proto.getname(); }

    /**
     * get the proto package name.  this is the package name given by the
     * {@code package} statement in the {@code .proto} file, which differs
     * from the java package.
     */
    public string getpackage() { return proto.getpackage(); }

    /** get the {@code fileoptions}, defined in {@code descriptor.proto}. */
    public fileoptions getoptions() { return proto.getoptions(); }

    /** get a list of top-level message types declared in this file. */
    public list<descriptor> getmessagetypes() {
      return collections.unmodifiablelist(arrays.aslist(messagetypes));
    }

    /** get a list of top-level enum types declared in this file. */
    public list<enumdescriptor> getenumtypes() {
      return collections.unmodifiablelist(arrays.aslist(enumtypes));
    }

    /** get a list of top-level services declared in this file. */
    public list<servicedescriptor> getservices() {
      return collections.unmodifiablelist(arrays.aslist(services));
    }

    /** get a list of top-level extensions declared in this file. */
    public list<fielddescriptor> getextensions() {
      return collections.unmodifiablelist(arrays.aslist(extensions));
    }

    /** get a list of this file's dependencies (imports). */
    public list<filedescriptor> getdependencies() {
      return collections.unmodifiablelist(arrays.aslist(dependencies));
    }

    /** get a list of this file's public dependencies (public imports). */
    public list<filedescriptor> getpublicdependencies() {
      return collections.unmodifiablelist(arrays.aslist(publicdependencies));
    }

    /**
     * find a message type in the file by name.  does not find nested types.
     *
     * @param name the unqualified type name to look for.
     * @return the message type's descriptor, or {@code null} if not found.
     */
    public descriptor findmessagetypebyname(string name) {
      // don't allow looking up nested types.  this will make optimization
      // easier later.
      if (name.indexof('.') != -1) {
        return null;
      }
      if (getpackage().length() > 0) {
        name = getpackage() + '.' + name;
      }
      final genericdescriptor result = pool.findsymbol(name);
      if (result != null && result instanceof descriptor &&
          result.getfile() == this) {
        return (descriptor)result;
      } else {
        return null;
      }
    }

    /**
     * find an enum type in the file by name.  does not find nested types.
     *
     * @param name the unqualified type name to look for.
     * @return the enum type's descriptor, or {@code null} if not found.
     */
    public enumdescriptor findenumtypebyname(string name) {
      // don't allow looking up nested types.  this will make optimization
      // easier later.
      if (name.indexof('.') != -1) {
        return null;
      }
      if (getpackage().length() > 0) {
        name = getpackage() + '.' + name;
      }
      final genericdescriptor result = pool.findsymbol(name);
      if (result != null && result instanceof enumdescriptor &&
          result.getfile() == this) {
        return (enumdescriptor)result;
      } else {
        return null;
      }
    }

    /**
     * find a service type in the file by name.
     *
     * @param name the unqualified type name to look for.
     * @return the service type's descriptor, or {@code null} if not found.
     */
    public servicedescriptor findservicebyname(string name) {
      // don't allow looking up nested types.  this will make optimization
      // easier later.
      if (name.indexof('.') != -1) {
        return null;
      }
      if (getpackage().length() > 0) {
        name = getpackage() + '.' + name;
      }
      final genericdescriptor result = pool.findsymbol(name);
      if (result != null && result instanceof servicedescriptor &&
          result.getfile() == this) {
        return (servicedescriptor)result;
      } else {
        return null;
      }
    }

    /**
     * find an extension in the file by name.  does not find extensions nested
     * inside message types.
     *
     * @param name the unqualified extension name to look for.
     * @return the extension's descriptor, or {@code null} if not found.
     */
    public fielddescriptor findextensionbyname(string name) {
      if (name.indexof('.') != -1) {
        return null;
      }
      if (getpackage().length() > 0) {
        name = getpackage() + '.' + name;
      }
      final genericdescriptor result = pool.findsymbol(name);
      if (result != null && result instanceof fielddescriptor &&
          result.getfile() == this) {
        return (fielddescriptor)result;
      } else {
        return null;
      }
    }

    /**
     * construct a {@code filedescriptor}.
     *
     * @param proto the protocol message form of the filedescriptor.
     * @param dependencies {@code filedescriptor}s corresponding to all of
     *                     the file's dependencies, in the exact order listed
     *                     in {@code proto}.
     * @throws descriptorvalidationexception {@code proto} is not a valid
     *           descriptor.  this can occur for a number of reasons, e.g.
     *           because a field has an undefined type or because two messages
     *           were defined with the same name.
     */
    public static filedescriptor buildfrom(final filedescriptorproto proto,
                                           final filedescriptor[] dependencies)
                                    throws descriptorvalidationexception {
      // building descriptors involves two steps:  translating and linking.
      // in the translation step (implemented by filedescriptor's
      // constructor), we build an object tree mirroring the
      // filedescriptorproto's tree and put all of the descriptors into the
      // descriptorpool's lookup tables.  in the linking step, we look up all
      // type references in the descriptorpool, so that, for example, a
      // fielddescriptor for an embedded message contains a pointer directly
      // to the descriptor for that message's type.  we also detect undefined
      // types in the linking step.
      final descriptorpool pool = new descriptorpool(dependencies);
      final filedescriptor result =
          new filedescriptor(proto, dependencies, pool);

      if (dependencies.length != proto.getdependencycount()) {
        throw new descriptorvalidationexception(result,
          "dependencies passed to filedescriptor.buildfrom() don't match " +
          "those listed in the filedescriptorproto.");
      }
      for (int i = 0; i < proto.getdependencycount(); i++) {
        if (!dependencies[i].getname().equals(proto.getdependency(i))) {
          throw new descriptorvalidationexception(result,
            "dependencies passed to filedescriptor.buildfrom() don't match " +
            "those listed in the filedescriptorproto.");
        }
      }

      result.crosslink();
      return result;
    }

    /**
     * this method is to be called by generated code only.  it is equivalent
     * to {@code buildfrom} except that the {@code filedescriptorproto} is
     * encoded in protocol buffer wire format.
     */
    public static void internalbuildgeneratedfilefrom(
        final string[] descriptordataparts,
        final filedescriptor[] dependencies,
        final internaldescriptorassigner descriptorassigner) {
      // hack:  we can't embed a raw byte array inside generated java code
      //   (at least, not efficiently), but we can embed strings.  so, the
      //   protocol compiler embeds the filedescriptorproto as a giant
      //   string literal which is passed to this function to construct the
      //   file's filedescriptor.  the string literal contains only 8-bit
      //   characters, each one representing a byte of the filedescriptorproto's
      //   serialized form.  so, if we convert it to bytes in iso-8859-1, we
      //   should get the original bytes that we want.

      // descriptordata may contain multiple strings in order to get around the
      // java 64k string literal limit.
      stringbuilder descriptordata = new stringbuilder();
      for (string part : descriptordataparts) {
        descriptordata.append(part);
      }

      final byte[] descriptorbytes;
      try {
        descriptorbytes = descriptordata.tostring().getbytes("iso-8859-1");
      } catch (unsupportedencodingexception e) {
        throw new runtimeexception(
          "standard encoding iso-8859-1 not supported by jvm.", e);
      }

      filedescriptorproto proto;
      try {
        proto = filedescriptorproto.parsefrom(descriptorbytes);
      } catch (invalidprotocolbufferexception e) {
        throw new illegalargumentexception(
          "failed to parse protocol buffer descriptor for generated code.", e);
      }

      final filedescriptor result;
      try {
        result = buildfrom(proto, dependencies);
      } catch (descriptorvalidationexception e) {
        throw new illegalargumentexception(
          "invalid embedded descriptor for \"" + proto.getname() + "\".", e);
      }

      final extensionregistry registry =
          descriptorassigner.assigndescriptors(result);

      if (registry != null) {
        // we must re-parse the proto using the registry.
        try {
          proto = filedescriptorproto.parsefrom(descriptorbytes, registry);
        } catch (invalidprotocolbufferexception e) {
          throw new illegalargumentexception(
            "failed to parse protocol buffer descriptor for generated code.",
            e);
        }

        result.setproto(proto);
      }
    }

    /**
     * this class should be used by generated code only.  when calling
     * {@link filedescriptor#internalbuildgeneratedfilefrom}, the caller
     * provides a callback implementing this interface.  the callback is called
     * after the filedescriptor has been constructed, in order to assign all
     * the global variables defined in the generated code which point at parts
     * of the filedescriptor.  the callback returns an extensionregistry which
     * contains any extensions which might be used in the descriptor -- that
     * is, extensions of the various "options" messages defined in
     * descriptor.proto.  the callback may also return null to indicate that
     * no extensions are used in the descriptor.
     */
    public interface internaldescriptorassigner {
      extensionregistry assigndescriptors(filedescriptor root);
    }

    private filedescriptorproto proto;
    private final descriptor[] messagetypes;
    private final enumdescriptor[] enumtypes;
    private final servicedescriptor[] services;
    private final fielddescriptor[] extensions;
    private final filedescriptor[] dependencies;
    private final filedescriptor[] publicdependencies;
    private final descriptorpool pool;

    private filedescriptor(final filedescriptorproto proto,
                           final filedescriptor[] dependencies,
                           final descriptorpool pool)
                    throws descriptorvalidationexception {
      this.pool = pool;
      this.proto = proto;
      this.dependencies = dependencies.clone();
      this.publicdependencies =
          new filedescriptor[proto.getpublicdependencycount()];
      for (int i = 0; i < proto.getpublicdependencycount(); i++) {
        int index = proto.getpublicdependency(i);
        if (index < 0 || index >= this.dependencies.length) {
          throw new descriptorvalidationexception(this,
              "invalid public dependency index.");
        }
        this.publicdependencies[i] =
            this.dependencies[proto.getpublicdependency(i)];
      }

      pool.addpackage(getpackage(), this);

      messagetypes = new descriptor[proto.getmessagetypecount()];
      for (int i = 0; i < proto.getmessagetypecount(); i++) {
        messagetypes[i] =
          new descriptor(proto.getmessagetype(i), this, null, i);
      }

      enumtypes = new enumdescriptor[proto.getenumtypecount()];
      for (int i = 0; i < proto.getenumtypecount(); i++) {
        enumtypes[i] = new enumdescriptor(proto.getenumtype(i), this, null, i);
      }

      services = new servicedescriptor[proto.getservicecount()];
      for (int i = 0; i < proto.getservicecount(); i++) {
        services[i] = new servicedescriptor(proto.getservice(i), this, i);
      }

      extensions = new fielddescriptor[proto.getextensioncount()];
      for (int i = 0; i < proto.getextensioncount(); i++) {
        extensions[i] = new fielddescriptor(
          proto.getextension(i), this, null, i, true);
      }
    }

    /** look up and cross-link all field types, etc. */
    private void crosslink() throws descriptorvalidationexception {
      for (final descriptor messagetype : messagetypes) {
        messagetype.crosslink();
      }

      for (final servicedescriptor service : services) {
        service.crosslink();
      }

      for (final fielddescriptor extension : extensions) {
        extension.crosslink();
      }
    }

    /**
     * replace our {@link filedescriptorproto} with the given one, which is
     * identical except that it might contain extensions that weren't present
     * in the original.  this method is needed for bootstrapping when a file
     * defines custom options.  the options may be defined in the file itself,
     * so we can't actually parse them until we've constructed the descriptors,
     * but to construct the descriptors we have to have parsed the descriptor
     * protos.  so, we have to parse the descriptor protos a second time after
     * constructing the descriptors.
     */
    private void setproto(final filedescriptorproto proto) {
      this.proto = proto;

      for (int i = 0; i < messagetypes.length; i++) {
        messagetypes[i].setproto(proto.getmessagetype(i));
      }

      for (int i = 0; i < enumtypes.length; i++) {
        enumtypes[i].setproto(proto.getenumtype(i));
      }

      for (int i = 0; i < services.length; i++) {
        services[i].setproto(proto.getservice(i));
      }

      for (int i = 0; i < extensions.length; i++) {
        extensions[i].setproto(proto.getextension(i));
      }
    }
  }

  // =================================================================

  /** describes a message type. */
  public static final class descriptor implements genericdescriptor {
    /**
     * get the index of this descriptor within its parent.  in other words,
     * given a {@link filedescriptor} {@code file}, the following is true:
     * <pre>
     *   for all i in [0, file.getmessagetypecount()):
     *     file.getmessagetype(i).getindex() == i
     * </pre>
     * similarly, for a {@link descriptor} {@code messagetype}:
     * <pre>
     *   for all i in [0, messagetype.getnestedtypecount()):
     *     messagetype.getnestedtype(i).getindex() == i
     * </pre>
     */
    public int getindex() { return index; }

    /** convert the descriptor to its protocol message representation. */
    public descriptorproto toproto() { return proto; }

    /** get the type's unqualified name. */
    public string getname() { return proto.getname(); }

    /**
     * get the type's fully-qualified name, within the proto language's
     * namespace.  this differs from the java name.  for example, given this
     * {@code .proto}:
     * <pre>
     *   package foo.bar;
     *   option java_package = "com.example.protos"
     *   message baz {}
     * </pre>
     * {@code baz}'s full name is "foo.bar.baz".
     */
    public string getfullname() { return fullname; }

    /** get the {@link filedescriptor} containing this descriptor. */
    public filedescriptor getfile() { return file; }

    /** if this is a nested type, get the outer descriptor, otherwise null. */
    public descriptor getcontainingtype() { return containingtype; }

    /** get the {@code messageoptions}, defined in {@code descriptor.proto}. */
    public messageoptions getoptions() { return proto.getoptions(); }

    /** get a list of this message type's fields. */
    public list<fielddescriptor> getfields() {
      return collections.unmodifiablelist(arrays.aslist(fields));
    }

    /** get a list of this message type's extensions. */
    public list<fielddescriptor> getextensions() {
      return collections.unmodifiablelist(arrays.aslist(extensions));
    }

    /** get a list of message types nested within this one. */
    public list<descriptor> getnestedtypes() {
      return collections.unmodifiablelist(arrays.aslist(nestedtypes));
    }

    /** get a list of enum types nested within this one. */
    public list<enumdescriptor> getenumtypes() {
      return collections.unmodifiablelist(arrays.aslist(enumtypes));
    }

    /** determines if the given field number is an extension. */
    public boolean isextensionnumber(final int number) {
      for (final descriptorproto.extensionrange range :
          proto.getextensionrangelist()) {
        if (range.getstart() <= number && number < range.getend()) {
          return true;
        }
      }
      return false;
    }

    /**
     * finds a field by name.
     * @param name the unqualified name of the field (e.g. "foo").
     * @return the field's descriptor, or {@code null} if not found.
     */
    public fielddescriptor findfieldbyname(final string name) {
      final genericdescriptor result =
          file.pool.findsymbol(fullname + '.' + name);
      if (result != null && result instanceof fielddescriptor) {
        return (fielddescriptor)result;
      } else {
        return null;
      }
    }

    /**
     * finds a field by field number.
     * @param number the field number within this message type.
     * @return the field's descriptor, or {@code null} if not found.
     */
    public fielddescriptor findfieldbynumber(final int number) {
      return file.pool.fieldsbynumber.get(
        new descriptorpool.descriptorintpair(this, number));
    }

    /**
     * finds a nested message type by name.
     * @param name the unqualified name of the nested type (e.g. "foo").
     * @return the types's descriptor, or {@code null} if not found.
     */
    public descriptor findnestedtypebyname(final string name) {
      final genericdescriptor result =
          file.pool.findsymbol(fullname + '.' + name);
      if (result != null && result instanceof descriptor) {
        return (descriptor)result;
      } else {
        return null;
      }
    }

    /**
     * finds a nested enum type by name.
     * @param name the unqualified name of the nested type (e.g. "foo").
     * @return the types's descriptor, or {@code null} if not found.
     */
    public enumdescriptor findenumtypebyname(final string name) {
      final genericdescriptor result =
          file.pool.findsymbol(fullname + '.' + name);
      if (result != null && result instanceof enumdescriptor) {
        return (enumdescriptor)result;
      } else {
        return null;
      }
    }

    private final int index;
    private descriptorproto proto;
    private final string fullname;
    private final filedescriptor file;
    private final descriptor containingtype;
    private final descriptor[] nestedtypes;
    private final enumdescriptor[] enumtypes;
    private final fielddescriptor[] fields;
    private final fielddescriptor[] extensions;

    private descriptor(final descriptorproto proto,
                       final filedescriptor file,
                       final descriptor parent,
                       final int index)
                throws descriptorvalidationexception {
      this.index = index;
      this.proto = proto;
      fullname = computefullname(file, parent, proto.getname());
      this.file = file;
      containingtype = parent;

      nestedtypes = new descriptor[proto.getnestedtypecount()];
      for (int i = 0; i < proto.getnestedtypecount(); i++) {
        nestedtypes[i] = new descriptor(
          proto.getnestedtype(i), file, this, i);
      }

      enumtypes = new enumdescriptor[proto.getenumtypecount()];
      for (int i = 0; i < proto.getenumtypecount(); i++) {
        enumtypes[i] = new enumdescriptor(
          proto.getenumtype(i), file, this, i);
      }

      fields = new fielddescriptor[proto.getfieldcount()];
      for (int i = 0; i < proto.getfieldcount(); i++) {
        fields[i] = new fielddescriptor(
          proto.getfield(i), file, this, i, false);
      }

      extensions = new fielddescriptor[proto.getextensioncount()];
      for (int i = 0; i < proto.getextensioncount(); i++) {
        extensions[i] = new fielddescriptor(
          proto.getextension(i), file, this, i, true);
      }

      file.pool.addsymbol(this);
    }

    /** look up and cross-link all field types, etc. */
    private void crosslink() throws descriptorvalidationexception {
      for (final descriptor nestedtype : nestedtypes) {
        nestedtype.crosslink();
      }

      for (final fielddescriptor field : fields) {
        field.crosslink();
      }

      for (final fielddescriptor extension : extensions) {
        extension.crosslink();
      }
    }

    /** see {@link filedescriptor#setproto}. */
    private void setproto(final descriptorproto proto) {
      this.proto = proto;

      for (int i = 0; i < nestedtypes.length; i++) {
        nestedtypes[i].setproto(proto.getnestedtype(i));
      }

      for (int i = 0; i < enumtypes.length; i++) {
        enumtypes[i].setproto(proto.getenumtype(i));
      }

      for (int i = 0; i < fields.length; i++) {
        fields[i].setproto(proto.getfield(i));
      }

      for (int i = 0; i < extensions.length; i++) {
        extensions[i].setproto(proto.getextension(i));
      }
    }
  }

  // =================================================================

  /** describes a field of a message type. */
  public static final class fielddescriptor
      implements genericdescriptor, comparable<fielddescriptor>,
                 fieldset.fielddescriptorlite<fielddescriptor> {
    /**
     * get the index of this descriptor within its parent.
     * @see descriptors.descriptor#getindex()
     */
    public int getindex() { return index; }

    /** convert the descriptor to its protocol message representation. */
    public fielddescriptorproto toproto() { return proto; }

    /** get the field's unqualified name. */
    public string getname() { return proto.getname(); }

    /** get the field's number. */
    public int getnumber() { return proto.getnumber(); }

    /**
     * get the field's fully-qualified name.
     * @see descriptors.descriptor#getfullname()
     */
    public string getfullname() { return fullname; }

    /**
     * get the field's java type.  this is just for convenience.  every
     * {@code fielddescriptorproto.type} maps to exactly one java type.
     */
    public javatype getjavatype() { return type.getjavatype(); }

    /** for internal use only. */
    public wireformat.javatype getlitejavatype() {
      return getlitetype().getjavatype();
    }

    /** get the {@code filedescriptor} containing this descriptor. */
    public filedescriptor getfile() { return file; }

    /** get the field's declared type. */
    public type gettype() { return type; }

    /** for internal use only. */
    public wireformat.fieldtype getlitetype() {
      return table[type.ordinal()];
    }
    // i'm pretty sure values() constructs a new array every time, since there
    // is nothing stopping the caller from mutating the array.  therefore we
    // make a static copy here.
    private static final wireformat.fieldtype[] table =
        wireformat.fieldtype.values();

    /** is this field declared required? */
    public boolean isrequired() {
      return proto.getlabel() == fielddescriptorproto.label.label_required;
    }

    /** is this field declared optional? */
    public boolean isoptional() {
      return proto.getlabel() == fielddescriptorproto.label.label_optional;
    }

    /** is this field declared repeated? */
    public boolean isrepeated() {
      return proto.getlabel() == fielddescriptorproto.label.label_repeated;
    }

    /** does this field have the {@code [packed = true]} option? */
    public boolean ispacked() {
      return getoptions().getpacked();
    }

    /** can this field be packed? i.e. is it a repeated primitive field? */
    public boolean ispackable() {
      return isrepeated() && getlitetype().ispackable();
    }

    /** returns true if the field had an explicitly-defined default value. */
    public boolean hasdefaultvalue() { return proto.hasdefaultvalue(); }

    /**
     * returns the field's default value.  valid for all types except for
     * messages and groups.  for all other types, the object returned is of
     * the same class that would returned by message.getfield(this).
     */
    public object getdefaultvalue() {
      if (getjavatype() == javatype.message) {
        throw new unsupportedoperationexception(
          "fielddescriptor.getdefaultvalue() called on an embedded message " +
          "field.");
      }
      return defaultvalue;
    }

    /** get the {@code fieldoptions}, defined in {@code descriptor.proto}. */
    public fieldoptions getoptions() { return proto.getoptions(); }

    /** is this field an extension? */
    public boolean isextension() { return proto.hasextendee(); }

    /**
     * get the field's containing type. for extensions, this is the type being
     * extended, not the location where the extension was defined.  see
     * {@link #getextensionscope()}.
     */
    public descriptor getcontainingtype() { return containingtype; }

    /**
     * for extensions defined nested within message types, gets the outer
     * type.  not valid for non-extension fields.  for example, consider
     * this {@code .proto} file:
     * <pre>
     *   message foo {
     *     extensions 1000 to max;
     *   }
     *   extend foo {
     *     optional int32 baz = 1234;
     *   }
     *   message bar {
     *     extend foo {
     *       optional int32 qux = 4321;
     *     }
     *   }
     * </pre>
     * both {@code baz}'s and {@code qux}'s containing type is {@code foo}.
     * however, {@code baz}'s extension scope is {@code null} while
     * {@code qux}'s extension scope is {@code bar}.
     */
    public descriptor getextensionscope() {
      if (!isextension()) {
        throw new unsupportedoperationexception(
          "this field is not an extension.");
      }
      return extensionscope;
    }

    /** for embedded message and group fields, gets the field's type. */
    public descriptor getmessagetype() {
      if (getjavatype() != javatype.message) {
        throw new unsupportedoperationexception(
          "this field is not of message type.");
      }
      return messagetype;
    }

    /** for enum fields, gets the field's type. */
    public enumdescriptor getenumtype() {
      if (getjavatype() != javatype.enum) {
        throw new unsupportedoperationexception(
          "this field is not of enum type.");
      }
      return enumtype;
    }

    /**
     * compare with another {@code fielddescriptor}.  this orders fields in
     * "canonical" order, which simply means ascending order by field number.
     * {@code other} must be a field of the same type -- i.e.
     * {@code getcontainingtype()} must return the same {@code descriptor} for
     * both fields.
     *
     * @return negative, zero, or positive if {@code this} is less than,
     *         equal to, or greater than {@code other}, respectively.
     */
    public int compareto(final fielddescriptor other) {
      if (other.containingtype != containingtype) {
        throw new illegalargumentexception(
          "fielddescriptors can only be compared to other fielddescriptors " +
          "for fields of the same message type.");
      }
      return getnumber() - other.getnumber();
    }

    private final int index;

    private fielddescriptorproto proto;
    private final string fullname;
    private final filedescriptor file;
    private final descriptor extensionscope;

    // possibly initialized during cross-linking.
    private type type;
    private descriptor containingtype;
    private descriptor messagetype;
    private enumdescriptor enumtype;
    private object defaultvalue;

    public enum type {
      double  (javatype.double     ),
      float   (javatype.float      ),
      int64   (javatype.long       ),
      uint64  (javatype.long       ),
      int32   (javatype.int        ),
      fixed64 (javatype.long       ),
      fixed32 (javatype.int        ),
      bool    (javatype.boolean    ),
      string  (javatype.string     ),
      group   (javatype.message    ),
      message (javatype.message    ),
      bytes   (javatype.byte_string),
      uint32  (javatype.int        ),
      enum    (javatype.enum       ),
      sfixed32(javatype.int        ),
      sfixed64(javatype.long       ),
      sint32  (javatype.int        ),
      sint64  (javatype.long       );

      type(final javatype javatype) {
        this.javatype = javatype;
      }

      private javatype javatype;

      public fielddescriptorproto.type toproto() {
        return fielddescriptorproto.type.valueof(ordinal() + 1);
      }
      public javatype getjavatype() { return javatype; }

      public static type valueof(final fielddescriptorproto.type type) {
        return values()[type.getnumber() - 1];
      }
    }

    static {
      // refuse to init if someone added a new declared type.
      if (type.values().length != fielddescriptorproto.type.values().length) {
        throw new runtimeexception(
          "descriptor.proto has a new declared type but desrciptors.java " +
          "wasn't updated.");
      }
    }

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
       * type.  this is meant for use inside this file only, hence is private.
       */
      private final object defaultdefault;
    }

    private fielddescriptor(final fielddescriptorproto proto,
                            final filedescriptor file,
                            final descriptor parent,
                            final int index,
                            final boolean isextension)
                     throws descriptorvalidationexception {
      this.index = index;
      this.proto = proto;
      fullname = computefullname(file, parent, proto.getname());
      this.file = file;

      if (proto.hastype()) {
        type = type.valueof(proto.gettype());
      }

      if (getnumber() <= 0) {
        throw new descriptorvalidationexception(this,
          "field numbers must be positive integers.");
      }

      // only repeated primitive fields may be packed.
      if (proto.getoptions().getpacked() && !ispackable()) {
        throw new descriptorvalidationexception(this,
          "[packed = true] can only be specified for repeated primitive " +
          "fields.");
      }

      if (isextension) {
        if (!proto.hasextendee()) {
          throw new descriptorvalidationexception(this,
            "fielddescriptorproto.extendee not set for extension field.");
        }
        containingtype = null;  // will be filled in when cross-linking
        if (parent != null) {
          extensionscope = parent;
        } else {
          extensionscope = null;
        }
      } else {
        if (proto.hasextendee()) {
          throw new descriptorvalidationexception(this,
            "fielddescriptorproto.extendee set for non-extension field.");
        }
        containingtype = parent;
        extensionscope = null;
      }

      file.pool.addsymbol(this);
    }

    /** look up and cross-link all field types, etc. */
    private void crosslink() throws descriptorvalidationexception {
      if (proto.hasextendee()) {
        final genericdescriptor extendee =
          file.pool.lookupsymbol(proto.getextendee(), this,
              descriptorpool.searchfilter.types_only);
        if (!(extendee instanceof descriptor)) {
          throw new descriptorvalidationexception(this,
              '\"' + proto.getextendee() + "\" is not a message type.");
        }
        containingtype = (descriptor)extendee;

        if (!getcontainingtype().isextensionnumber(getnumber())) {
          throw new descriptorvalidationexception(this,
              '\"' + getcontainingtype().getfullname() +
              "\" does not declare " + getnumber() +
              " as an extension number.");
        }
      }

      if (proto.hastypename()) {
        final genericdescriptor typedescriptor =
          file.pool.lookupsymbol(proto.gettypename(), this,
              descriptorpool.searchfilter.types_only);

        if (!proto.hastype()) {
          // choose field type based on symbol.
          if (typedescriptor instanceof descriptor) {
            type = type.message;
          } else if (typedescriptor instanceof enumdescriptor) {
            type = type.enum;
          } else {
            throw new descriptorvalidationexception(this,
                '\"' + proto.gettypename() + "\" is not a type.");
          }
        }

        if (getjavatype() == javatype.message) {
          if (!(typedescriptor instanceof descriptor)) {
            throw new descriptorvalidationexception(this,
                '\"' + proto.gettypename() + "\" is not a message type.");
          }
          messagetype = (descriptor)typedescriptor;

          if (proto.hasdefaultvalue()) {
            throw new descriptorvalidationexception(this,
              "messages can't have default values.");
          }
        } else if (getjavatype() == javatype.enum) {
          if (!(typedescriptor instanceof enumdescriptor)) {
            throw new descriptorvalidationexception(this,
                '\"' + proto.gettypename() + "\" is not an enum type.");
          }
          enumtype = (enumdescriptor)typedescriptor;
        } else {
          throw new descriptorvalidationexception(this,
            "field with primitive type has type_name.");
        }
      } else {
        if (getjavatype() == javatype.message ||
            getjavatype() == javatype.enum) {
          throw new descriptorvalidationexception(this,
            "field with message or enum type missing type_name.");
        }
      }

      // we don't attempt to parse the default value until here because for
      // enums we need the enum type's descriptor.
      if (proto.hasdefaultvalue()) {
        if (isrepeated()) {
          throw new descriptorvalidationexception(this,
            "repeated fields cannot have default values.");
        }

        try {
          switch (gettype()) {
            case int32:
            case sint32:
            case sfixed32:
              defaultvalue = textformat.parseint32(proto.getdefaultvalue());
              break;
            case uint32:
            case fixed32:
              defaultvalue = textformat.parseuint32(proto.getdefaultvalue());
              break;
            case int64:
            case sint64:
            case sfixed64:
              defaultvalue = textformat.parseint64(proto.getdefaultvalue());
              break;
            case uint64:
            case fixed64:
              defaultvalue = textformat.parseuint64(proto.getdefaultvalue());
              break;
            case float:
              if (proto.getdefaultvalue().equals("inf")) {
                defaultvalue = float.positive_infinity;
              } else if (proto.getdefaultvalue().equals("-inf")) {
                defaultvalue = float.negative_infinity;
              } else if (proto.getdefaultvalue().equals("nan")) {
                defaultvalue = float.nan;
              } else {
                defaultvalue = float.valueof(proto.getdefaultvalue());
              }
              break;
            case double:
              if (proto.getdefaultvalue().equals("inf")) {
                defaultvalue = double.positive_infinity;
              } else if (proto.getdefaultvalue().equals("-inf")) {
                defaultvalue = double.negative_infinity;
              } else if (proto.getdefaultvalue().equals("nan")) {
                defaultvalue = double.nan;
              } else {
                defaultvalue = double.valueof(proto.getdefaultvalue());
              }
              break;
            case bool:
              defaultvalue = boolean.valueof(proto.getdefaultvalue());
              break;
            case string:
              defaultvalue = proto.getdefaultvalue();
              break;
            case bytes:
              try {
                defaultvalue =
                  textformat.unescapebytes(proto.getdefaultvalue());
              } catch (textformat.invalidescapesequenceexception e) {
                throw new descriptorvalidationexception(this,
                  "couldn't parse default value: " + e.getmessage(), e);
              }
              break;
            case enum:
              defaultvalue = enumtype.findvaluebyname(proto.getdefaultvalue());
              if (defaultvalue == null) {
                throw new descriptorvalidationexception(this,
                  "unknown enum default value: \"" +
                  proto.getdefaultvalue() + '\"');
              }
              break;
            case message:
            case group:
              throw new descriptorvalidationexception(this,
                "message type had default value.");
          }
        } catch (numberformatexception e) {
          throw new descriptorvalidationexception(this, 
              "could not parse default value: \"" + 
              proto.getdefaultvalue() + '\"', e);
        }
      } else {
        // determine the default default for this field.
        if (isrepeated()) {
          defaultvalue = collections.emptylist();
        } else {
          switch (getjavatype()) {
            case enum:
              // we guarantee elsewhere that an enum type always has at least
              // one possible value.
              defaultvalue = enumtype.getvalues().get(0);
              break;
            case message:
              defaultvalue = null;
              break;
            default:
              defaultvalue = getjavatype().defaultdefault;
              break;
          }
        }
      }

      if (!isextension()) {
        file.pool.addfieldbynumber(this);
      }

      if (containingtype != null &&
          containingtype.getoptions().getmessagesetwireformat()) {
        if (isextension()) {
          if (!isoptional() || gettype() != type.message) {
            throw new descriptorvalidationexception(this,
              "extensions of messagesets must be optional messages.");
          }
        } else {
          throw new descriptorvalidationexception(this,
            "messagesets cannot have fields, only extensions.");
        }
      }
    }

    /** see {@link filedescriptor#setproto}. */
    private void setproto(final fielddescriptorproto proto) {
      this.proto = proto;
    }

    /**
     * for internal use only.  this is to satisfy the fielddescriptorlite
     * interface.
     */
    public messagelite.builder internalmergefrom(
        messagelite.builder to, messagelite from) {
      // fielddescriptors are only used with non-lite messages so we can just
      // down-cast and call mergefrom directly.
      return ((message.builder) to).mergefrom((message) from);
    }
  }

  // =================================================================

  /** describes an enum type. */
  public static final class enumdescriptor
      implements genericdescriptor, internal.enumlitemap<enumvaluedescriptor> {
    /**
     * get the index of this descriptor within its parent.
     * @see descriptors.descriptor#getindex()
     */
    public int getindex() { return index; }

    /** convert the descriptor to its protocol message representation. */
    public enumdescriptorproto toproto() { return proto; }

    /** get the type's unqualified name. */
    public string getname() { return proto.getname(); }

    /**
     * get the type's fully-qualified name.
     * @see descriptors.descriptor#getfullname()
     */
    public string getfullname() { return fullname; }

    /** get the {@link filedescriptor} containing this descriptor. */
    public filedescriptor getfile() { return file; }

    /** if this is a nested type, get the outer descriptor, otherwise null. */
    public descriptor getcontainingtype() { return containingtype; }

    /** get the {@code enumoptions}, defined in {@code descriptor.proto}. */
    public enumoptions getoptions() { return proto.getoptions(); }

    /** get a list of defined values for this enum. */
    public list<enumvaluedescriptor> getvalues() {
      return collections.unmodifiablelist(arrays.aslist(values));
    }

    /**
     * find an enum value by name.
     * @param name the unqualified name of the value (e.g. "foo").
     * @return the value's descriptor, or {@code null} if not found.
     */
    public enumvaluedescriptor findvaluebyname(final string name) {
      final genericdescriptor result =
          file.pool.findsymbol(fullname + '.' + name);
      if (result != null && result instanceof enumvaluedescriptor) {
        return (enumvaluedescriptor)result;
      } else {
        return null;
      }
    }

    /**
     * find an enum value by number.  if multiple enum values have the same
     * number, this returns the first defined value with that number.
     * @param number the value's number.
     * @return the value's descriptor, or {@code null} if not found.
     */
    public enumvaluedescriptor findvaluebynumber(final int number) {
      return file.pool.enumvaluesbynumber.get(
        new descriptorpool.descriptorintpair(this, number));
    }

    private final int index;
    private enumdescriptorproto proto;
    private final string fullname;
    private final filedescriptor file;
    private final descriptor containingtype;
    private enumvaluedescriptor[] values;

    private enumdescriptor(final enumdescriptorproto proto,
                           final filedescriptor file,
                           final descriptor parent,
                           final int index)
                    throws descriptorvalidationexception {
      this.index = index;
      this.proto = proto;
      fullname = computefullname(file, parent, proto.getname());
      this.file = file;
      containingtype = parent;

      if (proto.getvaluecount() == 0) {
        // we cannot allow enums with no values because this would mean there
        // would be no valid default value for fields of this type.
        throw new descriptorvalidationexception(this,
          "enums must contain at least one value.");
      }

      values = new enumvaluedescriptor[proto.getvaluecount()];
      for (int i = 0; i < proto.getvaluecount(); i++) {
        values[i] = new enumvaluedescriptor(
          proto.getvalue(i), file, this, i);
      }

      file.pool.addsymbol(this);
    }

    /** see {@link filedescriptor#setproto}. */
    private void setproto(final enumdescriptorproto proto) {
      this.proto = proto;

      for (int i = 0; i < values.length; i++) {
        values[i].setproto(proto.getvalue(i));
      }
    }
  }

  // =================================================================

  /**
   * describes one value within an enum type.  note that multiple defined
   * values may have the same number.  in generated java code, all values
   * with the same number after the first become aliases of the first.
   * however, they still have independent enumvaluedescriptors.
   */
  public static final class enumvaluedescriptor
      implements genericdescriptor, internal.enumlite {
    /**
     * get the index of this descriptor within its parent.
     * @see descriptors.descriptor#getindex()
     */
    public int getindex() { return index; }

    /** convert the descriptor to its protocol message representation. */
    public enumvaluedescriptorproto toproto() { return proto; }

    /** get the value's unqualified name. */
    public string getname() { return proto.getname(); }

    /** get the value's number. */
    public int getnumber() { return proto.getnumber(); }

    /**
     * get the value's fully-qualified name.
     * @see descriptors.descriptor#getfullname()
     */
    public string getfullname() { return fullname; }

    /** get the {@link filedescriptor} containing this descriptor. */
    public filedescriptor getfile() { return file; }

    /** get the value's enum type. */
    public enumdescriptor gettype() { return type; }

    /**
     * get the {@code enumvalueoptions}, defined in {@code descriptor.proto}.
     */
    public enumvalueoptions getoptions() { return proto.getoptions(); }

    private final int index;
    private enumvaluedescriptorproto proto;
    private final string fullname;
    private final filedescriptor file;
    private final enumdescriptor type;

    private enumvaluedescriptor(final enumvaluedescriptorproto proto,
                                final filedescriptor file,
                                final enumdescriptor parent,
                                final int index)
                         throws descriptorvalidationexception {
      this.index = index;
      this.proto = proto;
      this.file = file;
      type = parent;

      fullname = parent.getfullname() + '.' + proto.getname();

      file.pool.addsymbol(this);
      file.pool.addenumvaluebynumber(this);
    }

    /** see {@link filedescriptor#setproto}. */
    private void setproto(final enumvaluedescriptorproto proto) {
      this.proto = proto;
    }
  }

  // =================================================================

  /** describes a service type. */
  public static final class servicedescriptor implements genericdescriptor {
    /**
     * get the index of this descriptor within its parent.
     * * @see descriptors.descriptor#getindex()
     */
    public int getindex() { return index; }

    /** convert the descriptor to its protocol message representation. */
    public servicedescriptorproto toproto() { return proto; }

    /** get the type's unqualified name. */
    public string getname() { return proto.getname(); }

    /**
     * get the type's fully-qualified name.
     * @see descriptors.descriptor#getfullname()
     */
    public string getfullname() { return fullname; }

    /** get the {@link filedescriptor} containing this descriptor. */
    public filedescriptor getfile() { return file; }

    /** get the {@code serviceoptions}, defined in {@code descriptor.proto}. */
    public serviceoptions getoptions() { return proto.getoptions(); }

    /** get a list of methods for this service. */
    public list<methoddescriptor> getmethods() {
      return collections.unmodifiablelist(arrays.aslist(methods));
    }

    /**
     * find a method by name.
     * @param name the unqualified name of the method (e.g. "foo").
     * @return the method's descriptor, or {@code null} if not found.
     */
    public methoddescriptor findmethodbyname(final string name) {
      final genericdescriptor result =
          file.pool.findsymbol(fullname + '.' + name);
      if (result != null && result instanceof methoddescriptor) {
        return (methoddescriptor)result;
      } else {
        return null;
      }
    }

    private final int index;
    private servicedescriptorproto proto;
    private final string fullname;
    private final filedescriptor file;
    private methoddescriptor[] methods;

    private servicedescriptor(final servicedescriptorproto proto,
                              final filedescriptor file,
                              final int index)
                       throws descriptorvalidationexception {
      this.index = index;
      this.proto = proto;
      fullname = computefullname(file, null, proto.getname());
      this.file = file;

      methods = new methoddescriptor[proto.getmethodcount()];
      for (int i = 0; i < proto.getmethodcount(); i++) {
        methods[i] = new methoddescriptor(
          proto.getmethod(i), file, this, i);
      }

      file.pool.addsymbol(this);
    }

    private void crosslink() throws descriptorvalidationexception {
      for (final methoddescriptor method : methods) {
        method.crosslink();
      }
    }

    /** see {@link filedescriptor#setproto}. */
    private void setproto(final servicedescriptorproto proto) {
      this.proto = proto;

      for (int i = 0; i < methods.length; i++) {
        methods[i].setproto(proto.getmethod(i));
      }
    }
  }

  // =================================================================

  /**
   * describes one method within a service type.
   */
  public static final class methoddescriptor implements genericdescriptor {
    /**
     * get the index of this descriptor within its parent.
     * * @see descriptors.descriptor#getindex()
     */
    public int getindex() { return index; }

    /** convert the descriptor to its protocol message representation. */
    public methoddescriptorproto toproto() { return proto; }

    /** get the method's unqualified name. */
    public string getname() { return proto.getname(); }

    /**
     * get the method's fully-qualified name.
     * @see descriptors.descriptor#getfullname()
     */
    public string getfullname() { return fullname; }

    /** get the {@link filedescriptor} containing this descriptor. */
    public filedescriptor getfile() { return file; }

    /** get the method's service type. */
    public servicedescriptor getservice() { return service; }

    /** get the method's input type. */
    public descriptor getinputtype() { return inputtype; }

    /** get the method's output type. */
    public descriptor getoutputtype() { return outputtype; }

    /**
     * get the {@code methodoptions}, defined in {@code descriptor.proto}.
     */
    public methodoptions getoptions() { return proto.getoptions(); }

    private final int index;
    private methoddescriptorproto proto;
    private final string fullname;
    private final filedescriptor file;
    private final servicedescriptor service;

    // initialized during cross-linking.
    private descriptor inputtype;
    private descriptor outputtype;

    private methoddescriptor(final methoddescriptorproto proto,
                             final filedescriptor file,
                             final servicedescriptor parent,
                             final int index)
                      throws descriptorvalidationexception {
      this.index = index;
      this.proto = proto;
      this.file = file;
      service = parent;

      fullname = parent.getfullname() + '.' + proto.getname();

      file.pool.addsymbol(this);
    }

    private void crosslink() throws descriptorvalidationexception {
      final genericdescriptor input =
        file.pool.lookupsymbol(proto.getinputtype(), this,
            descriptorpool.searchfilter.types_only);
      if (!(input instanceof descriptor)) {
        throw new descriptorvalidationexception(this,
            '\"' + proto.getinputtype() + "\" is not a message type.");
      }
      inputtype = (descriptor)input;

      final genericdescriptor output =
        file.pool.lookupsymbol(proto.getoutputtype(), this,
            descriptorpool.searchfilter.types_only);
      if (!(output instanceof descriptor)) {
        throw new descriptorvalidationexception(this,
            '\"' + proto.getoutputtype() + "\" is not a message type.");
      }
      outputtype = (descriptor)output;
    }

    /** see {@link filedescriptor#setproto}. */
    private void setproto(final methoddescriptorproto proto) {
      this.proto = proto;
    }
  }

  // =================================================================

  private static string computefullname(final filedescriptor file,
                                        final descriptor parent,
                                        final string name) {
    if (parent != null) {
      return parent.getfullname() + '.' + name;
    } else if (file.getpackage().length() > 0) {
      return file.getpackage() + '.' + name;
    } else {
      return name;
    }
  }

  // =================================================================

  /**
   * all descriptors except {@code filedescriptor} implement this to make
   * {@code descriptorpool}'s life easier.
   */
  private interface genericdescriptor {
    message toproto();
    string getname();
    string getfullname();
    filedescriptor getfile();
  }

  /**
   * thrown when building descriptors fails because the source descriptorprotos
   * are not valid.
   */
  public static class descriptorvalidationexception extends exception {
    private static final long serialversionuid = 5750205775490483148l;

    /** gets the full name of the descriptor where the error occurred. */
    public string getproblemsymbolname() { return name; }

    /**
     * gets the protocol message representation of the invalid descriptor.
     */
    public message getproblemproto() { return proto; }

    /**
     * gets a human-readable description of the error.
     */
    public string getdescription() { return description; }

    private final string name;
    private final message proto;
    private final string description;

    private descriptorvalidationexception(
        final genericdescriptor problemdescriptor,
        final string description) {
      super(problemdescriptor.getfullname() + ": " + description);

      // note that problemdescriptor may be partially uninitialized, so we
      // don't want to expose it directly to the user.  so, we only provide
      // the name and the original proto.
      name = problemdescriptor.getfullname();
      proto = problemdescriptor.toproto();
      this.description = description;
    }

    private descriptorvalidationexception(
        final genericdescriptor problemdescriptor,
        final string description,
        final throwable cause) {
      this(problemdescriptor, description);
      initcause(cause);
    }

    private descriptorvalidationexception(
        final filedescriptor problemdescriptor,
        final string description) {
      super(problemdescriptor.getname() + ": " + description);

      // note that problemdescriptor may be partially uninitialized, so we
      // don't want to expose it directly to the user.  so, we only provide
      // the name and the original proto.
      name = problemdescriptor.getname();
      proto = problemdescriptor.toproto();
      this.description = description;
    }
  }

  // =================================================================

  /**
   * a private helper class which contains lookup tables containing all the
   * descriptors defined in a particular file.
   */
  private static final class descriptorpool {
    
    /** defines what subclass of descriptors to search in the descriptor pool. 
     */
    enum searchfilter {
      types_only, aggregates_only, all_symbols
    }
    
    descriptorpool(final filedescriptor[] dependencies) {
      this.dependencies = new hashset<filedescriptor>();

      for (int i = 0; i < dependencies.length; i++) {
        this.dependencies.add(dependencies[i]);
        importpublicdependencies(dependencies[i]);
      }

      for (final filedescriptor dependency : this.dependencies) {
        try {
          addpackage(dependency.getpackage(), dependency);
        } catch (descriptorvalidationexception e) {
          // can't happen, because addpackage() only fails when the name
          // conflicts with a non-package, but we have not yet added any
          // non-packages at this point.
          assert false;
        }
      }
    }

    /** find and put public dependencies of the file into dependencies set.*/
    private void importpublicdependencies(final filedescriptor file) {
      for (filedescriptor dependency : file.getpublicdependencies()) {
        if (dependencies.add(dependency)) {
          importpublicdependencies(dependency);
        }
      }
    }

    private final set<filedescriptor> dependencies;

    private final map<string, genericdescriptor> descriptorsbyname =
      new hashmap<string, genericdescriptor>();
    private final map<descriptorintpair, fielddescriptor> fieldsbynumber =
      new hashmap<descriptorintpair, fielddescriptor>();
    private final map<descriptorintpair, enumvaluedescriptor> enumvaluesbynumber
        = new hashmap<descriptorintpair, enumvaluedescriptor>();

    /** find a generic descriptor by fully-qualified name. */
    genericdescriptor findsymbol(final string fullname) {
      return findsymbol(fullname, searchfilter.all_symbols);
    }
    
    /** find a descriptor by fully-qualified name and given option to only 
     * search valid field type descriptors. 
     */
    genericdescriptor findsymbol(final string fullname,
                                 final searchfilter filter) {
      genericdescriptor result = descriptorsbyname.get(fullname);
      if (result != null) {
        if ((filter==searchfilter.all_symbols) ||
            ((filter==searchfilter.types_only) && istype(result)) ||
            ((filter==searchfilter.aggregates_only) && isaggregate(result))) {
          return result;
        }
      }

      for (final filedescriptor dependency : dependencies) {
        result = dependency.pool.descriptorsbyname.get(fullname);
        if (result != null) {
          if ((filter==searchfilter.all_symbols) ||
              ((filter==searchfilter.types_only) && istype(result)) ||
              ((filter==searchfilter.aggregates_only) && isaggregate(result))) {
            return result;
          }
        }
      }

      return null;
    }

    /** checks if the descriptor is a valid type for a message field. */
    boolean istype(genericdescriptor descriptor) {
      return (descriptor instanceof descriptor) || 
        (descriptor instanceof enumdescriptor);
    }
    
    /** checks if the descriptor is a valid namespace type. */
    boolean isaggregate(genericdescriptor descriptor) {
      return (descriptor instanceof descriptor) || 
        (descriptor instanceof enumdescriptor) || 
        (descriptor instanceof packagedescriptor) || 
        (descriptor instanceof servicedescriptor);
    }
       
    /**
     * look up a type descriptor by name, relative to some other descriptor.
     * the name may be fully-qualified (with a leading '.'),
     * partially-qualified, or unqualified.  c++-like name lookup semantics
     * are used to search for the matching descriptor.
     */
    genericdescriptor lookupsymbol(final string name,
                                   final genericdescriptor relativeto,
                                   final descriptorpool.searchfilter filter)
                            throws descriptorvalidationexception {
      // todo(kenton):  this could be optimized in a number of ways.

      genericdescriptor result;
      if (name.startswith(".")) {
        // fully-qualified name.
        result = findsymbol(name.substring(1), filter);
      } else {
        // if "name" is a compound identifier, we want to search for the
        // first component of it, then search within it for the rest.
        // if name is something like "foo.bar.baz", and symbols named "foo" are
        // defined in multiple parent scopes, we only want to find "bar.baz" in
        // the innermost one.  e.g., the following should produce an error:
        //   message bar { message baz {} }
        //   message foo {
        //     message bar {
        //     }
        //     optional bar.baz baz = 1;
        //   }
        // so, we look for just "foo" first, then look for "bar.baz" within it
        // if found.
        final int firstpartlength = name.indexof('.');
        final string firstpart;
        if (firstpartlength == -1) {
          firstpart = name;
        } else {
          firstpart = name.substring(0, firstpartlength);
        }

        // we will search each parent scope of "relativeto" looking for the
        // symbol.
        final stringbuilder scopetotry =
            new stringbuilder(relativeto.getfullname());

        while (true) {
          // chop off the last component of the scope.
          final int dotpos = scopetotry.lastindexof(".");
          if (dotpos == -1) {
            result = findsymbol(name, filter);
            break;
          } else {
            scopetotry.setlength(dotpos + 1);

            // append firstpart and try to find
            scopetotry.append(firstpart);
            result = findsymbol(scopetotry.tostring(), 
                descriptorpool.searchfilter.aggregates_only);

            if (result != null) {
              if (firstpartlength != -1) {
                // we only found the first part of the symbol.  now look for
                // the whole thing.  if this fails, we *don't* want to keep
                // searching parent scopes.
                scopetotry.setlength(dotpos + 1);
                scopetotry.append(name);
                result = findsymbol(scopetotry.tostring(), filter);
              }
              break;
            }

            // not found.  remove the name so we can try again.
            scopetotry.setlength(dotpos);
          }
        }
      }

      if (result == null) {
        throw new descriptorvalidationexception(relativeto,
            '\"' + name + "\" is not defined.");
      } else {
        return result;
      }
    }

    /**
     * adds a symbol to the symbol table.  if a symbol with the same name
     * already exists, throws an error.
     */
    void addsymbol(final genericdescriptor descriptor)
            throws descriptorvalidationexception {
      validatesymbolname(descriptor);

      final string fullname = descriptor.getfullname();
      final int dotpos = fullname.lastindexof('.');

      final genericdescriptor old = descriptorsbyname.put(fullname, descriptor);
      if (old != null) {
        descriptorsbyname.put(fullname, old);

        if (descriptor.getfile() == old.getfile()) {
          if (dotpos == -1) {
            throw new descriptorvalidationexception(descriptor,
                '\"' + fullname + "\" is already defined.");
          } else {
            throw new descriptorvalidationexception(descriptor,
                '\"' + fullname.substring(dotpos + 1) +
              "\" is already defined in \"" +
              fullname.substring(0, dotpos) + "\".");
          }
        } else {
          throw new descriptorvalidationexception(descriptor,
              '\"' + fullname + "\" is already defined in file \"" +
            old.getfile().getname() + "\".");
        }
      }
    }

    /**
     * represents a package in the symbol table.  we use packagedescriptors
     * just as placeholders so that someone cannot define, say, a message type
     * that has the same name as an existing package.
     */
    private static final class packagedescriptor implements genericdescriptor {
      public message toproto()        { return file.toproto(); }
      public string getname()         { return name;           }
      public string getfullname()     { return fullname;       }
      public filedescriptor getfile() { return file;           }

      packagedescriptor(final string name, final string fullname,
                        final filedescriptor file) {
        this.file = file;
        this.fullname = fullname;
        this.name = name;
      }

      private final string name;
      private final string fullname;
      private final filedescriptor file;
    }

    /**
     * adds a package to the symbol tables.  if a package by the same name
     * already exists, that is fine, but if some other kind of symbol exists
     * under the same name, an exception is thrown.  if the package has
     * multiple components, this also adds the parent package(s).
     */
    void addpackage(final string fullname, final filedescriptor file)
             throws descriptorvalidationexception {
      final int dotpos = fullname.lastindexof('.');
      final string name;
      if (dotpos == -1) {
        name = fullname;
      } else {
        addpackage(fullname.substring(0, dotpos), file);
        name = fullname.substring(dotpos + 1);
      }

      final genericdescriptor old =
        descriptorsbyname.put(fullname,
          new packagedescriptor(name, fullname, file));
      if (old != null) {
        descriptorsbyname.put(fullname, old);
        if (!(old instanceof packagedescriptor)) {
          throw new descriptorvalidationexception(file,
              '\"' + name + "\" is already defined (as something other than a "
              + "package) in file \"" + old.getfile().getname() + "\".");
        }
      }
    }

    /** a (genericdescriptor, int) pair, used as a map key. */
    private static final class descriptorintpair {
      private final genericdescriptor descriptor;
      private final int number;

      descriptorintpair(final genericdescriptor descriptor, final int number) {
        this.descriptor = descriptor;
        this.number = number;
      }

      @override
      public int hashcode() {
        return descriptor.hashcode() * ((1 << 16) - 1) + number;
      }
      @override
      public boolean equals(final object obj) {
        if (!(obj instanceof descriptorintpair)) {
          return false;
        }
        final descriptorintpair other = (descriptorintpair)obj;
        return descriptor == other.descriptor && number == other.number;
      }
    }

    /**
     * adds a field to the fieldsbynumber table.  throws an exception if a
     * field with the same containing type and number already exists.
     */
    void addfieldbynumber(final fielddescriptor field)
                   throws descriptorvalidationexception {
      final descriptorintpair key =
        new descriptorintpair(field.getcontainingtype(), field.getnumber());
      final fielddescriptor old = fieldsbynumber.put(key, field);
      if (old != null) {
        fieldsbynumber.put(key, old);
        throw new descriptorvalidationexception(field,
          "field number " + field.getnumber() +
          "has already been used in \"" +
          field.getcontainingtype().getfullname() +
          "\" by field \"" + old.getname() + "\".");
      }
    }

    /**
     * adds an enum value to the enumvaluesbynumber table.  if an enum value
     * with the same type and number already exists, does nothing.  (this is
     * allowed; the first value define with the number takes precedence.)
     */
    void addenumvaluebynumber(final enumvaluedescriptor value) {
      final descriptorintpair key =
        new descriptorintpair(value.gettype(), value.getnumber());
      final enumvaluedescriptor old = enumvaluesbynumber.put(key, value);
      if (old != null) {
        enumvaluesbynumber.put(key, old);
        // not an error:  multiple enum values may have the same number, but
        // we only want the first one in the map.
      }
    }

    /**
     * verifies that the descriptor's name is valid (i.e. it contains only
     * letters, digits, and underscores, and does not start with a digit).
     */
    static void validatesymbolname(final genericdescriptor descriptor)
                                   throws descriptorvalidationexception {
      final string name = descriptor.getname();
      if (name.length() == 0) {
        throw new descriptorvalidationexception(descriptor, "missing name.");
      } else {
        boolean valid = true;
        for (int i = 0; i < name.length(); i++) {
          final char c = name.charat(i);
          // non-ascii characters are not valid in protobuf identifiers, even
          // if they are letters or digits.
          if (c >= 128) {
            valid = false;
          }
          // first character must be letter or _.  subsequent characters may
          // be letters, numbers, or digits.
          if (character.isletter(c) || c == '_' ||
              (character.isdigit(c) && i > 0)) {
            // valid
          } else {
            valid = false;
          }
        }
        if (!valid) {
          throw new descriptorvalidationexception(descriptor,
              '\"' + name + "\" is not a valid identifier.");
        }
      }
    }
  }
}
