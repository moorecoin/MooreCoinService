#! /usr/bin/python
#
# protocol buffers - google's data interchange format
# copyright 2008 google inc.  all rights reserved.
# http://code.google.com/p/protobuf/
#
# redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * neither the name of google inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# this software is provided by the copyright holders and contributors
# "as is" and any express or implied warranties, including, but not
# limited to, the implied warranties of merchantability and fitness for
# a particular purpose are disclaimed. in no event shall the copyright
# owner or contributors be liable for any direct, indirect, incidental,
# special, exemplary, or consequential damages (including, but not
# limited to, procurement of substitute goods or services; loss of use,
# data, or profits; or business interruption) however caused and on any
# theory of liability, whether in contract, strict liability, or tort
# (including negligence or otherwise) arising in any way out of the use
# of this software, even if advised of the possibility of such damage.

"""test for google.protobuf.text_format."""

__author__ = 'kenton@google.com (kenton varda)'

import difflib
import re

import unittest
from google.protobuf import text_format
from google.protobuf.internal import test_util
from google.protobuf import unittest_pb2
from google.protobuf import unittest_mset_pb2


class textformattest(unittest.testcase):
  def readgolden(self, golden_filename):
    f = test_util.goldenfile(golden_filename)
    golden_lines = f.readlines()
    f.close()
    return golden_lines

  def comparetogoldenfile(self, text, golden_filename):
    golden_lines = self.readgolden(golden_filename)
    self.comparetogoldenlines(text, golden_lines)

  def comparetogoldentext(self, text, golden_text):
    self.comparetogoldenlines(text, golden_text.splitlines(1))

  def comparetogoldenlines(self, text, golden_lines):
    actual_lines = text.splitlines(1)
    self.assertequal(golden_lines, actual_lines,
      "text doesn't match golden.  diff:\n" +
      ''.join(difflib.ndiff(golden_lines, actual_lines)))

  def testprintallfields(self):
    message = unittest_pb2.testalltypes()
    test_util.setallfields(message)
    self.comparetogoldenfile(
      self.removeredundantzeros(text_format.messagetostring(message)),
      'text_format_unittest_data.txt')

  def testprintallextensions(self):
    message = unittest_pb2.testallextensions()
    test_util.setallextensions(message)
    self.comparetogoldenfile(
      self.removeredundantzeros(text_format.messagetostring(message)),
      'text_format_unittest_extensions_data.txt')

  def testprintmessageset(self):
    message = unittest_mset_pb2.testmessagesetcontainer()
    ext1 = unittest_mset_pb2.testmessagesetextension1.message_set_extension
    ext2 = unittest_mset_pb2.testmessagesetextension2.message_set_extension
    message.message_set.extensions[ext1].i = 23
    message.message_set.extensions[ext2].str = 'foo'
    self.comparetogoldentext(text_format.messagetostring(message),
      'message_set {\n'
      '  [protobuf_unittest.testmessagesetextension1] {\n'
      '    i: 23\n'
      '  }\n'
      '  [protobuf_unittest.testmessagesetextension2] {\n'
      '    str: \"foo\"\n'
      '  }\n'
      '}\n')

  def testprintbadenumvalue(self):
    message = unittest_pb2.testalltypes()
    message.optional_nested_enum = 100
    message.optional_foreign_enum = 101
    message.optional_import_enum = 102
    self.comparetogoldentext(
        text_format.messagetostring(message),
        'optional_nested_enum: 100\n'
        'optional_foreign_enum: 101\n'
        'optional_import_enum: 102\n')

  def testprintbadenumvalueextensions(self):
    message = unittest_pb2.testallextensions()
    message.extensions[unittest_pb2.optional_nested_enum_extension] = 100
    message.extensions[unittest_pb2.optional_foreign_enum_extension] = 101
    message.extensions[unittest_pb2.optional_import_enum_extension] = 102
    self.comparetogoldentext(
        text_format.messagetostring(message),
        '[protobuf_unittest.optional_nested_enum_extension]: 100\n'
        '[protobuf_unittest.optional_foreign_enum_extension]: 101\n'
        '[protobuf_unittest.optional_import_enum_extension]: 102\n')

  def testprintexotic(self):
    message = unittest_pb2.testalltypes()
    message.repeated_int64.append(-9223372036854775808)
    message.repeated_uint64.append(18446744073709551615)
    message.repeated_double.append(123.456)
    message.repeated_double.append(1.23e22)
    message.repeated_double.append(1.23e-18)
    message.repeated_string.append('\000\001\a\b\f\n\r\t\v\\\'"')
    message.repeated_string.append(u'\u00fc\ua71f')
    self.comparetogoldentext(
      self.removeredundantzeros(text_format.messagetostring(message)),
      'repeated_int64: -9223372036854775808\n'
      'repeated_uint64: 18446744073709551615\n'
      'repeated_double: 123.456\n'
      'repeated_double: 1.23e+22\n'
      'repeated_double: 1.23e-18\n'
      'repeated_string: '
        '"\\000\\001\\007\\010\\014\\n\\r\\t\\013\\\\\\\'\\""\n'
      'repeated_string: "\\303\\274\\352\\234\\237"\n')

  def testprintnestedmessageasoneline(self):
    message = unittest_pb2.testalltypes()
    msg = message.repeated_nested_message.add()
    msg.bb = 42;
    self.comparetogoldentext(
        text_format.messagetostring(message, as_one_line=true),
        'repeated_nested_message { bb: 42 }')

  def testprintrepeatedfieldsasoneline(self):
    message = unittest_pb2.testalltypes()
    message.repeated_int32.append(1)
    message.repeated_int32.append(1)
    message.repeated_int32.append(3)
    message.repeated_string.append("google")
    message.repeated_string.append("zurich")
    self.comparetogoldentext(
        text_format.messagetostring(message, as_one_line=true),
        'repeated_int32: 1 repeated_int32: 1 repeated_int32: 3 '
        'repeated_string: "google" repeated_string: "zurich"')

  def testprintnestednewlineinstringasoneline(self):
    message = unittest_pb2.testalltypes()
    message.optional_string = "a\nnew\nline"
    self.comparetogoldentext(
        text_format.messagetostring(message, as_one_line=true),
        'optional_string: "a\\nnew\\nline"')

  def testprintmessagesetasoneline(self):
    message = unittest_mset_pb2.testmessagesetcontainer()
    ext1 = unittest_mset_pb2.testmessagesetextension1.message_set_extension
    ext2 = unittest_mset_pb2.testmessagesetextension2.message_set_extension
    message.message_set.extensions[ext1].i = 23
    message.message_set.extensions[ext2].str = 'foo'
    self.comparetogoldentext(
        text_format.messagetostring(message, as_one_line=true),
        'message_set {'
        ' [protobuf_unittest.testmessagesetextension1] {'
        ' i: 23'
        ' }'
        ' [protobuf_unittest.testmessagesetextension2] {'
        ' str: \"foo\"'
        ' }'
        ' }')

  def testprintexoticasoneline(self):
    message = unittest_pb2.testalltypes()
    message.repeated_int64.append(-9223372036854775808)
    message.repeated_uint64.append(18446744073709551615)
    message.repeated_double.append(123.456)
    message.repeated_double.append(1.23e22)
    message.repeated_double.append(1.23e-18)
    message.repeated_string.append('\000\001\a\b\f\n\r\t\v\\\'"')
    message.repeated_string.append(u'\u00fc\ua71f')
    self.comparetogoldentext(
      self.removeredundantzeros(
          text_format.messagetostring(message, as_one_line=true)),
      'repeated_int64: -9223372036854775808'
      ' repeated_uint64: 18446744073709551615'
      ' repeated_double: 123.456'
      ' repeated_double: 1.23e+22'
      ' repeated_double: 1.23e-18'
      ' repeated_string: '
      '"\\000\\001\\007\\010\\014\\n\\r\\t\\013\\\\\\\'\\""'
      ' repeated_string: "\\303\\274\\352\\234\\237"')

  def testroundtripexoticasoneline(self):
    message = unittest_pb2.testalltypes()
    message.repeated_int64.append(-9223372036854775808)
    message.repeated_uint64.append(18446744073709551615)
    message.repeated_double.append(123.456)
    message.repeated_double.append(1.23e22)
    message.repeated_double.append(1.23e-18)
    message.repeated_string.append('\000\001\a\b\f\n\r\t\v\\\'"')
    message.repeated_string.append(u'\u00fc\ua71f')

    # test as_utf8 = false.
    wire_text = text_format.messagetostring(
        message, as_one_line=true, as_utf8=false)
    parsed_message = unittest_pb2.testalltypes()
    text_format.merge(wire_text, parsed_message)
    self.assertequals(message, parsed_message)

    # test as_utf8 = true.
    wire_text = text_format.messagetostring(
        message, as_one_line=true, as_utf8=true)
    parsed_message = unittest_pb2.testalltypes()
    text_format.merge(wire_text, parsed_message)
    self.assertequals(message, parsed_message)

  def testprintrawutf8string(self):
    message = unittest_pb2.testalltypes()
    message.repeated_string.append(u'\u00fc\ua71f')
    text = text_format.messagetostring(message, as_utf8 = true)
    self.comparetogoldentext(text, 'repeated_string: "\303\274\352\234\237"\n')
    parsed_message = unittest_pb2.testalltypes()
    text_format.merge(text, parsed_message)
    self.assertequals(message, parsed_message)

  def testmessagetostring(self):
    message = unittest_pb2.foreignmessage()
    message.c = 123
    self.assertequal('c: 123\n', str(message))

  def removeredundantzeros(self, text):
    # some platforms print 1e+5 as 1e+005.  this is fine, but we need to remove
    # these zeros in order to match the golden file.
    text = text.replace('e+0','e+').replace('e+0','e+') \
               .replace('e-0','e-').replace('e-0','e-')
    # floating point fields are printed with .0 suffix even if they are
    # actualy integer numbers.
    text = re.compile('\.0$', re.multiline).sub('', text)
    return text

  def testmergegolden(self):
    golden_text = '\n'.join(self.readgolden('text_format_unittest_data.txt'))
    parsed_message = unittest_pb2.testalltypes()
    text_format.merge(golden_text, parsed_message)

    message = unittest_pb2.testalltypes()
    test_util.setallfields(message)
    self.assertequals(message, parsed_message)

  def testmergegoldenextensions(self):
    golden_text = '\n'.join(self.readgolden(
        'text_format_unittest_extensions_data.txt'))
    parsed_message = unittest_pb2.testallextensions()
    text_format.merge(golden_text, parsed_message)

    message = unittest_pb2.testallextensions()
    test_util.setallextensions(message)
    self.assertequals(message, parsed_message)

  def testmergeallfields(self):
    message = unittest_pb2.testalltypes()
    test_util.setallfields(message)
    ascii_text = text_format.messagetostring(message)

    parsed_message = unittest_pb2.testalltypes()
    text_format.merge(ascii_text, parsed_message)
    self.assertequal(message, parsed_message)
    test_util.expectallfieldsset(self, message)

  def testmergeallextensions(self):
    message = unittest_pb2.testallextensions()
    test_util.setallextensions(message)
    ascii_text = text_format.messagetostring(message)

    parsed_message = unittest_pb2.testallextensions()
    text_format.merge(ascii_text, parsed_message)
    self.assertequal(message, parsed_message)

  def testmergemessageset(self):
    message = unittest_pb2.testalltypes()
    text = ('repeated_uint64: 1\n'
            'repeated_uint64: 2\n')
    text_format.merge(text, message)
    self.assertequal(1, message.repeated_uint64[0])
    self.assertequal(2, message.repeated_uint64[1])

    message = unittest_mset_pb2.testmessagesetcontainer()
    text = ('message_set {\n'
            '  [protobuf_unittest.testmessagesetextension1] {\n'
            '    i: 23\n'
            '  }\n'
            '  [protobuf_unittest.testmessagesetextension2] {\n'
            '    str: \"foo\"\n'
            '  }\n'
            '}\n')
    text_format.merge(text, message)
    ext1 = unittest_mset_pb2.testmessagesetextension1.message_set_extension
    ext2 = unittest_mset_pb2.testmessagesetextension2.message_set_extension
    self.assertequals(23, message.message_set.extensions[ext1].i)
    self.assertequals('foo', message.message_set.extensions[ext2].str)

  def testmergeexotic(self):
    message = unittest_pb2.testalltypes()
    text = ('repeated_int64: -9223372036854775808\n'
            'repeated_uint64: 18446744073709551615\n'
            'repeated_double: 123.456\n'
            'repeated_double: 1.23e+22\n'
            'repeated_double: 1.23e-18\n'
            'repeated_string: \n'
            '"\\000\\001\\007\\010\\014\\n\\r\\t\\013\\\\\\\'\\""\n'
            'repeated_string: "foo" \'corge\' "grault"\n'
            'repeated_string: "\\303\\274\\352\\234\\237"\n'
            'repeated_string: "\\xc3\\xbc"\n'
            'repeated_string: "\xc3\xbc"\n')
    text_format.merge(text, message)

    self.assertequal(-9223372036854775808, message.repeated_int64[0])
    self.assertequal(18446744073709551615, message.repeated_uint64[0])
    self.assertequal(123.456, message.repeated_double[0])
    self.assertequal(1.23e22, message.repeated_double[1])
    self.assertequal(1.23e-18, message.repeated_double[2])
    self.assertequal(
        '\000\001\a\b\f\n\r\t\v\\\'"', message.repeated_string[0])
    self.assertequal('foocorgegrault', message.repeated_string[1])
    self.assertequal(u'\u00fc\ua71f', message.repeated_string[2])
    self.assertequal(u'\u00fc', message.repeated_string[3])

  def testmergeemptytext(self):
    message = unittest_pb2.testalltypes()
    text = ''
    text_format.merge(text, message)
    self.assertequals(unittest_pb2.testalltypes(), message)

  def testmergeinvalidutf8(self):
    message = unittest_pb2.testalltypes()
    text = 'repeated_string: "\\xc3\\xc3"'
    self.assertraises(text_format.parseerror, text_format.merge, text, message)

  def testmergesingleword(self):
    message = unittest_pb2.testalltypes()
    text = 'foo'
    self.assertraiseswithmessage(
        text_format.parseerror,
        ('1:1 : message type "protobuf_unittest.testalltypes" has no field named '
         '"foo".'),
        text_format.merge, text, message)

  def testmergeunknownfield(self):
    message = unittest_pb2.testalltypes()
    text = 'unknown_field: 8\n'
    self.assertraiseswithmessage(
        text_format.parseerror,
        ('1:1 : message type "protobuf_unittest.testalltypes" has no field named '
         '"unknown_field".'),
        text_format.merge, text, message)

  def testmergebadextension(self):
    message = unittest_pb2.testallextensions()
    text = '[unknown_extension]: 8\n'
    self.assertraiseswithmessage(
        text_format.parseerror,
        '1:2 : extension "unknown_extension" not registered.',
        text_format.merge, text, message)
    message = unittest_pb2.testalltypes()
    self.assertraiseswithmessage(
        text_format.parseerror,
        ('1:2 : message type "protobuf_unittest.testalltypes" does not have '
         'extensions.'),
        text_format.merge, text, message)

  def testmergegroupnotclosed(self):
    message = unittest_pb2.testalltypes()
    text = 'repeatedgroup: <'
    self.assertraiseswithmessage(
        text_format.parseerror, '1:16 : expected ">".',
        text_format.merge, text, message)

    text = 'repeatedgroup: {'
    self.assertraiseswithmessage(
        text_format.parseerror, '1:16 : expected "}".',
        text_format.merge, text, message)

  def testmergeemptygroup(self):
    message = unittest_pb2.testalltypes()
    text = 'optionalgroup: {}'
    text_format.merge(text, message)
    self.asserttrue(message.hasfield('optionalgroup'))

    message.clear()

    message = unittest_pb2.testalltypes()
    text = 'optionalgroup: <>'
    text_format.merge(text, message)
    self.asserttrue(message.hasfield('optionalgroup'))

  def testmergebadenumvalue(self):
    message = unittest_pb2.testalltypes()
    text = 'optional_nested_enum: barr'
    self.assertraiseswithmessage(
        text_format.parseerror,
        ('1:23 : enum type "protobuf_unittest.testalltypes.nestedenum" '
         'has no value named barr.'),
        text_format.merge, text, message)

    message = unittest_pb2.testalltypes()
    text = 'optional_nested_enum: 100'
    self.assertraiseswithmessage(
        text_format.parseerror,
        ('1:23 : enum type "protobuf_unittest.testalltypes.nestedenum" '
         'has no value with number 100.'),
        text_format.merge, text, message)

  def testmergebadintvalue(self):
    message = unittest_pb2.testalltypes()
    text = 'optional_int32: bork'
    self.assertraiseswithmessage(
        text_format.parseerror,
        ('1:17 : couldn\'t parse integer: bork'),
        text_format.merge, text, message)

  def testmergestringfieldunescape(self):
    message = unittest_pb2.testalltypes()
    text = r'''repeated_string: "\xf\x62"
               repeated_string: "\\xf\\x62"
               repeated_string: "\\\xf\\\x62"
               repeated_string: "\\\\xf\\\\x62"
               repeated_string: "\\\\\xf\\\\\x62"
               repeated_string: "\x5cx20"'''
    text_format.merge(text, message)

    slash = '\\'
    self.assertequal('\x0fb', message.repeated_string[0])
    self.assertequal(slash + 'xf' + slash + 'x62', message.repeated_string[1])
    self.assertequal(slash + '\x0f' + slash + 'b', message.repeated_string[2])
    self.assertequal(slash + slash + 'xf' + slash + slash + 'x62',
                     message.repeated_string[3])
    self.assertequal(slash + slash + '\x0f' + slash + slash + 'b',
                     message.repeated_string[4])
    self.assertequal(slash + 'x20', message.repeated_string[5])

  def assertraiseswithmessage(self, e_class, e, func, *args, **kwargs):
    """same as assertraises, but also compares the exception message."""
    if hasattr(e_class, '__name__'):
      exc_name = e_class.__name__
    else:
      exc_name = str(e_class)

    try:
      func(*args, **kwargs)
    except e_class as expr:
      if str(expr) != e:
        msg = '%s raised, but with wrong message: "%s" instead of "%s"'
        raise self.failureexception(msg % (exc_name,
                                           str(expr).encode('string_escape'),
                                           e.encode('string_escape')))
      return
    else:
      raise self.failureexception('%s not raised' % exc_name)


