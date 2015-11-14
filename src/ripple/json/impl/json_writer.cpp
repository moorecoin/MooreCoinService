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
#include <ripple/json/json_writer.h>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>

namespace json
{

static bool iscontrolcharacter (char ch)
{
    return ch > 0 && ch <= 0x1f;
}

static bool containscontrolcharacter ( const char* str )
{
    while ( *str )
    {
        if ( iscontrolcharacter ( * (str++) ) )
            return true;
    }

    return false;
}
static void uinttostring ( unsigned int value,
                           char*& current )
{
    *--current = 0;

    do
    {
        *--current = (value % 10) + '0';
        value /= 10;
    }
    while ( value != 0 );
}

std::string valuetostring ( int value )
{
    char buffer[32];
    char* current = buffer + sizeof (buffer);
    bool isnegative = value < 0;

    if ( isnegative )
        value = -value;

    uinttostring ( uint (value), current );

    if ( isnegative )
        *--current = '-';

    assert ( current >= buffer );
    return current;
}


std::string valuetostring ( uint value )
{
    char buffer[32];
    char* current = buffer + sizeof (buffer);
    uinttostring ( value, current );
    assert ( current >= buffer );
    return current;
}

std::string valuetostring( double value )
{
    // allocate a buffer that is more than large enough to store the 16 digits of
    // precision requested below.
    char buffer[32];
    // print into the buffer. we need not request the alternative representation
    // that always has a decimal point because json doesn't distingish the
    // concepts of reals and integers.
#if defined(_msc_ver) && defined(__stdc_secure_lib__) // use secure version with visual studio 2005 to avoid warning.
    sprintf_s(buffer, sizeof(buffer), "%.16g", value);
#else
    snprintf(buffer, sizeof(buffer), "%.16g", value);
#endif
    return buffer;
}

std::string valuetostring ( bool value )
{
    return value ? "true" : "false";
}

std::string valuetoquotedstring ( const char* value )
{
    // not sure how to handle unicode...
    if (strpbrk (value, "\"\\\b\f\n\r\t") == nullptr && !containscontrolcharacter ( value ))
        return std::string ("\"") + value + "\"";

    // we have to walk value and escape any special characters.
    // appending to std::string is not efficient, but this should be rare.
    // (note: forward slashes are *not* rare, but i am not escaping them.)
    unsigned maxsize = strlen (value) * 2 + 3; // allescaped+quotes+null
    std::string result;
    result.reserve (maxsize); // to avoid lots of mallocs
    result += "\"";

    for (const char* c = value; *c != 0; ++c)
    {
        switch (*c)
        {
        case '\"':
            result += "\\\"";
            break;

        case '\\':
            result += "\\\\";
            break;

        case '\b':
            result += "\\b";
            break;

        case '\f':
            result += "\\f";
            break;

        case '\n':
            result += "\\n";
            break;

        case '\r':
            result += "\\r";
            break;

        case '\t':
            result += "\\t";
            break;

            //case '/':
            // even though \/ is considered a legal escape in json, a bare
            // slash is also legal, so i see no reason to escape it.
            // (i hope i am not misunderstanding something.
            // blep notes: actually escaping \/ may be useful in javascript to avoid </
            // sequence.
            // should add a flag to allow this compatibility mode and prevent this
            // sequence from occurring.
        default:
            if ( iscontrolcharacter ( *c ) )
            {
                std::ostringstream oss;
                oss << "\\u" << std::hex << std::uppercase << std::setfill ('0') << std::setw (4) << static_cast<int> (*c);
                result += oss.str ();
            }
            else
            {
                result += *c;
            }

            break;
        }
    }

    result += "\"";
    return result;
}

// class writer
// //////////////////////////////////////////////////////////////////
writer::~writer ()
{
}


// class fastwriter
// //////////////////////////////////////////////////////////////////

fastwriter::fastwriter ()
    : yamlcompatiblityenabled_ ( false )
{
}


void
fastwriter::enableyamlcompatibility ()
{
    yamlcompatiblityenabled_ = true;
}


std::string
fastwriter::write ( const value& root )
{
    document_ = "";
    writevalue ( root );
    document_ += "\n";
    return std::move (document_);
}


void
fastwriter::writevalue ( const value& value )
{
    switch ( value.type () )
    {
    case nullvalue:
        document_ += "null";
        break;

    case intvalue:
        document_ += valuetostring ( value.asint () );
        break;

    case uintvalue:
        document_ += valuetostring ( value.asuint () );
        break;

    case realvalue:
        document_ += valuetostring ( value.asdouble () );
        break;

    case stringvalue:
        document_ += valuetoquotedstring ( value.ascstring () );
        break;

    case booleanvalue:
        document_ += valuetostring ( value.asbool () );
        break;

    case arrayvalue:
    {
        document_ += "[";
        int size = value.size ();

        for ( int index = 0; index < size; ++index )
        {
            if ( index > 0 )
                document_ += ",";

            writevalue ( value[index] );
        }

        document_ += "]";
    }
    break;

    case objectvalue:
    {
        value::members members ( value.getmembernames () );
        document_ += "{";

        for ( value::members::iterator it = members.begin ();
                it != members.end ();
                ++it )
        {
            std::string const& name = *it;

            if ( it != members.begin () )
                document_ += ",";

            document_ += valuetoquotedstring ( name.c_str () );
            document_ += yamlcompatiblityenabled_ ? ": "
                         : ":";
            writevalue ( value[name] );
        }

        document_ += "}";
    }
    break;
    }
}


// class styledwriter
// //////////////////////////////////////////////////////////////////

styledwriter::styledwriter ()
    : rightmargin_ ( 74 )
    , indentsize_ ( 3 )
{
}


std::string
styledwriter::write ( const value& root )
{
    document_ = "";
    addchildvalues_ = false;
    indentstring_ = "";
    writecommentbeforevalue ( root );
    writevalue ( root );
    writecommentaftervalueonsameline ( root );
    document_ += "\n";
    return document_;
}


void
styledwriter::writevalue ( const value& value )
{
    switch ( value.type () )
    {
    case nullvalue:
        pushvalue ( "null" );
        break;

    case intvalue:
        pushvalue ( valuetostring ( value.asint () ) );
        break;

    case uintvalue:
        pushvalue ( valuetostring ( value.asuint () ) );
        break;

    case realvalue:
        pushvalue ( valuetostring ( value.asdouble () ) );
        break;

    case stringvalue:
        pushvalue ( valuetoquotedstring ( value.ascstring () ) );
        break;

    case booleanvalue:
        pushvalue ( valuetostring ( value.asbool () ) );
        break;

    case arrayvalue:
        writearrayvalue ( value);
        break;

    case objectvalue:
    {
        value::members members ( value.getmembernames () );

        if ( members.empty () )
            pushvalue ( "{}" );
        else
        {
            writewithindent ( "{" );
            indent ();
            value::members::iterator it = members.begin ();

            while ( true )
            {
                std::string const& name = *it;
                const value& childvalue = value[name];
                writecommentbeforevalue ( childvalue );
                writewithindent ( valuetoquotedstring ( name.c_str () ) );
                document_ += " : ";
                writevalue ( childvalue );

                if ( ++it == members.end () )
                {
                    writecommentaftervalueonsameline ( childvalue );
                    break;
                }

                document_ += ",";
                writecommentaftervalueonsameline ( childvalue );
            }

            unindent ();
            writewithindent ( "}" );
        }
    }
    break;
    }
}


void
styledwriter::writearrayvalue ( const value& value )
{
    unsigned size = value.size ();

    if ( size == 0 )
        pushvalue ( "[]" );
    else
    {
        bool isarraymultiline = ismultinearray ( value );

        if ( isarraymultiline )
        {
            writewithindent ( "[" );
            indent ();
            bool haschildvalue = !childvalues_.empty ();
            unsigned index = 0;

            while ( true )
            {
                const value& childvalue = value[index];
                writecommentbeforevalue ( childvalue );

                if ( haschildvalue )
                    writewithindent ( childvalues_[index] );
                else
                {
                    writeindent ();
                    writevalue ( childvalue );
                }

                if ( ++index == size )
                {
                    writecommentaftervalueonsameline ( childvalue );
                    break;
                }

                document_ += ",";
                writecommentaftervalueonsameline ( childvalue );
            }

            unindent ();
            writewithindent ( "]" );
        }
        else // output on a single line
        {
            assert ( childvalues_.size () == size );
            document_ += "[ ";

            for ( unsigned index = 0; index < size; ++index )
            {
                if ( index > 0 )
                    document_ += ", ";

                document_ += childvalues_[index];
            }

            document_ += " ]";
        }
    }
}


bool
styledwriter::ismultinearray ( const value& value )
{
    int size = value.size ();
    bool ismultiline = size * 3 >= rightmargin_ ;
    childvalues_.clear ();

    for ( int index = 0; index < size  &&  !ismultiline; ++index )
    {
        const value& childvalue = value[index];
        ismultiline = ismultiline  ||
                      ( (childvalue.isarray ()  ||  childvalue.isobject ())  &&
                        childvalue.size () > 0 );
    }

    if ( !ismultiline ) // check if line length > max line length
    {
        childvalues_.reserve ( size );
        addchildvalues_ = true;
        int linelength = 4 + (size - 1) * 2; // '[ ' + ', '*n + ' ]'

        for ( int index = 0; index < size  &&  !ismultiline; ++index )
        {
            writevalue ( value[index] );
            linelength += int ( childvalues_[index].length () );
            ismultiline = ismultiline  &&  hascommentforvalue ( value[index] );
        }

        addchildvalues_ = false;
        ismultiline = ismultiline  ||  linelength >= rightmargin_;
    }

    return ismultiline;
}


void
styledwriter::pushvalue ( std::string const& value )
{
    if ( addchildvalues_ )
        childvalues_.push_back ( value );
    else
        document_ += value;
}


void
styledwriter::writeindent ()
{
    if ( !document_.empty () )
    {
        char last = document_[document_.length () - 1];

        if ( last == ' ' )     // already indented
            return;

        if ( last != '\n' )    // comments may add new-line
            document_ += '\n';
    }

    document_ += indentstring_;
}


void
styledwriter::writewithindent ( std::string const& value )
{
    writeindent ();
    document_ += value;
}


void
styledwriter::indent ()
{
    indentstring_ += std::string ( indentsize_, ' ' );
}


void
styledwriter::unindent ()
{
    assert ( int (indentstring_.size ()) >= indentsize_ );
    indentstring_.resize ( indentstring_.size () - indentsize_ );
}


void
styledwriter::writecommentbeforevalue ( const value& root )
{
    if ( !root.hascomment ( commentbefore ) )
        return;

    document_ += normalizeeol ( root.getcomment ( commentbefore ) );
    document_ += "\n";
}


void
styledwriter::writecommentaftervalueonsameline ( const value& root )
{
    if ( root.hascomment ( commentafteronsameline ) )
        document_ += " " + normalizeeol ( root.getcomment ( commentafteronsameline ) );

    if ( root.hascomment ( commentafter ) )
    {
        document_ += "\n";
        document_ += normalizeeol ( root.getcomment ( commentafter ) );
        document_ += "\n";
    }
}


bool
styledwriter::hascommentforvalue ( const value& value )
{
    return value.hascomment ( commentbefore )
           ||  value.hascomment ( commentafteronsameline )
           ||  value.hascomment ( commentafter );
}


std::string
styledwriter::normalizeeol ( std::string const& text )
{
    std::string normalized;
    normalized.reserve ( text.length () );
    const char* begin = text.c_str ();
    const char* end = begin + text.length ();
    const char* current = begin;

    while ( current != end )
    {
        char c = *current++;

        if ( c == '\r' ) // mac or dos eol
        {
            if ( *current == '\n' ) // convert dos eol
                ++current;

            normalized += '\n';
        }
        else // handle unix eol & other char
            normalized += c;
    }

    return normalized;
}


// class styledstreamwriter
// //////////////////////////////////////////////////////////////////

styledstreamwriter::styledstreamwriter ( std::string indentation )
    : document_ (nullptr)
    , rightmargin_ ( 74 )
    , indentation_ ( indentation )
{
}


void
styledstreamwriter::write ( std::ostream& out, const value& root )
{
    document_ = &out;
    addchildvalues_ = false;
    indentstring_ = "";
    writecommentbeforevalue ( root );
    writevalue ( root );
    writecommentaftervalueonsameline ( root );
    *document_ << "\n";
    document_ = nullptr; // forget the stream, for safety.
}


void
styledstreamwriter::writevalue ( const value& value )
{
    switch ( value.type () )
    {
    case nullvalue:
        pushvalue ( "null" );
        break;

    case intvalue:
        pushvalue ( valuetostring ( value.asint () ) );
        break;

    case uintvalue:
        pushvalue ( valuetostring ( value.asuint () ) );
        break;

    case realvalue:
        pushvalue ( valuetostring ( value.asdouble () ) );
        break;

    case stringvalue:
        pushvalue ( valuetoquotedstring ( value.ascstring () ) );
        break;

    case booleanvalue:
        pushvalue ( valuetostring ( value.asbool () ) );
        break;

    case arrayvalue:
        writearrayvalue ( value);
        break;

    case objectvalue:
    {
        value::members members ( value.getmembernames () );

        if ( members.empty () )
            pushvalue ( "{}" );
        else
        {
            writewithindent ( "{" );
            indent ();
            value::members::iterator it = members.begin ();

            while ( true )
            {
                std::string const& name = *it;
                const value& childvalue = value[name];
                writecommentbeforevalue ( childvalue );
                writewithindent ( valuetoquotedstring ( name.c_str () ) );
                *document_ << " : ";
                writevalue ( childvalue );

                if ( ++it == members.end () )
                {
                    writecommentaftervalueonsameline ( childvalue );
                    break;
                }

                *document_ << ",";
                writecommentaftervalueonsameline ( childvalue );
            }

            unindent ();
            writewithindent ( "}" );
        }
    }
    break;
    }
}


void
styledstreamwriter::writearrayvalue ( const value& value )
{
    unsigned size = value.size ();

    if ( size == 0 )
        pushvalue ( "[]" );
    else
    {
        bool isarraymultiline = ismultinearray ( value );

        if ( isarraymultiline )
        {
            writewithindent ( "[" );
            indent ();
            bool haschildvalue = !childvalues_.empty ();
            unsigned index = 0;

            while ( true )
            {
                const value& childvalue = value[index];
                writecommentbeforevalue ( childvalue );

                if ( haschildvalue )
                    writewithindent ( childvalues_[index] );
                else
                {
                    writeindent ();
                    writevalue ( childvalue );
                }

                if ( ++index == size )
                {
                    writecommentaftervalueonsameline ( childvalue );
                    break;
                }

                *document_ << ",";
                writecommentaftervalueonsameline ( childvalue );
            }

            unindent ();
            writewithindent ( "]" );
        }
        else // output on a single line
        {
            assert ( childvalues_.size () == size );
            *document_ << "[ ";

            for ( unsigned index = 0; index < size; ++index )
            {
                if ( index > 0 )
                    *document_ << ", ";

                *document_ << childvalues_[index];
            }

            *document_ << " ]";
        }
    }
}


bool
styledstreamwriter::ismultinearray ( const value& value )
{
    int size = value.size ();
    bool ismultiline = size * 3 >= rightmargin_ ;
    childvalues_.clear ();

    for ( int index = 0; index < size  &&  !ismultiline; ++index )
    {
        const value& childvalue = value[index];
        ismultiline = ismultiline  ||
                      ( (childvalue.isarray ()  ||  childvalue.isobject ())  &&
                        childvalue.size () > 0 );
    }

    if ( !ismultiline ) // check if line length > max line length
    {
        childvalues_.reserve ( size );
        addchildvalues_ = true;
        int linelength = 4 + (size - 1) * 2; // '[ ' + ', '*n + ' ]'

        for ( int index = 0; index < size  &&  !ismultiline; ++index )
        {
            writevalue ( value[index] );
            linelength += int ( childvalues_[index].length () );
            ismultiline = ismultiline  &&  hascommentforvalue ( value[index] );
        }

        addchildvalues_ = false;
        ismultiline = ismultiline  ||  linelength >= rightmargin_;
    }

    return ismultiline;
}


void
styledstreamwriter::pushvalue ( std::string const& value )
{
    if ( addchildvalues_ )
        childvalues_.push_back ( value );
    else
        *document_ << value;
}


void
styledstreamwriter::writeindent ()
{
    /*
      some comments in this method would have been nice. ;-)

     if ( !document_.empty() )
     {
        char last = document_[document_.length()-1];
        if ( last == ' ' )     // already indented
           return;
        if ( last != '\n' )    // comments may add new-line
           *document_ << '\n';
     }
    */
    *document_ << '\n' << indentstring_;
}


void
styledstreamwriter::writewithindent ( std::string const& value )
{
    writeindent ();
    *document_ << value;
}


void
styledstreamwriter::indent ()
{
    indentstring_ += indentation_;
}


void
styledstreamwriter::unindent ()
{
    assert ( indentstring_.size () >= indentation_.size () );
    indentstring_.resize ( indentstring_.size () - indentation_.size () );
}


void
styledstreamwriter::writecommentbeforevalue ( const value& root )
{
    if ( !root.hascomment ( commentbefore ) )
        return;

    *document_ << normalizeeol ( root.getcomment ( commentbefore ) );
    *document_ << "\n";
}


void
styledstreamwriter::writecommentaftervalueonsameline ( const value& root )
{
    if ( root.hascomment ( commentafteronsameline ) )
        *document_ << " " + normalizeeol ( root.getcomment ( commentafteronsameline ) );

    if ( root.hascomment ( commentafter ) )
    {
        *document_ << "\n";
        *document_ << normalizeeol ( root.getcomment ( commentafter ) );
        *document_ << "\n";
    }
}


bool
styledstreamwriter::hascommentforvalue ( const value& value )
{
    return value.hascomment ( commentbefore )
           ||  value.hascomment ( commentafteronsameline )
           ||  value.hascomment ( commentafter );
}


std::string
styledstreamwriter::normalizeeol ( std::string const& text )
{
    std::string normalized;
    normalized.reserve ( text.length () );
    const char* begin = text.c_str ();
    const char* end = begin + text.length ();
    const char* current = begin;

    while ( current != end )
    {
        char c = *current++;

        if ( c == '\r' ) // mac or dos eol
        {
            if ( *current == '\n' ) // convert dos eol
                ++current;

            normalized += '\n';
        }
        else // handle unix eol & other char
            normalized += c;
    }

    return normalized;
}


std::ostream& operator<< ( std::ostream& sout, const value& root )
{
    json::styledstreamwriter writer;
    writer.write (sout, root);
    return sout;
}

//------------------------------------------------------------------------------

namespace detail {

inline
void
write_string (write_t write, std::string const& s)
{
    write(s.data(), s.size());
}

void
write_value (write_t write, value const& value)
{
    switch (value.type())
    {
    case nullvalue:
        write("null", 4);
        break;

    case intvalue:
        write_string(write, valuetostring(value.asint()));
        break;

    case uintvalue:
        write_string(write, valuetostring(value.asuint()));
        break;

    case realvalue:
        write_string(write, valuetostring(value.asdouble()));
        break;

    case stringvalue:
        write_string(write, valuetoquotedstring(value.ascstring()));
        break;

    case booleanvalue:
        write_string(write, valuetostring(value.asbool()));
        break;

    case arrayvalue:
    {
        write("[", 1);
        int const size = value.size();
        for (int index = 0; index < size; ++index)
        {
            if (index > 0)
                write(",", 1);
            write_value(write, value[index]);
        }
        write("]", 1);
        break;
    }

    case objectvalue:
    {
        value::members const members = value.getmembernames();
        write("{", 1);
        for (auto it = members.begin(); it != members.end(); ++it)
        {
            std::string const& name = *it;
            if (it != members.begin())
                write(",", 1);

            write_string(write, valuetoquotedstring(name.c_str()));
            write(":", 1);
            write_value(write, value[name]);
        }
        write("}", 1);
        break;
    }
    }
}

} // detail

void
stream (json::value const& jv, write_t write)
{
    detail::write_value(write, jv);
    write("\n", 1);
}

} // namespace json
