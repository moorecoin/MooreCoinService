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
import com.google.protobuf.descriptors.enumdescriptor;
import com.google.protobuf.descriptors.enumvaluedescriptor;

import java.io.ioexception;
import java.nio.charbuffer;
import java.math.biginteger;
import java.util.arraylist;
import java.util.list;
import java.util.locale;
import java.util.map;
import java.util.regex.matcher;
import java.util.regex.pattern;

/**
 * provide text parsing and formatting support for proto2 instances.
 * the implementation largely follows google/protobuf/text_format.cc.
 *
 * @author wenboz@google.com wenbo zhu
 * @author kenton@google.com kenton varda
 */
public final class textformat {
  private textformat() {}

  private static final printer default_printer = new printer();
  private static final printer single_line_printer =
      (new printer()).setsinglelinemode(true);
  private static final printer unicode_printer =
      (new printer()).setescapenonascii(false);

  /**
   * outputs a textual representation of the protocol message supplied into
   * the parameter output. (this representation is the new version of the
   * classic "protocolprinter" output from the original protocol buffer system)
   */
  public static void print(final messageorbuilder message, final appendable output)
                           throws ioexception {
    default_printer.print(message, new textgenerator(output));
  }

  /** outputs a textual representation of {@code fields} to {@code output}. */
  public static void print(final unknownfieldset fields,
                           final appendable output)
                           throws ioexception {
    default_printer.printunknownfields(fields, new textgenerator(output));
  }

  /**
   * generates a human readable form of this message, useful for debugging and
   * other purposes, with no newline characters.
   */
  public static string shortdebugstring(final messageorbuilder message) {
    try {
      final stringbuilder sb = new stringbuilder();
      single_line_printer.print(message, new textgenerator(sb));
      // single line mode currently might have an extra space at the end.
      return sb.tostring().trim();
    } catch (ioexception e) {
      throw new illegalstateexception(e);
    }
  }

  /**
   * generates a human readable form of the unknown fields, useful for debugging
   * and other purposes, with no newline characters.
   */
  public static string shortdebugstring(final unknownfieldset fields) {
    try {
      final stringbuilder sb = new stringbuilder();
      single_line_printer.printunknownfields(fields, new textgenerator(sb));
      // single line mode currently might have an extra space at the end.
      return sb.tostring().trim();
    } catch (ioexception e) {
      throw new illegalstateexception(e);
    }
  }

  /**
   * like {@code print()}, but writes directly to a {@code string} and
   * returns it.
   */
  public static string printtostring(final messageorbuilder message) {
    try {
      final stringbuilder text = new stringbuilder();
      print(message, text);
      return text.tostring();
    } catch (ioexception e) {
      throw new illegalstateexception(e);
    }
  }

  /**
   * like {@code print()}, but writes directly to a {@code string} and
   * returns it.
   */
  public static string printtostring(final unknownfieldset fields) {
    try {
      final stringbuilder text = new stringbuilder();
      print(fields, text);
      return text.tostring();
    } catch (ioexception e) {
      throw new illegalstateexception(e);
    }
  }

  /**
   * same as {@code printtostring()}, except that non-ascii characters
   * in string type fields are not escaped in backslash+octals.
   */
  public static string printtounicodestring(final messageorbuilder message) {
    try {
      final stringbuilder text = new stringbuilder();
      unicode_printer.print(message, new textgenerator(text));
      return text.tostring();
    } catch (ioexception e) {
      throw new illegalstateexception(e);
    }
  }

  /**
   * same as {@code printtostring()}, except that non-ascii characters
   * in string type fields are not escaped in backslash+octals.
   */
  public static string printtounicodestring(final unknownfieldset fields) {
    try {
      final stringbuilder text = new stringbuilder();
      unicode_printer.printunknownfields(fields, new textgenerator(text));
      return text.tostring();
    } catch (ioexception e) {
      throw new illegalstateexception(e);
    }
  }

  public static void printfield(final fielddescriptor field,
                                final object value,
                                final appendable output)
                                throws ioexception {
    default_printer.printfield(field, value, new textgenerator(output));
  }

  public static string printfieldtostring(final fielddescriptor field,
                                          final object value) {
    try {
      final stringbuilder text = new stringbuilder();
      printfield(field, value, text);
      return text.tostring();
    } catch (ioexception e) {
      throw new illegalstateexception(e);
    }
  }

  /**
   * outputs a textual representation of the value of given field value.
   *
   * @param field the descriptor of the field
   * @param value the value of the field
   * @param output the output to which to append the formatted value
   * @throws classcastexception if the value is not appropriate for the
   *     given field descriptor
   * @throws ioexception if there is an exception writing to the output
   */
  public static void printfieldvalue(final fielddescriptor field,
                                     final object value,
                                     final appendable output)
                                     throws ioexception {
    default_printer.printfieldvalue(field, value, new textgenerator(output));
  }

  /**
   * outputs a textual representation of the value of an unknown field.
   *
   * @param tag the field's tag number
   * @param value the value of the field
   * @param output the output to which to append the formatted value
   * @throws classcastexception if the value is not appropriate for the
   *     given field descriptor
   * @throws ioexception if there is an exception writing to the output
   */
  public static void printunknownfieldvalue(final int tag,
                                            final object value,
                                            final appendable output)
                                            throws ioexception {
    printunknownfieldvalue(tag, value, new textgenerator(output));
  }

