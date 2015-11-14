# -----------------------------------------------------------------------------
# ply: yacc.py
#
# copyright (c) 2001-2011,
# david m. beazley (dabeaz llc)
# all rights reserved.
#
# redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
# 
# * redistributions of source code must retain the above copyright notice,
#   this list of conditions and the following disclaimer.  
# * redistributions in binary form must reproduce the above copyright notice, 
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.  
# * neither the name of the david beazley or dabeaz llc may be used to
#   endorse or promote products derived from this software without
#  specific prior written permission. 
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
# -----------------------------------------------------------------------------
#
# this implements an lr parser that is constructed from grammar rules defined
# as python functions. the grammer is specified by supplying the bnf inside
# python documentation strings.  the inspiration for this technique was borrowed
# from john aycock's spark parsing system.  ply might be viewed as cross between
# spark and the gnu bison utility.
#
# the current implementation is only somewhat object-oriented. the
# lr parser itself is defined in terms of an object (which allows multiple
# parsers to co-exist).  however, most of the variables used during table
# construction are defined in terms of global variables.  users shouldn't
# notice unless they are trying to define multiple parsers at the same
# time using threads (in which case they should have their head examined).
#
# this implementation supports both slr and lalr(1) parsing.  lalr(1)
# support was originally implemented by elias ioup (ezioup@alumni.uchicago.edu),
# using the algorithm found in aho, sethi, and ullman "compilers: principles,
# techniques, and tools" (the dragon book).  lalr(1) has since been replaced
# by the more efficient deremer and pennello algorithm.
#
# :::::::: warning :::::::
#
# construction of lr parsing tables is fairly complicated and expensive.
# to make this module run fast, a *lot* of work has been put into
# optimization---often at the expensive of readability and what might
# consider to be good python "coding style."   modify the code at your
# own risk!
# ----------------------------------------------------------------------------

__version__    = "3.4"
__tabversion__ = "3.2"       # table version

#-----------------------------------------------------------------------------
#                     === user configurable parameters ===
#
# change these to modify the default behavior of yacc (if you wish)
#-----------------------------------------------------------------------------

yaccdebug   = 1                # debugging mode.  if set, yacc generates a
                               # a 'parser.out' file in the current directory

debug_file  = 'parser.out'     # default name of the debugging file
tab_module  = 'parsetab'       # default name of the table module
default_lr  = 'lalr'           # default lr table generation method

error_count = 3                # number of symbols that must be shifted to leave recovery mode

yaccdevel   = 0                # set to true if developing yacc.  this turns off optimized
                               # implementations of certain functions.

resultlimit = 40               # size limit of results when running in debug mode.

pickle_protocol = 0            # protocol to use when writing pickle files

import re, types, sys, os.path

# compatibility function for python 2.6/3.0
if sys.version_info[0] < 3:
    def func_code(f):
        return f.func_code
else:
    def func_code(f):
        return f.__code__

# compatibility
try:
    maxint = sys.maxint
except attributeerror:
    maxint = sys.maxsize

# python 2.x/3.0 compatibility.
def load_ply_lex():
    if sys.version_info[0] < 3:
        import lex
    else:
        import ply.lex as lex
    return lex

# this object is a stand-in for a logging object created by the 
# logging module.   ply will use this by default to create things
# such as the parser.out file.  if a user wants more detailed
# information, they can create their own logging object and pass
# it into ply.

class plylogger(object):
    def __init__(self,f):
        self.f = f
    def debug(self,msg,*args,**kwargs):
        self.f.write((msg % args) + "\n")
    info     = debug

    def warning(self,msg,*args,**kwargs):
        self.f.write("warning: "+ (msg % args) + "\n")

    def error(self,msg,*args,**kwargs):
        self.f.write("error: " + (msg % args) + "\n")

    critical = debug

# null logger is used when no output is generated. does nothing.
class nulllogger(object):
    def __getattribute__(self,name):
        return self
    def __call__(self,*args,**kwargs):
        return self
        
# exception raised for yacc-related errors
class yaccerror(exception):   pass

# format the result message that the parser produces when running in debug mode.
def format_result(r):
    repr_str = repr(r)
    if '\n' in repr_str: repr_str = repr(repr_str)
    if len(repr_str) > resultlimit:
        repr_str = repr_str[:resultlimit]+" ..."
    result = "<%s @ 0x%x> (%s)" % (type(r).__name__,id(r),repr_str)
    return result


# format stack entries when the parser is running in debug mode
def format_stack_entry(r):
    repr_str = repr(r)
    if '\n' in repr_str: repr_str = repr(repr_str)
    if len(repr_str) < 16:
        return repr_str
    else:
        return "<%s @ 0x%x>" % (type(r).__name__,id(r))

#-----------------------------------------------------------------------------
#                        ===  lr parsing engine ===
#
# the following classes are used for the lr parser itself.  these are not
# used during table construction and are independent of the actual lr
# table generation algorithm
#-----------------------------------------------------------------------------

# this class is used to hold non-terminal grammar symbols during parsing.
# it normally has the following attributes set:
#        .type       = grammar symbol type
#        .value      = symbol value
#        .lineno     = starting line number
#        .endlineno  = ending line number (optional, set automatically)
#        .lexpos     = starting lex position
#        .endlexpos  = ending lex position (optional, set automatically)

class yaccsymbol:
    def __str__(self):    return self.type
    def __repr__(self):   return str(self)

# this class is a wrapper around the objects actually passed to each
# grammar rule.   index lookup and assignment actually assign the
# .value attribute of the underlying yaccsymbol object.
# the lineno() method returns the line number of a given
# item (or 0 if not defined).   the linespan() method returns
# a tuple of (startline,endline) representing the range of lines
# for a symbol.  the lexspan() method returns a tuple (lexpos,endlexpos)
# representing the range of positional information for a symbol.

class yaccproduction:
    def __init__(self,s,stack=none):
        self.slice = s
        self.stack = stack
        self.lexer = none
        self.parser= none
    def __getitem__(self,n):
        if n >= 0: return self.slice[n].value
        else: return self.stack[n].value

    def __setitem__(self,n,v):
        self.slice[n].value = v

    def __getslice__(self,i,j):
        return [s.value for s in self.slice[i:j]]

    def __len__(self):
        return len(self.slice)

    def lineno(self,n):
        return getattr(self.slice[n],"lineno",0)

    def set_lineno(self,n,lineno):
        self.slice[n].lineno = lineno

    def linespan(self,n):
        startline = getattr(self.slice[n],"lineno",0)
        endline = getattr(self.slice[n],"endlineno",startline)
        return startline,endline

    def lexpos(self,n):
        return getattr(self.slice[n],"lexpos",0)

    def lexspan(self,n):
        startpos = getattr(self.slice[n],"lexpos",0)
        endpos = getattr(self.slice[n],"endlexpos",startpos)
        return startpos,endpos

    def error(self):
       raise syntaxerror


# -----------------------------------------------------------------------------
#                               == lrparser ==
#
# the lr parsing engine.
# -----------------------------------------------------------------------------

