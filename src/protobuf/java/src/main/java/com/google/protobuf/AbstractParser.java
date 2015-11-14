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

import com.google.protobuf.abstractmessagelite.builder.limitedinputstream;

import java.io.ioexception;
import java.io.inputstream;

/**
 * a partial implementation of the {@link parser} interface which implements
 * as many methods of that interface as possible in terms of other methods.
 *
 * note: this class implements all the convenience methods in the
 * {@link parser} interface. see {@link parser} for related javadocs.
 * subclasses need to implement
 * {@link parser#parsepartialfrom(codedinputstream, extensionregistrylite)}
 *
 * @author liujisi@google.com (pherl liu)
 */
public abstract class abstractparser<messagetype extends messagelite>
    implements parser<messagetype> {
  /**
   * creates an uninitializedmessageexception for messagetype.
   */
  private uninitializedmessageexception
      newuninitializedmessageexception(messagetype message) {
    if (message instanceof abstractmessagelite) {
      return ((abstractmessagelite) message).newuninitializedmessageexception();
    }
    return new uninitializedmessageexception(message);
  }

  /**
   * helper method to check if message is initialized.
   *
   * @throws invalidprotocolbufferexception if it is not initialized.
   * @return the message to check.
   */
  private messagetype checkmessageinitialized(messagetype message)
      throws invalidprotocolbufferexception {
    if (message != null && !message.isinitialized()) {
      throw newuninitializedmessageexception(message)
          .asinvalidprotocolbufferexception()
          .setunfinishedmessage(message);
    }
    return message;
  }

  private static final extensionregistrylite empty_registry
      = extensionregistrylite.getemptyregistry();

  public messagetype parsepartialfrom(codedinputstream input)
      throws invalidprotocolbufferexception {
    return parsepartialfrom(input, empty_registry);
  }

  public messagetype parsefrom(codedinputstream input,
                               extensionregistrylite extensionregistry)
      throws invalidprotocolbufferexception {
    return checkmessageinitialized(
        parsepartialfrom(input, extensionregistry));
  }

  public messagetype parsefrom(codedinputstream input)
      throws invalidprotocolbufferexception {
    return parsefrom(input, empty_registry);
  }

  public messagetype parsepartialfrom(bytestring data,
                                      extensionregistrylite extensionregistry)
    throws invalidprotocolbufferexception {
    messagetype message;
    try {
      codedinputstream input = data.newcodedinput();
      message = parsepartialfrom(input, extensionregistry);
      try {
        input.checklasttagwas(0);
      } catch (invalidprotocolbufferexception e) {
        throw e.setunfinishedmessage(message);
      }
      return message;
    } catch (invalidprotocolbufferexception e) {
      throw e;
    } catch (ioexception e) {
      throw new runtimeexception(
          "reading from a bytestring threw an ioexception (should " +
          "never happen).", e);
    }
  }

  public messagetype parsepartialfrom(bytestring data)
      throws invalidprotocolbufferexception {
    return parsepartialfrom(data, empty_registry);
  }

  public messagetype parsefrom(bytestring data,
                               extensionregistrylite extensionregistry)
      throws invalidprotocolbufferexception {
    return checkmessageinitialized(parsepartialfrom(data, extensionregistry));
  }

  public messagetype parsefrom(bytestring data)
      throws invalidprotocolbufferexception {
    return parsefrom(data, empty_registry);
  }

  public messagetype parsepartialfrom(byte[] data, int off, int len,
                                      extensionregistrylite extensionregistry)
      throws invalidprotocolbufferexception {
    try {
      codedinputstream input = codedinputstream.newinstance(data, off, len);
      messagetype message = parsepartialfrom(input, extensionregistry);
      try {
        input.checklasttagwas(0);
      } catch (invalidprotocolbufferexception e) {
        throw e.setunfinishedmessage(message);
      }
      return message;
    } catch (invalidprotocolbufferexception e) {
      throw e;
    } catch (ioexception e) {
      throw new runtimeexception(
          "reading from a byte array threw an ioexception (should " +
          "never happen).", e);
    }
  }

  public messagetype parsepartialfrom(byte[] data, int off, int len)
      throws invalidprotocolbufferexception {
    return parsepartialfrom(data, off, len, empty_registry);
  }

  public messagetype parsepartialfrom(byte[] data,
                                      extensionregistrylite extensionregistry)
      throws invalidprotocolbufferexception {
    return parsepartialfrom(data, 0, data.length, extensionregistry);
  }

  public messagetype parsepartialfrom(byte[] data)
      throws invalidprotocolbufferexception {
    return parsepartialfrom(data, 0, data.length, empty_registry);
  }

  public messagetype parsefrom(byte[] data, int off, int len,
                               extensionregistrylite extensionregistry)
      throws invalidprotocolbufferexception {
    return checkmessageinitialized(
        parsepartialfrom(data, off, len, extensionregistry));
  }

  public messagetype parsefrom(byte[] data, int off, int len)
      throws invalidprotocolbufferexception {
    return parsefrom(data, off, len, empty_registry);
  }

  public messagetype parsefrom(byte[] data,
                               extensionregistrylite extensionregistry)
      throws invalidprotocolbufferexception {
    return parsefrom(data, 0, data.length, extensionregistry);
  }

  public messagetype parsefrom(byte[] data)
      throws invalidprotocolbufferexception {
    return parsefrom(data, empty_registry);
  }

  public messagetype parsepartialfrom(inputstream input,
                                      extensionregistrylite extensionregistry)
      throws invalidprotocolbufferexception {
    codedinputstream codedinput = codedinputstream.newinstance(input);
    messagetype message = parsepartialfrom(codedinput, extensionregistry);
    try {
      codedinput.checklasttagwas(0);
    } catch (invalidprotocolbufferexception e) {
      throw e.setunfinishedmessage(message);
    }
    return message;
  }

  public messagetype parsepartialfrom(inputstream input)
      throws invalidprotocolbufferexception {
    return parsepartialfrom(input, empty_registry);
  }

  public messagetype parsefrom(inputstream input,
                               extensionregistrylite extensionregistry)
      throws invalidprotocolbufferexception {
    return checkmessageinitialized(
        parsepartialfrom(input, extensionregistry));
  }

  public messagetype parsefrom(inputstream input)
      throws invalidprotocolbufferexception {
    return parsefrom(input, empty_registry);
  }

  public messagetype parsepartialdelimitedfrom(
      inputstream input,
      extensionregistrylite extensionregistry)
      throws invalidprotocolbufferexception {
    int size;
    try {
      int firstbyte = input.read();
      if (firstbyte == -1) {
        return null;
      }
      size = codedinputstream.readrawvarint32(firstbyte, input);
    } catch (ioexception e) {
      throw new invalidprotocolbufferexception(e.getmessage());
    }
    inputstream limitedinput = new limitedinputstream(input, size);
    return parsepartialfrom(limitedinput, extensionregistry);
  }

  public messagetype parsepartialdelimitedfrom(inputstream input)
      throws invalidprotocolbufferexception {
    return parsepartialdelimitedfrom(input, empty_registry);
  }

  public messagetype parsedelimitedfrom(
      inputstream input,
      extensionregistrylite extensionregistry)
      throws invalidprotocolbufferexception {
    return checkmessageinitialized(
        parsepartialdelimitedfrom(input, extensionregistry));
  }

  public messagetype parsedelimitedfrom(inputstream input)
      throws invalidprotocolbufferexception {
    return parsedelimitedfrom(input, empty_registry);
  }
}