  private static void printunknownfieldvalue(final int tag,
                                             final object value,
                                             final textgenerator generator)
                                             throws ioexception {
    switch (wireformat.gettagwiretype(tag)) {
      case wireformat.wiretype_varint:
        generator.print(unsignedtostring((long) value));
        break;
      case wireformat.wiretype_fixed32:
        generator.print(
            string.format((locale) null, "0x%08x", (integer) value));
        break;
      case wireformat.wiretype_fixed64:
        generator.print(string.format((locale) null, "0x%016x", (long) value));
        break;
      case wireformat.wiretype_length_delimited:
        generator.print("\"");
        generator.print(escapebytes((bytestring) value));
        generator.print("\"");
        break;
      case wireformat.wiretype_start_group:
        default_printer.printunknownfields((unknownfieldset) value, generator);
        break;
      default:
        throw new illegalargumentexception("bad tag: " + tag);
    }
  }

  /** helper class for converting protobufs to text. */
  private static final class printer {
    /** whether to omit newlines from the output. */
    boolean singlelinemode = false;

    /** whether to escape non ascii characters with backslash and octal. */
    boolean escapenonascii = true;

    private printer() {}

    /** setter of singlelinemode */
    private printer setsinglelinemode(boolean singlelinemode) {
      this.singlelinemode = singlelinemode;
      return this;
    }

    /** setter of escapenonascii */
    private printer setescapenonascii(boolean escapenonascii) {
      this.escapenonascii = escapenonascii;
      return this;
    }

    private void print(final messageorbuilder message, final textgenerator generator)
        throws ioexception {
      for (map.entry<fielddescriptor, object> field
          : message.getallfields().entryset()) {
        printfield(field.getkey(), field.getvalue(), generator);
      }
      printunknownfields(message.getunknownfields(), generator);
    }

    private void printfield(final fielddescriptor field, final object value,
        final textgenerator generator) throws ioexception {
      if (field.isrepeated()) {
        // repeated field.  print each element.
        for (object element : (list<?>) value) {
          printsinglefield(field, element, generator);
        }
      } else {
        printsinglefield(field, value, generator);
      }
    }

    private void printsinglefield(final fielddescriptor field,
                                  final object value,
                                  final textgenerator generator)
                                  throws ioexception {
      if (field.isextension()) {
        generator.print("[");
        // we special-case messageset elements for compatibility with proto1.
        if (field.getcontainingtype().getoptions().getmessagesetwireformat()
            && (field.gettype() == fielddescriptor.type.message)
            && (field.isoptional())
            // object equality
            && (field.getextensionscope() == field.getmessagetype())) {
          generator.print(field.getmessagetype().getfullname());
        } else {
          generator.print(field.getfullname());
        }
        generator.print("]");
      } else {
        if (field.gettype() == fielddescriptor.type.group) {
          // groups must be serialized with their original capitalization.
          generator.print(field.getmessagetype().getname());
        } else {
          generator.print(field.getname());
        }
      }

      if (field.getjavatype() == fielddescriptor.javatype.message) {
        if (singlelinemode) {
          generator.print(" { ");
        } else {
          generator.print(" {\n");
          generator.indent();
        }
      } else {
        generator.print(": ");
      }

      printfieldvalue(field, value, generator);

      if (field.getjavatype() == fielddescriptor.javatype.message) {
        if (singlelinemode) {
          generator.print("} ");
        } else {
          generator.outdent();
          generator.print("}\n");
        }
      } else {
        if (singlelinemode) {
          generator.print(" ");
        } else {
          generator.print("\n");
        }
      }
    }

    private void printfieldvalue(final fielddescriptor field,
                                 final object value,
                                 final textgenerator generator)
                                 throws ioexception {
      switch (field.gettype()) {
        case int32:
        case sint32:
        case sfixed32:
          generator.print(((integer) value).tostring());
          break;

        case int64:
        case sint64:
        case sfixed64:
          generator.print(((long) value).tostring());
          break;

        case bool:
          generator.print(((boolean) value).tostring());
          break;

        case float:
          generator.print(((float) value).tostring());
          break;

        case double:
          generator.print(((double) value).tostring());
          break;

        case uint32:
        case fixed32:
          generator.print(unsignedtostring((integer) value));
          break;

        case uint64:
        case fixed64:
          generator.print(unsignedtostring((long) value));
          break;

        case string:
          generator.print("\"");
          generator.print(escapenonascii ?
              escapetext((string) value) :
              (string) value);
          generator.print("\"");
          break;

        case bytes:
          generator.print("\"");
          generator.print(escapebytes((bytestring) value));
          generator.print("\"");
          break;

        case enum:
          generator.print(((enumvaluedescriptor) value).getname());
          break;

        case message:
        case group:
          print((message) value, generator);
          break;
      }
    }

    private void printunknownfields(final unknownfieldset unknownfields,
                                    final textgenerator generator)
                                    throws ioexception {
      for (map.entry<integer, unknownfieldset.field> entry :
               unknownfields.asmap().entryset()) {
        final int number = entry.getkey();
        final unknownfieldset.field field = entry.getvalue();
        printunknownfield(number, wireformat.wiretype_varint,
            field.getvarintlist(), generator);
        printunknownfield(number, wireformat.wiretype_fixed32,
            field.getfixed32list(), generator);
        printunknownfield(number, wireformat.wiretype_fixed64,
            field.getfixed64list(), generator);
        printunknownfield(number, wireformat.wiretype_length_delimited,
            field.getlengthdelimitedlist(), generator);
        for (final unknownfieldset value : field.getgrouplist()) {
          generator.print(entry.getkey().tostring());
          if (singlelinemode) {
            generator.print(" { ");
          } else {
            generator.print(" {\n");
            generator.indent();
          }
          printunknownfields(value, generator);
          if (singlelinemode) {
            generator.print("} ");
          } else {
            generator.outdent();
            generator.print("}\n");
          }
        }
      }
    }

