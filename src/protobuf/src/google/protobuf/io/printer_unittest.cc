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

// author: kenton@google.com (kenton varda)
//  based on original protocol buffers design by
//  sanjay ghemawat, jeff dean, and others.

#include <vector>

#include <google/protobuf/io/printer.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/testing/googletest.h>
#include <gtest/gtest.h>

namespace google {
namespace protobuf {
namespace io {
namespace {

// each test repeats over several block sizes in order to test both cases
// where particular writes cross a buffer boundary and cases where they do
// not.

test(printer, emptyprinter) {
  char buffer[8192];
  const int block_size = 100;
  arrayoutputstream output(buffer, google_arraysize(buffer), block_size);
  printer printer(&output, '\0');
  expect_true(!printer.failed());
}

test(printer, basicprinting) {
  char buffer[8192];

  for (int block_size = 1; block_size < 512; block_size *= 2) {
    arrayoutputstream output(buffer, sizeof(buffer), block_size);

    {
      printer printer(&output, '\0');

      printer.print("hello world!");
      printer.print("  this is the same line.\n");
      printer.print("but this is a new one.\nand this is another one.");

      expect_false(printer.failed());
    }

    buffer[output.bytecount()] = '\0';

    expect_streq("hello world!  this is the same line.\n"
                 "but this is a new one.\n"
                 "and this is another one.",
                 buffer);
  }
}

test(printer, writeraw) {
  char buffer[8192];

  for (int block_size = 1; block_size < 512; block_size *= 2) {
    arrayoutputstream output(buffer, sizeof(buffer), block_size);

    {
      string string_obj = "from an object\n";
      printer printer(&output, '$');
      printer.writeraw("hello world!", 12);
      printer.printraw("  this is the same line.\n");
      printer.printraw("but this is a new one.\nand this is another one.");
      printer.writeraw("\n", 1);
      printer.printraw(string_obj);
      expect_false(printer.failed());
    }

    buffer[output.bytecount()] = '\0';

    expect_streq("hello world!  this is the same line.\n"
                 "but this is a new one.\n"
                 "and this is another one."
                 "\n"
                 "from an object\n",
                 buffer);
  }
}

test(printer, variablesubstitution) {
  char buffer[8192];

  for (int block_size = 1; block_size < 512; block_size *= 2) {
    arrayoutputstream output(buffer, sizeof(buffer), block_size);

    {
      printer printer(&output, '$');
      map<string, string> vars;

      vars["foo"] = "world";
      vars["bar"] = "$foo$";
      vars["abcdefg"] = "1234";

      printer.print(vars, "hello $foo$!\nbar = $bar$\n");
      printer.printraw("rawbit\n");
      printer.print(vars, "$abcdefg$\na literal dollar sign:  $$");

      vars["foo"] = "blah";
      printer.print(vars, "\nnow foo = $foo$.");

      expect_false(printer.failed());
    }

    buffer[output.bytecount()] = '\0';

    expect_streq("hello world!\n"
                 "bar = $foo$\n"
                 "rawbit\n"
                 "1234\n"
                 "a literal dollar sign:  $\n"
                 "now foo = blah.",
                 buffer);
  }
}

test(printer, inlinevariablesubstitution) {
  char buffer[8192];

  arrayoutputstream output(buffer, sizeof(buffer));

  {
    printer printer(&output, '$');
    printer.print("hello $foo$!\n", "foo", "world");
    printer.printraw("rawbit\n");
    printer.print("$foo$ $bar$\n", "foo", "one", "bar", "two");
    expect_false(printer.failed());
  }

  buffer[output.bytecount()] = '\0';

  expect_streq("hello world!\n"
               "rawbit\n"
               "one two\n",
               buffer);
}

test(printer, indenting) {
  char buffer[8192];

  for (int block_size = 1; block_size < 512; block_size *= 2) {
    arrayoutputstream output(buffer, sizeof(buffer), block_size);

    {
      printer printer(&output, '$');
      map<string, string> vars;

      vars["newline"] = "\n";

      printer.print("this is not indented.\n");
      printer.indent();
      printer.print("this is indented\nand so is this\n");
      printer.outdent();
      printer.print("but this is not.");
      printer.indent();
      printer.print("  and this is still the same line.\n"
                    "but this is indented.\n");
      printer.printraw("rawbit has indent at start\n");
      printer.printraw("but not after a raw newline\n");
      printer.print(vars, "note that a newline in a variable will break "
                    "indenting, as we see$newline$here.\n");
      printer.indent();
      printer.print("and this");
      printer.outdent();
      printer.outdent();
      printer.print(" is double-indented\nback to normal.");

      expect_false(printer.failed());
    }

    buffer[output.bytecount()] = '\0';

    expect_streq(
      "this is not indented.\n"
      "  this is indented\n"
      "  and so is this\n"
      "but this is not.  and this is still the same line.\n"
      "  but this is indented.\n"
      "  rawbit has indent at start\n"
      "but not after a raw newline\n"
      "note that a newline in a variable will break indenting, as we see\n"
      "here.\n"
      "    and this is double-indented\n"
      "back to normal.",
      buffer);
  }
}

// death tests do not work on windows as of yet.
#ifdef protobuf_has_death_test
test(printer, death) {
  char buffer[8192];

  arrayoutputstream output(buffer, sizeof(buffer));
  printer printer(&output, '$');

  expect_debug_death(printer.print("$nosuchvar$"), "undefined variable");
  expect_debug_death(printer.print("$unclosed"), "unclosed variable name");
  expect_debug_death(printer.outdent(), "without matching indent");
}
#endif  // protobuf__has_death_test

test(printer, writefailurepartial) {
  char buffer[17];

  arrayoutputstream output(buffer, sizeof(buffer));
  printer printer(&output, '$');

  // print 16 bytes to almost fill the buffer (should not fail).
  printer.print("0123456789abcdef");
  expect_false(printer.failed());

  // try to print 2 chars. only one fits.
  printer.print("<>");
  expect_true(printer.failed());

  // anything else should fail too.
  printer.print(" ");
  expect_true(printer.failed());
  printer.print("blah");
  expect_true(printer.failed());

  // buffer should contain the first 17 bytes written.
  expect_eq("0123456789abcdef<", string(buffer, sizeof(buffer)));
}

test(printer, writefailureexact) {
  char buffer[16];

  arrayoutputstream output(buffer, sizeof(buffer));
  printer printer(&output, '$');

  // print 16 bytes to fill the buffer exactly (should not fail).
  printer.print("0123456789abcdef");
  expect_false(printer.failed());

  // try to print one more byte (should fail).
  printer.print(" ");
  expect_true(printer.failed());

  // should not crash
  printer.print("blah");
  expect_true(printer.failed());

  // buffer should contain the first 16 bytes written.
  expect_eq("0123456789abcdef", string(buffer, sizeof(buffer)));
}

}  // namespace
}  // namespace io
}  // namespace protobuf
}  // namespace google
