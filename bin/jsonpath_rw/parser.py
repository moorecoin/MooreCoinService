from __future__ import print_function, absolute_import, division, generators, nested_scopes
import sys
import os.path
import logging

import ply.yacc

from jsonpath_rw.jsonpath import *
from jsonpath_rw.lexer import jsonpathlexer

logger = logging.getlogger(__name__)

def parse(string):
    return jsonpathparser().parse(string)

class jsonpathparser(object):
    '''
    an lalr-parser for jsonpath
    '''
    
    tokens = jsonpathlexer.tokens

    def __init__(self, debug=false, lexer_class=none):
        if self.__doc__ == none:
            raise exception('docstrings have been removed! by design of ply, jsonpath-rw requires docstrings. you must not use pythonoptimize=2 or python -oo.')

        self.debug = debug
        self.lexer_class = lexer_class or jsonpathlexer # crufty but works around statefulness in ply

    def parse(self, string, lexer = none):
        lexer = lexer or self.lexer_class()
        return self.parse_token_stream(lexer.tokenize(string))

    def parse_token_stream(self, token_iterator, start_symbol='jsonpath'):

        # since ply has some crufty aspects and dumps files, we try to keep them local
        # however, we need to derive the name of the output python file :-/
        output_directory = os.path.dirname(__file__)
        try:
            module_name = os.path.splitext(os.path.split(__file__)[1])[0]
        except:
            module_name = __name__
        
        parsing_table_module = '_'.join([module_name, start_symbol, 'parsetab'])

        # and we regenerate the parse table every time; it doesn't actually take that long!
        new_parser = ply.yacc.yacc(module=self,
                                   debug=self.debug,
                                   tabmodule = parsing_table_module,
                                   outputdir = output_directory,
                                   write_tables=0,
                                   start = start_symbol,
                                   errorlog = logger)

        return new_parser.parse(lexer = iteratortotokenstream(token_iterator))

    # ===================== ply parser specification =====================
    
    precedence = [
        ('left', ','),
        ('left', 'doubledot'),
        ('left', '.'),
        ('left', '|'),
        ('left', '&'),
        ('left', 'where'),
    ]

    def p_error(self, t):
        raise exception('parse error at %s:%s near token %s (%s)' % (t.lineno, t.col, t.value, t.type)) 

    def p_jsonpath_binop(self, p):
        """jsonpath : jsonpath '.' jsonpath 
                    | jsonpath doubledot jsonpath
                    | jsonpath where jsonpath
                    | jsonpath '|' jsonpath
                    | jsonpath '&' jsonpath"""
        op = p[2]

        if op == '.':
            p[0] = child(p[1], p[3])
        elif op == '..':
            p[0] = descendants(p[1], p[3])
        elif op == 'where':
            p[0] = where(p[1], p[3])
        elif op == '|':
            p[0] = union(p[1], p[3])
        elif op == '&':
            p[0] = intersect(p[1], p[3])

    def p_jsonpath_fields(self, p):
        "jsonpath : fields_or_any"
        p[0] = fields(*p[1])

    def p_jsonpath_named_operator(self, p):
        "jsonpath : named_operator"
        if p[1] == 'this':
            p[0] = this()
        elif p[1] == 'parent':
            p[0] = parent()
        else:
            raise exception('unknown named operator `%s` at %s:%s' % (p[1], p.lineno(1), p.lexpos(1)))

    def p_jsonpath_root(self, p):
        "jsonpath : '$'"
        p[0] = root()

    def p_jsonpath_idx(self, p):
        "jsonpath : '[' idx ']'"
        p[0] = p[2]

    def p_jsonpath_slice(self, p):
        "jsonpath : '[' slice ']'"
        p[0] = p[2]

    def p_jsonpath_fieldbrackets(self, p):
        "jsonpath : '[' fields ']'"
        p[0] = fields(*p[2])

    def p_jsonpath_child_fieldbrackets(self, p):
        "jsonpath : jsonpath '[' fields ']'"
        p[0] = child(p[1], fields(*p[3]))

    def p_jsonpath_child_idxbrackets(self, p):
        "jsonpath : jsonpath '[' idx ']'"
        p[0] = child(p[1], p[3])

    def p_jsonpath_child_slicebrackets(self, p):
        "jsonpath : jsonpath '[' slice ']'"
        p[0] = child(p[1], p[3])

    def p_jsonpath_parens(self, p):
        "jsonpath : '(' jsonpath ')'"
        p[0] = p[2]

    # because fields in brackets cannot be '*' - that is reserved for array indices
    def p_fields_or_any(self, p):
        """fields_or_any : fields 
                         | '*'    """
        if p[1] == '*':
            p[0] = ['*']
        else:
            p[0] = p[1]

    def p_fields_id(self, p):
        "fields : id"
        p[0] = [p[1]]

    def p_fields_comma(self, p):
        "fields : fields ',' fields"
        p[0] = p[1] + p[3]

    def p_idx(self, p):
        "idx : number"
        p[0] = index(p[1])

    def p_slice_any(self, p):
        "slice : '*'"
        p[0] = slice()

    def p_slice(self, p): # currently does not support `step`
        "slice : maybe_int ':' maybe_int"
        p[0] = slice(start=p[1], end=p[3])

    def p_maybe_int(self, p):
        """maybe_int : number
                     | empty"""
        p[0] = p[1]
    
    def p_empty(self, p):
        'empty :'
        p[0] = none

class iteratortotokenstream(object):
    def __init__(self, iterator):
        self.iterator = iterator

    def token(self):
        try:
            return next(self.iterator)
        except stopiteration:
            return none


if __name__ == '__main__':
    logging.basicconfig()
    parser = jsonpathparser(debug=true)
    print(parser.parse(sys.stdin.read()))