    private void printunknownfield(final int number,
                                   final int wiretype,
                                   final list<?> values,
                                   final textgenerator generator)
                                   throws ioexception {
      for (final object value : values) {
        generator.print(string.valueof(number));
        generator.print(": ");
        printunknownfieldvalue(wiretype, value, generator);
        generator.print(singlelinemode ? " " : "\n");
      }
    }
  }

  /** convert an unsigned 32-bit integer to a string. */
  private static string unsignedtostring(final int value) {
    if (value >= 0) {
      return integer.tostring(value);
    } else {
      return long.tostring(((long) value) & 0x00000000ffffffffl);
    }
  }

  /** convert an unsigned 64-bit integer to a string. */
  private static string unsignedtostring(final long value) {
    if (value >= 0) {
      return long.tostring(value);
    } else {
      // pull off the most-significant bit so that biginteger doesn't think
      // the number is negative, then set it again using setbit().
      return biginteger.valueof(value & 0x7fffffffffffffffl)
                       .setbit(63).tostring();
    }
  }

  /**
   * an inner class for writing text to the output stream.
   */
  private static final class textgenerator {
    private final appendable output;
    private final stringbuilder indent = new stringbuilder();
    private boolean atstartofline = true;

    private textgenerator(final appendable output) {
      this.output = output;
    }

    /**
     * indent text by two spaces.  after calling indent(), two spaces will be
     * inserted at the beginning of each line of text.  indent() may be called
     * multiple times to produce deeper indents.
     */
    public void indent() {
      indent.append("  ");
    }

    /**
     * reduces the current indent level by two spaces, or crashes if the indent
     * level is zero.
     */
    public void outdent() {
      final int length = indent.length();
      if (length == 0) {
        throw new illegalargumentexception(
            " outdent() without matching indent().");
      }
      indent.delete(length - 2, length);
    }

    /**
     * print text to the output stream.
     */
    public void print(final charsequence text) throws ioexception {
      final int size = text.length();
      int pos = 0;

      for (int i = 0; i < size; i++) {
        if (text.charat(i) == '\n') {
          write(text.subsequence(pos, size), i - pos + 1);
          pos = i + 1;
          atstartofline = true;
        }
      }
      write(text.subsequence(pos, size), size - pos);
    }

    private void write(final charsequence data, final int size)
                       throws ioexception {
      if (size == 0) {
        return;
      }
      if (atstartofline) {
        atstartofline = false;
        output.append(indent);
      }
      output.append(data);
    }
  }

  // =================================================================
  // parsing

  /**
   * represents a stream of tokens parsed from a {@code string}.
   *
   * <p>the java standard library provides many classes that you might think
   * would be useful for implementing this, but aren't.  for example:
   *
   * <ul>
   * <li>{@code java.io.streamtokenizer}:  this almost does what we want -- or,
   *   at least, something that would get us close to what we want -- except
   *   for one fatal flaw:  it automatically un-escapes strings using java
   *   escape sequences, which do not include all the escape sequences we
   *   need to support (e.g. '\x').
   * <li>{@code java.util.scanner}:  this seems like a great way at least to
   *   parse regular expressions out of a stream (so we wouldn't have to load
   *   the entire input into a single string before parsing).  sadly,
   *   {@code scanner} requires that tokens be delimited with some delimiter.
   *   thus, although the text "foo:" should parse to two tokens ("foo" and
   *   ":"), {@code scanner} would recognize it only as a single token.
   *   furthermore, {@code scanner} provides no way to inspect the contents
   *   of delimiters, making it impossible to keep track of line and column
   *   numbers.
   * </ul>
   *
   * <p>luckily, java's regular expression support does manage to be useful to
   * us.  (barely:  we need {@code matcher.usepattern()}, which is new in
   * java 1.5.)  so, we can use that, at least.  unfortunately, this implies
   * that we need to have the entire input in one contiguous string.
   */
  private static final class tokenizer {
    private final charsequence text;
    private final matcher matcher;
    private string currenttoken;

    // the character index within this.text at which the current token begins.
    private int pos = 0;

    // the line and column numbers of the current token.
    private int line = 0;
    private int column = 0;

    // the line and column numbers of the previous token (allows throwing
    // errors *after* consuming).
    private int previousline = 0;
    private int previouscolumn = 0;

    // we use possessive quantifiers (*+ and ++) because otherwise the java
    // regex matcher has stack overflows on large inputs.
    private static final pattern whitespace =
      pattern.compile("(\\s|(#.*$))++", pattern.multiline);
    private static final pattern token = pattern.compile(
      "[a-za-z_][0-9a-za-z_+-]*+|" +                // an identifier
      "[.]?[0-9+-][0-9a-za-z_.+-]*+|" +             // a number
      "\"([^\"\n\\\\]|\\\\.)*+(\"|\\\\?$)|" +       // a double-quoted string
      "\'([^\'\n\\\\]|\\\\.)*+(\'|\\\\?$)",         // a single-quoted string
      pattern.multiline);

    private static final pattern double_infinity = pattern.compile(
      "-?inf(inity)?",
      pattern.case_insensitive);
    private static final pattern float_infinity = pattern.compile(
      "-?inf(inity)?f?",
      pattern.case_insensitive);
    private static final pattern float_nan = pattern.compile(
      "nanf?",
      pattern.case_insensitive);

