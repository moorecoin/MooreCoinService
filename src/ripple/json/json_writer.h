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

#ifndef json_writer_h_included
#define json_writer_h_included

#include <ripple/json/json_config.h>
#include <ripple/json/json_forwards.h>
#include <ripple/json/json_value.h>
#include <vector>

namespace json
{

class value;

/** \brief abstract class for writers.
 */
class json_api writer
{
public:
    virtual ~writer ();

    virtual std::string write ( const value& root ) = 0;
};

/** \brief outputs a value in <a href="http://www.json.org">json</a> format without formatting (not human friendly).
 *
 * the json document is written in a single line. it is not intended for 'human' consumption,
 * but may be usefull to support feature such as rpc where bandwith is limited.
 * \sa reader, value
 */
class json_api fastwriter : public writer
{
public:
    fastwriter ();
    virtual ~fastwriter () {}

    void enableyamlcompatibility ();

public: // overridden from writer
    virtual std::string write ( const value& root );

private:
    void writevalue ( const value& value );

    std::string document_;
    bool yamlcompatiblityenabled_;
};

/** \brief writes a value in <a href="http://www.json.org">json</a> format in a human friendly way.
 *
 * the rules for line break and indent are as follow:
 * - object value:
 *     - if empty then print {} without indent and line break
 *     - if not empty the print '{', line break & indent, print one value per line
 *       and then unindent and line break and print '}'.
 * - array value:
 *     - if empty then print [] without indent and line break
 *     - if the array contains no object value, empty array or some other value types,
 *       and all the values fit on one lines, then print the array on a single line.
 *     - otherwise, it the values do not fit on one line, or the array contains
 *       object or non empty array, then print one value per line.
 *
 * if the value have comments then they are outputed according to their #commentplacement.
 *
 * \sa reader, value, value::setcomment()
 */
class json_api styledwriter: public writer
{
public:
    styledwriter ();
    virtual ~styledwriter () {}

public: // overridden from writer
    /** \brief serialize a value in <a href="http://www.json.org">json</a> format.
     * \param root value to serialize.
     * \return string containing the json document that represents the root value.
     */
    virtual std::string write ( const value& root );

private:
    void writevalue ( const value& value );
    void writearrayvalue ( const value& value );
    bool ismultinearray ( const value& value );
    void pushvalue ( std::string const& value );
    void writeindent ();
    void writewithindent ( std::string const& value );
    void indent ();
    void unindent ();
    void writecommentbeforevalue ( const value& root );
    void writecommentaftervalueonsameline ( const value& root );
    bool hascommentforvalue ( const value& value );
    static std::string normalizeeol ( std::string const& text );

    typedef std::vector<std::string> childvalues;

    childvalues childvalues_;
    std::string document_;
    std::string indentstring_;
    int rightmargin_;
    int indentsize_;
    bool addchildvalues_;
};

/** \brief writes a value in <a href="http://www.json.org">json</a> format in a human friendly way,
     to a stream rather than to a string.
 *
 * the rules for line break and indent are as follow:
 * - object value:
 *     - if empty then print {} without indent and line break
 *     - if not empty the print '{', line break & indent, print one value per line
 *       and then unindent and line break and print '}'.
 * - array value:
 *     - if empty then print [] without indent and line break
 *     - if the array contains no object value, empty array or some other value types,
 *       and all the values fit on one lines, then print the array on a single line.
 *     - otherwise, it the values do not fit on one line, or the array contains
 *       object or non empty array, then print one value per line.
 *
 * if the value have comments then they are outputed according to their #commentplacement.
 *
 * \param indentation each level will be indented by this amount extra.
 * \sa reader, value, value::setcomment()
 */
class json_api styledstreamwriter
{
public:
    styledstreamwriter ( std::string indentation = "\t" );
    ~styledstreamwriter () {}

public:
    /** \brief serialize a value in <a href="http://www.json.org">json</a> format.
     * \param out stream to write to. (can be ostringstream, e.g.)
     * \param root value to serialize.
     * \note there is no point in deriving from writer, since write() should not return a value.
     */
    void write ( std::ostream& out, const value& root );

private:
    void writevalue ( const value& value );
    void writearrayvalue ( const value& value );
    bool ismultinearray ( const value& value );
    void pushvalue ( std::string const& value );
    void writeindent ();
    void writewithindent ( std::string const& value );
    void indent ();
    void unindent ();
    void writecommentbeforevalue ( const value& root );
    void writecommentaftervalueonsameline ( const value& root );
    bool hascommentforvalue ( const value& value );
    static std::string normalizeeol ( std::string const& text );

    typedef std::vector<std::string> childvalues;

    childvalues childvalues_;
    std::ostream* document_;
    std::string indentstring_;
    int rightmargin_;
    std::string indentation_;
    bool addchildvalues_;
};

std::string json_api valuetostring ( int value );
std::string json_api valuetostring ( uint value );
std::string json_api valuetostring ( double value );
std::string json_api valuetostring ( bool value );
std::string json_api valuetoquotedstring ( const char* value );

/// \brief output using the styledstreamwriter.
/// \see json::operator>>()
std::ostream& operator<< ( std::ostream&, const value& root );

} // namespace json



#endif // json_writer_h_included
