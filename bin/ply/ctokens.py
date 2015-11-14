# ----------------------------------------------------------------------
# ctokens.py
#
# token specifications for symbols in ansi c and c++.  this file is
# meant to be used as a library in other tokenizers.
# ----------------------------------------------------------------------

# reserved words

tokens = [
    # literals (identifier, integer constant, float constant, string constant, char const)
    'id', 'typeid', 'iconst', 'fconst', 'sconst', 'cconst',

    # operators (+,-,*,/,%,|,&,~,^,<<,>>, ||, &&, !, <, <=, >, >=, ==, !=)
    'plus', 'minus', 'times', 'divide', 'mod',
    'or', 'and', 'not', 'xor', 'lshift', 'rshift',
    'lor', 'land', 'lnot',
    'lt', 'le', 'gt', 'ge', 'eq', 'ne',
    
    # assignment (=, *=, /=, %=, +=, -=, <<=, >>=, &=, ^=, |=)
    'equals', 'timesequal', 'divequal', 'modequal', 'plusequal', 'minusequal',
    'lshiftequal','rshiftequal', 'andequal', 'xorequal', 'orequal',

    # increment/decrement (++,--)
    'plusplus', 'minusminus',

    # structure dereference (->)
    'arrow',

    # ternary operator (?)
    'ternary',
    
    # delimeters ( ) [ ] { } , . ; :
    'lparen', 'rparen',
    'lbracket', 'rbracket',
    'lbrace', 'rbrace',
    'comma', 'period', 'semi', 'colon',

    # ellipsis (...)
    'ellipsis',
]
    
# operators
t_plus             = r'\+'
t_minus            = r'-'
t_times            = r'\*'
t_divide           = r'/'
t_modulo           = r'%'
t_or               = r'\|'
t_and              = r'&'
t_not              = r'~'
t_xor              = r'\^'
t_lshift           = r'<<'
t_rshift           = r'>>'
t_lor              = r'\|\|'
t_land             = r'&&'
t_lnot             = r'!'
t_lt               = r'<'
t_gt               = r'>'
t_le               = r'<='
t_ge               = r'>='
t_eq               = r'=='
t_ne               = r'!='

# assignment operators

t_equals           = r'='
t_timesequal       = r'\*='
t_divequal         = r'/='
t_modequal         = r'%='
t_plusequal        = r'\+='
t_minusequal       = r'-='
t_lshiftequal      = r'<<='
t_rshiftequal      = r'>>='
t_andequal         = r'&='
t_orequal          = r'\|='
t_xorequal         = r'^='

# increment/decrement
t_increment        = r'\+\+'
t_decrement        = r'--'

# ->
t_arrow            = r'->'

# ?
t_ternary          = r'\?'

# delimeters
t_lparen           = r'\('
t_rparen           = r'\)'
t_lbracket         = r'\['
t_rbracket         = r'\]'
t_lbrace           = r'\{'
t_rbrace           = r'\}'
t_comma            = r','
t_period           = r'\.'
t_semi             = r';'
t_colon            = r':'
t_ellipsis         = r'\.\.\.'

# identifiers
t_id = r'[a-za-z_][a-za-z0-9_]*'

# integer literal
t_integer = r'\d+([uu]|[ll]|[uu][ll]|[ll][uu])?'

# floating literal
t_float = r'((\d+)(\.\d+)(e(\+|-)?(\d+))? | (\d+)e(\+|-)?(\d+))([ll]|[ff])?'

# string literal
t_string = r'\"([^\\\n]|(\\.))*?\"'

# character constant 'c' or l'c'
t_character = r'(l)?\'([^\\\n]|(\\.))*?\''

# comment (c-style)
def t_comment(t):
    r'/\*(.|\n)*?\*/'
    t.lexer.lineno += t.value.count('\n')
    return t

# comment (c++-style)
def t_cppcomment(t):
    r'//.*\n'
    t.lexer.lineno += 1
    return t


    



