# -----------------------------------------------------------------------------
# cpp.py
#
# author:  david beazley (http://www.dabeaz.com)
# copyright (c) 2007
# all rights reserved
#
# this module implements an ansi-c style lexical preprocessor for ply. 
# -----------------------------------------------------------------------------
from __future__ import generators

# -----------------------------------------------------------------------------
# default preprocessor lexer definitions.   these tokens are enough to get
# a basic preprocessor working.   other modules may import these if they want
# -----------------------------------------------------------------------------

tokens = (
   'cpp_id','cpp_integer', 'cpp_float', 'cpp_string', 'cpp_char', 'cpp_ws', 'cpp_comment', 'cpp_pound','cpp_dpound'
)

literals = "+-*/%|&~^<>=!?()[]{}.,;:\\\'\""

# whitespace
def t_cpp_ws(t):
    r'\s+'
    t.lexer.lineno += t.value.count("\n")
    return t

t_cpp_pound = r'\#'
t_cpp_dpound = r'\#\#'

# identifier
t_cpp_id = r'[a-za-z_][\w_]*'

# integer literal
def cpp_integer(t):
    r'(((((0x)|(0x))[0-9a-fa-f]+)|(\d+))([uu]|[ll]|[uu][ll]|[ll][uu])?)'
    return t

t_cpp_integer = cpp_integer

# floating literal
t_cpp_float = r'((\d+)(\.\d+)(e(\+|-)?(\d+))? | (\d+)e(\+|-)?(\d+))([ll]|[ff])?'

# string literal
def t_cpp_string(t):
    r'\"([^\\\n]|(\\(.|\n)))*?\"'
    t.lexer.lineno += t.value.count("\n")
    return t

# character constant 'c' or l'c'
def t_cpp_char(t):
    r'(l)?\'([^\\\n]|(\\(.|\n)))*?\''
    t.lexer.lineno += t.value.count("\n")
    return t

# comment
def t_cpp_comment(t):
    r'(/\*(.|\n)*?\*/)|(//.*?\n)'
    t.lexer.lineno += t.value.count("\n")
    return t
    
def t_error(t):
    t.type = t.value[0]
    t.value = t.value[0]
    t.lexer.skip(1)
    return t

import re
import copy
import time
import os.path

# -----------------------------------------------------------------------------
# trigraph()
# 
# given an input string, this function replaces all trigraph sequences. 
# the following mapping is used:
#
#     ??=    #
#     ??/    \
#     ??'    ^
#     ??(    [
#     ??)    ]
#     ??!    |
#     ??<    {
#     ??>    }
#     ??-    ~
# -----------------------------------------------------------------------------

_trigraph_pat = re.compile(r'''\?\?[=/\'\(\)\!<>\-]''')
_trigraph_rep = {
    '=':'#',
    '/':'\\',
    "'":'^',
    '(':'[',
    ')':']',
    '!':'|',
    '<':'{',
    '>':'}',
    '-':'~'
}

def trigraph(input):
    return _trigraph_pat.sub(lambda g: _trigraph_rep[g.group()[-1]],input)

# ------------------------------------------------------------------
# macro object
#
# this object holds information about preprocessor macros
#
#    .name      - macro name (string)
#    .value     - macro value (a list of tokens)
#    .arglist   - list of argument names
#    .variadic  - boolean indicating whether or not variadic macro
#    .vararg    - name of the variadic parameter
#
# when a macro is created, the macro replacement token sequence is
# pre-scanned and used to create patch lists that are later used
# during macro expansion
# ------------------------------------------------------------------

class macro(object):
    def __init__(self,name,value,arglist=none,variadic=false):
        self.name = name
        self.value = value
        self.arglist = arglist
        self.variadic = variadic
        if variadic:
            self.vararg = arglist[-1]
        self.source = none

# ------------------------------------------------------------------
# preprocessor object
#
# object representing a preprocessor.  contains macro definitions,
# include directories, and other information
# ------------------------------------------------------------------