    /** construct a tokenizer that parses tokens from the given text. */
    private tokenizer(final charsequence text) {
      this.text = text;
      this.matcher = whitespace.matcher(text);
      skipwhitespace();
      nexttoken();
    }

    /** are we at the end of the input? */
    public boolean atend() {
      return currenttoken.length() == 0;
    }

    /** advance to the next token. */
    public void nexttoken() {
      previousline = line;
      previouscolumn = column;

      // advance the line counter to the current position.
      while (pos < matcher.regionstart()) {
        if (text.charat(pos) == '\n') {
          ++line;
          column = 0;
        } else {
          ++column;
        }
        ++pos;
      }

      // match the next token.
      if (matcher.regionstart() == matcher.regionend()) {
        // eof
        currenttoken = "";
      } else {
        matcher.usepattern(token);
        if (matcher.lookingat()) {
          currenttoken = matcher.group();
          matcher.region(matcher.end(), matcher.regionend());
        } else {
          // take one character.
          currenttoken = string.valueof(text.charat(pos));
          matcher.region(pos + 1, matcher.regionend());
        }

        skipwhitespace();
      }
    }

    /**
     * skip over any whitespace so that the matcher region starts at the next
     * token.
     */
    private void skipwhitespace() {
      matcher.usepattern(whitespace);
      if (matcher.lookingat()) {
        matcher.region(matcher.end(), matcher.regionend());
      }
    }

    /**
     * if the next token exactly matches {@code token}, consume it and return
     * {@code true}.  otherwise, return {@code false} without doing anything.
     */
    public boolean tryconsume(final string token) {
      if (currenttoken.equals(token)) {
        nexttoken();
        return true;
      } else {
        return false;
      }
    }

    /**
     * if the next token exactly matches {@code token}, consume it.  otherwise,
     * throw a {@link parseexception}.
     */
    public void consume(final string token) throws parseexception {
      if (!tryconsume(token)) {
        throw parseexception("expected \"" + token + "\".");
      }
    }

    /**
     * returns {@code true} if the next token is an integer, but does
     * not consume it.
     */
    public boolean lookingatinteger() {
      if (currenttoken.length() == 0) {
        return false;
      }

      final char c = currenttoken.charat(0);
      return ('0' <= c && c <= '9') ||
             c == '-' || c == '+';
    }

    /**
     * if the next token is an identifier, consume it and return its value.
     * otherwise, throw a {@link parseexception}.
     */
    public string consumeidentifier() throws parseexception {
      for (int i = 0; i < currenttoken.length(); i++) {
        final char c = currenttoken.charat(i);
        if (('a' <= c && c <= 'z') ||
            ('a' <= c && c <= 'z') ||
            ('0' <= c && c <= '9') ||
            (c == '_') || (c == '.')) {
          // ok
        } else {
          throw parseexception("expected identifier.");
        }
      }

      final string result = currenttoken;
      nexttoken();
      return result;
    }

    /**
     * if the next token is a 32-bit signed integer, consume it and return its
     * value.  otherwise, throw a {@link parseexception}.
     */
    public int consumeint32() throws parseexception {
      try {
        final int result = parseint32(currenttoken);
        nexttoken();
        return result;
      } catch (numberformatexception e) {
        throw integerparseexception(e);
      }
    }

    /**
     * if the next token is a 32-bit unsigned integer, consume it and return its
     * value.  otherwise, throw a {@link parseexception}.
     */
    public int consumeuint32() throws parseexception {
      try {
        final int result = parseuint32(currenttoken);
        nexttoken();
        return result;
      } catch (numberformatexception e) {
        throw integerparseexception(e);
      }
    }

    /**
     * if the next token is a 64-bit signed integer, consume it and return its
     * value.  otherwise, throw a {@link parseexception}.
     */
    public long consumeint64() throws parseexception {
      try {
        final long result = parseint64(currenttoken);
        nexttoken();
        return result;
      } catch (numberformatexception e) {
        throw integerparseexception(e);
      }
    }

    /**
     * if the next token is a 64-bit unsigned integer, consume it and return its
     * value.  otherwise, throw a {@link parseexception}.
     */
    public long consumeuint64() throws parseexception {
      try {
        final long result = parseuint64(currenttoken);
        nexttoken();
        return result;
      } catch (numberformatexception e) {
        throw integerparseexception(e);
      }
    }

    /**
     * if the next token is a double, consume it and return its value.
     * otherwise, throw a {@link parseexception}.
     */
    public double consumedouble() throws parseexception {
      // we need to parse infinity and nan separately because
      // double.parsedouble() does not accept "inf", "infinity", or "nan".
      if (double_infinity.matcher(currenttoken).matches()) {
        final boolean negative = currenttoken.startswith("-");
        nexttoken();
        return negative ? double.negative_infinity : double.positive_infinity;
      }
      if (currenttoken.equalsignorecase("nan")) {
        nexttoken();
        return double.nan;
      }
      try {
        final double result = double.parsedouble(currenttoken);
        nexttoken();
        return result;
      } catch (numberformatexception e) {
        throw floatparseexception(e);
      }
    }

