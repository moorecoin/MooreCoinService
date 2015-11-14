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

import java.io.filterinputstream;
import java.io.inputstream;
import java.io.ioexception;
import java.io.outputstream;
import java.util.collection;

/**
 * a partial implementation of the {@link messagelite} interface which
 * implements as many methods of that interface as possible in terms of other
 * methods.
 *
 * @author kenton@google.com kenton varda
 */
public abstract class abstractmessagelite implements messagelite {
  public bytestring tobytestring() {
    try {
      final bytestring.codedbuilder out =
        bytestring.newcodedbuilder(getserializedsize());
      writeto(out.getcodedoutput());
      return out.build();
    } catch (ioexception e) {
      throw new runtimeexception(
        "serializing to a bytestring threw an ioexception (should " +
        "never happen).", e);
    }
  }

  public byte[] tobytearray() {
    try {
      final byte[] result = new byte[getserializedsize()];
      final codedoutputstream output = codedoutputstream.newinstance(result);
      writeto(output);
      output.checknospaceleft();
      return result;
    } catch (ioexception e) {
      throw new runtimeexception(
        "serializing to a byte array threw an ioexception " +
        "(should never happen).", e);
    }
  }

  public void writeto(final outputstream output) throws ioexception {
    final int buffersize =
        codedoutputstream.computepreferredbuffersize(getserializedsize());
    final codedoutputstream codedoutput =
        codedoutputstream.newinstance(output, buffersize);
    writeto(codedoutput);
    codedoutput.flush();
  }

  public void writedelimitedto(final outputstream output) throws ioexception {
    final int serialized = getserializedsize();
    final int buffersize = codedoutputstream.computepreferredbuffersize(
        codedoutputstream.computerawvarint32size(serialized) + serialized);
    final codedoutputstream codedoutput =
        codedoutputstream.newinstance(output, buffersize);
    codedoutput.writerawvarint32(serialized);
    writeto(codedoutput);
    codedoutput.flush();
  }

  /**
   * package private helper method for abstractparser to create
   * uninitializedmessageexception.
   */
  uninitializedmessageexception newuninitializedmessageexception() {
    return new uninitializedmessageexception(this);
  }

