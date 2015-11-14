from __future__ import unicode_literals, print_function, absolute_import, division, generators, nested_scopes
import sys
import logging

import ply.lex

logger = logging.getlogger(__name__)

class jsonpathlexererror(exception):
    pass

class jsonpathlexer(object):
    '''
    a lexical analyzer for jsonpath.
    '''

    def __init__(self, debug=false):
        self.debug = debug
        if self.__doc__ == none:
            raise jsonpathlexererror('docstrings have been removed! by design of ply, jsonpath-rw requires docstrings. you must not use pythonoptimize=2 or python -oo.')

    def tokenize(self, string):
        '''
        maps a string to an iterator over tokens. in other words: [char] -> [token]
        '''

        new_lexer = ply.lex.lex(module=self, debug=self.debug, errorlog=logger)
        new_lexer.latest_newline = 0
        new_lexer.string_value = none
        new_lexer.input(string)

        while true:
            t = new_lexer.token()
            if t is none: break
            t.col = t.lexpos - new_lexer.latest_newline
            yield t

        if new_lexer.string_value is not none:
            raise jsonpathlexererror('unexpected eof in string literal or identifier')

    # ============== ply lexer specification ==================
    #
    # this probably should be private but:
    #   - the parser requires access to `tokens` (perhaps they should be defined in a third, shared dependency)
    #   - things like `literals` might be a legitimate part of the public interface.
    #
    # anyhow, it is pythonic to give some rope to hang oneself with :-)

    literals = ['*', '.', '[', ']', '(', ')', '$', ',', ':', '|', '&']

    reserved_words = { 'where': 'where' }

    tokens = ['doubledot', 'number', 'id', 'named_operator'] + list(reserved_words.values())

    states = [ ('singlequote', 'exclusive'),
               ('doublequote', 'exclusive'),
               ('backquote', 'exclusive') ]

    # normal lexing, rather easy
    t_doubledot = r'\.\.'
    t_ignore = ' \t'

    def t_id(self, t):
        r'[a-za-z_@][a-za-z0-9_@\-]*'
        t.type = self.reserved_words.get(t.value, 'id')
        return t

    def t_number(self, t):
        r'-?\d+'
        t.value = int(t.value)
        return t


    # single-quoted strings
    t_singlequote_ignore = ''
    def t_singlequote(self, t):
        r"'"
        t.lexer.string_start = t.lexer.lexpos
        t.lexer.string_value = ''
        t.lexer.push_state('singlequote')

    def t_singlequote_content(self, t):
        r"[^'\\]+"
        t.lexer.string_value += t.value

    def t_singlequote_escape(self, t):
        r'\\.'
        t.lexer.string_value += t.value[1]

    def t_singlequote_end(self, t):
        r"'"
        t.value = t.lexer.string_value
        t.type = 'id'
        t.lexer.string_value = none
        t.lexer.pop_state()
        return t

    def t_singlequote_error(self, t):
        raise jsonpathlexererror('error on line %s, col %s while lexing singlequoted field: unexpected character: %s ' % (t.lexer.lineno, t.lexpos - t.lexer.latest_newline, t.value[0]))


    # double-quoted strings
    t_doublequote_ignore = ''
    def t_doublequote(self, t):
        r'"'
        t.lexer.string_start = t.lexer.lexpos
        t.lexer.string_value = ''
        t.lexer.push_state('doublequote')

    def t_doublequote_content(self, t):
        r'[^"\\]+'
        t.lexer.string_value += t.value

    def t_doublequote_escape(self, t):
        r'\\.'
        t.lexer.string_value += t.value[1]

    def t_doublequote_end(self, t):
        r'"'
        t.value = t.lexer.string_value
        t.type = 'id'
        t.lexer.string_value = none
        t.lexer.pop_state()
        return t

    def t_doublequote_error(self, t):
        raise jsonpathlexererror('error on line %s, col %s while lexing doublequoted field: unexpected character: %s ' % (t.lexer.lineno, t.lexpos - t.lexer.latest_newline, t.value[0]))


    # back-quoted "magic" operators
    t_backquote_ignore = ''
    def t_backquote(self, t):
        r'`'
        t.lexer.string_start = t.lexer.lexpos
        t.lexer.string_value = ''
        t.lexer.push_state('backquote')

    def t_backquote_escape(self, t):
        r'\\.'
        t.lexer.string_value += t.value[1]

    def t_backquote_content(self, t):
        r"[^`\\]+"
        t.lexer.string_value += t.value

    def t_backquote_end(self, t):
        r'`'
        t.value = t.lexer.string_value
        t.type = 'named_operator'
        t.lexer.string_value = none
        t.lexer.pop_state()
        return t

    def t_backquote_error(self, t):
        raise jsonpathlexererror('error on line %s, col %s while lexing backquoted operator: unexpected character: %s ' % (t.lexer.lineno, t.lexpos - t.lexer.latest_newline, t.value[0]))


    # counting lines, handling errors
    def t_newline(self, t):
        r'\n'
        t.lexer.lineno += 1
        t.lexer.latest_newline = t.lexpos

    def t_error(self, t):
        raise jsonpathlexererror('error on line %s, col %s: unexpected character: %s ' % (t.lexer.lineno, t.lexpos - t.lexer.latest_newline, t.value[0]))

if __name__ == '__main__':
    logging.basicconfig()
    lexer = jsonpathlexer(debug=true)
    for token in lexer.tokenize(sys.stdin.read()):
        print('%-20s%s' % (token.value, token.type))