class lrparser:
    def __init__(self,lrtab,errorf):
        self.productions = lrtab.lr_productions
        self.action      = lrtab.lr_action
        self.goto        = lrtab.lr_goto
        self.errorfunc   = errorf

    def errok(self):
        self.errorok     = 1

    def restart(self):
        del self.statestack[:]
        del self.symstack[:]
        sym = yaccsymbol()
        sym.type = '$end'
        self.symstack.append(sym)
        self.statestack.append(0)

    def parse(self,input=none,lexer=none,debug=0,tracking=0,tokenfunc=none):
        if debug or yaccdevel:
            if isinstance(debug,int):
                debug = plylogger(sys.stderr)
            return self.parsedebug(input,lexer,debug,tracking,tokenfunc)
        elif tracking:
            return self.parseopt(input,lexer,debug,tracking,tokenfunc)
        else:
            return self.parseopt_notrack(input,lexer,debug,tracking,tokenfunc)
        

    # !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    # parsedebug().
    #
    # this is the debugging enabled version of parse().  all changes made to the
    # parsing engine should be made here.   for the non-debugging version,
    # copy this code to a method parseopt() and delete all of the sections
    # enclosed in:
    #
    #      #--! debug
    #      statements
    #      #--! debug
    #
    # !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

    def parsedebug(self,input=none,lexer=none,debug=none,tracking=0,tokenfunc=none):
        lookahead = none                 # current lookahead symbol
        lookaheadstack = [ ]             # stack of lookahead symbols
        actions = self.action            # local reference to action table (to avoid lookup on self.)
        goto    = self.goto              # local reference to goto table (to avoid lookup on self.)
        prod    = self.productions       # local reference to production list (to avoid lookup on self.)
        pslice  = yaccproduction(none)   # production object passed to grammar rules
        errorcount = 0                   # used during error recovery 

        # --! debug
        debug.info("ply: parse debug start")
        # --! debug

        # if no lexer was given, we will try to use the lex module
        if not lexer:
            lex = load_ply_lex()
            lexer = lex.lexer

        # set up the lexer and parser objects on pslice
        pslice.lexer = lexer
        pslice.parser = self

        # if input was supplied, pass to lexer
        if input is not none:
            lexer.input(input)

        if tokenfunc is none:
           # tokenize function
           get_token = lexer.token
        else:
           get_token = tokenfunc

        # set up the state and symbol stacks

        statestack = [ ]                # stack of parsing states
        self.statestack = statestack
        symstack   = [ ]                # stack of grammar symbols
        self.symstack = symstack

        pslice.stack = symstack         # put in the production
        errtoken   = none               # err token

        # the start state is assumed to be (0,$end)

        statestack.append(0)
        sym = yaccsymbol()
        sym.type = "$end"
        symstack.append(sym)
        state = 0
        while 1:
            # get the next symbol on the input.  if a lookahead symbol
            # is already set, we just use that. otherwise, we'll pull
            # the next token off of the lookaheadstack or from the lexer

            # --! debug
            debug.debug('')
            debug.debug('state  : %s', state)
            # --! debug

            if not lookahead:
                if not lookaheadstack:
                    lookahead = get_token()     # get the next token
                else:
                    lookahead = lookaheadstack.pop()
                if not lookahead:
                    lookahead = yaccsymbol()
                    lookahead.type = "$end"

            # --! debug
            debug.debug('stack  : %s',
                        ("%s . %s" % (" ".join([xx.type for xx in symstack][1:]), str(lookahead))).lstrip())
            # --! debug

            # check the action table
            ltype = lookahead.type
            t = actions[state].get(ltype)

            if t is not none:
                if t > 0:
                    # shift a symbol on the stack
                    statestack.append(t)
                    state = t
                    
                    # --! debug
                    debug.debug("action : shift and goto state %s", t)
                    # --! debug

                    symstack.append(lookahead)
                    lookahead = none

                    # decrease error count on successful shift
                    if errorcount: errorcount -=1
                    continue

                if t < 0:
                    # reduce a symbol on the stack, emit a production
                    p = prod[-t]
                    pname = p.name
                    plen  = p.len

                    # get production function
                    sym = yaccsymbol()
                    sym.type = pname       # production name
                    sym.value = none

                    # --! debug
                    if plen:
                        debug.info("action : reduce rule [%s] with %s and goto state %d", p.str, "["+",".join([format_stack_entry(_v.value) for _v in symstack[-plen:]])+"]",-t)
                    else:
                        debug.info("action : reduce rule [%s] with %s and goto state %d", p.str, [],-t)
                        
                    # --! debug

                    if plen:
                        targ = symstack[-plen-1:]
                        targ[0] = sym

                        # --! tracking
                        if tracking:
                           t1 = targ[1]
                           sym.lineno = t1.lineno
                           sym.lexpos = t1.lexpos
                           t1 = targ[-1]
                           sym.endlineno = getattr(t1,"endlineno",t1.lineno)
                           sym.endlexpos = getattr(t1,"endlexpos",t1.lexpos)

                        # --! tracking

                        # !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
                        # the code enclosed in this section is duplicated 
                        # below as a performance optimization.  make sure
                        # changes get made in both locations.

                        pslice.slice = targ
                        
                        try:
                            # call the grammar rule with our special slice object
                            del symstack[-plen:]
                            del statestack[-plen:]
                            p.callable(pslice)
                            # --! debug
                            debug.info("result : %s", format_result(pslice[0]))
                            # --! debug
                            symstack.append(sym)
                            state = goto[statestack[-1]][pname]
                            statestack.append(state)
                        except syntaxerror:
                            # if an error was set. enter error recovery state
                            lookaheadstack.append(lookahead)
                            symstack.pop()
                            statestack.pop()
                            state = statestack[-1]
                            sym.type = 'error'
                            lookahead = sym
                            errorcount = error_count
                            self.errorok = 0
                        continue
                        # !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    
                    else:

                        # --! tracking
                        if tracking:
                           sym.lineno = lexer.lineno
                           sym.lexpos = lexer.lexpos
                        # --! tracking

                        targ = [ sym ]

                        # !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
                        # the code enclosed in this section is duplicated 
                        # above as a performance optimization.  make sure
                        # changes get made in both locations.

                        pslice.slice = targ

                        try:
                            # call the grammar rule with our special slice object
                            p.callable(pslice)
                            # --! debug
                            debug.info("result : %s", format_result(pslice[0]))
                            # --! debug
                            symstack.append(sym)
                            state = goto[statestack[-1]][pname]
                            statestack.append(state)
                        except syntaxerror:
                            # if an error was set. enter error recovery state
                            lookaheadstack.append(lookahead)
                            symstack.pop()
                            statestack.pop()
                            state = statestack[-1]
                            sym.type = 'error'
                            lookahead = sym
                            errorcount = error_count
                            self.errorok = 0
                        continue
                        # !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

                if t == 0:
                    n = symstack[-1]
                    result = getattr(n,"value",none)
                    # --! debug
                    debug.info("done   : returning %s", format_result(result))
                    debug.info("ply: parse debug end")
                    # --! debug
                    return result

            if t == none:

                # --! debug
                debug.error('error  : %s',
                            ("%s . %s" % (" ".join([xx.type for xx in symstack][1:]), str(lookahead))).lstrip())
                # --! debug

                # we have some kind of parsing error here.  to handle
                # this, we are going to push the current token onto
                # the tokenstack and replace it with an 'error' token.
                # if there are any synchronization rules, they may
                # catch it.
                #
                # in addition to pushing the error token, we call call
                # the user defined p_error() function if this is the
                # first syntax error.  this function is only called if
                # errorcount == 0.
                if errorcount == 0 or self.errorok:
                    errorcount = error_count
                    self.errorok = 0
                    errtoken = lookahead
                    if errtoken.type == "$end":
                        errtoken = none               # end of file!
                    if self.errorfunc:
                        global errok,token,restart
                        errok = self.errok        # set some special functions available in error recovery
                        token = get_token
                        restart = self.restart
                        if errtoken and not hasattr(errtoken,'lexer'):
                            errtoken.lexer = lexer
                        tok = self.errorfunc(errtoken)
                        del errok, token, restart   # delete special functions

                        if self.errorok:
                            # user must have done some kind of panic
                            # mode recovery on their own.  the
                            # returned token is the next lookahead
                            lookahead = tok
                            errtoken = none
                            continue
                    else:
                        if errtoken:
                            if hasattr(errtoken,"lineno"): lineno = lookahead.lineno
                            else: lineno = 0
                            if lineno:
                                sys.stderr.write("yacc: syntax error at line %d, token=%s\n" % (lineno, errtoken.type))
                            else:
                                sys.stderr.write("yacc: syntax error, token=%s" % errtoken.type)
                        else:
                            sys.stderr.write("yacc: parse error in input. eof\n")
                            return

                else:
                    errorcount = error_count

                # case 1:  the statestack only has 1 entry on it.  if we're in this state, the
                # entire parse has been rolled back and we're completely hosed.   the token is
                # discarded and we just keep going.

                if len(statestack) <= 1 and lookahead.type != "$end":
                    lookahead = none
                    errtoken = none
                    state = 0
                    # nuke the pushback stack
                    del lookaheadstack[:]
                    continue

                # case 2: the statestack has a couple of entries on it, but we're
                # at the end of the file. nuke the top entry and generate an error token

                # start nuking entries on the stack
                if lookahead.type == "$end":
                    # whoa. we're really hosed here. bail out
                    return

                if lookahead.type != 'error':
                    sym = symstack[-1]
                    if sym.type == 'error':
                        # hmmm. error is on top of stack, we'll just nuke input
                        # symbol and continue
                        lookahead = none
                        continue
                    t = yaccsymbol()
                    t.type = 'error'
                    if hasattr(lookahead,"lineno"):
                        t.lineno = lookahead.lineno
                    t.value = lookahead
                    lookaheadstack.append(lookahead)
                    lookahead = t
                else:
                    symstack.pop()
                    statestack.pop()
                    state = statestack[-1]       # potential bug fix

                continue

            # call an error function here
            raise runtimeerror("yacc: internal parser error!!!\n")

    # !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    # parseopt().
    #
    # optimized version of parse() method.  do not edit this code directly.
    # edit the debug version above, then copy any modifications to the method
    # below while removing #--! debug sections.
    # !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!


    def parseopt(self,input=none,lexer=none,debug=0,tracking=0,tokenfunc=none):
        lookahead = none                 # current lookahead symbol
        lookaheadstack = [ ]             # stack of lookahead symbols
        actions = self.action            # local reference to action table (to avoid lookup on self.)
        goto    = self.goto              # local reference to goto table (to avoid lookup on self.)
        prod    = self.productions       # local reference to production list (to avoid lookup on self.)
        pslice  = yaccproduction(none)   # production object passed to grammar rules
        errorcount = 0                   # used during error recovery 

        # if no lexer was given, we will try to use the lex module
        if not lexer:
            lex = load_ply_lex()
            lexer = lex.lexer
        
        # set up the lexer and parser objects on pslice
        pslice.lexer = lexer
        pslice.parser = self

        # if input was supplied, pass to lexer
        if input is not none:
            lexer.input(input)

        if tokenfunc is none:
           # tokenize function
           get_token = lexer.token
        else:
           get_token = tokenfunc

        # set up the state and symbol stacks

        statestack = [ ]                # stack of parsing states
        self.statestack = statestack
        symstack   = [ ]                # stack of grammar symbols
        self.symstack = symstack

        pslice.stack = symstack         # put in the production
        errtoken   = none               # err token

        # the start state is assumed to be (0,$end)

        statestack.append(0)
        sym = yaccsymbol()
        sym.type = '$end'
        symstack.append(sym)
        state = 0
        while 1:
            # get the next symbol on the input.  if a lookahead symbol
            # is already set, we just use that. otherwise, we'll pull
            # the next token off of the lookaheadstack or from the lexer

            if not lookahead:
                if not lookaheadstack:
                    lookahead = get_token()     # get the next token
                else:
                    lookahead = lookaheadstack.pop()
                if not lookahead:
                    lookahead = yaccsymbol()
                    lookahead.type = '$end'

            # check the action table
            ltype = lookahead.type
            t = actions[state].get(ltype)

            if t is not none:
                if t > 0:
                    # shift a symbol on the stack
                    statestack.append(t)
                    state = t

                    symstack.append(lookahead)
                    lookahead = none

                    # decrease error count on successful shift
                    if errorcount: errorcount -=1
                    continue

                if t < 0:
                    # reduce a symbol on the stack, emit a production
                    p = prod[-t]
                    pname = p.name
                    plen  = p.len

                    # get production function
                    sym = yaccsymbol()
                    sym.type = pname       # production name
                    sym.value = none

                    if plen:
                        targ = symstack[-plen-1:]
                        targ[0] = sym

                        # --! tracking
                        if tracking:
                           t1 = targ[1]
                           sym.lineno = t1.lineno
                           sym.lexpos = t1.lexpos
                           t1 = targ[-1]
                           sym.endlineno = getattr(t1,"endlineno",t1.lineno)
                           sym.endlexpos = getattr(t1,"endlexpos",t1.lexpos)

                        # --! tracking

                        # !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
                        # the code enclosed in this section is duplicated 
                        # below as a performance optimization.  make sure
                        # changes get made in both locations.

                        pslice.slice = targ
                        
                        try:
                            # call the grammar rule with our special slice object
                            del symstack[-plen:]
                            del statestack[-plen:]
                            p.callable(pslice)
                            symstack.append(sym)
                            state = goto[statestack[-1]][pname]
                            statestack.append(state)
                        except syntaxerror:
                            # if an error was set. enter error recovery state
                            lookaheadstack.append(lookahead)
                            symstack.pop()
                            statestack.pop()
                            state = statestack[-1]
                            sym.type = 'error'
                            lookahead = sym
                            errorcount = error_count
                            self.errorok = 0
                        continue
                        # !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    
                    else:

                        # --! tracking
                        if tracking:
                           sym.lineno = lexer.lineno
                           sym.lexpos = lexer.lexpos
                        # --! tracking

                        targ = [ sym ]

                        # !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
                        # the code enclosed in this section is duplicated 
                        # above as a performance optimization.  make sure
                        # changes get made in both locations.

                        pslice.slice = targ

                        try:
                            # call the grammar rule with our special slice object
                            p.callable(pslice)
                            symstack.append(sym)
                            state = goto[statestack[-1]][pname]
                            statestack.append(state)
                        except syntaxerror:
                            # if an error was set. enter error recovery state
                            lookaheadstack.append(lookahead)
                            symstack.pop()
                            statestack.pop()
                            state = statestack[-1]
                            sym.type = 'error'
                            lookahead = sym
                            errorcount = error_count
                            self.errorok = 0
                        continue
                        # !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

                if t == 0:
                    n = symstack[-1]
                    return getattr(n,"value",none)

            if t == none:

                # we have some kind of parsing error here.  to handle
                # this, we are going to push the current token onto
                # the tokenstack and replace it with an 'error' token.
                # if there are any synchronization rules, they may
                # catch it.
                #
                # in addition to pushing the error token, we call call
                # the user defined p_error() function if this is the
                # first syntax error.  this function is only called if
                # errorcount == 0.
                if errorcount == 0 or self.errorok:
                    errorcount = error_count
                    self.errorok = 0
                    errtoken = lookahead
                    if errtoken.type == '$end':
                        errtoken = none               # end of file!
                    if self.errorfunc:
                        global errok,token,restart
                        errok = self.errok        # set some special functions available in error recovery
                        token = get_token
                        restart = self.restart
                        if errtoken and not hasattr(errtoken,'lexer'):
                            errtoken.lexer = lexer
                        tok = self.errorfunc(errtoken)
                        del errok, token, restart   # delete special functions

                        if self.errorok:
                            # user must have done some kind of panic
                            # mode recovery on their own.  the
                            # returned token is the next lookahead
                            lookahead = tok
                            errtoken = none
                            continue
                    else:
                        if errtoken:
                            if hasattr(errtoken,"lineno"): lineno = lookahead.lineno
                            else: lineno = 0
                            if lineno:
                                sys.stderr.write("yacc: syntax error at line %d, token=%s\n" % (lineno, errtoken.type))
                            else:
                                sys.stderr.write("yacc: syntax error, token=%s" % errtoken.type)
                        else:
                            sys.stderr.write("yacc: parse error in input. eof\n")
                            return

                else:
                    errorcount = error_count

                # case 1:  the statestack only has 1 entry on it.  if we're in this state, the
                # entire parse has been rolled back and we're completely hosed.   the token is
                # discarded and we just keep going.

                if len(statestack) <= 1 and lookahead.type != '$end':
                    lookahead = none
                    errtoken = none
                    state = 0
                    # nuke the pushback stack
                    del lookaheadstack[:]
                    continue

                # case 2: the statestack has a couple of entries on it, but we're
                # at the end of the file. nuke the top entry and generate an error token

                # start nuking entries on the stack
                if lookahead.type == '$end':
                    # whoa. we're really hosed here. bail out
                    return

                if lookahead.type != 'error':
                    sym = symstack[-1]
                    if sym.type == 'error':
                        # hmmm. error is on top of stack, we'll just nuke input
                        # symbol and continue
                        lookahead = none
                        continue
                    t = yaccsymbol()
                    t.type = 'error'
                    if hasattr(lookahead,"lineno"):
                        t.lineno = lookahead.lineno
                    t.value = lookahead
                    lookaheadstack.append(lookahead)
                    lookahead = t
                else:
                    symstack.pop()
                    statestack.pop()
                    state = statestack[-1]       # potential bug fix

                continue

            # call an error function here
            raise runtimeerror("yacc: internal parser error!!!\n")

    # !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    # parseopt_notrack().
    #
    # optimized version of parseopt() with line number tracking removed. 
    # do not edit this code directly. copy the optimized version and remove
    # code in the #--! tracking sections
    # !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

    def parseopt_notrack(self,input=none,lexer=none,debug=0,tracking=0,tokenfunc=none):
        lookahead = none                 # current lookahead symbol
        lookaheadstack = [ ]             # stack of lookahead symbols
        actions = self.action            # local reference to action table (to avoid lookup on self.)
        goto    = self.goto              # local reference to goto table (to avoid lookup on self.)
        prod    = self.productions       # local reference to production list (to avoid lookup on self.)
        pslice  = yaccproduction(none)   # production object passed to grammar rules
        errorcount = 0                   # used during error recovery 

        # if no lexer was given, we will try to use the lex module
        if not lexer:
            lex = load_ply_lex()
            lexer = lex.lexer
        
        # set up the lexer and parser objects on pslice
        pslice.lexer = lexer
        pslice.parser = self

        # if input was supplied, pass to lexer
        if input is not none:
            lexer.input(input)

        if tokenfunc is none:
           # tokenize function
           get_token = lexer.token
        else:
           get_token = tokenfunc

        # set up the state and symbol stacks

        statestack = [ ]                # stack of parsing states
        self.statestack = statestack
        symstack   = [ ]                # stack of grammar symbols
        self.symstack = symstack

        pslice.stack = symstack         # put in the production
        errtoken   = none               # err token

        # the start state is assumed to be (0,$end)

        statestack.append(0)
        sym = yaccsymbol()
        sym.type = '$end'
        symstack.append(sym)
        state = 0
        while 1:
            # get the next symbol on the input.  if a lookahead symbol
            # is already set, we just use that. otherwise, we'll pull
            # the next token off of the lookaheadstack or from the lexer

            if not lookahead:
                if not lookaheadstack:
                    lookahead = get_token()     # get the next token
                else:
                    lookahead = lookaheadstack.pop()
                if not lookahead:
                    lookahead = yaccsymbol()
                    lookahead.type = '$end'

            # check the action table
            ltype = lookahead.type
            t = actions[state].get(ltype)

            if t is not none:
                if t > 0:
                    # shift a symbol on the stack
                    statestack.append(t)
                    state = t

                    symstack.append(lookahead)
                    lookahead = none

                    # decrease error count on successful shift
                    if errorcount: errorcount -=1
                    continue

                if t < 0:
                    # reduce a symbol on the stack, emit a production
                    p = prod[-t]
                    pname = p.name
                    plen  = p.len

                    # get production function
                    sym = yaccsymbol()
                    sym.type = pname       # production name
                    sym.value = none

                    if plen:
                        targ = symstack[-plen-1:]
                        targ[0] = sym

                        # !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
                        # the code enclosed in this section is duplicated 
                        # below as a performance optimization.  make sure
                        # changes get made in both locations.

                        pslice.slice = targ
                        
                        try:
                            # call the grammar rule with our special slice object
                            del symstack[-plen:]
                            del statestack[-plen:]
                            p.callable(pslice)
                            symstack.append(sym)
                            state = goto[statestack[-1]][pname]
                            statestack.append(state)
                        except syntaxerror:
                            # if an error was set. enter error recovery state
                            lookaheadstack.append(lookahead)
                            symstack.pop()
                            statestack.pop()
                            state = statestack[-1]
                            sym.type = 'error'
                            lookahead = sym
                            errorcount = error_count
                            self.errorok = 0
                        continue
                        # !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    
                    else:

                        targ = [ sym ]

                        # !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
                        # the code enclosed in this section is duplicated 
                        # above as a performance optimization.  make sure
                        # changes get made in both locations.

                        pslice.slice = targ

                        try:
                            # call the grammar rule with our special slice object
                            p.callable(pslice)
                            symstack.append(sym)
                            state = goto[statestack[-1]][pname]
                            statestack.append(state)
                        except syntaxerror:
                            # if an error was set. enter error recovery state
                            lookaheadstack.append(lookahead)
                            symstack.pop()
                            statestack.pop()
                            state = statestack[-1]
                            sym.type = 'error'
                            lookahead = sym
                            errorcount = error_count
                            self.errorok = 0
                        continue
                        # !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

                if t == 0:
                    n = symstack[-1]
                    return getattr(n,"value",none)

            if t == none:

                # we have some kind of parsing error here.  to handle
                # this, we are going to push the current token onto
                # the tokenstack and replace it with an 'error' token.
                # if there are any synchronization rules, they may
                # catch it.
                #
                # in addition to pushing the error token, we call call
                # the user defined p_error() function if this is the
                # first syntax error.  this function is only called if
                # errorcount == 0.
                if errorcount == 0 or self.errorok:
                    errorcount = error_count
                    self.errorok = 0
                    errtoken = lookahead
                    if errtoken.type == '$end':
                        errtoken = none               # end of file!
                    if self.errorfunc:
                        global errok,token,restart
                        errok = self.errok        # set some special functions available in error recovery
                        token = get_token
                        restart = self.restart
                        if errtoken and not hasattr(errtoken,'lexer'):
                            errtoken.lexer = lexer
                        tok = self.errorfunc(errtoken)
                        del errok, token, restart   # delete special functions

                        if self.errorok:
                            # user must have done some kind of panic
                            # mode recovery on their own.  the
                            # returned token is the next lookahead
                            lookahead = tok
                            errtoken = none
                            continue
                    else:
                        if errtoken:
                            if hasattr(errtoken,"lineno"): lineno = lookahead.lineno
                            else: lineno = 0
                            if lineno:
                                sys.stderr.write("yacc: syntax error at line %d, token=%s\n" % (lineno, errtoken.type))
                            else:
                                sys.stderr.write("yacc: syntax error, token=%s" % errtoken.type)
                        else:
                            sys.stderr.write("yacc: parse error in input. eof\n")
                            return

                else:
                    errorcount = error_count

                # case 1:  the statestack only has 1 entry on it.  if we're in this state, the
                # entire parse has been rolled back and we're completely hosed.   the token is
                # discarded and we just keep going.

                if len(statestack) <= 1 and lookahead.type != '$end':
                    lookahead = none
                    errtoken = none
                    state = 0
                    # nuke the pushback stack
                    del lookaheadstack[:]
                    continue

                # case 2: the statestack has a couple of entries on it, but we're
                # at the end of the file. nuke the top entry and generate an error token

                # start nuking entries on the stack
                if lookahead.type == '$end':
                    # whoa. we're really hosed here. bail out
                    return

                if lookahead.type != 'error':
                    sym = symstack[-1]
                    if sym.type == 'error':
                        # hmmm. error is on top of stack, we'll just nuke input
                        # symbol and continue
                        lookahead = none
                        continue
                    t = yaccsymbol()
                    t.type = 'error'
                    if hasattr(lookahead,"lineno"):
                        t.lineno = lookahead.lineno
                    t.value = lookahead
                    lookaheadstack.append(lookahead)
                    lookahead = t
                else:
                    symstack.pop()
                    statestack.pop()
                    state = statestack[-1]       # potential bug fix

                continue

            # call an error function here
            raise runtimeerror("yacc: internal parser error!!!\n")