  /**
   * a partial implementation of the {@link message.builder} interface which
   * implements as many methods of that interface as possible in terms of
   * other methods.
   */
  @suppresswarnings("unchecked")
  public static abstract class builder<buildertype extends builder>
      implements messagelite.builder {
    // the compiler produces an error if this is not declared explicitly.
    @override
    public abstract buildertype clone();

    public buildertype mergefrom(final codedinputstream input)
                                 throws ioexception {
      return mergefrom(input, extensionregistrylite.getemptyregistry());
    }

    // re-defined here for return type covariance.
    public abstract buildertype mergefrom(
        final codedinputstream input,
        final extensionregistrylite extensionregistry)
        throws ioexception;

    public buildertype mergefrom(final bytestring data)
        throws invalidprotocolbufferexception {
      try {
        final codedinputstream input = data.newcodedinput();
        mergefrom(input);
        input.checklasttagwas(0);
        return (buildertype) this;
      } catch (invalidprotocolbufferexception e) {
        throw e;
      } catch (ioexception e) {
        throw new runtimeexception(
          "reading from a bytestring threw an ioexception (should " +
          "never happen).", e);
      }
    }

    public buildertype mergefrom(
        final bytestring data,
        final extensionregistrylite extensionregistry)
        throws invalidprotocolbufferexception {
      try {
        final codedinputstream input = data.newcodedinput();
        mergefrom(input, extensionregistry);
        input.checklasttagwas(0);
        return (buildertype) this;
      } catch (invalidprotocolbufferexception e) {
        throw e;
      } catch (ioexception e) {
        throw new runtimeexception(
          "reading from a bytestring threw an ioexception (should " +
          "never happen).", e);
      }
    }

    public buildertype mergefrom(final byte[] data)
        throws invalidprotocolbufferexception {
      return mergefrom(data, 0, data.length);
    }

    public buildertype mergefrom(final byte[] data, final int off,
                                 final int len)
                                 throws invalidprotocolbufferexception {
      try {
        final codedinputstream input =
            codedinputstream.newinstance(data, off, len);
        mergefrom(input);
        input.checklasttagwas(0);
        return (buildertype) this;
      } catch (invalidprotocolbufferexception e) {
        throw e;
      } catch (ioexception e) {
        throw new runtimeexception(
          "reading from a byte array threw an ioexception (should " +
          "never happen).", e);
      }
    }

    public buildertype mergefrom(
        final byte[] data,
        final extensionregistrylite extensionregistry)
        throws invalidprotocolbufferexception {
      return mergefrom(data, 0, data.length, extensionregistry);
    }

    public buildertype mergefrom(
        final byte[] data, final int off, final int len,
        final extensionregistrylite extensionregistry)
        throws invalidprotocolbufferexception {
      try {
        final codedinputstream input =
            codedinputstream.newinstance(data, off, len);
        mergefrom(input, extensionregistry);
        input.checklasttagwas(0);
        return (buildertype) this;
      } catch (invalidprotocolbufferexception e) {
        throw e;
      } catch (ioexception e) {
        throw new runtimeexception(
          "reading from a byte array threw an ioexception (should " +
          "never happen).", e);
      }
    }

    public buildertype mergefrom(final inputstream input) throws ioexception {
      final codedinputstream codedinput = codedinputstream.newinstance(input);
      mergefrom(codedinput);
      codedinput.checklasttagwas(0);
      return (buildertype) this;
    }

    public buildertype mergefrom(
        final inputstream input,
        final extensionregistrylite extensionregistry)
        throws ioexception {
      final codedinputstream codedinput = codedinputstream.newinstance(input);
      mergefrom(codedinput, extensionregistry);
      codedinput.checklasttagwas(0);
      return (buildertype) this;
    }

    /**
     * an inputstream implementations which reads from some other inputstream
     * but is limited to a particular number of bytes.  used by
     * mergedelimitedfrom().  this is intentionally package-private so that
     * unknownfieldset can share it.
     */
    static final class limitedinputstream extends filterinputstream {
      private int limit;

      limitedinputstream(inputstream in, int limit) {
        super(in);
        this.limit = limit;
      }

      @override
      public int available() throws ioexception {
        return math.min(super.available(), limit);
      }

      @override
      public int read() throws ioexception {
        if (limit <= 0) {
          return -1;
        }
        final int result = super.read();
        if (result >= 0) {
          --limit;
        }
        return result;
      }

      @override
      public int read(final byte[] b, final int off, int len)
                      throws ioexception {
        if (limit <= 0) {
          return -1;
        }
        len = math.min(len, limit);
        final int result = super.read(b, off, len);
        if (result >= 0) {
          limit -= result;
        }
        return result;
      }

      @override
      public long skip(final long n) throws ioexception {
        final long result = super.skip(math.min(n, limit));
        if (result >= 0) {
          limit -= result;
        }
        return result;
      }
    }

    public boolean mergedelimitedfrom(
        final inputstream input,
        final extensionregistrylite extensionregistry)
        throws ioexception {
      final int firstbyte = input.read();
      if (firstbyte == -1) {
        return false;
      }
      final int size = codedinputstream.readrawvarint32(firstbyte, input);
      final inputstream limitedinput = new limitedinputstream(input, size);
      mergefrom(limitedinput, extensionregistry);
      return true;
    }

    public boolean mergedelimitedfrom(final inputstream input)
        throws ioexception {
      return mergedelimitedfrom(input,
          extensionregistrylite.getemptyregistry());
    }

    /**
     * construct an uninitializedmessageexception reporting missing fields in
     * the given message.
     */
    protected static uninitializedmessageexception
        newuninitializedmessageexception(messagelite message) {
      return new uninitializedmessageexception(message);
    }

    /**
     * adds the {@code values} to the {@code list}.  this is a helper method
     * used by generated code.  users should ignore it.
     *
     * @throws nullpointerexception if any of the elements of {@code values} is
     * null.
     */
    protected static <t> void addall(final iterable<t> values,
                                     final collection<? super t> list) {
      if (values instanceof lazystringlist) {
        // for stringorbytestringlists, check the underlying elements to avoid
        // forcing conversions of bytestrings to strings.
        checkfornullvalues(((lazystringlist) values).getunderlyingelements());
      } else {
        checkfornullvalues(values);
      }
      if (values instanceof collection) {
        final collection<t> collection = (collection<t>) values;
        list.addall(collection);
      } else {
        for (final t value : values) {
          list.add(value);
        }
      }
    }

    private static void checkfornullvalues(final iterable<?> values) {
      for (final object value : values) {
        if (value == null) {
          throw new nullpointerexception();
        }
      }
    }
  }
}