    /**
     * if the next token is a float, consume it and return its value.
     * otherwise, throw a {@link parseexception}.
     */
    public float consumefloat() throws parseexception {
      // we need to parse infinity and nan separately because
      // float.parsefloat() does not accept "inf", "infinity", or "nan".
      if (float_infinity.matcher(currenttoken).matches()) {
        final boolean negative = currenttoken.startswith("-");
        nexttoken();
        return negative ? float.negative_infinity : float.positive_infinity;
      }
      if (float_nan.matcher(currenttoken).matches()) {
        nexttoken();
        return float.nan;
      }
      try {
        final float result = float.parsefloat(currenttoken);
        nexttoken();
        return result;
      } catch (numberformatexception e) {
        throw floatparseexception(e);
      }
    }

    /**
     * if the next token is a boolean, consume it and return its value.
     * otherwise, throw a {@link parseexception}.
     */
    public boolean consumeboolean() throws parseexception {
      if (currenttoken.equals("true") ||
          currenttoken.equals("t") ||
          currenttoken.equals("1")) {
        nexttoken();
        return true;
      } else if (currenttoken.equals("false") ||
                 currenttoken.equals("f") ||
                 currenttoken.equals("0")) {
        nexttoken();
        return false;
      } else {
        throw parseexception("expected \"true\" or \"false\".");
      }
    }

    /**
     * if the next token is a string, consume it and return its (unescaped)
     * value.  otherwise, throw a {@link parseexception}.
     */
    public string consumestring() throws parseexception {
      return consumebytestring().tostringutf8();
    }

    /**
     * if the next token is a string, consume it, unescape it as a
     * {@link bytestring}, and return it.  otherwise, throw a
     * {@link parseexception}.
     */
    public bytestring consumebytestring() throws parseexception {
      list<bytestring> list = new arraylist<bytestring>();
      consumebytestring(list);
      while (currenttoken.startswith("'") || currenttoken.startswith("\"")) {
        consumebytestring(list);
      }
      return bytestring.copyfrom(list);
    }

    /**
     * like {@link #consumebytestring()} but adds each token of the string to
     * the given list.  string literals (whether bytes or text) may come in
     * multiple adjacent tokens which are automatically concatenated, like in
     * c or python.
     */
    private void consumebytestring(list<bytestring> list) throws parseexception {
      final char quote = currenttoken.length() > 0 ? currenttoken.charat(0)
                                                   : '\0';
      if (quote != '\"' && quote != '\'') {
        throw parseexception("expected string.");
      }

      if (currenttoken.length() < 2 ||
          currenttoken.charat(currenttoken.length() - 1) != quote) {
        throw parseexception("string missing ending quote.");
      }

      try {
        final string escaped =
            currenttoken.substring(1, currenttoken.length() - 1);
        final bytestring result = unescapebytes(escaped);
        nexttoken();
        list.add(result);
      } catch (invalidescapesequenceexception e) {
        throw parseexception(e.getmessage());
      }
    }

    /**
     * returns a {@link parseexception} with the current line and column
     * numbers in the description, suitable for throwing.
     */
    public parseexception parseexception(final string description) {
      // note:  people generally prefer one-based line and column numbers.
      return new parseexception(
        line + 1, column + 1, description);
    }

    /**
     * returns a {@link parseexception} with the line and column numbers of
     * the previous token in the description, suitable for throwing.
     */
    public parseexception parseexceptionprevioustoken(
        final string description) {
      // note:  people generally prefer one-based line and column numbers.
      return new parseexception(
        previousline + 1, previouscolumn + 1, description);
    }

    /**
     * constructs an appropriate {@link parseexception} for the given
     * {@code numberformatexception} when trying to parse an integer.
     */
    private parseexception integerparseexception(
        final numberformatexception e) {
      return parseexception("couldn't parse integer: " + e.getmessage());
    }

    /**
     * constructs an appropriate {@link parseexception} for the given
     * {@code numberformatexception} when trying to parse a float or double.
     */
    private parseexception floatparseexception(final numberformatexception e) {
      return parseexception("couldn't parse number: " + e.getmessage());
    }
  }

  /** thrown when parsing an invalid text format message. */
  public static class parseexception extends ioexception {
    private static final long serialversionuid = 3196188060225107702l;

    private final int line;
    private final int column;

    /** create a new instance, with -1 as the line and column numbers. */
    public parseexception(final string message) {
      this(-1, -1, message);
    }

    /**
     * create a new instance
     *
     * @param line the line number where the parse error occurred,
     * using 1-offset.
     * @param column the column number where the parser error occurred,
     * using 1-offset.
     */
    public parseexception(final int line, final int column,
        final string message) {
      super(integer.tostring(line) + ":" + column + ": " + message);
      this.line = line;
      this.column = column;
    }

    /**
     * return the line where the parse exception occurred, or -1 when
     * none is provided. the value is specified as 1-offset, so the first
     * line is line 1.
     */
    public int getline() {
      return line;
    }

    /**
     * return the column where the parse exception occurred, or -1 when
     * none is provided. the value is specified as 1-offset, so the first
     * line is line 1.
     */
    public int getcolumn() {
      return column;
    }
  }

  /**
   * parse a text-format message from {@code input} and merge the contents
   * into {@code builder}.
   */
  public static void merge(final readable input,
                           final message.builder builder)
                           throws ioexception {
    merge(input, extensionregistry.getemptyregistry(), builder);
  }

  /**
   * parse a text-format message from {@code input} and merge the contents
   * into {@code builder}.
   */
  public static void merge(final charsequence input,
                           final message.builder builder)
                           throws parseexception {
    merge(input, extensionregistry.getemptyregistry(), builder);
  }