# -----------------------------------------------------------------------------
#                          === grammar representation ===
#
# the following functions, classes, and variables are used to represent and
# manipulate the rules that make up a grammar. 
# -----------------------------------------------------------------------------

import re

# regex matching identifiers
_is_identifier = re.compile(r'^[a-za-z0-9_-]+$')

# -----------------------------------------------------------------------------
# class production:
#
# this class stores the raw information about a single production or grammar rule.
# a grammar rule refers to a specification such as this:
#
#       expr : expr plus term 
#
# here are the basic attributes defined on all productions
#
#       name     - name of the production.  for example 'expr'
#       prod     - a list of symbols on the right side ['expr','plus','term']
#       prec     - production precedence level
#       number   - production number.
#       func     - function that executes on reduce
#       file     - file where production function is defined
#       lineno   - line number where production function is defined
#
# the following attributes are defined or optional.
#
#       len       - length of the production (number of symbols on right hand side)
#       usyms     - set of unique symbols found in the production
# -----------------------------------------------------------------------------

class production(object):
    reduced = 0
    def __init__(self,number,name,prod,precedence=('right',0),func=none,file='',line=0):
        self.name     = name
        self.prod     = tuple(prod)
        self.number   = number
        self.func     = func
        self.callable = none
        self.file     = file
        self.line     = line
        self.prec     = precedence

        # internal settings used during table construction
        
        self.len  = len(self.prod)   # length of the production

        # create a list of unique production symbols used in the production
        self.usyms = [ ]             
        for s in self.prod:
            if s not in self.usyms:
                self.usyms.append(s)

        # list of all lr items for the production
        self.lr_items = []
        self.lr_next = none

        # create a string representation
        if self.prod:
            self.str = "%s -> %s" % (self.name," ".join(self.prod))
        else:
            self.str = "%s -> <empty>" % self.name

    def __str__(self):
        return self.str

    def __repr__(self):
        return "production("+str(self)+")"

    def __len__(self):
        return len(self.prod)

    def __nonzero__(self):
        return 1

    def __getitem__(self,index):
        return self.prod[index]
            
    # return the nth lr_item from the production (or none if at the end)
    def lr_item(self,n):
        if n > len(self.prod): return none
        p = lritem(self,n)

        # precompute the list of productions immediately following.  hack. remove later
        try:
            p.lr_after = prodnames[p.prod[n+1]]
        except (indexerror,keyerror):
            p.lr_after = []
        try:
            p.lr_before = p.prod[n-1]
        except indexerror:
            p.lr_before = none

        return p
    
    # bind the production function name to a callable
    def bind(self,pdict):
        if self.func:
            self.callable = pdict[self.func]

# this class serves as a minimal standin for production objects when
# reading table data from files.   it only contains information
# actually used by the lr parsing engine, plus some additional
# debugging information.
class miniproduction(object):
    def __init__(self,str,name,len,func,file,line):
        self.name     = name
        self.len      = len
        self.func     = func
        self.callable = none
        self.file     = file
        self.line     = line
        self.str      = str
    def __str__(self):
        return self.str
    def __repr__(self):
        return "miniproduction(%s)" % self.str

    # bind the production function name to a callable
    def bind(self,pdict):
        if self.func:
            self.callable = pdict[self.func]


# -----------------------------------------------------------------------------
# class lritem
#
# this class represents a specific stage of parsing a production rule.  for
# example: 
#
#       expr : expr . plus term 
#
# in the above, the "." represents the current location of the parse.  here
# basic attributes:
#
#       name       - name of the production.  for example 'expr'
#       prod       - a list of symbols on the right side ['expr','.', 'plus','term']
#       number     - production number.
#
#       lr_next      next lr item. example, if we are ' expr -> expr . plus term'
#                    then lr_next refers to 'expr -> expr plus . term'
#       lr_index   - lr item index (location of the ".") in the prod list.
#       lookaheads - lalr lookahead symbols for this item
#       len        - length of the production (number of symbols on right hand side)
#       lr_after    - list of all productions that immediately follow
#       lr_before   - grammar symbol immediately before
# -----------------------------------------------------------------------------

class lritem(object):
    def __init__(self,p,n):
        self.name       = p.name
        self.prod       = list(p.prod)
        self.number     = p.number
        self.lr_index   = n
        self.lookaheads = { }
        self.prod.insert(n,".")
        self.prod       = tuple(self.prod)
        self.len        = len(self.prod)
        self.usyms      = p.usyms

    def __str__(self):
        if self.prod:
            s = "%s -> %s" % (self.name," ".join(self.prod))
        else:
            s = "%s -> <empty>" % self.name
        return s

    def __repr__(self):
        return "lritem("+str(self)+")"