class preprocessor(object):
    def __init__(self,lexer=none):
        if lexer is none:
            lexer = lex.lexer
        self.lexer = lexer
        self.macros = { }
        self.path = []
        self.temp_path = []

        # probe the lexer for selected tokens
        self.lexprobe()

        tm = time.localtime()
        self.define("__date__ \"%s\"" % time.strftime("%b %d %y",tm))
        self.define("__time__ \"%s\"" % time.strftime("%h:%m:%s",tm))
        self.parser = none

    # -----------------------------------------------------------------------------
    # tokenize()
    #
    # utility function. given a string of text, tokenize into a list of tokens
    # -----------------------------------------------------------------------------

    def tokenize(self,text):
        tokens = []
        self.lexer.input(text)
        while true:
            tok = self.lexer.token()
            if not tok: break
            tokens.append(tok)
        return tokens

    # ---------------------------------------------------------------------
    # error()
    #
    # report a preprocessor error/warning of some kind
    # ----------------------------------------------------------------------

    def error(self,file,line,msg):
        print("%s:%d %s" % (file,line,msg))

    # ----------------------------------------------------------------------
    # lexprobe()
    #
    # this method probes the preprocessor lexer object to discover
    # the token types of symbols that are important to the preprocessor.
    # if this works right, the preprocessor will simply "work"
    # with any suitable lexer regardless of how tokens have been named.
    # ----------------------------------------------------------------------

    def lexprobe(self):

        # determine the token type for identifiers
        self.lexer.input("identifier")
        tok = self.lexer.token()
        if not tok or tok.value != "identifier":
            print("couldn't determine identifier type")
        else:
            self.t_id = tok.type

        # determine the token type for integers
        self.lexer.input("12345")
        tok = self.lexer.token()
        if not tok or int(tok.value) != 12345:
            print("couldn't determine integer type")
        else:
            self.t_integer = tok.type
            self.t_integer_type = type(tok.value)

        # determine the token type for strings enclosed in double quotes
        self.lexer.input("\"filename\"")
        tok = self.lexer.token()
        if not tok or tok.value != "\"filename\"":
            print("couldn't determine string type")
        else:
            self.t_string = tok.type

        # determine the token type for whitespace--if any
        self.lexer.input("  ")
        tok = self.lexer.token()
        if not tok or tok.value != "  ":
            self.t_space = none
        else:
            self.t_space = tok.type

        # determine the token type for newlines
        self.lexer.input("\n")
        tok = self.lexer.token()
        if not tok or tok.value != "\n":
            self.t_newline = none
            print("couldn't determine token for newlines")
        else:
            self.t_newline = tok.type

        self.t_ws = (self.t_space, self.t_newline)

        # check for other characters used by the preprocessor
        chars = [ '<','>','#','##','\\','(',')',',','.']
        for c in chars:
            self.lexer.input(c)
            tok = self.lexer.token()
            if not tok or tok.value != c:
                print("unable to lex '%s' required for preprocessor" % c)

    # ----------------------------------------------------------------------
    # add_path()
    #
    # adds a search path to the preprocessor.  
    # ----------------------------------------------------------------------

    def add_path(self,path):
        self.path.append(path)

    # ----------------------------------------------------------------------
    # group_lines()
    #
    # given an input string, this function splits it into lines.  trailing whitespace
    # is removed.   any line ending with \ is grouped with the next line.  this
    # function forms the lowest level of the preprocessor---grouping into text into
    # a line-by-line format.
    # ----------------------------------------------------------------------

    def group_lines(self,input):
        lex = self.lexer.clone()
        lines = [x.rstrip() for x in input.splitlines()]
        for i in xrange(len(lines)):
            j = i+1
            while lines[i].endswith('\\') and (j < len(lines)):
                lines[i] = lines[i][:-1]+lines[j]
                lines[j] = ""
                j += 1

        input = "\n".join(lines)
        lex.input(input)
        lex.lineno = 1

        current_line = []
        while true:
            tok = lex.token()
            if not tok:
                break
            current_line.append(tok)
            if tok.type in self.t_ws and '\n' in tok.value:
                yield current_line
                current_line = []

        if current_line:
            yield current_line

    # ----------------------------------------------------------------------
    # tokenstrip()
    # 
    # remove leading/trailing whitespace tokens from a token list
    # ----------------------------------------------------------------------

    def tokenstrip(self,tokens):
        i = 0
        while i < len(tokens) and tokens[i].type in self.t_ws:
            i += 1
        del tokens[:i]
        i = len(tokens)-1
        while i >= 0 and tokens[i].type in self.t_ws:
            i -= 1
        del tokens[i+1:]
        return tokens


    # ----------------------------------------------------------------------
    # collect_args()
    #
    # collects comma separated arguments from a list of tokens.   the arguments
    # must be enclosed in parenthesis.  returns a tuple (tokencount,args,positions)
    # where tokencount is the number of tokens consumed, args is a list of arguments,
    # and positions is a list of integers containing the starting index of each
    # argument.  each argument is represented by a list of tokens.
    #
    # when collecting arguments, leading and trailing whitespace is removed
    # from each argument.  
    #
    # this function properly handles nested parenthesis and commas---these do not
    # define new arguments.
    # ----------------------------------------------------------------------

    def collect_args(self,tokenlist):
        args = []
        positions = []
        current_arg = []
        nesting = 1
        tokenlen = len(tokenlist)
    
        # search for the opening '('.
        i = 0
        while (i < tokenlen) and (tokenlist[i].type in self.t_ws):
            i += 1

        if (i < tokenlen) and (tokenlist[i].value == '('):
            positions.append(i+1)
        else:
            self.error(self.source,tokenlist[0].lineno,"missing '(' in macro arguments")
            return 0, [], []

        i += 1

        while i < tokenlen:
            t = tokenlist[i]
            if t.value == '(':
                current_arg.append(t)
                nesting += 1
            elif t.value == ')':
                nesting -= 1
                if nesting == 0:
                    if current_arg:
                        args.append(self.tokenstrip(current_arg))
                        positions.append(i)
                    return i+1,args,positions
                current_arg.append(t)
            elif t.value == ',' and nesting == 1:
                args.append(self.tokenstrip(current_arg))
                positions.append(i+1)
                current_arg = []
            else:
                current_arg.append(t)
            i += 1
    
        # missing end argument
        self.error(self.source,tokenlist[-1].lineno,"missing ')' in macro arguments")
        return 0, [],[]

    # ----------------------------------------------------------------------
    # macro_prescan()
    #
    # examine the macro value (token sequence) and identify patch points
    # this is used to speed up macro expansion later on---we'll know
    # right away where to apply patches to the value to form the expansion
    # ----------------------------------------------------------------------
    
    def macro_prescan(self,macro):
        macro.patch     = []             # standard macro arguments 
        macro.str_patch = []             # string conversion expansion
        macro.var_comma_patch = []       # variadic macro comma patch
        i = 0
        while i < len(macro.value):
            if macro.value[i].type == self.t_id and macro.value[i].value in macro.arglist:
                argnum = macro.arglist.index(macro.value[i].value)
                # conversion of argument to a string
                if i > 0 and macro.value[i-1].value == '#':
                    macro.value[i] = copy.copy(macro.value[i])
                    macro.value[i].type = self.t_string
                    del macro.value[i-1]
                    macro.str_patch.append((argnum,i-1))
                    continue
                # concatenation
                elif (i > 0 and macro.value[i-1].value == '##'):
                    macro.patch.append(('c',argnum,i-1))
                    del macro.value[i-1]
                    continue
                elif ((i+1) < len(macro.value) and macro.value[i+1].value == '##'):
                    macro.patch.append(('c',argnum,i))
                    i += 1
                    continue
                # standard expansion
                else:
                    macro.patch.append(('e',argnum,i))
            elif macro.value[i].value == '##':
                if macro.variadic and (i > 0) and (macro.value[i-1].value == ',') and \
                        ((i+1) < len(macro.value)) and (macro.value[i+1].type == self.t_id) and \
                        (macro.value[i+1].value == macro.vararg):
                    macro.var_comma_patch.append(i-1)
            i += 1
        macro.patch.sort(key=lambda x: x[2],reverse=true)

    # ----------------------------------------------------------------------
    # macro_expand_args()
    #
    # given a macro and list of arguments (each a token list), this method
    # returns an expanded version of a macro.  the return value is a token sequence
    # representing the replacement macro tokens
    # ----------------------------------------------------------------------

    def macro_expand_args(self,macro,args):
        # make a copy of the macro token sequence
        rep = [copy.copy(_x) for _x in macro.value]

        # make string expansion patches.  these do not alter the length of the replacement sequence
        
        str_expansion = {}
        for argnum, i in macro.str_patch:
            if argnum not in str_expansion:
                str_expansion[argnum] = ('"%s"' % "".join([x.value for x in args[argnum]])).replace("\\","\\\\")
            rep[i] = copy.copy(rep[i])
            rep[i].value = str_expansion[argnum]

        # make the variadic macro comma patch.  if the variadic macro argument is empty, we get rid
        comma_patch = false
        if macro.variadic and not args[-1]:
            for i in macro.var_comma_patch:
                rep[i] = none
                comma_patch = true

        # make all other patches.   the order of these matters.  it is assumed that the patch list
        # has been sorted in reverse order of patch location since replacements will cause the
        # size of the replacement sequence to expand from the patch point.
        
        expanded = { }
        for ptype, argnum, i in macro.patch:
            # concatenation.   argument is left unexpanded
            if ptype == 'c':
                rep[i:i+1] = args[argnum]
            # normal expansion.  argument is macro expanded first
            elif ptype == 'e':
                if argnum not in expanded:
                    expanded[argnum] = self.expand_macros(args[argnum])
                rep[i:i+1] = expanded[argnum]

        # get rid of removed comma if necessary
        if comma_patch:
            rep = [_i for _i in rep if _i]

        return rep


    # ----------------------------------------------------------------------
    # expand_macros()
    #
    # given a list of tokens, this function performs macro expansion.
    # the expanded argument is a dictionary that contains macros already
    # expanded.  this is used to prevent infinite recursion.
    # ----------------------------------------------------------------------

    def expand_macros(self,tokens,expanded=none):
        if expanded is none:
            expanded = {}
        i = 0
        while i < len(tokens):
            t = tokens[i]
            if t.type == self.t_id:
                if t.value in self.macros and t.value not in expanded:
                    # yes, we found a macro match
                    expanded[t.value] = true
                    
                    m = self.macros[t.value]
                    if not m.arglist:
                        # a simple macro
                        ex = self.expand_macros([copy.copy(_x) for _x in m.value],expanded)
                        for e in ex:
                            e.lineno = t.lineno
                        tokens[i:i+1] = ex
                        i += len(ex)
                    else:
                        # a macro with arguments
                        j = i + 1
                        while j < len(tokens) and tokens[j].type in self.t_ws:
                            j += 1
                        if tokens[j].value == '(':
                            tokcount,args,positions = self.collect_args(tokens[j:])
                            if not m.variadic and len(args) !=  len(m.arglist):
                                self.error(self.source,t.lineno,"macro %s requires %d arguments" % (t.value,len(m.arglist)))
                                i = j + tokcount
                            elif m.variadic and len(args) < len(m.arglist)-1:
                                if len(m.arglist) > 2:
                                    self.error(self.source,t.lineno,"macro %s must have at least %d arguments" % (t.value, len(m.arglist)-1))
                                else:
                                    self.error(self.source,t.lineno,"macro %s must have at least %d argument" % (t.value, len(m.arglist)-1))
                                i = j + tokcount
                            else:
                                if m.variadic:
                                    if len(args) == len(m.arglist)-1:
                                        args.append([])
                                    else:
                                        args[len(m.arglist)-1] = tokens[j+positions[len(m.arglist)-1]:j+tokcount-1]
                                        del args[len(m.arglist):]
                                        
                                # get macro replacement text
                                rep = self.macro_expand_args(m,args)
                                rep = self.expand_macros(rep,expanded)
                                for r in rep:
                                    r.lineno = t.lineno
                                tokens[i:j+tokcount] = rep
                                i += len(rep)
                    del expanded[t.value]
                    continue
                elif t.value == '__line__':
                    t.type = self.t_integer
                    t.value = self.t_integer_type(t.lineno)
                
            i += 1
        return tokens

    # ----------------------------------------------------------------------    
    # evalexpr()
    # 
    # evaluate an expression token sequence for the purposes of evaluating
    # integral expressions.
    # ----------------------------------------------------------------------

    def evalexpr(self,tokens):
        # tokens = tokenize(line)
        # search for defined macros
        i = 0
        while i < len(tokens):
            if tokens[i].type == self.t_id and tokens[i].value == 'defined':
                j = i + 1
                needparen = false
                result = "0l"
                while j < len(tokens):
                    if tokens[j].type in self.t_ws:
                        j += 1
                        continue
                    elif tokens[j].type == self.t_id:
                        if tokens[j].value in self.macros:
                            result = "1l"
                        else:
                            result = "0l"
                        if not needparen: break
                    elif tokens[j].value == '(':
                        needparen = true
                    elif tokens[j].value == ')':
                        break
                    else:
                        self.error(self.source,tokens[i].lineno,"malformed defined()")
                    j += 1
                tokens[i].type = self.t_integer
                tokens[i].value = self.t_integer_type(result)
                del tokens[i+1:j+1]
            i += 1
        tokens = self.expand_macros(tokens)
        for i,t in enumerate(tokens):
            if t.type == self.t_id:
                tokens[i] = copy.copy(t)
                tokens[i].type = self.t_integer
                tokens[i].value = self.t_integer_type("0l")
            elif t.type == self.t_integer:
                tokens[i] = copy.copy(t)
                # strip off any trailing suffixes
                tokens[i].value = str(tokens[i].value)
                while tokens[i].value[-1] not in "0123456789abcdefabcdef":
                    tokens[i].value = tokens[i].value[:-1]
        
        expr = "".join([str(x.value) for x in tokens])
        expr = expr.replace("&&"," and ")
        expr = expr.replace("||"," or ")
        expr = expr.replace("!"," not ")
        try:
            result = eval(expr)
        except standarderror:
            self.error(self.source,tokens[0].lineno,"couldn't evaluate expression")
            result = 0
        return result

    # ----------------------------------------------------------------------
    # parsegen()
    #
    # parse an input string/
    # ----------------------------------------------------------------------
    def parsegen(self,input,source=none):

        # replace trigraph sequences
        t = trigraph(input)
        lines = self.group_lines(t)

        if not source:
            source = ""
            
        self.define("__file__ \"%s\"" % source)

        self.source = source
        chunk = []
        enable = true
        iftrigger = false
        ifstack = []

        for x in lines:
            for i,tok in enumerate(x):
                if tok.type not in self.t_ws: break
            if tok.value == '#':
                # preprocessor directive

                for tok in x:
                    if tok in self.t_ws and '\n' in tok.value:
                        chunk.append(tok)
                
                dirtokens = self.tokenstrip(x[i+1:])
                if dirtokens:
                    name = dirtokens[0].value
                    args = self.tokenstrip(dirtokens[1:])
                else:
                    name = ""
                    args = []
                
                if name == 'define':
                    if enable:
                        for tok in self.expand_macros(chunk):
                            yield tok
                        chunk = []
                        self.define(args)
                elif name == 'include':
                    if enable:
                        for tok in self.expand_macros(chunk):
                            yield tok
                        chunk = []
                        oldfile = self.macros['__file__']
                        for tok in self.include(args):
                            yield tok
                        self.macros['__file__'] = oldfile
                        self.source = source
                elif name == 'undef':
                    if enable:
                        for tok in self.expand_macros(chunk):
                            yield tok
                        chunk = []
                        self.undef(args)
                elif name == 'ifdef':
                    ifstack.append((enable,iftrigger))
                    if enable:
                        if not args[0].value in self.macros:
                            enable = false
                            iftrigger = false
                        else:
                            iftrigger = true
                elif name == 'ifndef':
                    ifstack.append((enable,iftrigger))
                    if enable:
                        if args[0].value in self.macros:
                            enable = false
                            iftrigger = false
                        else:
                            iftrigger = true
                elif name == 'if':
                    ifstack.append((enable,iftrigger))
                    if enable:
                        result = self.evalexpr(args)
                        if not result:
                            enable = false
                            iftrigger = false
                        else:
                            iftrigger = true
                elif name == 'elif':
                    if ifstack:
                        if ifstack[-1][0]:     # we only pay attention if outer "if" allows this
                            if enable:         # if already true, we flip enable false
                                enable = false
                            elif not iftrigger:   # if false, but not triggered yet, we'll check expression
                                result = self.evalexpr(args)
                                if result:
                                    enable  = true
                                    iftrigger = true
                    else:
                        self.error(self.source,dirtokens[0].lineno,"misplaced #elif")
                        
                elif name == 'else':
                    if ifstack:
                        if ifstack[-1][0]:
                            if enable:
                                enable = false
                            elif not iftrigger:
                                enable = true
                                iftrigger = true
                    else:
                        self.error(self.source,dirtokens[0].lineno,"misplaced #else")

                elif name == 'endif':
                    if ifstack:
                        enable,iftrigger = ifstack.pop()
                    else:
                        self.error(self.source,dirtokens[0].lineno,"misplaced #endif")
                else:
                    # unknown preprocessor directive
                    pass

            else:
                # normal text
                if enable:
                    chunk.extend(x)

        for tok in self.expand_macros(chunk):
            yield tok
        chunk = []

    # ----------------------------------------------------------------------
    # include()
    #
    # implementation of file-inclusion
    # ----------------------------------------------------------------------

    def include(self,tokens):
        # try to extract the filename and then process an include file
        if not tokens:
            return
        if tokens:
            if tokens[0].value != '<' and tokens[0].type != self.t_string:
                tokens = self.expand_macros(tokens)

            if tokens[0].value == '<':
                # include <...>
                i = 1
                while i < len(tokens):
                    if tokens[i].value == '>':
                        break
                    i += 1
                else:
                    print("malformed #include <...>")
                    return
                filename = "".join([x.value for x in tokens[1:i]])
                path = self.path + [""] + self.temp_path
            elif tokens[0].type == self.t_string:
                filename = tokens[0].value[1:-1]
                path = self.temp_path + [""] + self.path
            else:
                print("malformed #include statement")
                return
        for p in path:
            iname = os.path.join(p,filename)
            try:
                data = open(iname,"r").read()
                dname = os.path.dirname(iname)
                if dname:
                    self.temp_path.insert(0,dname)
                for tok in self.parsegen(data,filename):
                    yield tok
                if dname:
                    del self.temp_path[0]
                break
            except ioerror:
                pass
        else:
            print("couldn't find '%s'" % filename)

    # ----------------------------------------------------------------------
    # define()
    #
    # define a new macro
    # ----------------------------------------------------------------------

    def define(self,tokens):
        if isinstance(tokens,(str,unicode)):
            tokens = self.tokenize(tokens)

        linetok = tokens
        try:
            name = linetok[0]
            if len(linetok) > 1:
                mtype = linetok[1]
            else:
                mtype = none
            if not mtype:
                m = macro(name.value,[])
                self.macros[name.value] = m
            elif mtype.type in self.t_ws:
                # a normal macro
                m = macro(name.value,self.tokenstrip(linetok[2:]))
                self.macros[name.value] = m
            elif mtype.value == '(':
                # a macro with arguments
                tokcount, args, positions = self.collect_args(linetok[1:])
                variadic = false
                for a in args:
                    if variadic:
                        print("no more arguments may follow a variadic argument")
                        break
                    astr = "".join([str(_i.value) for _i in a])
                    if astr == "...":
                        variadic = true
                        a[0].type = self.t_id
                        a[0].value = '__va_args__'
                        variadic = true
                        del a[1:]
                        continue
                    elif astr[-3:] == "..." and a[0].type == self.t_id:
                        variadic = true
                        del a[1:]
                        # if, for some reason, "." is part of the identifier, strip off the name for the purposes
                        # of macro expansion
                        if a[0].value[-3:] == '...':
                            a[0].value = a[0].value[:-3]
                        continue
                    if len(a) > 1 or a[0].type != self.t_id:
                        print("invalid macro argument")
                        break
                else:
                    mvalue = self.tokenstrip(linetok[1+tokcount:])
                    i = 0
                    while i < len(mvalue):
                        if i+1 < len(mvalue):
                            if mvalue[i].type in self.t_ws and mvalue[i+1].value == '##':
                                del mvalue[i]
                                continue
                            elif mvalue[i].value == '##' and mvalue[i+1].type in self.t_ws:
                                del mvalue[i+1]
                        i += 1
                    m = macro(name.value,mvalue,[x[0].value for x in args],variadic)
                    self.macro_prescan(m)
                    self.macros[name.value] = m
            else:
                print("bad macro definition")
        except lookuperror:
            print("bad macro definition")

    # ----------------------------------------------------------------------
    # undef()
    #
    # undefine a macro
    # ----------------------------------------------------------------------

    def undef(self,tokens):
        id = tokens[0].value
        try:
            del self.macros[id]
        except lookuperror:
            pass

    # ----------------------------------------------------------------------
    # parse()
    #
    # parse input text.
    # ----------------------------------------------------------------------
    def parse(self,input,source=none,ignore={}):
        self.ignore = ignore
        self.parser = self.parsegen(input,source)
        
    # ----------------------------------------------------------------------
    # token()
    #
    # method to return individual tokens
    # ----------------------------------------------------------------------
    def token(self):
        try:
            while true:
                tok = next(self.parser)
                if tok.type not in self.ignore: return tok
        except stopiteration:
            self.parser = none
            return none

if __name__ == '__main__':
    import ply.lex as lex
    lexer = lex.lex()

    # run a preprocessor
    import sys
    f = open(sys.argv[1])
    input = f.read()

    p = preprocessor(lexer)
    p.parse(input,sys.argv[1])
    while true:
        tok = p.token()
        if not tok: break
        print(p.source, tok)




    