  /**
   * parse a text-format message from {@code input} and merge the contents
   * into {@code builder}.  extensions will be recognized if they are
   * registered in {@code extensionregistry}.
   */
  public static void merge(final readable input,
                           final extensionregistry extensionregistry,
                           final message.builder builder)
                           throws ioexception {
    // read the entire input to a string then parse that.

    // if streamtokenizer were not quite so crippled, or if there were a kind
    // of reader that could read in chunks that match some particular regex,
    // or if we wanted to write a custom reader to tokenize our stream, then
    // we would not have to read to one big string.  alas, none of these is
    // the case.  oh well.

    merge(tostringbuilder(input), extensionregistry, builder);
  }

  private static final int buffer_size = 4096;

  // todo(chrisn): see if working around java.io.reader#read(charbuffer)
  // overhead is worthwhile
  private static stringbuilder tostringbuilder(final readable input)
      throws ioexception {
    final stringbuilder text = new stringbuilder();
    final charbuffer buffer = charbuffer.allocate(buffer_size);
    while (true) {
      final int n = input.read(buffer);
      if (n == -1) {
        break;
      }
      buffer.flip();
      text.append(buffer, 0, n);
    }
    return text;
  }

  /**
   * parse a text-format message from {@code input} and merge the contents
   * into {@code builder}.  extensions will be recognized if they are
   * registered in {@code extensionregistry}.
   */
  public static void merge(final charsequence input,
                           final extensionregistry extensionregistry,
                           final message.builder builder)
                           throws parseexception {
    final tokenizer tokenizer = new tokenizer(input);

    while (!tokenizer.atend()) {
      mergefield(tokenizer, extensionregistry, builder);
    }
  }

  /**
   * parse a single field from {@code tokenizer} and merge it into
   * {@code builder}.
   */
  private static void mergefield(final tokenizer tokenizer,
                                 final extensionregistry extensionregistry,
                                 final message.builder builder)
                                 throws parseexception {
    fielddescriptor field;
    final descriptor type = builder.getdescriptorfortype();
    extensionregistry.extensioninfo extension = null;

    if (tokenizer.tryconsume("[")) {
      // an extension.
      final stringbuilder name =
          new stringbuilder(tokenizer.consumeidentifier());
      while (tokenizer.tryconsume(".")) {
        name.append('.');
        name.append(tokenizer.consumeidentifier());
      }

      extension = extensionregistry.findextensionbyname(name.tostring());

      if (extension == null) {
        throw tokenizer.parseexceptionprevioustoken(
          "extension \"" + name + "\" not found in the extensionregistry.");
      } else if (extension.descriptor.getcontainingtype() != type) {
        throw tokenizer.parseexceptionprevioustoken(
          "extension \"" + name + "\" does not extend message type \"" +
          type.getfullname() + "\".");
      }

      tokenizer.consume("]");

      field = extension.descriptor;
    } else {
      final string name = tokenizer.consumeidentifier();
      field = type.findfieldbyname(name);

      // group names are expected to be capitalized as they appear in the
      // .proto file, which actually matches their type names, not their field
      // names.
      if (field == null) {
        // explicitly specify us locale so that this code does not break when
        // executing in turkey.
        final string lowername = name.tolowercase(locale.us);
        field = type.findfieldbyname(lowername);
        // if the case-insensitive match worked but the field is not a group,
        if (field != null && field.gettype() != fielddescriptor.type.group) {
          field = null;
        }
      }
      // again, special-case group names as described above.
      if (field != null && field.gettype() == fielddescriptor.type.group &&
          !field.getmessagetype().getname().equals(name)) {
        field = null;
      }

      if (field == null) {
        throw tokenizer.parseexceptionprevioustoken(
          "message type \"" + type.getfullname() +
          "\" has no field named \"" + name + "\".");
      }
    }

    object value = null;

    if (field.getjavatype() == fielddescriptor.javatype.message) {
      tokenizer.tryconsume(":");  // optional

      final string endtoken;
      if (tokenizer.tryconsume("<")) {
        endtoken = ">";
      } else {
        tokenizer.consume("{");
        endtoken = "}";
      }

      final message.builder subbuilder;
      if (extension == null) {
        subbuilder = builder.newbuilderforfield(field);
      } else {
        subbuilder = extension.defaultinstance.newbuilderfortype();
      }

      while (!tokenizer.tryconsume(endtoken)) {
        if (tokenizer.atend()) {
          throw tokenizer.parseexception(
            "expected \"" + endtoken + "\".");
        }
        mergefield(tokenizer, extensionregistry, subbuilder);
      }

      value = subbuilder.buildpartial();

    } else {
      tokenizer.consume(":");

      switch (field.gettype()) {
        case int32:
        case sint32:
        case sfixed32:
          value = tokenizer.consumeint32();
          break;

        case int64:
        case sint64:
        case sfixed64:
          value = tokenizer.consumeint64();
          break;

        case uint32:
        case fixed32:
          value = tokenizer.consumeuint32();
          break;

        case uint64:
        case fixed64:
          value = tokenizer.consumeuint64();
          break;

        case float:
          value = tokenizer.consumefloat();
          break;

        case double:
          value = tokenizer.consumedouble();
          break;

        case bool:
          value = tokenizer.consumeboolean();
          break;

        case string:
          value = tokenizer.consumestring();
          break;

        case bytes:
          value = tokenizer.consumebytestring();
          break;

        case enum:
          final enumdescriptor enumtype = field.getenumtype();

          if (tokenizer.lookingatinteger()) {
            final int number = tokenizer.consumeint32();
            value = enumtype.findvaluebynumber(number);
            if (value == null) {
              throw tokenizer.parseexceptionprevioustoken(
                "enum type \"" + enumtype.getfullname() +
                "\" has no value with number " + number + '.');
            }
          } else {
            final string id = tokenizer.consumeidentifier();
            value = enumtype.findvaluebyname(id);
            if (value == null) {
              throw tokenizer.parseexceptionprevioustoken(
                "enum type \"" + enumtype.getfullname() +
                "\" has no value named \"" + id + "\".");
            }
          }

          break;

        case message:
        case group:
          throw new runtimeexception("can't get here.");
      }
    }

    if (field.isrepeated()) {
      builder.addrepeatedfield(field, value);
    } else {
      builder.setfield(field, value);
    }
  }