# -----------------------------------------------------------------------------
# rightmost_terminal()
#
# return the rightmost terminal from a list of symbols.  used in add_production()
# -----------------------------------------------------------------------------
def rightmost_terminal(symbols, terminals):
    i = len(symbols) - 1
    while i >= 0:
        if symbols[i] in terminals:
            return symbols[i]
        i -= 1
    return none

# -----------------------------------------------------------------------------
#                           === grammar class ===
#
# the following class represents the contents of the specified grammar along
# with various computed properties such as first sets, follow sets, lr items, etc.
# this data is used for critical parts of the table generation process later.
# -----------------------------------------------------------------------------

class grammarerror(yaccerror): pass

class grammar(object):
    def __init__(self,terminals):
        self.productions  = [none]  # a list of all of the productions.  the first
                                    # entry is always reserved for the purpose of
                                    # building an augmented grammar

        self.prodnames    = { }     # a dictionary mapping the names of nonterminals to a list of all
                                    # productions of that nonterminal.

        self.prodmap      = { }     # a dictionary that is only used to detect duplicate
                                    # productions.

        self.terminals    = { }     # a dictionary mapping the names of terminal symbols to a
                                    # list of the rules where they are used.

        for term in terminals:
            self.terminals[term] = []

        self.terminals['error'] = []

        self.nonterminals = { }     # a dictionary mapping names of nonterminals to a list
                                    # of rule numbers where they are used.

        self.first        = { }     # a dictionary of precomputed first(x) symbols

        self.follow       = { }     # a dictionary of precomputed follow(x) symbols

        self.precedence   = { }     # precedence rules for each terminal. contains tuples of the
                                    # form ('right',level) or ('nonassoc', level) or ('left',level)

        self.usedprecedence = { }   # precedence rules that were actually used by the grammer.
                                    # this is only used to provide error checking and to generate
                                    # a warning about unused precedence rules.

        self.start = none           # starting symbol for the grammar


    def __len__(self):
        return len(self.productions)

    def __getitem__(self,index):
        return self.productions[index]

    # -----------------------------------------------------------------------------
    # set_precedence()
    #
    # sets the precedence for a given terminal. assoc is the associativity such as
    # 'left','right', or 'nonassoc'.  level is a numeric level.
    #
    # -----------------------------------------------------------------------------

    def set_precedence(self,term,assoc,level):
        assert self.productions == [none],"must call set_precedence() before add_production()"
        if term in self.precedence:
            raise grammarerror("precedence already specified for terminal '%s'" % term)
        if assoc not in ['left','right','nonassoc']:
            raise grammarerror("associativity must be one of 'left','right', or 'nonassoc'")
        self.precedence[term] = (assoc,level)
 
    # -----------------------------------------------------------------------------
    # add_production()
    #
    # given an action function, this function assembles a production rule and
    # computes its precedence level.
    #
    # the production rule is supplied as a list of symbols.   for example,
    # a rule such as 'expr : expr plus term' has a production name of 'expr' and
    # symbols ['expr','plus','term'].
    #
    # precedence is determined by the precedence of the right-most non-terminal
    # or the precedence of a terminal specified by %prec.
    #
    # a variety of error checks are performed to make sure production symbols
    # are valid and that %prec is used correctly.
    # -----------------------------------------------------------------------------

    def add_production(self,prodname,syms,func=none,file='',line=0):

        if prodname in self.terminals:
            raise grammarerror("%s:%d: illegal rule name '%s'. already defined as a token" % (file,line,prodname))
        if prodname == 'error':
            raise grammarerror("%s:%d: illegal rule name '%s'. error is a reserved word" % (file,line,prodname))
        if not _is_identifier.match(prodname):
            raise grammarerror("%s:%d: illegal rule name '%s'" % (file,line,prodname))

        # look for literal tokens 
        for n,s in enumerate(syms):
            if s[0] in "'\"":
                 try:
                     c = eval(s)
                     if (len(c) > 1):
                          raise grammarerror("%s:%d: literal token %s in rule '%s' may only be a single character" % (file,line,s, prodname))
                     if not c in self.terminals:
                          self.terminals[c] = []
                     syms[n] = c
                     continue
                 except syntaxerror:
                     pass
            if not _is_identifier.match(s) and s != '%prec':
                raise grammarerror("%s:%d: illegal name '%s' in rule '%s'" % (file,line,s, prodname))
        
        # determine the precedence level
        if '%prec' in syms:
            if syms[-1] == '%prec':
                raise grammarerror("%s:%d: syntax error. nothing follows %%prec" % (file,line))
            if syms[-2] != '%prec':
                raise grammarerror("%s:%d: syntax error. %%prec can only appear at the end of a grammar rule" % (file,line))
            precname = syms[-1]
            prodprec = self.precedence.get(precname,none)
            if not prodprec:
                raise grammarerror("%s:%d: nothing known about the precedence of '%s'" % (file,line,precname))
            else:
                self.usedprecedence[precname] = 1
            del syms[-2:]     # drop %prec from the rule
        else:
            # if no %prec, precedence is determined by the rightmost terminal symbol
            precname = rightmost_terminal(syms,self.terminals)
            prodprec = self.precedence.get(precname,('right',0)) 
            
        # see if the rule is already in the rulemap
        map = "%s -> %s" % (prodname,syms)
        if map in self.prodmap:
            m = self.prodmap[map]
            raise grammarerror("%s:%d: duplicate rule %s. " % (file,line, m) +
                               "previous definition at %s:%d" % (m.file, m.line))

        # from this point on, everything is valid.  create a new production instance
        pnumber  = len(self.productions)
        if not prodname in self.nonterminals:
            self.nonterminals[prodname] = [ ]

        # add the production number to terminals and nonterminals
        for t in syms:
            if t in self.terminals:
                self.terminals[t].append(pnumber)
            else:
                if not t in self.nonterminals:
                    self.nonterminals[t] = [ ]
                self.nonterminals[t].append(pnumber)

        # create a production and add it to the list of productions
        p = production(pnumber,prodname,syms,prodprec,func,file,line)
        self.productions.append(p)
        self.prodmap[map] = p

        # add to the global productions list
        try:
            self.prodnames[prodname].append(p)
        except keyerror:
            self.prodnames[prodname] = [ p ]
        return 0

    # -----------------------------------------------------------------------------
    # set_start()
    #
    # sets the starting symbol and creates the augmented grammar.  production 
    # rule 0 is s' -> start where start is the start symbol.
    # -----------------------------------------------------------------------------

    def set_start(self,start=none):
        if not start:
            start = self.productions[1].name
        if start not in self.nonterminals:
            raise grammarerror("start symbol %s undefined" % start)
        self.productions[0] = production(0,"s'",[start])
        self.nonterminals[start].append(0)
        self.start = start

    # -----------------------------------------------------------------------------
    # find_unreachable()
    #
    # find all of the nonterminal symbols that can't be reached from the starting
    # symbol.  returns a list of nonterminals that can't be reached.
    # -----------------------------------------------------------------------------

    def find_unreachable(self):
        
        # mark all symbols that are reachable from a symbol s
        def mark_reachable_from(s):
            if reachable[s]:
                # we've already reached symbol s.
                return
            reachable[s] = 1
            for p in self.prodnames.get(s,[]):
                for r in p.prod:
                    mark_reachable_from(r)

        reachable   = { }
        for s in list(self.terminals) + list(self.nonterminals):
            reachable[s] = 0

        mark_reachable_from( self.productions[0].prod[0] )

        return [s for s in list(self.nonterminals)
                        if not reachable[s]]
    
    # -----------------------------------------------------------------------------
    # infinite_cycles()
    #
    # this function looks at the various parsing rules and tries to detect
    # infinite recursion cycles (grammar rules where there is no possible way
    # to derive a string of only terminals).
    # -----------------------------------------------------------------------------

    def infinite_cycles(self):
        terminates = {}

        # terminals:
        for t in self.terminals:
            terminates[t] = 1

        terminates['$end'] = 1

        # nonterminals:

        # initialize to false:
        for n in self.nonterminals:
            terminates[n] = 0

        # then propagate termination until no change:
        while 1:
            some_change = 0
            for (n,pl) in self.prodnames.items():
                # nonterminal n terminates iff any of its productions terminates.
                for p in pl:
                    # production p terminates iff all of its rhs symbols terminate.
                    for s in p.prod:
                        if not terminates[s]:
                            # the symbol s does not terminate,
                            # so production p does not terminate.
                            p_terminates = 0
                            break
                    else:
                        # didn't break from the loop,
                        # so every symbol s terminates
                        # so production p terminates.
                        p_terminates = 1

                    if p_terminates:
                        # symbol n terminates!
                        if not terminates[n]:
                            terminates[n] = 1
                            some_change = 1
                        # don't need to consider any more productions for this n.
                        break

            if not some_change:
                break

        infinite = []
        for (s,term) in terminates.items():
            if not term:
                if not s in self.prodnames and not s in self.terminals and s != 'error':
                    # s is used-but-not-defined, and we've already warned of that,
                    # so it would be overkill to say that it's also non-terminating.
                    pass
                else:
                    infinite.append(s)

        return infinite


    # -----------------------------------------------------------------------------
    # undefined_symbols()
    #
    # find all symbols that were used the grammar, but not defined as tokens or
    # grammar rules.  returns a list of tuples (sym, prod) where sym in the symbol
    # and prod is the production where the symbol was used. 
    # -----------------------------------------------------------------------------
    def undefined_symbols(self):
        result = []
        for p in self.productions:
            if not p: continue

            for s in p.prod:
                if not s in self.prodnames and not s in self.terminals and s != 'error':
                    result.append((s,p))
        return result

    # -----------------------------------------------------------------------------
    # unused_terminals()
    #
    # find all terminals that were defined, but not used by the grammar.  returns
    # a list of all symbols.
    # -----------------------------------------------------------------------------
    def unused_terminals(self):
        unused_tok = []
        for s,v in self.terminals.items():
            if s != 'error' and not v:
                unused_tok.append(s)

        return unused_tok

    # ------------------------------------------------------------------------------
    # unused_rules()
    #
    # find all grammar rules that were defined,  but not used (maybe not reachable)
    # returns a list of productions.
    # ------------------------------------------------------------------------------

    def unused_rules(self):
        unused_prod = []
        for s,v in self.nonterminals.items():
            if not v:
                p = self.prodnames[s][0]
                unused_prod.append(p)
        return unused_prod

    # -----------------------------------------------------------------------------
    # unused_precedence()
    #
    # returns a list of tuples (term,precedence) corresponding to precedence
    # rules that were never used by the grammar.  term is the name of the terminal
    # on which precedence was applied and precedence is a string such as 'left' or
    # 'right' corresponding to the type of precedence. 
    # -----------------------------------------------------------------------------

    def unused_precedence(self):
        unused = []
        for termname in self.precedence:
            if not (termname in self.terminals or termname in self.usedprecedence):
                unused.append((termname,self.precedence[termname][0]))
                
        return unused

    # -------------------------------------------------------------------------
    # _first()
    #
    # compute the value of first1(beta) where beta is a tuple of symbols.
    #
    # during execution of compute_first1, the result may be incomplete.
    # afterward (e.g., when called from compute_follow()), it will be complete.
    # -------------------------------------------------------------------------
    def _first(self,beta):

        # we are computing first(x1,x2,x3,...,xn)
        result = [ ]
        for x in beta:
            x_produces_empty = 0

            # add all the non-<empty> symbols of first[x] to the result.
            for f in self.first[x]:
                if f == '<empty>':
                    x_produces_empty = 1
                else:
                    if f not in result: result.append(f)

            if x_produces_empty:
                # we have to consider the next x in beta,
                # i.e. stay in the loop.
                pass
            else:
                # we don't have to consider any further symbols in beta.
                break
        else:
            # there was no 'break' from the loop,
            # so x_produces_empty was true for all x in beta,
            # so beta produces empty as well.
            result.append('<empty>')

        return result

    # -------------------------------------------------------------------------
    # compute_first()
    #
    # compute the value of first1(x) for all symbols
    # -------------------------------------------------------------------------
    def compute_first(self):
        if self.first:
            return self.first

        # terminals:
        for t in self.terminals:
            self.first[t] = [t]

        self.first['$end'] = ['$end']

        # nonterminals:

        # initialize to the empty set:
        for n in self.nonterminals:
            self.first[n] = []

        # then propagate symbols until no change:
        while 1:
            some_change = 0
            for n in self.nonterminals:
                for p in self.prodnames[n]:
                    for f in self._first(p.prod):
                        if f not in self.first[n]:
                            self.first[n].append( f )
                            some_change = 1
            if not some_change:
                break
        
        return self.first

    # ---------------------------------------------------------------------
    # compute_follow()
    #
    # computes all of the follow sets for every non-terminal symbol.  the
    # follow set is the set of all symbols that might follow a given
    # non-terminal.  see the dragon book, 2nd ed. p. 189.
    # ---------------------------------------------------------------------
    def compute_follow(self,start=none):
        # if already computed, return the result
        if self.follow:
            return self.follow

        # if first sets not computed yet, do that first.
        if not self.first:
            self.compute_first()

        # add '$end' to the follow list of the start symbol
        for k in self.nonterminals:
            self.follow[k] = [ ]

        if not start:
            start = self.productions[1].name

        self.follow[start] = [ '$end' ]

        while 1:
            didadd = 0
            for p in self.productions[1:]:
                # here is the production set
                for i in range(len(p.prod)):
                    b = p.prod[i]
                    if b in self.nonterminals:
                        # okay. we got a non-terminal in a production
                        fst = self._first(p.prod[i+1:])
                        hasempty = 0
                        for f in fst:
                            if f != '<empty>' and f not in self.follow[b]:
                                self.follow[b].append(f)
                                didadd = 1
                            if f == '<empty>':
                                hasempty = 1
                        if hasempty or i == (len(p.prod)-1):
                            # add elements of follow(a) to follow(b)
                            for f in self.follow[p.name]:
                                if f not in self.follow[b]:
                                    self.follow[b].append(f)
                                    didadd = 1
            if not didadd: break
        return self.follow


    # -----------------------------------------------------------------------------
    # build_lritems()
    #
    # this function walks the list of productions and builds a complete set of the
    # lr items.  the lr items are stored in two ways:  first, they are uniquely
    # numbered and placed in the list _lritems.  second, a linked list of lr items
    # is built for each production.  for example:
    #
    #   e -> e plus e
    #
    # creates the list
    #
    #  [e -> . e plus e, e -> e . plus e, e -> e plus . e, e -> e plus e . ]
    # -----------------------------------------------------------------------------

    def build_lritems(self):
        for p in self.productions:
            lastlri = p
            i = 0
            lr_items = []
            while 1:
                if i > len(p):
                    lri = none
                else:
                    lri = lritem(p,i)
                    # precompute the list of productions immediately following
                    try:
                        lri.lr_after = self.prodnames[lri.prod[i+1]]
                    except (indexerror,keyerror):
                        lri.lr_after = []
                    try:
                        lri.lr_before = lri.prod[i-1]
                    except indexerror:
                        lri.lr_before = none

                lastlri.lr_next = lri
                if not lri: break
                lr_items.append(lri)
                lastlri = lri
                i += 1
            p.lr_items = lr_items