class tokenizertest(unittest.testcase):

  def testsimpletokencases(self):
    text = ('identifier1:"string1"\n     \n\n'
            'identifier2 : \n \n123  \n  identifier3 :\'string\'\n'
            'identifier_4 : 1.1e+2 id5:-0.23 id6:\'aaaa\\\'bbbb\'\n'
            'id7 : "aa\\"bb"\n\n\n\n id8: {a:inf b:-inf c:true d:false}\n'
            'id9: 22 id10: -111111111111111111 id11: -22\n'
            'id12: 2222222222222222222 id13: 1.23456f id14: 1.2e+2f '
            'false_bool:  0 true_bool:t \n true_bool1:  1 false_bool1:f ' )
    tokenizer = text_format._tokenizer(text)
    methods = [(tokenizer.consumeidentifier, 'identifier1'),
               ':',
               (tokenizer.consumestring, 'string1'),
               (tokenizer.consumeidentifier, 'identifier2'),
               ':',
               (tokenizer.consumeint32, 123),
               (tokenizer.consumeidentifier, 'identifier3'),
               ':',
               (tokenizer.consumestring, 'string'),
               (tokenizer.consumeidentifier, 'identifier_4'),
               ':',
               (tokenizer.consumefloat, 1.1e+2),
               (tokenizer.consumeidentifier, 'id5'),
               ':',
               (tokenizer.consumefloat, -0.23),
               (tokenizer.consumeidentifier, 'id6'),
               ':',
               (tokenizer.consumestring, 'aaaa\'bbbb'),
               (tokenizer.consumeidentifier, 'id7'),
               ':',
               (tokenizer.consumestring, 'aa\"bb'),
               (tokenizer.consumeidentifier, 'id8'),
               ':',
               '{',
               (tokenizer.consumeidentifier, 'a'),
               ':',
               (tokenizer.consumefloat, float('inf')),
               (tokenizer.consumeidentifier, 'b'),
               ':',
               (tokenizer.consumefloat, -float('inf')),
               (tokenizer.consumeidentifier, 'c'),
               ':',
               (tokenizer.consumebool, true),
               (tokenizer.consumeidentifier, 'd'),
               ':',
               (tokenizer.consumebool, false),
               '}',
               (tokenizer.consumeidentifier, 'id9'),
               ':',
               (tokenizer.consumeuint32, 22),
               (tokenizer.consumeidentifier, 'id10'),
               ':',
               (tokenizer.consumeint64, -111111111111111111),
               (tokenizer.consumeidentifier, 'id11'),
               ':',
               (tokenizer.consumeint32, -22),
               (tokenizer.consumeidentifier, 'id12'),
               ':',
               (tokenizer.consumeuint64, 2222222222222222222),
               (tokenizer.consumeidentifier, 'id13'),
               ':',
               (tokenizer.consumefloat, 1.23456),
               (tokenizer.consumeidentifier, 'id14'),
               ':',
               (tokenizer.consumefloat, 1.2e+2),
               (tokenizer.consumeidentifier, 'false_bool'),
               ':',
               (tokenizer.consumebool, false),
               (tokenizer.consumeidentifier, 'true_bool'),
               ':',
               (tokenizer.consumebool, true),
               (tokenizer.consumeidentifier, 'true_bool1'),
               ':',
               (tokenizer.consumebool, true),
               (tokenizer.consumeidentifier, 'false_bool1'),
               ':',
               (tokenizer.consumebool, false)]

    i = 0
    while not tokenizer.atend():
      m = methods[i]
      if type(m) == str:
        token = tokenizer.token
        self.assertequal(token, m)
        tokenizer.nexttoken()
      else:
        self.assertequal(m[1], m[0]())
      i += 1

  def testconsumeintegers(self):
    # this test only tests the failures in the integer parsing methods as well
    # as the '0' special cases.
    int64_max = (1 << 63) - 1
    uint32_max = (1 << 32) - 1
    text = '-1 %d %d' % (uint32_max + 1, int64_max + 1)
    tokenizer = text_format._tokenizer(text)
    self.assertraises(text_format.parseerror, tokenizer.consumeuint32)
    self.assertraises(text_format.parseerror, tokenizer.consumeuint64)
    self.assertequal(-1, tokenizer.consumeint32())

    self.assertraises(text_format.parseerror, tokenizer.consumeuint32)
    self.assertraises(text_format.parseerror, tokenizer.consumeint32)
    self.assertequal(uint32_max + 1, tokenizer.consumeint64())

    self.assertraises(text_format.parseerror, tokenizer.consumeint64)
    self.assertequal(int64_max + 1, tokenizer.consumeuint64())
    self.asserttrue(tokenizer.atend())

    text = '-0 -0 0 0'
    tokenizer = text_format._tokenizer(text)
    self.assertequal(0, tokenizer.consumeuint32())
    self.assertequal(0, tokenizer.consumeuint64())
    self.assertequal(0, tokenizer.consumeuint32())
    self.assertequal(0, tokenizer.consumeuint64())
    self.asserttrue(tokenizer.atend())

  def testconsumebytestring(self):
    text = '"string1\''
    tokenizer = text_format._tokenizer(text)
    self.assertraises(text_format.parseerror, tokenizer.consumebytestring)

    text = 'string1"'
    tokenizer = text_format._tokenizer(text)
    self.assertraises(text_format.parseerror, tokenizer.consumebytestring)

    text = '\n"\\xt"'
    tokenizer = text_format._tokenizer(text)
    self.assertraises(text_format.parseerror, tokenizer.consumebytestring)

    text = '\n"\\"'
    tokenizer = text_format._tokenizer(text)
    self.assertraises(text_format.parseerror, tokenizer.consumebytestring)

    text = '\n"\\x"'
    tokenizer = text_format._tokenizer(text)
    self.assertraises(text_format.parseerror, tokenizer.consumebytestring)

  def testconsumebool(self):
    text = 'not-a-bool'
    tokenizer = text_format._tokenizer(text)
    self.assertraises(text_format.parseerror, tokenizer.consumebool)


if __name__ == '__main__':
  unittest.main()