  // =================================================================
  // utility functions
  //
  // some of these methods are package-private because descriptors.java uses
  // them.

  /**
   * escapes bytes in the format used in protocol buffer text format, which
   * is the same as the format used for c string literals.  all bytes
   * that are not printable 7-bit ascii characters are escaped, as well as
   * backslash, single-quote, and double-quote characters.  characters for
   * which no defined short-hand escape sequence is defined will be escaped
   * using 3-digit octal sequences.
   */
  static string escapebytes(final bytestring input) {
    final stringbuilder builder = new stringbuilder(input.size());
    for (int i = 0; i < input.size(); i++) {
      final byte b = input.byteat(i);
      switch (b) {
        // java does not recognize \a or \v, apparently.
        case 0x07: builder.append("\\a" ); break;
        case '\b': builder.append("\\b" ); break;
        case '\f': builder.append("\\f" ); break;
        case '\n': builder.append("\\n" ); break;
        case '\r': builder.append("\\r" ); break;
        case '\t': builder.append("\\t" ); break;
        case 0x0b: builder.append("\\v" ); break;
        case '\\': builder.append("\\\\"); break;
        case '\'': builder.append("\\\'"); break;
        case '"' : builder.append("\\\""); break;
        default:
          // note:  bytes with the high-order bit set should be escaped.  since
          //   bytes are signed, such bytes will compare less than 0x20, hence
          //   the following line is correct.
          if (b >= 0x20) {
            builder.append((char) b);
          } else {
            builder.append('\\');
            builder.append((char) ('0' + ((b >>> 6) & 3)));
            builder.append((char) ('0' + ((b >>> 3) & 7)));
            builder.append((char) ('0' + (b & 7)));
          }
          break;
      }
    }
    return builder.tostring();
  }

  /**
   * un-escape a byte sequence as escaped using
   * {@link #escapebytes(bytestring)}.  two-digit hex escapes (starting with
   * "\x") are also recognized.
   */
  static bytestring unescapebytes(final charsequence charstring)
      throws invalidescapesequenceexception {
    // first convert the java character sequence to utf-8 bytes.
    bytestring input = bytestring.copyfromutf8(charstring.tostring());
    // then unescape certain byte sequences introduced by ascii '\\'.  the valid
    // escapes can all be expressed with ascii characters, so it is safe to
    // operate on bytes here.
    //
    // unescaping the input byte array will result in a byte sequence that's no
    // longer than the input.  that's because each escape sequence is between
    // two and four bytes long and stands for a single byte.
    final byte[] result = new byte[input.size()];
    int pos = 0;
    for (int i = 0; i < input.size(); i++) {
      byte c = input.byteat(i);
      if (c == '\\') {
        if (i + 1 < input.size()) {
          ++i;
          c = input.byteat(i);
          if (isoctal(c)) {
            // octal escape.
            int code = digitvalue(c);
            if (i + 1 < input.size() && isoctal(input.byteat(i + 1))) {
              ++i;
              code = code * 8 + digitvalue(input.byteat(i));
            }
            if (i + 1 < input.size() && isoctal(input.byteat(i + 1))) {
              ++i;
              code = code * 8 + digitvalue(input.byteat(i));
            }
            // todo: check that 0 <= code && code <= 0xff.
            result[pos++] = (byte)code;
          } else {
            switch (c) {
              case 'a' : result[pos++] = 0x07; break;
              case 'b' : result[pos++] = '\b'; break;
              case 'f' : result[pos++] = '\f'; break;
              case 'n' : result[pos++] = '\n'; break;
              case 'r' : result[pos++] = '\r'; break;
              case 't' : result[pos++] = '\t'; break;
              case 'v' : result[pos++] = 0x0b; break;
              case '\\': result[pos++] = '\\'; break;
              case '\'': result[pos++] = '\''; break;
              case '"' : result[pos++] = '\"'; break;

              case 'x':
                // hex escape
                int code = 0;
                if (i + 1 < input.size() && ishex(input.byteat(i + 1))) {
                  ++i;
                  code = digitvalue(input.byteat(i));
                } else {
                  throw new invalidescapesequenceexception(
                      "invalid escape sequence: '\\x' with no digits");
                }
                if (i + 1 < input.size() && ishex(input.byteat(i + 1))) {
                  ++i;
                  code = code * 16 + digitvalue(input.byteat(i));
                }
                result[pos++] = (byte)code;
                break;

              default:
                throw new invalidescapesequenceexception(
                    "invalid escape sequence: '\\" + (char)c + '\'');
            }
          }
        } else {
          throw new invalidescapesequenceexception(
              "invalid escape sequence: '\\' at end of string.");
        }
      } else {
        result[pos++] = c;
      }
    }

    return bytestring.copyfrom(result, 0, pos);
  }