# -----------------------------------------------------------------------------
#                            == class lrtable ==
#
# this basic class represents a basic table of lr parsing information.  
# methods for generating the tables are not defined here.  they are defined
# in the derived class lrgeneratedtable.
# -----------------------------------------------------------------------------

class versionerror(yaccerror): pass

class lrtable(object):
    def __init__(self):
        self.lr_action = none
        self.lr_goto = none
        self.lr_productions = none
        self.lr_method = none

    def read_table(self,module):
        if isinstance(module,types.moduletype):
            parsetab = module
        else:
            if sys.version_info[0] < 3:
                exec("import %s as parsetab" % module)
            else:
                env = { }
                exec("import %s as parsetab" % module, env, env)
                parsetab = env['parsetab']

        if parsetab._tabversion != __tabversion__:
            raise versionerror("yacc table file version is out of date")

        self.lr_action = parsetab._lr_action
        self.lr_goto = parsetab._lr_goto

        self.lr_productions = []
        for p in parsetab._lr_productions:
            self.lr_productions.append(miniproduction(*p))

        self.lr_method = parsetab._lr_method
        return parsetab._lr_signature

    def read_pickle(self,filename):
        try:
            import cpickle as pickle
        except importerror:
            import pickle

        in_f = open(filename,"rb")

        tabversion = pickle.load(in_f)
        if tabversion != __tabversion__:
            raise versionerror("yacc table file version is out of date")
        self.lr_method = pickle.load(in_f)
        signature      = pickle.load(in_f)
        self.lr_action = pickle.load(in_f)
        self.lr_goto   = pickle.load(in_f)
        productions    = pickle.load(in_f)

        self.lr_productions = []
        for p in productions:
            self.lr_productions.append(miniproduction(*p))

        in_f.close()
        return signature

    # bind all production function names to callable objects in pdict
    def bind_callables(self,pdict):
        for p in self.lr_productions:
            p.bind(pdict)
    
# -----------------------------------------------------------------------------
#                           === lr generator ===
#
# the following classes and functions are used to generate lr parsing tables on 
# a grammar.
# -----------------------------------------------------------------------------

# -----------------------------------------------------------------------------
# digraph()
# traverse()
#
# the following two functions are used to compute set valued functions
# of the form:
#
#     f(x) = f'(x) u u{f(y) | x r y}
#
# this is used to compute the values of read() sets as well as follow sets
# in lalr(1) generation.
#
# inputs:  x    - an input set
#          r    - a relation
#          fp   - set-valued function
# ------------------------------------------------------------------------------

def digraph(x,r,fp):
    n = { }
    for x in x:
       n[x] = 0
    stack = []
    f = { }
    for x in x:
        if n[x] == 0: traverse(x,n,stack,f,x,r,fp)
    return f

def traverse(x,n,stack,f,x,r,fp):
    stack.append(x)
    d = len(stack)
    n[x] = d
    f[x] = fp(x)             # f(x) <- f'(x)

    rel = r(x)               # get y's related to x
    for y in rel:
        if n[y] == 0:
             traverse(y,n,stack,f,x,r,fp)
        n[x] = min(n[x],n[y])
        for a in f.get(y,[]):
            if a not in f[x]: f[x].append(a)
    if n[x] == d:
       n[stack[-1]] = maxint
       f[stack[-1]] = f[x]
       element = stack.pop()
       while element != x:
           n[stack[-1]] = maxint
           f[stack[-1]] = f[x]
           element = stack.pop()

class lalrerror(yaccerror): pass

# -----------------------------------------------------------------------------
#                             == lrgeneratedtable ==
#
# this class implements the lr table generation algorithm.  there are no
# public methods except for write()
# -----------------------------------------------------------------------------

