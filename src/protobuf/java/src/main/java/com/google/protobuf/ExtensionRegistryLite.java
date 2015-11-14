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

import java.util.collections;
import java.util.hashmap;
import java.util.map;

/**
 * equivalent to {@link extensionregistry} but supports only "lite" types.
 * <p>
 * if all of your types are lite types, then you only need to use
 * {@code extensionregistrylite}.  similarly, if all your types are regular
 * types, then you only need {@link extensionregistry}.  typically it does not
 * make sense to mix the two, since if you have any regular types in your
 * program, you then require the full runtime and lose all the benefits of
 * the lite runtime, so you might as well make all your types be regular types.
 * however, in some cases (e.g. when depending on multiple third-party libraries
 * where one uses lite types and one uses regular), you may find yourself
 * wanting to mix the two.  in this case things get more complicated.
 * <p>
 * there are three factors to consider:  whether the type being extended is
 * lite, whether the embedded type (in the case of a message-typed extension)
 * is lite, and whether the extension itself is lite.  since all three are
 * declared in different files, they could all be different.  here are all
 * the combinations and which type of registry to use:
 * <pre>
 *   extended type     inner type    extension         use registry
 *   =======================================================================
 *   lite              lite          lite              extensionregistrylite
 *   lite              regular       lite              extensionregistry
 *   regular           regular       regular           extensionregistry
 *   all other combinations                            not supported
 * </pre>
 * <p>
 * note that just as regular types are not allowed to contain lite-type fields,
 * they are also not allowed to contain lite-type extensions.  this is because
 * regular types must be fully accessible via reflection, which in turn means
 * that all the inner messages must also support reflection.  on the other hand,
 * since regular types implement the entire lite interface, there is no problem
 * with embedding regular types inside lite types.
 *
 * @author kenton@google.com kenton varda
 */
public class extensionregistrylite {

  // set true to enable lazy parsing feature for messageset.
  //
  // todo(xiangl): now we use a global flag to control whether enable lazy
  // parsing feature for messageset, which may be too crude for some
  // applications. need to support this feature on smaller granularity.
  private static volatile boolean eagerlyparsemessagesets = false;

  public static boolean iseagerlyparsemessagesets() {
    return eagerlyparsemessagesets;
  }

  public static void seteagerlyparsemessagesets(boolean iseagerlyparse) {
    eagerlyparsemessagesets = iseagerlyparse;
  }

  /** construct a new, empty instance. */
  public static extensionregistrylite newinstance() {
    return new extensionregistrylite();
  }

  /** get the unmodifiable singleton empty instance. */
  public static extensionregistrylite getemptyregistry() {
    return empty;
  }

  /** returns an unmodifiable view of the registry. */
  public extensionregistrylite getunmodifiable() {
    return new extensionregistrylite(this);
  }

  /**
   * find an extension by containing type and field number.
   *
   * @return information about the extension if found, or {@code null}
   *         otherwise.
   */
  @suppresswarnings("unchecked")
  public <containingtype extends messagelite>
      generatedmessagelite.generatedextension<containingtype, ?>
        findliteextensionbynumber(
          final containingtype containingtypedefaultinstance,
          final int fieldnumber) {
    return (generatedmessagelite.generatedextension<containingtype, ?>)
      extensionsbynumber.get(
        new objectintpair(containingtypedefaultinstance, fieldnumber));
  }

  /** add an extension from a lite generated file to the registry. */
  public final void add(
      final generatedmessagelite.generatedextension<?, ?> extension) {
    extensionsbynumber.put(
      new objectintpair(extension.getcontainingtypedefaultinstance(),
                        extension.getnumber()),
      extension);
  }

  // =================================================================
  // private stuff.

  // constructors are package-private so that extensionregistry can subclass
  // this.

  extensionregistrylite() {
    this.extensionsbynumber =
        new hashmap<objectintpair,
                    generatedmessagelite.generatedextension<?, ?>>();
  }

  extensionregistrylite(extensionregistrylite other) {
    if (other == empty) {
      this.extensionsbynumber = collections.emptymap();
    } else {
      this.extensionsbynumber =
        collections.unmodifiablemap(other.extensionsbynumber);
    }
  }

  private final map<objectintpair,
                    generatedmessagelite.generatedextension<?, ?>>
      extensionsbynumber;

  private extensionregistrylite(boolean empty) {
    this.extensionsbynumber = collections.emptymap();
  }
  private static final extensionregistrylite empty =
    new extensionregistrylite(true);

  /** a (object, int) pair, used as a map key. */
  private static final class objectintpair {
    private final object object;
    private final int number;

    objectintpair(final object object, final int number) {
      this.object = object;
      this.number = number;
    }

    @override
    public int hashcode() {
      return system.identityhashcode(object) * ((1 << 16) - 1) + number;
    }
    @override
    public boolean equals(final object obj) {
      if (!(obj instanceof objectintpair)) {
        return false;
      }
      final objectintpair other = (objectintpair)obj;
      return object == other.object && number == other.number;
    }
  }
}
