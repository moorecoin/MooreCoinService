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

#include <limits.h>
#include <math.h>

#include <vector>

#include <google/protobuf/io/tokenizer.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/strutil.h>
#include <google/protobuf/stubs/substitute.h>
#include <google/protobuf/testing/googletest.h>
#include <gtest/gtest.h>

namespace google {
namespace protobuf {
namespace io {
namespace {

// ===================================================================
// data-driven test infrastructure

// todo(kenton):  this is copied from coded_stream_unittest.  this is
//   temporary until these fetaures are integrated into gtest itself.

// test_1d and test_2d are macros i'd eventually like to see added to
// gtest.  these macros can be used to declare tests which should be
// run multiple times, once for each item in some input array.  test_1d
// tests all cases in a single input array.  test_2d tests all
// combinations of cases from two arrays.  the arrays must be statically
// defined such that the google_arraysize() macro works on them.  example:
//
// int kcases[] = {1, 2, 3, 4}
// test_1d(myfixture, mytest, kcases) {
//   expect_gt(kcases_case, 0);
// }
//
// this test iterates through the numbers 1, 2, 3, and 4 and tests that
// they are all grater than zero.  in case of failure, the exact case
// which failed will be printed.  the case type must be printable using
// ostream::operator<<.

#define test_1d(fixture, name, cases)                                      \
  class fixture##_##name##_dd : public fixture {                           \
   protected:                                                              \
    template <typename casetype>                                           \
    void dosinglecase(const casetype& cases##_case);                       \
  };                                                                       \
                                                                           \
  test_f(fixture##_##name##_dd, name) {                                    \
    for (int i = 0; i < google_arraysize(cases); i++) {                           \
      scoped_trace(testing::message()                                      \
        << #cases " case #" << i << ": " << cases[i]);                     \
      dosinglecase(cases[i]);                                              \
    }                                                                      \
  }                                                                        \
                                                                           \
  template <typename casetype>                                             \
  void fixture##_##name##_dd::dosinglecase(const casetype& cases##_case)

#define test_2d(fixture, name, cases1, cases2)                             \
  class fixture##_##name##_dd : public fixture {                           \
   protected:                                                              \
    template <typename casetype1, typename casetype2>                      \
    void dosinglecase(const casetype1& cases1##_case,                      \
                      const casetype2& cases2##_case);                     \
  };                                                                       \
                                                                           \
  test_f(fixture##_##name##_dd, name) {                                    \
    for (int i = 0; i < google_arraysize(cases1); i++) {                          \
      for (int j = 0; j < google_arraysize(cases2); j++) {                        \
        scoped_trace(testing::message()                                    \
          << #cases1 " case #" << i << ": " << cases1[i] << ", "           \
          << #cases2 " case #" << j << ": " << cases2[j]);                 \
        dosinglecase(cases1[i], cases2[j]);                                \
      }                                                                    \
    }                                                                      \
  }                                                                        \
                                                                           \
  template <typename casetype1, typename casetype2>                        \
  void fixture##_##name##_dd::dosinglecase(const casetype1& cases1##_case, \
                                           const casetype2& cases2##_case)

// -------------------------------------------------------------------

// an input stream that is basically like an arrayinputstream but sometimes
// returns empty buffers, just to throw us off.
class testinputstream : public zerocopyinputstream {
 public:
  testinputstream(const void* data, int size, int block_size)
    : array_stream_(data, size, block_size), counter_(0) {}
  ~testinputstream() {}

  // implements zerocopyinputstream ----------------------------------
  bool next(const void** data, int* size) {
    // we'll return empty buffers starting with the first buffer, and every
    // 3 and 5 buffers after that.
    if (counter_ % 3 == 0 || counter_ % 5 == 0) {
      *data = null;
      *size = 0;
      ++counter_;
      return true;
    } else {
      ++counter_;
      return array_stream_.next(data, size);
    }
  }

  void backup(int count)  { return array_stream_.backup(count); }
  bool skip(int count)    { return array_stream_.skip(count);   }
  int64 bytecount() const { return array_stream_.bytecount();   }

 private:
  arrayinputstream array_stream_;
  int counter_;
};

// -------------------------------------------------------------------

// an error collector which simply concatenates all its errors into a big
// block of text which can be checked.
class testerrorcollector : public errorcollector {
 public:
  testerrorcollector() {}
  ~testerrorcollector() {}

  string text_;

  // implements errorcollector ---------------------------------------
  void adderror(int line, int column, const string& message) {
    strings::substituteandappend(&text_, "$0:$1: $2\n",
                                 line, column, message);
  }
};

// -------------------------------------------------------------------

// we test each operation over a variety of block sizes to insure that
// we test cases where reads cross buffer boundaries as well as cases
// where they don't.  this is sort of a brute-force approach to this,
// but it's easy to write and easy to understand.
const int kblocksizes[] = {1, 2, 3, 5, 7, 13, 32, 1024};

class tokenizertest : public testing::test {
 protected:
  // for easy testing.
  uint64 parseinteger(const string& text) {
    uint64 result;
    expect_true(tokenizer::parseinteger(text, kuint64max, &result));
    return result;
  }
};

// ===================================================================

// these tests causes gcc 3.3.5 (and earlier?) to give the cryptic error:
//   "sorry, unimplemented: `method_call_expr' not supported by dump_expr"
#if !defined(__gnuc__) || __gnuc__ > 3 || (__gnuc__ == 3 && __gnuc_minor__ > 3)

// in each test case, the entire input text should parse as a single token
// of the given type.
struct simpletokencase {
  string input;
  tokenizer::tokentype type;
};

inline ostream& operator<<(ostream& out,
                           const simpletokencase& test_case) {
  return out << cescape(test_case.input);
}

simpletokencase ksimpletokencases[] = {
  // test identifiers.
  { "hello",       tokenizer::type_identifier },

  // test integers.
  { "123",         tokenizer::type_integer },
  { "0xab6",       tokenizer::type_integer },
  { "0xab6",       tokenizer::type_integer },
  { "0x1234567",   tokenizer::type_integer },
  { "0x89abcdef",  tokenizer::type_integer },
  { "0x89abcdef",  tokenizer::type_integer },
  { "01234567",    tokenizer::type_integer },

  // test floats.
  { "123.45",      tokenizer::type_float },
  { "1.",          tokenizer::type_float },
  { "1e3",         tokenizer::type_float },
  { "1e3",         tokenizer::type_float },
  { "1e-3",        tokenizer::type_float },
  { "1e+3",        tokenizer::type_float },
  { "1.e3",        tokenizer::type_float },
  { "1.2e3",       tokenizer::type_float },
  { ".1",          tokenizer::type_float },
  { ".1e3",        tokenizer::type_float },
  { ".1e-3",       tokenizer::type_float },
  { ".1e+3",       tokenizer::type_float },

  // test strings.
  { "'hello'",     tokenizer::type_string },
  { "\"foo\"",     tokenizer::type_string },
  { "'a\"b'",      tokenizer::type_string },
  { "\"a'b\"",     tokenizer::type_string },
  { "'a\\'b'",     tokenizer::type_string },
  { "\"a\\\"b\"",  tokenizer::type_string },
  { "'\\xf'",      tokenizer::type_string },
  { "'\\0'",       tokenizer::type_string },

  // test symbols.
  { "+",           tokenizer::type_symbol },
  { ".",           tokenizer::type_symbol },
};

test_2d(tokenizertest, simpletokens, ksimpletokencases, kblocksizes) {
  // set up the tokenizer.
  testinputstream input(ksimpletokencases_case.input.data(),
                        ksimpletokencases_case.input.size(),
                        kblocksizes_case);
  testerrorcollector error_collector;
  tokenizer tokenizer(&input, &error_collector);

  // before next() is called, the initial token should always be type_start.
  expect_eq(tokenizer::type_start, tokenizer.current().type);
  expect_eq("", tokenizer.current().text);
  expect_eq(0, tokenizer.current().line);
  expect_eq(0, tokenizer.current().column);
  expect_eq(0, tokenizer.current().end_column);

  // parse the token.
  assert_true(tokenizer.next());

  // check that it has the right type.
  expect_eq(ksimpletokencases_case.type, tokenizer.current().type);
  // check that it contains the complete input text.
  expect_eq(ksimpletokencases_case.input, tokenizer.current().text);
  // check that it is located at the beginning of the input
  expect_eq(0, tokenizer.current().line);
  expect_eq(0, tokenizer.current().column);
  expect_eq(ksimpletokencases_case.input.size(),
            tokenizer.current().end_column);

  // there should be no more input.
  expect_false(tokenizer.next());

  // after next() returns false, the token should have type type_end.
  expect_eq(tokenizer::type_end, tokenizer.current().type);
  expect_eq("", tokenizer.current().text);
  expect_eq(0, tokenizer.current().line);
  expect_eq(ksimpletokencases_case.input.size(), tokenizer.current().column);
  expect_eq(ksimpletokencases_case.input.size(),
            tokenizer.current().end_column);

  // there should be no errors.
  expect_true(error_collector.text_.empty());
}

test_1d(tokenizertest, floatsuffix, kblocksizes) {
  // test the "allow_f_after_float" option.

  // set up the tokenizer.
  const char* text = "1f 2.5f 6e3f 7f";
  testinputstream input(text, strlen(text), kblocksizes_case);
  testerrorcollector error_collector;
  tokenizer tokenizer(&input, &error_collector);
  tokenizer.set_allow_f_after_float(true);

  // advance through tokens and check that they are parsed as expected.
  assert_true(tokenizer.next());
  expect_eq(tokenizer.current().text, "1f");
  expect_eq(tokenizer.current().type, tokenizer::type_float);
  assert_true(tokenizer.next());
  expect_eq(tokenizer.current().text, "2.5f");
  expect_eq(tokenizer.current().type, tokenizer::type_float);
  assert_true(tokenizer.next());
  expect_eq(tokenizer.current().text, "6e3f");
  expect_eq(tokenizer.current().type, tokenizer::type_float);
  assert_true(tokenizer.next());
  expect_eq(tokenizer.current().text, "7f");
  expect_eq(tokenizer.current().type, tokenizer::type_float);

  // there should be no more input.
  expect_false(tokenizer.next());
  // there should be no errors.
  expect_true(error_collector.text_.empty());
}

#endif

// -------------------------------------------------------------------

// in each case, the input is parsed to produce a list of tokens.  the
// last token in "output" must have type type_end.
struct multitokencase {
  string input;
  tokenizer::token output[10];  // the compiler wants a constant array
                                // size for initialization to work.  there
                                // is no reason this can't be increased if
                                // needed.
};

inline ostream& operator<<(ostream& out,
                           const multitokencase& test_case) {
  return out << cescape(test_case.input);
}

multitokencase kmultitokencases[] = {
  // test empty input.
  { "", {
    { tokenizer::type_end       , ""     , 0,  0 },
  }},

  // test all token types at the same time.
  { "foo 1 1.2 + 'bar'", {
    { tokenizer::type_identifier, "foo"  , 0,  0,  3 },
    { tokenizer::type_integer   , "1"    , 0,  4,  5 },
    { tokenizer::type_float     , "1.2"  , 0,  6,  9 },
    { tokenizer::type_symbol    , "+"    , 0, 10, 11 },
    { tokenizer::type_string    , "'bar'", 0, 12, 17 },
    { tokenizer::type_end       , ""     , 0, 17, 17 },
  }},

  // test that consecutive symbols are parsed as separate tokens.
  { "!@+%", {
    { tokenizer::type_symbol    , "!"    , 0, 0, 1 },
    { tokenizer::type_symbol    , "@"    , 0, 1, 2 },
    { tokenizer::type_symbol    , "+"    , 0, 2, 3 },
    { tokenizer::type_symbol    , "%"    , 0, 3, 4 },
    { tokenizer::type_end       , ""     , 0, 4, 4 },
  }},

  // test that newlines affect line numbers correctly.
  { "foo bar\nrab oof", {
    { tokenizer::type_identifier, "foo", 0,  0, 3 },
    { tokenizer::type_identifier, "bar", 0,  4, 7 },
    { tokenizer::type_identifier, "rab", 1,  0, 3 },
    { tokenizer::type_identifier, "oof", 1,  4, 7 },
    { tokenizer::type_end       , ""   , 1,  7, 7 },
  }},

  // test that tabs affect column numbers correctly.
  { "foo\tbar  \tbaz", {
    { tokenizer::type_identifier, "foo", 0,  0,  3 },
    { tokenizer::type_identifier, "bar", 0,  8, 11 },
    { tokenizer::type_identifier, "baz", 0, 16, 19 },
    { tokenizer::type_end       , ""   , 0, 19, 19 },
  }},

  // test that tabs in string literals affect column numbers correctly.
  { "\"foo\tbar\" baz", {
    { tokenizer::type_string    , "\"foo\tbar\"", 0,  0, 12 },
    { tokenizer::type_identifier, "baz"         , 0, 13, 16 },
    { tokenizer::type_end       , ""            , 0, 16, 16 },
  }},

  // test that line comments are ignored.
  { "foo // this is a comment\n"
    "bar // this is another comment", {
    { tokenizer::type_identifier, "foo", 0,  0,  3 },
    { tokenizer::type_identifier, "bar", 1,  0,  3 },
    { tokenizer::type_end       , ""   , 1, 30, 30 },
  }},

  // test that block comments are ignored.
  { "foo /* this is a block comment */ bar", {
    { tokenizer::type_identifier, "foo", 0,  0,  3 },
    { tokenizer::type_identifier, "bar", 0, 34, 37 },
    { tokenizer::type_end       , ""   , 0, 37, 37 },
  }},

  // test that sh-style comments are not ignored by default.
  { "foo # bar\n"
    "baz", {
    { tokenizer::type_identifier, "foo", 0, 0, 3 },
    { tokenizer::type_symbol    , "#"  , 0, 4, 5 },
    { tokenizer::type_identifier, "bar", 0, 6, 9 },
    { tokenizer::type_identifier, "baz", 1, 0, 3 },
    { tokenizer::type_end       , ""   , 1, 3, 3 },
  }},

  // bytes with the high-order bit set should not be seen as control characters.
  { "\300", {
    { tokenizer::type_symbol, "\300", 0, 0, 1 },
    { tokenizer::type_end   , ""    , 0, 1, 1 },
  }},

  // test all whitespace chars
  { "foo\n\t\r\v\fbar", {
    { tokenizer::type_identifier, "foo", 0,  0,  3 },
    { tokenizer::type_identifier, "bar", 1, 11, 14 },
    { tokenizer::type_end       , ""   , 1, 14, 14 },
  }},
};

test_2d(tokenizertest, multipletokens, kmultitokencases, kblocksizes) {
  // set up the tokenizer.
  testinputstream input(kmultitokencases_case.input.data(),
                        kmultitokencases_case.input.size(),
                        kblocksizes_case);
  testerrorcollector error_collector;
  tokenizer tokenizer(&input, &error_collector);

  // before next() is called, the initial token should always be type_start.
  expect_eq(tokenizer::type_start, tokenizer.current().type);
  expect_eq("", tokenizer.current().text);
  expect_eq(0, tokenizer.current().line);
  expect_eq(0, tokenizer.current().column);
  expect_eq(0, tokenizer.current().end_column);

  // loop through all expected tokens.
  int i = 0;
  tokenizer::token token;
  do {
    token = kmultitokencases_case.output[i++];

    scoped_trace(testing::message() << "token #" << i << ": " << token.text);

    tokenizer::token previous = tokenizer.current();

    // next() should only return false when it hits the end token.
    if (token.type != tokenizer::type_end) {
      assert_true(tokenizer.next());
    } else {
      assert_false(tokenizer.next());
    }

    // check that the previous token is set correctly.
    expect_eq(previous.type, tokenizer.previous().type);
    expect_eq(previous.text, tokenizer.previous().text);
    expect_eq(previous.line, tokenizer.previous().line);
    expect_eq(previous.column, tokenizer.previous().column);
    expect_eq(previous.end_column, tokenizer.previous().end_column);

    // check that the token matches the expected one.
    expect_eq(token.type, tokenizer.current().type);
    expect_eq(token.text, tokenizer.current().text);
    expect_eq(token.line, tokenizer.current().line);
    expect_eq(token.column, tokenizer.current().column);
    expect_eq(token.end_column, tokenizer.current().end_column);

  } while (token.type != tokenizer::type_end);

  // there should be no errors.
  expect_true(error_collector.text_.empty());
}

// this test causes gcc 3.3.5 (and earlier?) to give the cryptic error:
//   "sorry, unimplemented: `method_call_expr' not supported by dump_expr"
#if !defined(__gnuc__) || __gnuc__ > 3 || (__gnuc__ == 3 && __gnuc_minor__ > 3)

test_1d(tokenizertest, shcommentstyle, kblocksizes) {
  // test the "comment_style" option.

  const char* text = "foo # bar\n"
                     "baz // qux\n"
                     "corge /* grault */\n"
                     "garply";
  const char* const ktokens[] = {"foo",  // "# bar" is ignored
                                 "baz", "/", "/", "qux",
                                 "corge", "/", "*", "grault", "*", "/",
                                 "garply"};

  // set up the tokenizer.
  testinputstream input(text, strlen(text), kblocksizes_case);
  testerrorcollector error_collector;
  tokenizer tokenizer(&input, &error_collector);
  tokenizer.set_comment_style(tokenizer::sh_comment_style);

  // advance through tokens and check that they are parsed as expected.
  for (int i = 0; i < google_arraysize(ktokens); i++) {
    expect_true(tokenizer.next());
    expect_eq(tokenizer.current().text, ktokens[i]);
  }

  // there should be no more input.
  expect_false(tokenizer.next());
  // there should be no errors.
  expect_true(error_collector.text_.empty());
}

#endif

// -------------------------------------------------------------------

// in each case, the input is expected to have two tokens named "prev" and
// "next" with comments in between.
struct doccommentcase {
  string input;

  const char* prev_trailing_comments;
  const char* detached_comments[10];
  const char* next_leading_comments;
};

inline ostream& operator<<(ostream& out,
                           const doccommentcase& test_case) {
  return out << cescape(test_case.input);
}

doccommentcase kdoccommentcases[] = {
  {
    "prev next",

    "",
    {},
    ""
      },

        {
      "prev /* ignored */ next",

      "",
      {},
      ""
        },

          {
        "prev // trailing comment\n"
            "next",

            " trailing comment\n",
            {},
            ""
          },

            {
          "prev\n"
              "// leading comment\n"
              "// line 2\n"
              "next",

              "",
              {},
              " leading comment\n"
              " line 2\n"
            },

              {
            "prev\n"
                "// trailing comment\n"
                "// line 2\n"
                "\n"
                "next",

                " trailing comment\n"
                " line 2\n",
                {},
                ""
              },

                {
              "prev // trailing comment\n"
                  "// leading comment\n"
                  "// line 2\n"
                  "next",

                  " trailing comment\n",
                  {},
                  " leading comment\n"
                  " line 2\n"
                },

                  {
                "prev /* trailing block comment */\n"
                    "/* leading block comment\n"
                    " * line 2\n"
                    " * line 3 */"
                    "next",

                    " trailing block comment ",
                    {},
                    " leading block comment\n"
                    " line 2\n"
                    " line 3 "
                  },

                    {
                  "prev\n"
                      "/* trailing block comment\n"
                      " * line 2\n"
                      " * line 3\n"
                      " */\n"
                      "/* leading block comment\n"
                      " * line 2\n"
                      " * line 3 */"
                      "next",

                      " trailing block comment\n"
                      " line 2\n"
                      " line 3\n",
                      {},
                      " leading block comment\n"
                      " line 2\n"
                      " line 3 "
                    },

                      {
                    "prev\n"
                        "// trailing comment\n"
                        "\n"
                        "// detached comment\n"
                        "// line 2\n"
                        "\n"
                        "// second detached comment\n"
                        "/* third detached comment\n"
                        " * line 2 */\n"
                        "// leading comment\n"
                        "next",

                        " trailing comment\n",
                        {
                      " detached comment\n"
                          " line 2\n",
                          " second detached comment\n",
                          " third detached comment\n"
                          " line 2 "
                        },
                          " leading comment\n"
                        },

                          {
                        "prev /**/\n"
                            "\n"
                            "// detached comment\n"
                            "\n"
                            "// leading comment\n"
                            "next",

                            "",
                            {
                          " detached comment\n"
                            },
                              " leading comment\n"
                            },

                              {
                            "prev /**/\n"
                                "// leading comment\n"
                                "next",

                                "",
                                {},
                                " leading comment\n"
                              },
                              };

test_2d(tokenizertest, doccomments, kdoccommentcases, kblocksizes) {
  // set up the tokenizer.
  testinputstream input(kdoccommentcases_case.input.data(),
                        kdoccommentcases_case.input.size(),
                        kblocksizes_case);
  testerrorcollector error_collector;
  tokenizer tokenizer(&input, &error_collector);

  // set up a second tokenizer where we'll pass all nulls to nextwithcomments().
  testinputstream input2(kdoccommentcases_case.input.data(),
                        kdoccommentcases_case.input.size(),
                        kblocksizes_case);
  tokenizer tokenizer2(&input2, &error_collector);

  tokenizer.next();
  tokenizer2.next();

  expect_eq("prev", tokenizer.current().text);
  expect_eq("prev", tokenizer2.current().text);

  string prev_trailing_comments;
  vector<string> detached_comments;
  string next_leading_comments;
  tokenizer.nextwithcomments(&prev_trailing_comments, &detached_comments,
                             &next_leading_comments);
  tokenizer2.nextwithcomments(null, null, null);
  expect_eq("next", tokenizer.current().text);
  expect_eq("next", tokenizer2.current().text);

  expect_eq(kdoccommentcases_case.prev_trailing_comments,
            prev_trailing_comments);

  for (int i = 0; i < detached_comments.size(); i++) {
    assert_lt(i, google_arraysize(kdoccommentcases));
    assert_true(kdoccommentcases_case.detached_comments[i] != null);
    expect_eq(kdoccommentcases_case.detached_comments[i],
              detached_comments[i]);
  }

  // verify that we matched all the detached comments.
  expect_eq(null,
      kdoccommentcases_case.detached_comments[detached_comments.size()]);

  expect_eq(kdoccommentcases_case.next_leading_comments,
            next_leading_comments);
}

// -------------------------------------------------------------------

// test parse helpers.  it's not really worth setting up a full data-driven
// test here.
test_f(tokenizertest, parseinteger) {
  expect_eq(0, parseinteger("0"));
  expect_eq(123, parseinteger("123"));
  expect_eq(0xabcdef12u, parseinteger("0xabcdef12"));
  expect_eq(0xabcdef12u, parseinteger("0xabcdef12"));
  expect_eq(kuint64max, parseinteger("0xffffffffffffffff"));
  expect_eq(01234567, parseinteger("01234567"));
  expect_eq(0x123, parseinteger("0x123"));

  // test invalid integers that may still be tokenized as integers.
  expect_eq(0, parseinteger("0x"));

  uint64 i;
#ifdef protobuf_hasdeath_test  // death tests do not work on windows yet
  // test invalid integers that will never be tokenized as integers.
  expect_debug_death(tokenizer::parseinteger("zxy", kuint64max, &i),
    "passed text that could not have been tokenized as an integer");
  expect_debug_death(tokenizer::parseinteger("1.2", kuint64max, &i),
    "passed text that could not have been tokenized as an integer");
  expect_debug_death(tokenizer::parseinteger("08", kuint64max, &i),
    "passed text that could not have been tokenized as an integer");
  expect_debug_death(tokenizer::parseinteger("0xg", kuint64max, &i),
    "passed text that could not have been tokenized as an integer");
  expect_debug_death(tokenizer::parseinteger("-1", kuint64max, &i),
    "passed text that could not have been tokenized as an integer");
#endif  // protobuf_hasdeath_test

  // test overflows.
  expect_true (tokenizer::parseinteger("0", 0, &i));
  expect_false(tokenizer::parseinteger("1", 0, &i));
  expect_true (tokenizer::parseinteger("1", 1, &i));
  expect_true (tokenizer::parseinteger("12345", 12345, &i));
  expect_false(tokenizer::parseinteger("12346", 12345, &i));
  expect_true (tokenizer::parseinteger("0xffffffffffffffff" , kuint64max, &i));
  expect_false(tokenizer::parseinteger("0x10000000000000000", kuint64max, &i));
}

test_f(tokenizertest, parsefloat) {
  expect_double_eq(1    , tokenizer::parsefloat("1."));
  expect_double_eq(1e3  , tokenizer::parsefloat("1e3"));
  expect_double_eq(1e3  , tokenizer::parsefloat("1e3"));
  expect_double_eq(1.5e3, tokenizer::parsefloat("1.5e3"));
  expect_double_eq(.1   , tokenizer::parsefloat(".1"));
  expect_double_eq(.25  , tokenizer::parsefloat(".25"));
  expect_double_eq(.1e3 , tokenizer::parsefloat(".1e3"));
  expect_double_eq(.25e3, tokenizer::parsefloat(".25e3"));
  expect_double_eq(.1e+3, tokenizer::parsefloat(".1e+3"));
  expect_double_eq(.1e-3, tokenizer::parsefloat(".1e-3"));
  expect_double_eq(5    , tokenizer::parsefloat("5"));
  expect_double_eq(6e-12, tokenizer::parsefloat("6e-12"));
  expect_double_eq(1.2  , tokenizer::parsefloat("1.2"));
  expect_double_eq(1.e2 , tokenizer::parsefloat("1.e2"));

  // test invalid integers that may still be tokenized as integers.
  expect_double_eq(1, tokenizer::parsefloat("1e"));
  expect_double_eq(1, tokenizer::parsefloat("1e-"));
  expect_double_eq(1, tokenizer::parsefloat("1.e"));

  // test 'f' suffix.
  expect_double_eq(1, tokenizer::parsefloat("1f"));
  expect_double_eq(1, tokenizer::parsefloat("1.0f"));
  expect_double_eq(1, tokenizer::parsefloat("1f"));

  // these should parse successfully even though they are out of range.
  // overflows become infinity and underflows become zero.
  expect_eq(     0.0, tokenizer::parsefloat("1e-9999999999999999999999999999"));
  expect_eq(huge_val, tokenizer::parsefloat("1e+9999999999999999999999999999"));

#ifdef protobuf_hasdeath_test  // death tests do not work on windows yet
  // test invalid integers that will never be tokenized as integers.
  expect_debug_death(tokenizer::parsefloat("zxy"),
    "passed text that could not have been tokenized as a float");
  expect_debug_death(tokenizer::parsefloat("1-e0"),
    "passed text that could not have been tokenized as a float");
  expect_debug_death(tokenizer::parsefloat("-1.0"),
    "passed text that could not have been tokenized as a float");
#endif  // protobuf_hasdeath_test
}

test_f(tokenizertest, parsestring) {
  string output;
  tokenizer::parsestring("'hello'", &output);
  expect_eq("hello", output);
  tokenizer::parsestring("\"blah\\nblah2\"", &output);
  expect_eq("blah\nblah2", output);
  tokenizer::parsestring("'\\1x\\1\\123\\739\\52\\334n\\3'", &output);
  expect_eq("\1x\1\123\739\52\334n\3", output);
  tokenizer::parsestring("'\\x20\\x4'", &output);
  expect_eq("\x20\x4", output);

  // test invalid strings that may still be tokenized as strings.
  tokenizer::parsestring("\"\\a\\l\\v\\t", &output);  // \l is invalid
  expect_eq("\a?\v\t", output);
  tokenizer::parsestring("'", &output);
  expect_eq("", output);
  tokenizer::parsestring("'\\", &output);
  expect_eq("\\", output);

  // experiment with unicode escapes. here are one-, two- and three-byte unicode
  // characters.
  tokenizer::parsestring("'\\u0024\\u00a2\\u20ac\\u00024b62xx'", &output);
  expect_eq("$垄鈧きx", output);
  // same thing encoded using utf16.
  tokenizer::parsestring("'\\u0024\\u00a2\\u20ac\\ud852\\udf62xx'", &output);
  expect_eq("$垄鈧きx", output);
  // here's some broken utf16; there's a head surrogate with no tail surrogate.
  // we just output this as if it were utf8; it's not a defined code point, but
  // it has a defined encoding.
  tokenizer::parsestring("'\\ud852xx'", &output);
  expect_eq("\xed\xa1\x92xx", output);
  // malformed escape: demons may fly out of the nose.
  tokenizer::parsestring("\\u0", &output);
  expect_eq("u0", output);

  // test invalid strings that will never be tokenized as strings.
#ifdef protobuf_hasdeath_test  // death tests do not work on windows yet
  expect_debug_death(tokenizer::parsestring("", &output),
    "passed text that could not have been tokenized as a string");
#endif  // protobuf_hasdeath_test
}

test_f(tokenizertest, parsestringappend) {
  // check that parsestring and parsestringappend differ.
  string output("stuff+");
  tokenizer::parsestringappend("'hello'", &output);
  expect_eq("stuff+hello", output);
  tokenizer::parsestring("'hello'", &output);
  expect_eq("hello", output);
}

// -------------------------------------------------------------------

// each case parses some input text, ignoring the tokens produced, and
// checks that the error output matches what is expected.
struct errorcase {
  string input;
  bool recoverable;  // true if the tokenizer should be able to recover and
                     // parse more tokens after seeing this error.  cases
                     // for which this is true must end with "foo" as
                     // the last token, which the test will check for.
  const char* errors;
};

inline ostream& operator<<(ostream& out,
                           const errorcase& test_case) {
  return out << cescape(test_case.input);
}

errorcase kerrorcases[] = {
  // string errors.
  { "'\\l' foo", true,
    "0:2: invalid escape sequence in string literal.\n" },
  { "'\\x' foo", true,
    "0:3: expected hex digits for escape sequence.\n" },
  { "'foo", false,
    "0:4: string literals cannot cross line boundaries.\n" },
  { "'bar\nfoo", true,
    "0:4: string literals cannot cross line boundaries.\n" },
  { "'\\u01' foo", true,
    "0:5: expected four hex digits for \\u escape sequence.\n" },
  { "'\\u01' foo", true,
    "0:5: expected four hex digits for \\u escape sequence.\n" },
  { "'\\uxyz' foo", true,
    "0:3: expected four hex digits for \\u escape sequence.\n" },

  // integer errors.
  { "123foo", true,
    "0:3: need space between number and identifier.\n" },

  // hex/octal errors.
  { "0x foo", true,
    "0:2: \"0x\" must be followed by hex digits.\n" },
  { "0541823 foo", true,
    "0:4: numbers starting with leading zero must be in octal.\n" },
  { "0x123z foo", true,
    "0:5: need space between number and identifier.\n" },
  { "0x123.4 foo", true,
    "0:5: hex and octal numbers must be integers.\n" },
  { "0123.4 foo", true,
    "0:4: hex and octal numbers must be integers.\n" },

  // float errors.
  { "1e foo", true,
    "0:2: \"e\" must be followed by exponent.\n" },
  { "1e- foo", true,
    "0:3: \"e\" must be followed by exponent.\n" },
  { "1.2.3 foo", true,
    "0:3: already saw decimal point or exponent; can't have another one.\n" },
  { "1e2.3 foo", true,
    "0:3: already saw decimal point or exponent; can't have another one.\n" },
  { "a.1 foo", true,
    "0:1: need space between identifier and decimal point.\n" },
  // allow_f_after_float not enabled, so this should be an error.
  { "1.0f foo", true,
    "0:3: need space between number and identifier.\n" },

  // block comment errors.
  { "/*", false,
    "0:2: end-of-file inside block comment.\n"
    "0:0:   comment started here.\n"},
  { "/*/*/ foo", true,
    "0:3: \"/*\" inside block comment.  block comments cannot be nested.\n"},

  // control characters.  multiple consecutive control characters should only
  // produce one error.
  { "\b foo", true,
    "0:0: invalid control characters encountered in text.\n" },
  { "\b\b foo", true,
    "0:0: invalid control characters encountered in text.\n" },

  // check that control characters at end of input don't result in an
  // infinite loop.
  { "\b", false,
    "0:0: invalid control characters encountered in text.\n" },

  // check recovery from '\0'.  we have to explicitly specify the length of
  // these strings because otherwise the string constructor will just call
  // strlen() which will see the first '\0' and think that is the end of the
  // string.
  { string("\0foo", 4), true,
    "0:0: invalid control characters encountered in text.\n" },
  { string("\0\0foo", 5), true,
    "0:0: invalid control characters encountered in text.\n" },
};

test_2d(tokenizertest, errors, kerrorcases, kblocksizes) {
  // set up the tokenizer.
  testinputstream input(kerrorcases_case.input.data(),
                        kerrorcases_case.input.size(),
                        kblocksizes_case);
  testerrorcollector error_collector;
  tokenizer tokenizer(&input, &error_collector);

  // ignore all input, except remember if the last token was "foo".
  bool last_was_foo = false;
  while (tokenizer.next()) {
    last_was_foo = tokenizer.current().text == "foo";
  }

  // check that the errors match what was expected.
  expect_eq(kerrorcases_case.errors, error_collector.text_);

  // if the error was recoverable, make sure we saw "foo" after it.
  if (kerrorcases_case.recoverable) {
    expect_true(last_was_foo);
  }
}

// -------------------------------------------------------------------

test_1d(tokenizertest, backupondestruction, kblocksizes) {
  string text = "foo bar";
  testinputstream input(text.data(), text.size(), kblocksizes_case);

  // create a tokenizer, read one token, then destroy it.
  {
    testerrorcollector error_collector;
    tokenizer tokenizer(&input, &error_collector);

    tokenizer.next();
  }

  // only "foo" should have been read.
  expect_eq(strlen("foo"), input.bytecount());
}


}  // namespace
}  // namespace io
}  // namespace protobuf
}  // namespace google