class lrgeneratedtable(lrtable):
    def __init__(self,grammar,method='lalr',log=none):
        if method not in ['slr','lalr']:
            raise lalrerror("unsupported method %s" % method)

        self.grammar = grammar
        self.lr_method = method

        # set up the logger
        if not log:
            log = nulllogger()
        self.log = log

        # internal attributes
        self.lr_action     = {}        # action table
        self.lr_goto       = {}        # goto table
        self.lr_productions  = grammar.productions    # copy of grammar production array
        self.lr_goto_cache = {}        # cache of computed gotos
        self.lr0_cidhash   = {}        # cache of closures

        self._add_count    = 0         # internal counter used to detect cycles

        # diagonistic information filled in by the table generator
        self.sr_conflict   = 0
        self.rr_conflict   = 0
        self.conflicts     = []        # list of conflicts

        self.sr_conflicts  = []
        self.rr_conflicts  = []

        # build the tables
        self.grammar.build_lritems()
        self.grammar.compute_first()
        self.grammar.compute_follow()
        self.lr_parse_table()

    # compute the lr(0) closure operation on i, where i is a set of lr(0) items.

    def lr0_closure(self,i):
        self._add_count += 1

        # add everything in i to j
        j = i[:]
        didadd = 1
        while didadd:
            didadd = 0
            for j in j:
                for x in j.lr_after:
                    if getattr(x,"lr0_added",0) == self._add_count: continue
                    # add b --> .g to j
                    j.append(x.lr_next)
                    x.lr0_added = self._add_count
                    didadd = 1

        return j

    # compute the lr(0) goto function goto(i,x) where i is a set
    # of lr(0) items and x is a grammar symbol.   this function is written
    # in a way that guarantees uniqueness of the generated goto sets
    # (i.e. the same goto set will never be returned as two different python
    # objects).  with uniqueness, we can later do fast set comparisons using
    # id(obj) instead of element-wise comparison.

    def lr0_goto(self,i,x):
        # first we look for a previously cached entry
        g = self.lr_goto_cache.get((id(i),x),none)
        if g: return g

        # now we generate the goto set in a way that guarantees uniqueness
        # of the result

        s = self.lr_goto_cache.get(x,none)
        if not s:
            s = { }
            self.lr_goto_cache[x] = s

        gs = [ ]
        for p in i:
            n = p.lr_next
            if n and n.lr_before == x:
                s1 = s.get(id(n),none)
                if not s1:
                    s1 = { }
                    s[id(n)] = s1
                gs.append(n)
                s = s1
        g = s.get('$end',none)
        if not g:
            if gs:
                g = self.lr0_closure(gs)
                s['$end'] = g
            else:
                s['$end'] = gs
        self.lr_goto_cache[(id(i),x)] = g
        return g

    # compute the lr(0) sets of item function
    def lr0_items(self):

        c = [ self.lr0_closure([self.grammar.productions[0].lr_next]) ]
        i = 0
        for i in c:
            self.lr0_cidhash[id(i)] = i
            i += 1

        # loop over the items in c and each grammar symbols
        i = 0
        while i < len(c):
            i = c[i]
            i += 1

            # collect all of the symbols that could possibly be in the goto(i,x) sets
            asyms = { }
            for ii in i:
                for s in ii.usyms:
                    asyms[s] = none

            for x in asyms:
                g = self.lr0_goto(i,x)
                if not g:  continue
                if id(g) in self.lr0_cidhash: continue
                self.lr0_cidhash[id(g)] = len(c)
                c.append(g)

        return c

    # -----------------------------------------------------------------------------
    #                       ==== lalr(1) parsing ====
    #
    # lalr(1) parsing is almost exactly the same as slr except that instead of
    # relying upon follow() sets when performing reductions, a more selective
    # lookahead set that incorporates the state of the lr(0) machine is utilized.
    # thus, we mainly just have to focus on calculating the lookahead sets.
    #
    # the method used here is due to deremer and pennelo (1982).
    #
    # deremer, f. l., and t. j. pennelo: "efficient computation of lalr(1)
    #     lookahead sets", acm transactions on programming languages and systems,
    #     vol. 4, no. 4, oct. 1982, pp. 615-649
    #
    # further details can also be found in:
    #
    #  j. tremblay and p. sorenson, "the theory and practice of compiler writing",
    #      mcgraw-hill book company, (1985).
    #
    # -----------------------------------------------------------------------------

    # -----------------------------------------------------------------------------
    # compute_nullable_nonterminals()
    #
    # creates a dictionary containing all of the non-terminals that might produce
    # an empty production.
    # -----------------------------------------------------------------------------

    def compute_nullable_nonterminals(self):
        nullable = {}
        num_nullable = 0
        while 1:
           for p in self.grammar.productions[1:]:
               if p.len == 0:
                    nullable[p.name] = 1
                    continue
               for t in p.prod:
                    if not t in nullable: break
               else:
                    nullable[p.name] = 1
           if len(nullable) == num_nullable: break
           num_nullable = len(nullable)
        return nullable

    # -----------------------------------------------------------------------------
    # find_nonterminal_trans(c)
    #
    # given a set of lr(0) items, this functions finds all of the non-terminal
    # transitions.    these are transitions in which a dot appears immediately before
    # a non-terminal.   returns a list of tuples of the form (state,n) where state
    # is the state number and n is the nonterminal symbol.
    #
    # the input c is the set of lr(0) items.
    # -----------------------------------------------------------------------------

    def find_nonterminal_transitions(self,c):
         trans = []
         for state in range(len(c)):
             for p in c[state]:
                 if p.lr_index < p.len - 1:
                      t = (state,p.prod[p.lr_index+1])
                      if t[1] in self.grammar.nonterminals:
                            if t not in trans: trans.append(t)
             state = state + 1
         return trans

    # -----------------------------------------------------------------------------
    # dr_relation()
    #
    # computes the dr(p,a) relationships for non-terminal transitions.  the input
    # is a tuple (state,n) where state is a number and n is a nonterminal symbol.
    #
    # returns a list of terminals.
    # -----------------------------------------------------------------------------

    def dr_relation(self,c,trans,nullable):
        dr_set = { }
        state,n = trans
        terms = []

        g = self.lr0_goto(c[state],n)
        for p in g:
           if p.lr_index < p.len - 1:
               a = p.prod[p.lr_index+1]
               if a in self.grammar.terminals:
                   if a not in terms: terms.append(a)

        # this extra bit is to handle the start state
        if state == 0 and n == self.grammar.productions[0].prod[0]:
           terms.append('$end')

        return terms

    # -----------------------------------------------------------------------------
    # reads_relation()
    #
    # computes the reads() relation (p,a) reads (t,c).
    # -----------------------------------------------------------------------------

    def reads_relation(self,c, trans, empty):
        # look for empty transitions
        rel = []
        state, n = trans

        g = self.lr0_goto(c[state],n)
        j = self.lr0_cidhash.get(id(g),-1)
        for p in g:
            if p.lr_index < p.len - 1:
                 a = p.prod[p.lr_index + 1]
                 if a in empty:
                      rel.append((j,a))

        return rel

    # -----------------------------------------------------------------------------
    # compute_lookback_includes()
    #
    # determines the lookback and includes relations
    #
    # lookback:
    #
    # this relation is determined by running the lr(0) state machine forward.
    # for example, starting with a production "n : . a b c", we run it forward
    # to obtain "n : a b c ."   we then build a relationship between this final
    # state and the starting state.   these relationships are stored in a dictionary
    # lookdict.
    #
    # includes:
    #
    # computes the include() relation (p,a) includes (p',b).
    #
    # this relation is used to determine non-terminal transitions that occur
    # inside of other non-terminal transition states.   (p,a) includes (p', b)
    # if the following holds:
    #
    #       b -> lat, where t -> epsilon and p' -l-> p
    #
    # l is essentially a prefix (which may be empty), t is a suffix that must be
    # able to derive an empty string.  state p' must lead to state p with the string l.
    #
    # -----------------------------------------------------------------------------

    def compute_lookback_includes(self,c,trans,nullable):

        lookdict = {}          # dictionary of lookback relations
        includedict = {}       # dictionary of include relations

        # make a dictionary of non-terminal transitions
        dtrans = {}
        for t in trans:
            dtrans[t] = 1

        # loop over all transitions and compute lookbacks and includes
        for state,n in trans:
            lookb = []
            includes = []
            for p in c[state]:
                if p.name != n: continue

                # okay, we have a name match.  we now follow the production all the way
                # through the state machine until we get the . on the right hand side

                lr_index = p.lr_index
                j = state
                while lr_index < p.len - 1:
                     lr_index = lr_index + 1
                     t = p.prod[lr_index]

                     # check to see if this symbol and state are a non-terminal transition
                     if (j,t) in dtrans:
                           # yes.  okay, there is some chance that this is an includes relation
                           # the only way to know for certain is whether the rest of the
                           # production derives empty

                           li = lr_index + 1
                           while li < p.len:
                                if p.prod[li] in self.grammar.terminals: break      # no forget it
                                if not p.prod[li] in nullable: break
                                li = li + 1
                           else:
                                # appears to be a relation between (j,t) and (state,n)
                                includes.append((j,t))

                     g = self.lr0_goto(c[j],t)               # go to next set
                     j = self.lr0_cidhash.get(id(g),-1)     # go to next state

                # when we get here, j is the final state, now we have to locate the production
                for r in c[j]:
                     if r.name != p.name: continue
                     if r.len != p.len:   continue
                     i = 0
                     # this look is comparing a production ". a b c" with "a b c ."
                     while i < r.lr_index:
                          if r.prod[i] != p.prod[i+1]: break
                          i = i + 1
                     else:
                          lookb.append((j,r))
            for i in includes:
                 if not i in includedict: includedict[i] = []
                 includedict[i].append((state,n))
            lookdict[(state,n)] = lookb

        return lookdict,includedict

    # -----------------------------------------------------------------------------
    # compute_read_sets()
    #
    # given a set of lr(0) items, this function computes the read sets.
    #
    # inputs:  c        =  set of lr(0) items
    #          ntrans   = set of nonterminal transitions
    #          nullable = set of empty transitions
    #
    # returns a set containing the read sets
    # -----------------------------------------------------------------------------

    def compute_read_sets(self,c, ntrans, nullable):
        fp = lambda x: self.dr_relation(c,x,nullable)
        r =  lambda x: self.reads_relation(c,x,nullable)
        f = digraph(ntrans,r,fp)
        return f

    # -----------------------------------------------------------------------------
    # compute_follow_sets()
    #
    # given a set of lr(0) items, a set of non-terminal transitions, a readset,
    # and an include set, this function computes the follow sets
    #
    # follow(p,a) = read(p,a) u u {follow(p',b) | (p,a) includes (p',b)}
    #
    # inputs:
    #            ntrans     = set of nonterminal transitions
    #            readsets   = readset (previously computed)
    #            inclsets   = include sets (previously computed)
    #
    # returns a set containing the follow sets
    # -----------------------------------------------------------------------------

    def compute_follow_sets(self,ntrans,readsets,inclsets):
         fp = lambda x: readsets[x]
         r  = lambda x: inclsets.get(x,[])
         f = digraph(ntrans,r,fp)
         return f

    # -----------------------------------------------------------------------------
    # add_lookaheads()
    #
    # attaches the lookahead symbols to grammar rules.
    #
    # inputs:    lookbacks         -  set of lookback relations
    #            followset         -  computed follow set
    #
    # this function directly attaches the lookaheads to productions contained
    # in the lookbacks set
    # -----------------------------------------------------------------------------

    def add_lookaheads(self,lookbacks,followset):
        for trans,lb in lookbacks.items():
            # loop over productions in lookback
            for state,p in lb:
                 if not state in p.lookaheads:
                      p.lookaheads[state] = []
                 f = followset.get(trans,[])
                 for a in f:
                      if a not in p.lookaheads[state]: p.lookaheads[state].append(a)

    # -----------------------------------------------------------------------------
    # add_lalr_lookaheads()
    #
    # this function does all of the work of adding lookahead information for use
    # with lalr parsing
    # -----------------------------------------------------------------------------

    def add_lalr_lookaheads(self,c):
        # determine all of the nullable nonterminals
        nullable = self.compute_nullable_nonterminals()

        # find all non-terminal transitions
        trans = self.find_nonterminal_transitions(c)

        # compute read sets
        readsets = self.compute_read_sets(c,trans,nullable)

        # compute lookback/includes relations
        lookd, included = self.compute_lookback_includes(c,trans,nullable)

        # compute lalr follow sets
        followsets = self.compute_follow_sets(trans,readsets,included)

        # add all of the lookaheads
        self.add_lookaheads(lookd,followsets)

    # -----------------------------------------------------------------------------
    # lr_parse_table()
    #
    # this function constructs the parse tables for slr or lalr
    # -----------------------------------------------------------------------------
    def lr_parse_table(self):
        productions = self.grammar.productions
        precedence  = self.grammar.precedence
        goto   = self.lr_goto         # goto array
        action = self.lr_action       # action array
        log    = self.log             # logger for output

        actionp = { }                 # action production array (temporary)
        
        log.info("parsing method: %s", self.lr_method)

        # step 1: construct c = { i0, i1, ... in}, collection of lr(0) items
        # this determines the number of states

        c = self.lr0_items()

        if self.lr_method == 'lalr':
            self.add_lalr_lookaheads(c)

        # build the parser table, state by state
        st = 0
        for i in c:
            # loop over each production in i
            actlist = [ ]              # list of actions
            st_action  = { }
            st_actionp = { }
            st_goto    = { }
            log.info("")
            log.info("state %d", st)
            log.info("")
            for p in i:
                log.info("    (%d) %s", p.number, str(p))
            log.info("")

            for p in i:
                    if p.len == p.lr_index + 1:
                        if p.name == "s'":
                            # start symbol. accept!
                            st_action["$end"] = 0
                            st_actionp["$end"] = p
                        else:
                            # we are at the end of a production.  reduce!
                            if self.lr_method == 'lalr':
                                laheads = p.lookaheads[st]
                            else:
                                laheads = self.grammar.follow[p.name]
                            for a in laheads:
                                actlist.append((a,p,"reduce using rule %d (%s)" % (p.number,p)))
                                r = st_action.get(a,none)
                                if r is not none:
                                    # whoa. have a shift/reduce or reduce/reduce conflict
                                    if r > 0:
                                        # need to decide on shift or reduce here
                                        # by default we favor shifting. need to add
                                        # some precedence rules here.
                                        sprec,slevel = productions[st_actionp[a].number].prec
                                        rprec,rlevel = precedence.get(a,('right',0))
                                        if (slevel < rlevel) or ((slevel == rlevel) and (rprec == 'left')):
                                            # we really need to reduce here.
                                            st_action[a] = -p.number
                                            st_actionp[a] = p
                                            if not slevel and not rlevel:
                                                log.info("  ! shift/reduce conflict for %s resolved as reduce",a)
                                                self.sr_conflicts.append((st,a,'reduce'))
                                            productions[p.number].reduced += 1
                                        elif (slevel == rlevel) and (rprec == 'nonassoc'):
                                            st_action[a] = none
                                        else:
                                            # hmmm. guess we'll keep the shift
                                            if not rlevel:
                                                log.info("  ! shift/reduce conflict for %s resolved as shift",a)
                                                self.sr_conflicts.append((st,a,'shift'))
                                    elif r < 0:
                                        # reduce/reduce conflict.   in this case, we favor the rule
                                        # that was defined first in the grammar file
                                        oldp = productions[-r]
                                        pp = productions[p.number]
                                        if oldp.line > pp.line:
                                            st_action[a] = -p.number
                                            st_actionp[a] = p
                                            chosenp,rejectp = pp,oldp
                                            productions[p.number].reduced += 1
                                            productions[oldp.number].reduced -= 1
                                        else:
                                            chosenp,rejectp = oldp,pp
                                        self.rr_conflicts.append((st,chosenp,rejectp))
                                        log.info("  ! reduce/reduce conflict for %s resolved using rule %d (%s)", a,st_actionp[a].number, st_actionp[a])
                                    else:
                                        raise lalrerror("unknown conflict in state %d" % st)
                                else:
                                    st_action[a] = -p.number
                                    st_actionp[a] = p
                                    productions[p.number].reduced += 1
                    else:
                        i = p.lr_index
                        a = p.prod[i+1]       # get symbol right after the "."
                        if a in self.grammar.terminals:
                            g = self.lr0_goto(i,a)
                            j = self.lr0_cidhash.get(id(g),-1)
                            if j >= 0:
                                # we are in a shift state
                                actlist.append((a,p,"shift and go to state %d" % j))
                                r = st_action.get(a,none)
                                if r is not none:
                                    # whoa have a shift/reduce or shift/shift conflict
                                    if r > 0:
                                        if r != j:
                                            raise lalrerror("shift/shift conflict in state %d" % st)
                                    elif r < 0:
                                        # do a precedence check.
                                        #   -  if precedence of reduce rule is higher, we reduce.
                                        #   -  if precedence of reduce is same and left assoc, we reduce.
                                        #   -  otherwise we shift
                                        rprec,rlevel = productions[st_actionp[a].number].prec
                                        sprec,slevel = precedence.get(a,('right',0))
                                        if (slevel > rlevel) or ((slevel == rlevel) and (rprec == 'right')):
                                            # we decide to shift here... highest precedence to shift
                                            productions[st_actionp[a].number].reduced -= 1
                                            st_action[a] = j
                                            st_actionp[a] = p
                                            if not rlevel:
                                                log.info("  ! shift/reduce conflict for %s resolved as shift",a)
                                                self.sr_conflicts.append((st,a,'shift'))
                                        elif (slevel == rlevel) and (rprec == 'nonassoc'):
                                            st_action[a] = none
                                        else:
                                            # hmmm. guess we'll keep the reduce
                                            if not slevel and not rlevel:
                                                log.info("  ! shift/reduce conflict for %s resolved as reduce",a)
                                                self.sr_conflicts.append((st,a,'reduce'))

                                    else:
                                        raise lalrerror("unknown conflict in state %d" % st)
                                else:
                                    st_action[a] = j
                                    st_actionp[a] = p

            # print the actions associated with each terminal
            _actprint = { }
            for a,p,m in actlist:
                if a in st_action:
                    if p is st_actionp[a]:
                        log.info("    %-15s %s",a,m)
                        _actprint[(a,m)] = 1
            log.info("")
            # print the actions that were not used. (debugging)
            not_used = 0
            for a,p,m in actlist:
                if a in st_action:
                    if p is not st_actionp[a]:
                        if not (a,m) in _actprint:
                            log.debug("  ! %-15s [ %s ]",a,m)
                            not_used = 1
                            _actprint[(a,m)] = 1
            if not_used:
                log.debug("")

            # construct the goto table for this state

            nkeys = { }
            for ii in i:
                for s in ii.usyms:
                    if s in self.grammar.nonterminals:
                        nkeys[s] = none
            for n in nkeys:
                g = self.lr0_goto(i,n)
                j = self.lr0_cidhash.get(id(g),-1)
                if j >= 0:
                    st_goto[n] = j
                    log.info("    %-30s shift and go to state %d",n,j)

            action[st] = st_action
            actionp[st] = st_actionp
            goto[st] = st_goto
            st += 1


    # -----------------------------------------------------------------------------
    # write()
    #
    # this function writes the lr parsing tables to a file
    # -----------------------------------------------------------------------------

    def write_table(self,modulename,outputdir='',signature=""):
        basemodulename = modulename.split(".")[-1]
        filename = os.path.join(outputdir,basemodulename) + ".py"
        try:
            f = open(filename,"w")

            f.write("""
# %s
# this file is automatically generated. do not edit.
_tabversion = %r

_lr_method = %r

_lr_signature = %r
    """ % (filename, __tabversion__, self.lr_method, signature))

            # change smaller to 0 to go back to original tables
            smaller = 1

            # factor out names to try and make smaller
            if smaller:
                items = { }

                for s,nd in self.lr_action.items():
                   for name,v in nd.items():
                      i = items.get(name)
                      if not i:
                         i = ([],[])
                         items[name] = i
                      i[0].append(s)
                      i[1].append(v)

                f.write("\n_lr_action_items = {")
                for k,v in items.items():
                    f.write("%r:([" % k)
                    for i in v[0]:
                        f.write("%r," % i)
                    f.write("],[")
                    for i in v[1]:
                        f.write("%r," % i)

                    f.write("]),")
                f.write("}\n")

                f.write("""
_lr_action = { }
for _k, _v in _lr_action_items.items():
   for _x,_y in zip(_v[0],_v[1]):
      if not _x in _lr_action:  _lr_action[_x] = { }
      _lr_action[_x][_k] = _y
del _lr_action_items
""")

            else:
                f.write("\n_lr_action = { ");
                for k,v in self.lr_action.items():
                    f.write("(%r,%r):%r," % (k[0],k[1],v))
                f.write("}\n");

            if smaller:
                # factor out names to try and make smaller
                items = { }

                for s,nd in self.lr_goto.items():
                   for name,v in nd.items():
                      i = items.get(name)
                      if not i:
                         i = ([],[])
                         items[name] = i
                      i[0].append(s)
                      i[1].append(v)

                f.write("\n_lr_goto_items = {")
                for k,v in items.items():
                    f.write("%r:([" % k)
                    for i in v[0]:
                        f.write("%r," % i)
                    f.write("],[")
                    for i in v[1]:
                        f.write("%r," % i)

                    f.write("]),")
                f.write("}\n")

                f.write("""
_lr_goto = { }
for _k, _v in _lr_goto_items.items():
   for _x,_y in zip(_v[0],_v[1]):
       if not _x in _lr_goto: _lr_goto[_x] = { }
       _lr_goto[_x][_k] = _y
del _lr_goto_items
""")
            else:
                f.write("\n_lr_goto = { ");
                for k,v in self.lr_goto.items():
                    f.write("(%r,%r):%r," % (k[0],k[1],v))
                f.write("}\n");

            # write production table
            f.write("_lr_productions = [\n")
            for p in self.lr_productions:
                if p.func:
                    f.write("  (%r,%r,%d,%r,%r,%d),\n" % (p.str,p.name, p.len, p.func,p.file,p.line))
                else:
                    f.write("  (%r,%r,%d,none,none,none),\n" % (str(p),p.name, p.len))
            f.write("]\n")
            f.close()

        except ioerror:
            e = sys.exc_info()[1]
            sys.stderr.write("unable to create '%s'\n" % filename)
            sys.stderr.write(str(e)+"\n")
            return


    # -----------------------------------------------------------------------------
    # pickle_table()
    #
    # this function pickles the lr parsing tables to a supplied file object
    # -----------------------------------------------------------------------------

    def pickle_table(self,filename,signature=""):
        try:
            import cpickle as pickle
        except importerror:
            import pickle
        outf = open(filename,"wb")
        pickle.dump(__tabversion__,outf,pickle_protocol)
        pickle.dump(self.lr_method,outf,pickle_protocol)
        pickle.dump(signature,outf,pickle_protocol)
        pickle.dump(self.lr_action,outf,pickle_protocol)
        pickle.dump(self.lr_goto,outf,pickle_protocol)

        outp = []
        for p in self.lr_productions:
            if p.func:
                outp.append((p.str,p.name, p.len, p.func,p.file,p.line))
            else:
                outp.append((str(p),p.name,p.len,none,none,none))
        pickle.dump(outp,outf,pickle_protocol)
        outf.close()

