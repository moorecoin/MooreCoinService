//------------------------------------------------------------------------------
/*
    this file is part of rippled: https://github.com/ripple/rippled
    copyright (c) 2012, 2013 ripple labs inc.

    permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    the  software is provided "as is" and the author disclaims all warranties
    with  regard  to  this  software  including  all  implied  warranties  of
    merchantability  and  fitness. in no event shall the author be liable for
    any  special ,  direct, indirect, or consequential damages or any damages
    whatsoever  resulting  from  loss  of use, data or profits, whether in an
    action  of  contract, negligence or other tortious action, arising out of
    or in connection with the use or performance of this software.
*/
//==============================================================================

#include <beastconfig.h>
#include <ripple/json/json_reader.h>
#include <string>

namespace json
{

// implementation of class features
// ////////////////////////////////

features::features ()
    : allowcomments_ ( true )
    , strictroot_ ( false )
{
}


features
features::all ()
{
    return features ();
}


features
features::strictmode ()
{
    features features;
    features.allowcomments_ = false;
    features.strictroot_ = true;
    return features;
}

// implementation of class reader
// ////////////////////////////////


static inline bool
in ( reader::char c, reader::char c1, reader::char c2, reader::char c3, reader::char c4 )
{
    return c == c1  ||  c == c2  ||  c == c3  ||  c == c4;
}

static inline bool
in ( reader::char c, reader::char c1, reader::char c2, reader::char c3, reader::char c4, reader::char c5 )
{
    return c == c1  ||  c == c2  ||  c == c3  ||  c == c4  ||  c == c5;
}


static bool
containsnewline ( reader::location begin,
                  reader::location end )
{
    for ( ; begin < end; ++begin )
        if ( *begin == '\n'  ||  *begin == '\r' )
            return true;

    return false;
}

static std::string codepointtoutf8 (unsigned int cp)
{
    std::string result;

    // based on description from http://en.wikipedia.org/wiki/utf-8

    if (cp <= 0x7f)
    {
        result.resize (1);
        result[0] = static_cast<char> (cp);
    }
    else if (cp <= 0x7ff)
    {
        result.resize (2);
        result[1] = static_cast<char> (0x80 | (0x3f & cp));
        result[0] = static_cast<char> (0xc0 | (0x1f & (cp >> 6)));
    }
    else if (cp <= 0xffff)
    {
        result.resize (3);
        result[2] = static_cast<char> (0x80 | (0x3f & cp));
        result[1] = 0x80 | static_cast<char> ((0x3f & (cp >> 6)));
        result[0] = 0xe0 | static_cast<char> ((0xf & (cp >> 12)));
    }
    else if (cp <= 0x10ffff)
    {
        result.resize (4);
        result[3] = static_cast<char> (0x80 | (0x3f & cp));
        result[2] = static_cast<char> (0x80 | (0x3f & (cp >> 6)));
        result[1] = static_cast<char> (0x80 | (0x3f & (cp >> 12)));
        result[0] = static_cast<char> (0xf0 | (0x7 & (cp >> 18)));
    }

    return result;
}


// class reader
// //////////////////////////////////////////////////////////////////

reader::reader ()
    : features_ ( features::all () )
{
}


reader::reader ( const features& features )
    : features_ ( features )
{
}


bool
reader::parse ( std::string const& document,
                value& root,
                bool collectcomments )
{
    document_ = document;
    const char* begin = document_.c_str ();
    const char* end = begin + document_.length ();
    return parse ( begin, end, root, collectcomments );
}


bool
reader::parse ( std::istream& sin,
                value& root,
                bool collectcomments )
{
    //std::istream_iterator<char> begin(sin);
    //std::istream_iterator<char> end;
    // those would allow streamed input from a file, if parse() were a
    // template function.

    // since std::string is reference-counted, this at least does not
    // create an extra copy.
    std::string doc;
    std::getline (sin, doc, (char)eof);
    return parse ( doc, root, collectcomments );
}

bool
reader::parse ( const char* begindoc, const char* enddoc,
                value& root,
                bool collectcomments )
{
    if ( !features_.allowcomments_ )
    {
        collectcomments = false;
    }

    begin_ = begindoc;
    end_ = enddoc;
    collectcomments_ = collectcomments;
    current_ = begin_;
    lastvalueend_ = 0;
    lastvalue_ = 0;
    commentsbefore_ = "";
    errors_.clear ();

    while ( !nodes_.empty () )
        nodes_.pop ();

    nodes_.push ( &root );

    bool successful = readvalue ();
    token token;
    skipcommenttokens ( token );

    if ( collectcomments_  &&  !commentsbefore_.empty () )
        root.setcomment ( commentsbefore_, commentafter );

    if ( features_.strictroot_ )
    {
        if ( !root.isarray ()  &&  !root.isobject () )
        {
            // set error location to start of doc, ideally should be first token found in doc
            token.type_ = tokenerror;
            token.start_ = begindoc;
            token.end_ = enddoc;
            adderror ( "a valid json document must be either an array or an object value.",
                       token );
            return false;
        }
    }

    return successful;
}


bool
reader::readvalue ()
{
    token token;
    skipcommenttokens ( token );
    bool successful = true;

    if ( collectcomments_  &&  !commentsbefore_.empty () )
    {
        currentvalue ().setcomment ( commentsbefore_, commentbefore );
        commentsbefore_ = "";
    }


    switch ( token.type_ )
    {
    case tokenobjectbegin:
        successful = readobject ( token );
        break;

    case tokenarraybegin:
        successful = readarray ( token );
        break;

    case tokennumber:
        successful = decodenumber ( token );
        break;

    case tokenstring:
        successful = decodestring ( token );
        break;

    case tokentrue:
        currentvalue () = true;
        break;

    case tokenfalse:
        currentvalue () = false;
        break;

    case tokennull:
        currentvalue () = value ();
        break;

    default:
        return adderror ( "syntax error: value, object or array expected.", token );
    }

    if ( collectcomments_ )
    {
        lastvalueend_ = current_;
        lastvalue_ = &currentvalue ();
    }

    return successful;
}


void
reader::skipcommenttokens ( token& token )
{
    if ( features_.allowcomments_ )
    {
        do
        {
            readtoken ( token );
        }
        while ( token.type_ == tokencomment );
    }
    else
    {
        readtoken ( token );
    }
}


bool
reader::expecttoken ( tokentype type, token& token, const char* message )
{
    readtoken ( token );

    if ( token.type_ != type )
        return adderror ( message, token );

    return true;
}


bool
reader::readtoken ( token& token )
{
    skipspaces ();
    token.start_ = current_;
    char c = getnextchar ();
    bool ok = true;

    switch ( c )
    {
    case '{':
        token.type_ = tokenobjectbegin;
        break;

    case '}':
        token.type_ = tokenobjectend;
        break;

    case '[':
        token.type_ = tokenarraybegin;
        break;

    case ']':
        token.type_ = tokenarrayend;
        break;

    case '"':
        token.type_ = tokenstring;
        ok = readstring ();
        break;

    case '/':
        token.type_ = tokencomment;
        ok = readcomment ();
        break;

    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    case '-':
        token.type_ = tokennumber;
        readnumber ();
        break;

    case 't':
        token.type_ = tokentrue;
        ok = match ( "rue", 3 );
        break;

    case 'f':
        token.type_ = tokenfalse;
        ok = match ( "alse", 4 );
        break;

    case 'n':
        token.type_ = tokennull;
        ok = match ( "ull", 3 );
        break;

    case ',':
        token.type_ = tokenarrayseparator;
        break;

    case ':':
        token.type_ = tokenmemberseparator;
        break;

    case 0:
        token.type_ = tokenendofstream;
        break;

    default:
        ok = false;
        break;
    }

    if ( !ok )
        token.type_ = tokenerror;

    token.end_ = current_;
    return true;
}


void
reader::skipspaces ()
{
    while ( current_ != end_ )
    {
        char c = *current_;

        if ( c == ' '  ||  c == '\t'  ||  c == '\r'  ||  c == '\n' )
            ++current_;
        else
            break;
    }
}


bool
reader::match ( location pattern,
                int patternlength )
{
    if ( end_ - current_ < patternlength )
        return false;

    int index = patternlength;

    while ( index-- )
        if ( current_[index] != pattern[index] )
            return false;

    current_ += patternlength;
    return true;
}


bool
reader::readcomment ()
{
    location commentbegin = current_ - 1;
    char c = getnextchar ();
    bool successful = false;

    if ( c == '*' )
        successful = readcstylecomment ();
    else if ( c == '/' )
        successful = readcppstylecomment ();

    if ( !successful )
        return false;

    if ( collectcomments_ )
    {
        commentplacement placement = commentbefore;

        if ( lastvalueend_  &&  !containsnewline ( lastvalueend_, commentbegin ) )
        {
            if ( c != '*'  ||  !containsnewline ( commentbegin, current_ ) )
                placement = commentafteronsameline;
        }

        addcomment ( commentbegin, current_, placement );
    }

    return true;
}


void
reader::addcomment ( location begin,
                     location end,
                     commentplacement placement )
{
    assert ( collectcomments_ );

    if ( placement == commentafteronsameline )
    {
        assert ( lastvalue_ != 0 );
        lastvalue_->setcomment ( std::string ( begin, end ), placement );
    }
    else
    {
        if ( !commentsbefore_.empty () )
            commentsbefore_ += "\n";

        commentsbefore_ += std::string ( begin, end );
    }
}


bool
reader::readcstylecomment ()
{
    while ( current_ != end_ )
    {
        char c = getnextchar ();

        if ( c == '*'  &&  *current_ == '/' )
            break;
    }

    return getnextchar () == '/';
}


bool
reader::readcppstylecomment ()
{
    while ( current_ != end_ )
    {
        char c = getnextchar ();

        if (  c == '\r'  ||  c == '\n' )
            break;
    }

    return true;
}


void
reader::readnumber ()
{
    while ( current_ != end_ )
    {
        if ( ! (*current_ >= '0'  &&  *current_ <= '9')  &&
                !in ( *current_, '.', 'e', 'e', '+', '-' ) )
            break;

        ++current_;
    }
}

bool
reader::readstring ()
{
    char c = 0;

    while ( current_ != end_ )
    {
        c = getnextchar ();

        if ( c == '\\' )
            getnextchar ();
        else if ( c == '"' )
            break;
    }

    return c == '"';
}


bool
reader::readobject ( token& tokenstart )
{
    token tokenname;
    std::string name;
    currentvalue () = value ( objectvalue );

    while ( readtoken ( tokenname ) )
    {
        bool initialtokenok = true;

        while ( tokenname.type_ == tokencomment  &&  initialtokenok )
            initialtokenok = readtoken ( tokenname );

        if  ( !initialtokenok )
            break;

        if ( tokenname.type_ == tokenobjectend  &&  name.empty () ) // empty object
            return true;

        if ( tokenname.type_ != tokenstring )
            break;

        name = "";

        if ( !decodestring ( tokenname, name ) )
            return recoverfromerror ( tokenobjectend );

        token colon;

        if ( !readtoken ( colon ) ||  colon.type_ != tokenmemberseparator )
        {
            return adderrorandrecover ( "missing ':' after object member name",
                                        colon,
                                        tokenobjectend );
        }

        // reject duplicate names
        if (currentvalue ().ismember (name))
            return adderror ( "key '" + name + "' appears twice.", tokenname );

        value& value = currentvalue ()[ name ];
        nodes_.push ( &value );
        bool ok = readvalue ();
        nodes_.pop ();

        if ( !ok ) // error already set
            return recoverfromerror ( tokenobjectend );

        token comma;

        if ( !readtoken ( comma )
                ||  ( comma.type_ != tokenobjectend  &&
                      comma.type_ != tokenarrayseparator &&
                      comma.type_ != tokencomment ) )
        {
            return adderrorandrecover ( "missing ',' or '}' in object declaration",
                                        comma,
                                        tokenobjectend );
        }

        bool finalizetokenok = true;

        while ( comma.type_ == tokencomment &&
                finalizetokenok )
            finalizetokenok = readtoken ( comma );

        if ( comma.type_ == tokenobjectend )
            return true;
    }

    return adderrorandrecover ( "missing '}' or object member name",
                                tokenname,
                                tokenobjectend );
}


bool
reader::readarray ( token& tokenstart )
{
    currentvalue () = value ( arrayvalue );
    skipspaces ();

    if ( *current_ == ']' ) // empty array
    {
        token endarray;
        readtoken ( endarray );
        return true;
    }

    int index = 0;

    while ( true )
    {
        value& value = currentvalue ()[ index++ ];
        nodes_.push ( &value );
        bool ok = readvalue ();
        nodes_.pop ();

        if ( !ok ) // error already set
            return recoverfromerror ( tokenarrayend );

        token token;
        // accept comment after last item in the array.
        ok = readtoken ( token );

        while ( token.type_ == tokencomment  &&  ok )
        {
            ok = readtoken ( token );
        }

        bool badtokentype = ( token.type_ != tokenarrayseparator &&
                              token.type_ != tokenarrayend );

        if ( !ok  ||  badtokentype )
        {
            return adderrorandrecover ( "missing ',' or ']' in array declaration",
                                        token,
                                        tokenarrayend );
        }

        if ( token.type_ == tokenarrayend )
            break;
    }

    return true;
}


bool
reader::decodenumber ( token& token )
{
    bool isdouble = false;

    for ( location inspect = token.start_; inspect != token.end_; ++inspect )
    {
        isdouble = isdouble
                   ||  in ( *inspect, '.', 'e', 'e', '+' )
                   ||  ( *inspect == '-'  &&  inspect != token.start_ );
    }

    if ( isdouble )
        return decodedouble ( token );

    location current = token.start_;
    bool isnegative = *current == '-';

    if ( isnegative )
        ++current;

    std::int64_t value = 0;

    static_assert(sizeof(value) > sizeof(value::maxuint),
        "the json integer overflow logic will need to be reworked.");

    while (current < token.end_ && (value <= value::maxuint))
    {
        char c = *current++;

        if ( c < '0'  ||  c > '9' )
        {
            return adderror ( "'" + std::string ( token.start_, token.end_ ) +
                "' is not a number.", token );
        }

        value = (value * 10) + (c - '0');
    }

    // more tokens left -> input is larger than largest possible return value
    if (current != token.end_)
    {
        return adderror ( "'" + std::string ( token.start_, token.end_ ) +
            "' exceeds the allowable range.", token );
    }

    if ( isnegative )
    {
        value = -value;

        if (value < value::minint || value > value::maxint)
        {
            return adderror ( "'" + std::string ( token.start_, token.end_ ) +
                "' exceeds the allowable range.", token );
        }

        currentvalue () = static_cast<value::int>( value );
    }
    else
    {
        if (value > value::maxuint)
        {
            return adderror ( "'" + std::string ( token.start_, token.end_ ) +
                "' exceeds the allowable range.", token );
        }

        // if it's representable as a signed integer, construct it as one.
        if ( value <= value::maxint )
            currentvalue () = static_cast<value::int>( value );
        else
            currentvalue () = static_cast<value::uint>( value );
    }

    return true;
}

bool
reader::decodedouble( token &token )
{
    double value = 0;
    const int buffersize = 32;
    int count;
    int length = int(token.end_ - token.start_);
    // sanity check to avoid buffer overflow exploits.
    if (length < 0) {
        return adderror( "unable to parse token length", token );
    }
    // avoid using a string constant for the format control string given to
    // sscanf, as this can cause hard to debug crashes on os x. see here for more
    // info:
    //
    // http://developer.apple.com/library/mac/#documentation/developertools/gcc-4.0.1/gcc/incompatibilities.html
    char format[] = "%lf";
    if ( length <= buffersize )
    {
        char buffer[buffersize+1];
        memcpy( buffer, token.start_, length );
        buffer[length] = 0;
        count = sscanf( buffer, format, &value );
    }
    else
    {
        std::string buffer( token.start_, token.end_ );
        count = sscanf( buffer.c_str(), format, &value );
    }
    if ( count != 1 )
        return adderror( "'" + std::string( token.start_, token.end_ ) + "' is not a number.", token );
    currentvalue() = value;
    return true;
}



bool
reader::decodestring ( token& token )
{
    std::string decoded;

    if ( !decodestring ( token, decoded ) )
        return false;

    currentvalue () = decoded;
    return true;
}


bool
reader::decodestring ( token& token, std::string& decoded )
{
    decoded.reserve ( token.end_ - token.start_ - 2 );
    location current = token.start_ + 1; // skip '"'
    location end = token.end_ - 1;      // do not include '"'

    while ( current != end )
    {
        char c = *current++;

        if ( c == '"' )
            break;
        else if ( c == '\\' )
        {
            if ( current == end )
                return adderror ( "empty escape sequence in string", token, current );

            char escape = *current++;

            switch ( escape )
            {
            case '"':
                decoded += '"';
                break;

            case '/':
                decoded += '/';
                break;

            case '\\':
                decoded += '\\';
                break;

            case 'b':
                decoded += '\b';
                break;

            case 'f':
                decoded += '\f';
                break;

            case 'n':
                decoded += '\n';
                break;

            case 'r':
                decoded += '\r';
                break;

            case 't':
                decoded += '\t';
                break;

            case 'u':
            {
                unsigned int unicode;

                if ( !decodeunicodecodepoint ( token, current, end, unicode ) )
                    return false;

                decoded += codepointtoutf8 (unicode);
            }
            break;

            default:
                return adderror ( "bad escape sequence in string", token, current );
            }
        }
        else
        {
            decoded += c;
        }
    }

    return true;
}

bool
reader::decodeunicodecodepoint ( token& token,
                                 location& current,
                                 location end,
                                 unsigned int& unicode )
{

    if ( !decodeunicodeescapesequence ( token, current, end, unicode ) )
        return false;

    if (unicode >= 0xd800 && unicode <= 0xdbff)
    {
        // surrogate pairs
        if (end - current < 6)
            return adderror ( "additional six characters expected to parse unicode surrogate pair.", token, current );

        unsigned int surrogatepair;

        if (* (current++) == '\\' && * (current++) == 'u')
        {
            if (decodeunicodeescapesequence ( token, current, end, surrogatepair ))
            {
                unicode = 0x10000 + ((unicode & 0x3ff) << 10) + (surrogatepair & 0x3ff);
            }
            else
                return false;
        }
        else
            return adderror ( "expecting another \\u token to begin the second half of a unicode surrogate pair", token, current );
    }

    return true;
}

bool
reader::decodeunicodeescapesequence ( token& token,
                                      location& current,
                                      location end,
                                      unsigned int& unicode )
{
    if ( end - current < 4 )
        return adderror ( "bad unicode escape sequence in string: four digits expected.", token, current );

    unicode = 0;

    for ( int index = 0; index < 4; ++index )
    {
        char c = *current++;
        unicode *= 16;

        if ( c >= '0'  &&  c <= '9' )
            unicode += c - '0';
        else if ( c >= 'a'  &&  c <= 'f' )
            unicode += c - 'a' + 10;
        else if ( c >= 'a'  &&  c <= 'f' )
            unicode += c - 'a' + 10;
        else
            return adderror ( "bad unicode escape sequence in string: hexadecimal digit expected.", token, current );
    }

    return true;
}


bool
reader::adderror ( std::string const& message,
                   token& token,
                   location extra )
{
    errorinfo info;
    info.token_ = token;
    info.message_ = message;
    info.extra_ = extra;
    errors_.push_back ( info );
    return false;
}


bool
reader::recoverfromerror ( tokentype skipuntiltoken )
{
    int errorcount = int (errors_.size ());
    token skip;

    while ( true )
    {
        if ( !readtoken (skip) )
            errors_.resize ( errorcount ); // discard errors caused by recovery

        if ( skip.type_ == skipuntiltoken  ||  skip.type_ == tokenendofstream )
            break;
    }

    errors_.resize ( errorcount );
    return false;
}


bool
reader::adderrorandrecover ( std::string const& message,
                             token& token,
                             tokentype skipuntiltoken )
{
    adderror ( message, token );
    return recoverfromerror ( skipuntiltoken );
}


value&
reader::currentvalue ()
{
    return * (nodes_.top ());
}


reader::char
reader::getnextchar ()
{
    if ( current_ == end_ )
        return 0;

    return *current_++;
}


void
reader::getlocationlineandcolumn ( location location,
                                   int& line,
                                   int& column ) const
{
    location current = begin_;
    location lastlinestart = current;
    line = 0;

    while ( current < location  &&  current != end_ )
    {
        char c = *current++;

        if ( c == '\r' )
        {
            if ( *current == '\n' )
                ++current;

            lastlinestart = current;
            ++line;
        }
        else if ( c == '\n' )
        {
            lastlinestart = current;
            ++line;
        }
    }

    // column & line start at 1
    column = int (location - lastlinestart) + 1;
    ++line;
}


std::string
reader::getlocationlineandcolumn ( location location ) const
{
    int line, column;
    getlocationlineandcolumn ( location, line, column );
    char buffer[18 + 16 + 16 + 1];
    sprintf ( buffer, "line %d, column %d", line, column );
    return buffer;
}


std::string
reader::getformatederrormessages () const
{
    std::string formattedmessage;

    for ( errors::const_iterator iterror = errors_.begin ();
            iterror != errors_.end ();
            ++iterror )
    {
        const errorinfo& error = *iterror;
        formattedmessage += "* " + getlocationlineandcolumn ( error.token_.start_ ) + "\n";
        formattedmessage += "  " + error.message_ + "\n";

        if ( error.extra_ )
            formattedmessage += "see " + getlocationlineandcolumn ( error.extra_ ) + " for detail.\n";
    }

    return formattedmessage;
}


std::istream& operator>> ( std::istream& sin, value& root )
{
    json::reader reader;
    bool ok = reader.parse (sin, root, true);

    //json_assert( ok );
    if (!ok) throw std::runtime_error (reader.getformatederrormessages ());

    return sin;
}


} // namespace json
