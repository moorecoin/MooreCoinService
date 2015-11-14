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
import java.util.iterator;
import java.util.map.entry;

/**
 * lazyfield encapsulates the logic of lazily parsing message fields. it stores
 * the message in a bytestring initially and then parse it on-demand.
 *
 * lazyfield is thread-compatible e.g. concurrent read are safe, however,
 * synchronizations are needed under read/write situations.
 *
 * now lazyfield is only used to lazily load messageset.
 * todo(xiangl): use lazyfield to lazily load all messages.
 *
 * @author xiangl@google.com (xiang li)
 */
class lazyfield {

  final private messagelite defaultinstance;
  final private extensionregistrylite extensionregistry;

  // mutable because it is initialized lazily.
  private bytestring bytes;
  private volatile messagelite value;
  private volatile boolean isdirty = false;

  public lazyfield(messagelite defaultinstance,
      extensionregistrylite extensionregistry, bytestring bytes) {
    this.defaultinstance = defaultinstance;
    this.extensionregistry = extensionregistry;
    this.bytes = bytes;
  }

  public messagelite getvalue() {
    ensureinitialized();
    return value;
  }

  /**
   * lazyfield is not thread-safe for write access. synchronizations are needed
   * under read/write situations.
   */
  public messagelite setvalue(messagelite value) {
    messagelite originalvalue = this.value;
    this.value = value;
    bytes = null;
    isdirty = true;
    return originalvalue;
  }

  /**
   * due to the optional field can be duplicated at the end of serialized
   * bytes, which will make the serialized size changed after lazyfield
   * parsed. be careful when using this method.
   */
  public int getserializedsize() {
    if (isdirty) {
      return value.getserializedsize();
    }
    return bytes.size();
  }

  public bytestring tobytestring() {
    if (!isdirty) {
      return bytes;
    }
    synchronized (this) {
      if (!isdirty) {
        return bytes;
      }
      bytes = value.tobytestring();
      isdirty = false;
      return bytes;
    }
  }

  @override
  public int hashcode() {
    ensureinitialized();
    return value.hashcode();
  }

  @override
  public boolean equals(object obj) {
    ensureinitialized();
    return value.equals(obj);
  }

  @override
  public string tostring() {
    ensureinitialized();
    return value.tostring();
  }

  private void ensureinitialized() {
    if (value != null) {
      return;
    }
    synchronized (this) {
      if (value != null) {
        return;
      }
      try {
        if (bytes != null) {
          value = defaultinstance.getparserfortype()
              .parsefrom(bytes, extensionregistry);
        }
      } catch (ioexception e) {
        // todo(xiangl): refactory the api to support the exception thrown from
        // lazily load messages.
      }
    }
  }

  // ====================================================

  /**
   * lazyentry and lazyiterator are used to encapsulate the lazyfield, when
   * users iterate all fields from fieldset.
   */
  static class lazyentry<k> implements entry<k, object> {
    private entry<k, lazyfield> entry;

    private lazyentry(entry<k, lazyfield> entry) {
      this.entry = entry;
    }

    public k getkey() {
      return entry.getkey();
    }

    public object getvalue() {
      lazyfield field = entry.getvalue();
      if (field == null) {
        return null;
      }
      return field.getvalue();
    }

    public lazyfield getfield() {
      return entry.getvalue();
    }

    public object setvalue(object value) {
      if (!(value instanceof messagelite)) {
        throw new illegalargumentexception(
            "lazyfield now only used for messageset, "
            + "and the value of messageset must be an instance of messagelite");
      }
      return entry.getvalue().setvalue((messagelite) value);
    }
  }

  static class lazyiterator<k> implements iterator<entry<k, object>> {
    private iterator<entry<k, object>> iterator;

    public lazyiterator(iterator<entry<k, object>> iterator) {
      this.iterator = iterator;
    }

    public boolean hasnext() {
      return iterator.hasnext();
    }

    @suppresswarnings("unchecked")
    public entry<k, object> next() {
      entry<k, ?> entry = iterator.next();
      if (entry.getvalue() instanceof lazyfield) {
        return new lazyentry<k>((entry<k, lazyfield>) entry);
      }
      return (entry<k, object>) entry;
    }

    public void remove() {
      iterator.remove();
    }
  }
}