# -----------------------------------------------------------------------------
#                            === introspection ===
#
# the following functions and classes are used to implement the ply
# introspection features followed by the yacc() function itself.
# -----------------------------------------------------------------------------

# -----------------------------------------------------------------------------
# get_caller_module_dict()
#
# this function returns a dictionary containing all of the symbols defined within
# a caller further down the call stack.  this is used to get the environment
# associated with the yacc() call if none was provided.
# -----------------------------------------------------------------------------

def get_caller_module_dict(levels):
    try:
        raise runtimeerror
    except runtimeerror:
        e,b,t = sys.exc_info()
        f = t.tb_frame
        while levels > 0:
            f = f.f_back                   
            levels -= 1
        ldict = f.f_globals.copy()
        if f.f_globals != f.f_locals:
            ldict.update(f.f_locals)

        return ldict

# -----------------------------------------------------------------------------
# parse_grammar()
#
# this takes a raw grammar rule string and parses it into production data
# -----------------------------------------------------------------------------
def parse_grammar(doc,file,line):
    grammar = []
    # split the doc string into lines
    pstrings = doc.splitlines()
    lastp = none
    dline = line
    for ps in pstrings:
        dline += 1
        p = ps.split()
        if not p: continue
        try:
            if p[0] == '|':
                # this is a continuation of a previous rule
                if not lastp:
                    raise syntaxerror("%s:%d: misplaced '|'" % (file,dline))
                prodname = lastp
                syms = p[1:]
            else:
                prodname = p[0]
                lastp = prodname
                syms   = p[2:]
                assign = p[1]
                if assign != ':' and assign != '::=':
                    raise syntaxerror("%s:%d: syntax error. expected ':'" % (file,dline))

            grammar.append((file,dline,prodname,syms))
        except syntaxerror:
            raise
        except exception:
            raise syntaxerror("%s:%d: syntax error in rule '%s'" % (file,dline,ps.strip()))

    return grammar

