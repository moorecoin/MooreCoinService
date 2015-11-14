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
 * grab-bag of utility functions useful when dealing with rpcs.
 *
 * @author kenton@google.com kenton varda
 */
public final class rpcutil {
  private rpcutil() {}

  /**
   * take an {@code rpccallback<message>} and convert it to an
   * {@code rpccallback} accepting a specific message type.  this is always
   * type-safe (parameter type contravariance).
   */
  @suppresswarnings("unchecked")
  public static <type extends message> rpccallback<type>
  specializecallback(final rpccallback<message> originalcallback) {
    return (rpccallback<type>)originalcallback;
    // the above cast works, but only due to technical details of the java
    // implementation.  a more theoretically correct -- but less efficient --
    // implementation would be as follows:
    //   return new rpccallback<type>() {
    //     public void run(type parameter) {
    //       originalcallback.run(parameter);
    //     }
    //   };
  }

  /**
   * take an {@code rpccallback} accepting a specific message type and convert
   * it to an {@code rpccallback<message>}.  the generalized callback will
   * accept any message object which has the same descriptor, and will convert
   * it to the correct class before calling the original callback.  however,
   * if the generalized callback is given a message with a different descriptor,
   * an exception will be thrown.
   */
  public static <type extends message>
  rpccallback<message> generalizecallback(
      final rpccallback<type> originalcallback,
      final class<type> originalclass,
      final type defaultinstance) {
    return new rpccallback<message>() {
      public void run(final message parameter) {
        type typedparameter;
        try {
          typedparameter = originalclass.cast(parameter);
        } catch (classcastexception ignored) {
          typedparameter = copyastype(defaultinstance, parameter);
        }
        originalcallback.run(typedparameter);
      }
    };
  }

  /**
   * creates a new message of type "type" which is a copy of "source".  "source"
   * must have the same descriptor but may be a different class (e.g.
   * dynamicmessage).
   */
  @suppresswarnings("unchecked")
  private static <type extends message> type copyastype(
      final type typedefaultinstance, final message source) {
    return (type)typedefaultinstance.newbuilderfortype()
                                    .mergefrom(source)
                                    .build();
  }

  /**
   * creates a callback which can only be called once.  this may be useful for
   * security, when passing a callback to untrusted code:  most callbacks do
   * not expect to be called more than once, so doing so may expose bugs if it
   * is not prevented.
   */
  public static <parametertype>
    rpccallback<parametertype> newonetimecallback(
      final rpccallback<parametertype> originalcallback) {
    return new rpccallback<parametertype>() {
      private boolean alreadycalled = false;

      public void run(final parametertype parameter) {
        synchronized(this) {
          if (alreadycalled) {
            throw new alreadycalledexception();
          }
          alreadycalled = true;
        }

        originalcallback.run(parameter);
      }
    };
  }

  /**
   * exception thrown when a one-time callback is called more than once.
   */
  public static final class alreadycalledexception extends runtimeexception {
    private static final long serialversionuid = 5469741279507848266l;

    public alreadycalledexception() {
      super("this rpccallback was already called and cannot be called " +
            "multiple times.");
    }
  }
}
