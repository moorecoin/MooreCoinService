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

import java.io.unsupportedencodingexception;

/**
 * the classes contained within are used internally by the protocol buffer
 * library and generated message implementations. they are public only because
 * those generated messages do not reside in the {@code protobuf} package.
 * others should not use this class directly.
 *
 * @author kenton@google.com (kenton varda)
 */
public class internal {
  /**
   * helper called by generated code to construct default values for string
   * fields.
   * <p>
   * the protocol compiler does not actually contain a utf-8 decoder -- it
   * just pushes utf-8-encoded text around without touching it.  the one place
   * where this presents a problem is when generating java string literals.
   * unicode characters in the string literal would normally need to be encoded
   * using a unicode escape sequence, which would require decoding them.
   * to get around this, protoc instead embeds the utf-8 bytes into the
   * generated code and leaves it to the runtime library to decode them.
   * <p>
   * it gets worse, though.  if protoc just generated a byte array, like:
   *   new byte[] {0x12, 0x34, 0x56, 0x78}
   * java actually generates *code* which allocates an array and then fills
   * in each value.  this is much less efficient than just embedding the bytes
   * directly into the bytecode.  to get around this, we need another
   * work-around.  string literals are embedded directly, so protoc actually
   * generates a string literal corresponding to the bytes.  the easiest way
   * to do this is to use the iso-8859-1 character set, which corresponds to
   * the first 256 characters of the unicode range.  protoc can then use
   * good old cescape to generate the string.
   * <p>
   * so we have a string literal which represents a set of bytes which
   * represents another string.  this function -- stringdefaultvalue --
   * converts from the generated string to the string we actually want.  the
   * generated code calls this automatically.
   */
  public static string stringdefaultvalue(string bytes) {
    try {
      return new string(bytes.getbytes("iso-8859-1"), "utf-8");
    } catch (unsupportedencodingexception e) {
      // this should never happen since all jvms are required to implement
      // both of the above character sets.
      throw new illegalstateexception(
          "java vm does not support a standard character set.", e);
    }
  }

  /**
   * helper called by generated code to construct default values for bytes
   * fields.
   * <p>
   * this is a lot like {@link #stringdefaultvalue}, but for bytes fields.
   * in this case we only need the second of the two hacks -- allowing us to
   * embed raw bytes as a string literal with iso-8859-1 encoding.
   */
  public static bytestring bytesdefaultvalue(string bytes) {
    try {
      return bytestring.copyfrom(bytes.getbytes("iso-8859-1"));
    } catch (unsupportedencodingexception e) {
      // this should never happen since all jvms are required to implement
      // iso-8859-1.
      throw new illegalstateexception(
          "java vm does not support a standard character set.", e);
    }
  }

  /**
   * helper called by generated code to determine if a byte array is a valid
   * utf-8 encoded string such that the original bytes can be converted to
   * a string object and then back to a byte array round tripping the bytes
   * without loss.  more precisely, returns {@code true} whenever:
   * <pre>   {@code
   * arrays.equals(bytestring.tobytearray(),
   *     new string(bytestring.tobytearray(), "utf-8").getbytes("utf-8"))
   * }</pre>
   *
   * <p>this method rejects "overlong" byte sequences, as well as
   * 3-byte sequences that would map to a surrogate character, in
   * accordance with the restricted definition of utf-8 introduced in
   * unicode 3.1.  note that the utf-8 decoder included in oracle's
   * jdk has been modified to also reject "overlong" byte sequences,
   * but currently (2011) still accepts 3-byte surrogate character
   * byte sequences.
   *
   * <p>see the unicode standard,</br>
   * table 3-6. <em>utf-8 bit distribution</em>,</br>
   * table 3-7. <em>well formed utf-8 byte sequences</em>.
   *
   * <p>as of 2011-02, this method simply returns the result of {@link
   * bytestring#isvalidutf8()}.  calling that method directly is preferred.
   *
   * @param bytestring the string to check
   * @return whether the byte array is round trippable
   */
  public static boolean isvalidutf8(bytestring bytestring) {
    return bytestring.isvalidutf8();
  }

  /**
   * interface for an enum value or value descriptor, to be used in fieldset.
   * the lite library stores enum values directly in fieldsets but the full
   * library stores enumvaluedescriptors in order to better support reflection.
   */
  public interface enumlite {
    int getnumber();
  }

  /**
   * interface for an object which maps integers to {@link enumlite}s.
   * {@link descriptors.enumdescriptor} implements this interface by mapping
   * numbers to {@link descriptors.enumvaluedescriptor}s.  additionally,
   * every generated enum type has a static method internalgetvaluemap() which
   * returns an implementation of this type that maps numbers to enum values.
   */
  public interface enumlitemap<t extends enumlite> {
    t findvaluebynumber(int number);
  }
}