# -----------------------------------------------------------------------------
# parserreflect()
#
# this class represents information extracted for building a parser including
# start symbol, error function, tokens, precedence list, action functions,
# etc.
# -----------------------------------------------------------------------------
class parserreflect(object):
    def __init__(self,pdict,log=none):
        self.pdict      = pdict
        self.start      = none
        self.error_func = none
        self.tokens     = none
        self.files      = {}
        self.grammar    = []
        self.error      = 0

        if log is none:
            self.log = plylogger(sys.stderr)
        else:
            self.log = log

    # get all of the basic information
    def get_all(self):
        self.get_start()
        self.get_error_func()
        self.get_tokens()
        self.get_precedence()
        self.get_pfunctions()
        
    # validate all of the information
    def validate_all(self):
        self.validate_start()
        self.validate_error_func()
        self.validate_tokens()
        self.validate_precedence()
        self.validate_pfunctions()
        self.validate_files()
        return self.error

    # compute a signature over the grammar
    def signature(self):
        try:
            from hashlib import md5
        except importerror:
            from md5 import md5
        try:
            sig = md5()
            if self.start:
                sig.update(self.start.encode('latin-1'))
            if self.prec:
                sig.update("".join(["".join(p) for p in self.prec]).encode('latin-1'))
            if self.tokens:
                sig.update(" ".join(self.tokens).encode('latin-1'))
            for f in self.pfuncs:
                if f[3]:
                    sig.update(f[3].encode('latin-1'))
        except (typeerror,valueerror):
            pass
        return sig.digest()

    # -----------------------------------------------------------------------------
    # validate_file()
    #
    # this method checks to see if there are duplicated p_rulename() functions
    # in the parser module file.  without this function, it is really easy for
    # users to make mistakes by cutting and pasting code fragments (and it's a real
    # bugger to try and figure out why the resulting parser doesn't work).  therefore,
    # we just do a little regular expression pattern matching of def statements
    # to try and detect duplicates.
    # -----------------------------------------------------------------------------

    def validate_files(self):
        # match def p_funcname(
        fre = re.compile(r'\s*def\s+(p_[a-za-z_0-9]*)\(')

        for filename in self.files.keys():
            base,ext = os.path.splitext(filename)
            if ext != '.py': return 1          # no idea. assume it's okay.

            try:
                f = open(filename)
                lines = f.readlines()
                f.close()
            except ioerror:
                continue

            counthash = { }
            for linen,l in enumerate(lines):
                linen += 1
                m = fre.match(l)
                if m:
                    name = m.group(1)
                    prev = counthash.get(name)
                    if not prev:
                        counthash[name] = linen
                    else:
                        self.log.warning("%s:%d: function %s redefined. previously defined on line %d", filename,linen,name,prev)

    # get the start symbol
    def get_start(self):
        self.start = self.pdict.get('start')

    # validate the start symbol
    def validate_start(self):
        if self.start is not none:
            if not isinstance(self.start,str):
                self.log.error("'start' must be a string")

    # look for error handler
    def get_error_func(self):
        self.error_func = self.pdict.get('p_error')

    # validate the error function
    def validate_error_func(self):
        if self.error_func:
            if isinstance(self.error_func,types.functiontype):
                ismethod = 0
            elif isinstance(self.error_func, types.methodtype):
                ismethod = 1
            else:
                self.log.error("'p_error' defined, but is not a function or method")
                self.error = 1
                return

            eline = func_code(self.error_func).co_firstlineno
            efile = func_code(self.error_func).co_filename
            self.files[efile] = 1

            if (func_code(self.error_func).co_argcount != 1+ismethod):
                self.log.error("%s:%d: p_error() requires 1 argument",efile,eline)
                self.error = 1

    # get the tokens map
    def get_tokens(self):
        tokens = self.pdict.get("tokens",none)
        if not tokens:
            self.log.error("no token list is defined")
            self.error = 1
            return

        if not isinstance(tokens,(list, tuple)):
            self.log.error("tokens must be a list or tuple")
            self.error = 1
            return
        
        if not tokens:
            self.log.error("tokens is empty")
            self.error = 1
            return

        self.tokens = tokens

    # validate the tokens
    def validate_tokens(self):
        # validate the tokens.
        if 'error' in self.tokens:
            self.log.error("illegal token name 'error'. is a reserved word")
            self.error = 1
            return

        terminals = {}
        for n in self.tokens:
            if n in terminals:
                self.log.warning("token '%s' multiply defined", n)
            terminals[n] = 1

    # get the precedence map (if any)
    def get_precedence(self):
        self.prec = self.pdict.get("precedence",none)

    # validate and parse the precedence map
    def validate_precedence(self):
        preclist = []
        if self.prec:
            if not isinstance(self.prec,(list,tuple)):
                self.log.error("precedence must be a list or tuple")
                self.error = 1
                return
            for level,p in enumerate(self.prec):
                if not isinstance(p,(list,tuple)):
                    self.log.error("bad precedence table")
                    self.error = 1
                    return

                if len(p) < 2:
                    self.log.error("malformed precedence entry %s. must be (assoc, term, ..., term)",p)
                    self.error = 1
                    return
                assoc = p[0]
                if not isinstance(assoc,str):
                    self.log.error("precedence associativity must be a string")
                    self.error = 1
                    return
                for term in p[1:]:
                    if not isinstance(term,str):
                        self.log.error("precedence items must be strings")
                        self.error = 1
                        return
                    preclist.append((term,assoc,level+1))
        self.preclist = preclist

    # get all p_functions from the grammar
    def get_pfunctions(self):
        p_functions = []
        for name, item in self.pdict.items():
            if name[:2] != 'p_': continue
            if name == 'p_error': continue
            if isinstance(item,(types.functiontype,types.methodtype)):
                line = func_code(item).co_firstlineno
                file = func_code(item).co_filename
                p_functions.append((line,file,name,item.__doc__))

        # sort all of the actions by line number
        p_functions.sort()
        self.pfuncs = p_functions


    # validate all of the p_functions
    def validate_pfunctions(self):
        grammar = []
        # check for non-empty symbols
        if len(self.pfuncs) == 0:
            self.log.error("no rules of the form p_rulename are defined")
            self.error = 1
            return 
        
        for line, file, name, doc in self.pfuncs:
            func = self.pdict[name]
            if isinstance(func, types.methodtype):
                reqargs = 2
            else:
                reqargs = 1
            if func_code(func).co_argcount > reqargs:
                self.log.error("%s:%d: rule '%s' has too many arguments",file,line,func.__name__)
                self.error = 1
            elif func_code(func).co_argcount < reqargs:
                self.log.error("%s:%d: rule '%s' requires an argument",file,line,func.__name__)
                self.error = 1
            elif not func.__doc__:
                self.log.warning("%s:%d: no documentation string specified in function '%s' (ignored)",file,line,func.__name__)
            else:
                try:
                    parsed_g = parse_grammar(doc,file,line)
                    for g in parsed_g:
                        grammar.append((name, g))
                except syntaxerror:
                    e = sys.exc_info()[1]
                    self.log.error(str(e))
                    self.error = 1

                # looks like a valid grammar rule
                # mark the file in which defined.
                self.files[file] = 1

        # secondary validation step that looks for p_ definitions that are not functions
        # or functions that look like they might be grammar rules.

        for n,v in self.pdict.items():
            if n[0:2] == 'p_' and isinstance(v, (types.functiontype, types.methodtype)): continue
            if n[0:2] == 't_': continue
            if n[0:2] == 'p_' and n != 'p_error':
                self.log.warning("'%s' not defined as a function", n)
            if ((isinstance(v,types.functiontype) and func_code(v).co_argcount == 1) or
                (isinstance(v,types.methodtype) and func_code(v).co_argcount == 2)):
                try:
                    doc = v.__doc__.split(" ")
                    if doc[1] == ':':
                        self.log.warning("%s:%d: possible grammar rule '%s' defined without p_ prefix",
                                         func_code(v).co_filename, func_code(v).co_firstlineno,n)
                except exception:
                    pass

        self.grammar = grammar

# -----------------------------------------------------------------------------
# yacc(module)
#
# build a parser
# -----------------------------------------------------------------------------

def yacc(method='lalr', debug=yaccdebug, module=none, tabmodule=tab_module, start=none, 
         check_recursion=1, optimize=0, write_tables=1, debugfile=debug_file,outputdir='',
         debuglog=none, errorlog = none, picklefile=none):

    global parse                 # reference to the parsing method of the last built parser

    # if pickling is enabled, table files are not created

    if picklefile:
        write_tables = 0

    if errorlog is none:
        errorlog = plylogger(sys.stderr)

    # get the module dictionary used for the parser
    if module:
        _items = [(k,getattr(module,k)) for k in dir(module)]
        pdict = dict(_items)
    else:
        pdict = get_caller_module_dict(2)

    # collect parser information from the dictionary
    pinfo = parserreflect(pdict,log=errorlog)
    pinfo.get_all()

    if pinfo.error:
        raise yaccerror("unable to build parser")

    # check signature against table files (if any)
    signature = pinfo.signature()

    # read the tables
    try:
        lr = lrtable()
        if picklefile:
            read_signature = lr.read_pickle(picklefile)
        else:
            read_signature = lr.read_table(tabmodule)
        if optimize or (read_signature == signature):
            try:
                lr.bind_callables(pinfo.pdict)
                parser = lrparser(lr,pinfo.error_func)
                parse = parser.parse
                return parser
            except exception:
                e = sys.exc_info()[1]
                errorlog.warning("there was a problem loading the table file: %s", repr(e))
    except versionerror:
        e = sys.exc_info()
        errorlog.warning(str(e))
    except exception:
        pass

    if debuglog is none:
        if debug:
            debuglog = plylogger(open(debugfile,"w"))
        else:
            debuglog = nulllogger()

    debuglog.info("created by ply version %s (http://www.dabeaz.com/ply)", __version__)


    errors = 0

    # validate the parser information
    if pinfo.validate_all():
        raise yaccerror("unable to build parser")
    
    if not pinfo.error_func:
        errorlog.warning("no p_error() function is defined")

    # create a grammar object
    grammar = grammar(pinfo.tokens)

    # set precedence level for terminals
    for term, assoc, level in pinfo.preclist:
        try:
            grammar.set_precedence(term,assoc,level)
        except grammarerror:
            e = sys.exc_info()[1]
            errorlog.warning("%s",str(e))

    # add productions to the grammar
    for funcname, gram in pinfo.grammar:
        file, line, prodname, syms = gram
        try:
            grammar.add_production(prodname,syms,funcname,file,line)
        except grammarerror:
            e = sys.exc_info()[1]
            errorlog.error("%s",str(e))
            errors = 1

    # set the grammar start symbols
    try:
        if start is none:
            grammar.set_start(pinfo.start)
        else:
            grammar.set_start(start)
    except grammarerror:
        e = sys.exc_info()[1]
        errorlog.error(str(e))
        errors = 1

    if errors:
        raise yaccerror("unable to build parser")

    # verify the grammar structure
    undefined_symbols = grammar.undefined_symbols()
    for sym, prod in undefined_symbols:
        errorlog.error("%s:%d: symbol '%s' used, but not defined as a token or a rule",prod.file,prod.line,sym)
        errors = 1

    unused_terminals = grammar.unused_terminals()
    if unused_terminals:
        debuglog.info("")
        debuglog.info("unused terminals:")
        debuglog.info("")
        for term in unused_terminals:
            errorlog.warning("token '%s' defined, but not used", term)
            debuglog.info("    %s", term)

    # print out all productions to the debug log
    if debug:
        debuglog.info("")
        debuglog.info("grammar")
        debuglog.info("")
        for n,p in enumerate(grammar.productions):
            debuglog.info("rule %-5d %s", n, p)

    # find unused non-terminals
    unused_rules = grammar.unused_rules()
    for prod in unused_rules:
        errorlog.warning("%s:%d: rule '%s' defined, but not used", prod.file, prod.line, prod.name)

    if len(unused_terminals) == 1:
        errorlog.warning("there is 1 unused token")
    if len(unused_terminals) > 1:
        errorlog.warning("there are %d unused tokens", len(unused_terminals))

    if len(unused_rules) == 1:
        errorlog.warning("there is 1 unused rule")
    if len(unused_rules) > 1:
        errorlog.warning("there are %d unused rules", len(unused_rules))

    if debug:
        debuglog.info("")
        debuglog.info("terminals, with rules where they appear")
        debuglog.info("")
        terms = list(grammar.terminals)
        terms.sort()
        for term in terms:
            debuglog.info("%-20s : %s", term, " ".join([str(s) for s in grammar.terminals[term]]))
        
        debuglog.info("")
        debuglog.info("nonterminals, with rules where they appear")
        debuglog.info("")
        nonterms = list(grammar.nonterminals)
        nonterms.sort()
        for nonterm in nonterms:
            debuglog.info("%-20s : %s", nonterm, " ".join([str(s) for s in grammar.nonterminals[nonterm]]))
        debuglog.info("")

    if check_recursion:
        unreachable = grammar.find_unreachable()
        for u in unreachable:
            errorlog.warning("symbol '%s' is unreachable",u)

        infinite = grammar.infinite_cycles()
        for inf in infinite:
            errorlog.error("infinite recursion detected for symbol '%s'", inf)
            errors = 1
        
    unused_prec = grammar.unused_precedence()
    for term, assoc in unused_prec:
        errorlog.error("precedence rule '%s' defined for unknown symbol '%s'", assoc, term)
        errors = 1

    if errors:
        raise yaccerror("unable to build parser")
    
    # run the lrgeneratedtable on the grammar
    if debug:
        errorlog.debug("generating %s tables", method)
            
    lr = lrgeneratedtable(grammar,method,debuglog)

    if debug:
        num_sr = len(lr.sr_conflicts)

        # report shift/reduce and reduce/reduce conflicts
        if num_sr == 1:
            errorlog.warning("1 shift/reduce conflict")
        elif num_sr > 1:
            errorlog.warning("%d shift/reduce conflicts", num_sr)

        num_rr = len(lr.rr_conflicts)
        if num_rr == 1:
            errorlog.warning("1 reduce/reduce conflict")
        elif num_rr > 1:
            errorlog.warning("%d reduce/reduce conflicts", num_rr)

    # write out conflicts to the output file
    if debug and (lr.sr_conflicts or lr.rr_conflicts):
        debuglog.warning("")
        debuglog.warning("conflicts:")
        debuglog.warning("")

        for state, tok, resolution in lr.sr_conflicts:
            debuglog.warning("shift/reduce conflict for %s in state %d resolved as %s",  tok, state, resolution)
        
        already_reported = {}
        for state, rule, rejected in lr.rr_conflicts:
            if (state,id(rule),id(rejected)) in already_reported:
                continue
            debuglog.warning("reduce/reduce conflict in state %d resolved using rule (%s)", state, rule)
            debuglog.warning("rejected rule (%s) in state %d", rejected,state)
            errorlog.warning("reduce/reduce conflict in state %d resolved using rule (%s)", state, rule)
            errorlog.warning("rejected rule (%s) in state %d", rejected, state)
            already_reported[state,id(rule),id(rejected)] = 1
        
        warned_never = []
        for state, rule, rejected in lr.rr_conflicts:
            if not rejected.reduced and (rejected not in warned_never):
                debuglog.warning("rule (%s) is never reduced", rejected)
                errorlog.warning("rule (%s) is never reduced", rejected)
                warned_never.append(rejected)

    # write the table file if requested
    if write_tables:
        lr.write_table(tabmodule,outputdir,signature)

    # write a pickled version of the tables
    if picklefile:
        lr.pickle_table(picklefile,signature)

    # build the parser
    lr.bind_callables(pinfo.pdict)
    parser = lrparser(lr,pinfo.error_func)

    parse = parser.parse
    return parser