  /**
   * thrown by {@link textformat#unescapebytes} and
   * {@link textformat#unescapetext} when an invalid escape sequence is seen.
   */
  static class invalidescapesequenceexception extends ioexception {
    private static final long serialversionuid = -8164033650142593304l;

    invalidescapesequenceexception(final string description) {
      super(description);
    }
  }

  /**
   * like {@link #escapebytes(bytestring)}, but escapes a text string.
   * non-ascii characters are first encoded as utf-8, then each byte is escaped
   * individually as a 3-digit octal escape.  yes, it's weird.
   */
  static string escapetext(final string input) {
    return escapebytes(bytestring.copyfromutf8(input));
  }

  /**
   * un-escape a text string as escaped using {@link #escapetext(string)}.
   * two-digit hex escapes (starting with "\x") are also recognized.
   */
  static string unescapetext(final string input)
                             throws invalidescapesequenceexception {
    return unescapebytes(input).tostringutf8();
  }

  /** is this an octal digit? */
  private static boolean isoctal(final byte c) {
    return '0' <= c && c <= '7';
  }

  /** is this a hex digit? */
  private static boolean ishex(final byte c) {
    return ('0' <= c && c <= '9') ||
           ('a' <= c && c <= 'f') ||
           ('a' <= c && c <= 'f');
  }

  /**
   * interpret a character as a digit (in any base up to 36) and return the
   * numeric value.  this is like {@code character.digit()} but we don't accept
   * non-ascii digits.
   */
  private static int digitvalue(final byte c) {
    if ('0' <= c && c <= '9') {
      return c - '0';
    } else if ('a' <= c && c <= 'z') {
      return c - 'a' + 10;
    } else {
      return c - 'a' + 10;
    }
  }

  /**
   * parse a 32-bit signed integer from the text.  unlike the java standard
   * {@code integer.parseint()}, this function recognizes the prefixes "0x"
   * and "0" to signify hexadecimal and octal numbers, respectively.
   */
  static int parseint32(final string text) throws numberformatexception {
    return (int) parseinteger(text, true, false);
  }

  /**
   * parse a 32-bit unsigned integer from the text.  unlike the java standard
   * {@code integer.parseint()}, this function recognizes the prefixes "0x"
   * and "0" to signify hexadecimal and octal numbers, respectively.  the
   * result is coerced to a (signed) {@code int} when returned since java has
   * no unsigned integer type.
   */
  static int parseuint32(final string text) throws numberformatexception {
    return (int) parseinteger(text, false, false);
  }

  /**
   * parse a 64-bit signed integer from the text.  unlike the java standard
   * {@code integer.parseint()}, this function recognizes the prefixes "0x"
   * and "0" to signify hexadecimal and octal numbers, respectively.
   */
  static long parseint64(final string text) throws numberformatexception {
    return parseinteger(text, true, true);
  }

  /**
   * parse a 64-bit unsigned integer from the text.  unlike the java standard
   * {@code integer.parseint()}, this function recognizes the prefixes "0x"
   * and "0" to signify hexadecimal and octal numbers, respectively.  the
   * result is coerced to a (signed) {@code long} when returned since java has
   * no unsigned long type.
   */
  static long parseuint64(final string text) throws numberformatexception {
    return parseinteger(text, false, true);
  }

  private static long parseinteger(final string text,
                                   final boolean issigned,
                                   final boolean islong)
                                   throws numberformatexception {
    int pos = 0;

    boolean negative = false;
    if (text.startswith("-", pos)) {
      if (!issigned) {
        throw new numberformatexception("number must be positive: " + text);
      }
      ++pos;
      negative = true;
    }

    int radix = 10;
    if (text.startswith("0x", pos)) {
      pos += 2;
      radix = 16;
    } else if (text.startswith("0", pos)) {
      radix = 8;
    }

    final string numbertext = text.substring(pos);

    long result = 0;
    if (numbertext.length() < 16) {
      // can safely assume no overflow.
      result = long.parselong(numbertext, radix);
      if (negative) {
        result = -result;
      }

      // check bounds.
      // no need to check for 64-bit numbers since they'd have to be 16 chars
      // or longer to overflow.
      if (!islong) {
        if (issigned) {
          if (result > integer.max_value || result < integer.min_value) {
            throw new numberformatexception(
              "number out of range for 32-bit signed integer: " + text);
          }
        } else {
          if (result >= (1l << 32) || result < 0) {
            throw new numberformatexception(
              "number out of range for 32-bit unsigned integer: " + text);
          }
        }
      }
    } else {
      biginteger bigvalue = new biginteger(numbertext, radix);
      if (negative) {
        bigvalue = bigvalue.negate();
      }

      // check bounds.
      if (!islong) {
        if (issigned) {
          if (bigvalue.bitlength() > 31) {
            throw new numberformatexception(
              "number out of range for 32-bit signed integer: " + text);
          }
        } else {
          if (bigvalue.bitlength() > 32) {
            throw new numberformatexception(
              "number out of range for 32-bit unsigned integer: " + text);
          }
        }
      } else {
        if (issigned) {
          if (bigvalue.bitlength() > 63) {
            throw new numberformatexception(
              "number out of range for 64-bit signed integer: " + text);
          }
        } else {
          if (bigvalue.bitlength() > 64) {
            throw new numberformatexception(
              "number out of range for 64-bit unsigned integer: " + text);
          }
        }
      }

      result = bigvalue.longvalue();
    }

    return result;
  }
}
