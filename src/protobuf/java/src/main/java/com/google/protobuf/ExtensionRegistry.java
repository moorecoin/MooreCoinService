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

import java.util.collections;
import java.util.hashmap;
import java.util.map;

/**
 * a table of known extensions, searchable by name or field number.  when
 * parsing a protocol message that might have extensions, you must provide
 * an {@code extensionregistry} in which you have registered any extensions
 * that you want to be able to parse.  otherwise, those extensions will just
 * be treated like unknown fields.
 *
 * <p>for example, if you had the {@code .proto} file:
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
 * then you might write code like:
 *
 * <pre>
 * extensionregistry registry = extensionregistry.newinstance();
 * registry.add(myproto.bar);
 * myproto.foo message = myproto.foo.parsefrom(input, registry);
 * </pre>
 *
 * <p>background:
 *
 * <p>you might wonder why this is necessary.  two alternatives might come to
 * mind.  first, you might imagine a system where generated extensions are
 * automatically registered when their containing classes are loaded.  this
 * is a popular technique, but is bad design; among other things, it creates a
 * situation where behavior can change depending on what classes happen to be
 * loaded.  it also introduces a security vulnerability, because an
 * unprivileged class could cause its code to be called unexpectedly from a
 * privileged class by registering itself as an extension of the right type.
 *
 * <p>another option you might consider is lazy parsing: do not parse an
 * extension until it is first requested, at which point the caller must
 * provide a type to use.  this introduces a different set of problems.  first,
 * it would require a mutex lock any time an extension was accessed, which
 * would be slow.  second, corrupt data would not be detected until first
 * access, at which point it would be much harder to deal with it.  third, it
 * could violate the expectation that message objects are immutable, since the
 * type provided could be any arbitrary message class.  an unprivileged user
 * could take advantage of this to inject a mutable object into a message
 * belonging to privileged code and create mischief.
 *
 * @author kenton@google.com kenton varda
 */
public final class extensionregistry extends extensionregistrylite {
  /** construct a new, empty instance. */
  public static extensionregistry newinstance() {
    return new extensionregistry();
  }

  /** get the unmodifiable singleton empty instance. */
  public static extensionregistry getemptyregistry() {
    return empty;
  }

  /** returns an unmodifiable view of the registry. */
  @override
  public extensionregistry getunmodifiable() {
    return new extensionregistry(this);
  }

  /** a (descriptor, message) pair, returned by lookup methods. */
  public static final class extensioninfo {
    /** the extension's descriptor. */
    public final fielddescriptor descriptor;

    /**
     * a default instance of the extension's type, if it has a message type.
     * otherwise, {@code null}.
     */
    public final message defaultinstance;

    private extensioninfo(final fielddescriptor descriptor) {
      this.descriptor = descriptor;
      defaultinstance = null;
    }
    private extensioninfo(final fielddescriptor descriptor,
                          final message defaultinstance) {
      this.descriptor = descriptor;
      this.defaultinstance = defaultinstance;
    }
  }

  /**
   * find an extension by fully-qualified field name, in the proto namespace.
   * i.e. {@code result.descriptor.fullname()} will match {@code fullname} if
   * a match is found.
   *
   * @return information about the extension if found, or {@code null}
   *         otherwise.
   */
  public extensioninfo findextensionbyname(final string fullname) {
    return extensionsbyname.get(fullname);
  }

  /**
   * find an extension by containing type and field number.
   *
   * @return information about the extension if found, or {@code null}
   *         otherwise.
   */
  public extensioninfo findextensionbynumber(final descriptor containingtype,
                                             final int fieldnumber) {
    return extensionsbynumber.get(
      new descriptorintpair(containingtype, fieldnumber));
  }

  /** add an extension from a generated file to the registry. */
  public void add(final generatedmessage.generatedextension<?, ?> extension) {
    if (extension.getdescriptor().getjavatype() ==
        fielddescriptor.javatype.message) {
      if (extension.getmessagedefaultinstance() == null) {
        throw new illegalstateexception(
            "registered message-type extension had null default instance: " +
            extension.getdescriptor().getfullname());
      }
      add(new extensioninfo(extension.getdescriptor(),
                            extension.getmessagedefaultinstance()));
    } else {
      add(new extensioninfo(extension.getdescriptor(), null));
    }
  }

  /** add a non-message-type extension to the registry by descriptor. */
  public void add(final fielddescriptor type) {
    if (type.getjavatype() == fielddescriptor.javatype.message) {
      throw new illegalargumentexception(
        "extensionregistry.add() must be provided a default instance when " +
        "adding an embedded message extension.");
    }
    add(new extensioninfo(type, null));
  }

  /** add a message-type extension to the registry by descriptor. */
  public void add(final fielddescriptor type, final message defaultinstance) {
    if (type.getjavatype() != fielddescriptor.javatype.message) {
      throw new illegalargumentexception(
        "extensionregistry.add() provided a default instance for a " +
        "non-message extension.");
    }
    add(new extensioninfo(type, defaultinstance));
  }

  // =================================================================
  // private stuff.

  private extensionregistry() {
    this.extensionsbyname = new hashmap<string, extensioninfo>();
    this.extensionsbynumber = new hashmap<descriptorintpair, extensioninfo>();
  }

  private extensionregistry(extensionregistry other) {
    super(other);
    this.extensionsbyname = collections.unmodifiablemap(other.extensionsbyname);
    this.extensionsbynumber =
        collections.unmodifiablemap(other.extensionsbynumber);
  }

  private final map<string, extensioninfo> extensionsbyname;
  private final map<descriptorintpair, extensioninfo> extensionsbynumber;

  private extensionregistry(boolean empty) {
    super(extensionregistrylite.getemptyregistry());
    this.extensionsbyname = collections.<string, extensioninfo>emptymap();
    this.extensionsbynumber =
        collections.<descriptorintpair, extensioninfo>emptymap();
  }
  private static final extensionregistry empty = new extensionregistry(true);

  private void add(final extensioninfo extension) {
    if (!extension.descriptor.isextension()) {
      throw new illegalargumentexception(
        "extensionregistry.add() was given a fielddescriptor for a regular " +
        "(non-extension) field.");
    }

    extensionsbyname.put(extension.descriptor.getfullname(), extension);
    extensionsbynumber.put(
      new descriptorintpair(extension.descriptor.getcontainingtype(),
                            extension.descriptor.getnumber()),
      extension);

    final fielddescriptor field = extension.descriptor;
    if (field.getcontainingtype().getoptions().getmessagesetwireformat() &&
        field.gettype() == fielddescriptor.type.message &&
        field.isoptional() &&
        field.getextensionscope() == field.getmessagetype()) {
      // this is an extension of a messageset type defined within the extension
      // type's own scope.  for backwards-compatibility, allow it to be looked
      // up by type name.
      extensionsbyname.put(field.getmessagetype().getfullname(), extension);
    }
  }

  /** a (genericdescriptor, int) pair, used as a map key. */
  private static final class descriptorintpair {
    private final descriptor descriptor;
    private final int number;

    descriptorintpair(final descriptor descriptor, final int number) {
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
}
