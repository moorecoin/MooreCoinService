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

#ifndef cpptl_json_reader_h_included
# define cpptl_json_reader_h_included

#include <ripple/json/json_config.h>
#include <ripple/json/json_features.h>
#include <ripple/json/json_forwards.h>
#include <ripple/json/json_value.h>

#include <stack>

namespace json
{

/** \brief unserialize a <a href="http://www.json.org">json</a> document into a value.
 *
 */
class json_api reader
{
public:
    typedef char char;
    typedef const char* location;

    /** \brief constructs a reader allowing all features
     * for parsing.
     */
    reader ();

    /** \brief constructs a reader allowing the specified feature set
     * for parsing.
     */
    reader ( const features& features );

    /** \brief read a value from a <a href="http://www.json.org">json</a> document.
     * \param document utf-8 encoded string containing the document to read.
     * \param root [out] contains the root value of the document if it was
     *             successfully parsed.
     * \param collectcomments \c true to collect comment and allow writing them back during
     *                        serialization, \c false to discard comments.
     *                        this parameter is ignored if features::allowcomments_
     *                        is \c false.
     * \return \c true if the document was successfully parsed, \c false if an error occurred.
     */
    bool parse ( std::string const& document,
                 value& root,
                 bool collectcomments = true );

    /** \brief read a value from a <a href="http://www.json.org">json</a> document.
     * \param document utf-8 encoded string containing the document to read.
     * \param root [out] contains the root value of the document if it was
     *             successfully parsed.
     * \param collectcomments \c true to collect comment and allow writing them back during
     *                        serialization, \c false to discard comments.
     *                        this parameter is ignored if features::allowcomments_
     *                        is \c false.
     * \return \c true if the document was successfully parsed, \c false if an error occurred.
     */
    bool parse ( const char* begindoc, const char* enddoc,
                 value& root,
                 bool collectcomments = true );

    /// \brief parse from input stream.
    /// \see json::operator>>(std::istream&, json::value&).
    bool parse ( std::istream& is,
                 value& root,
                 bool collectcomments = true );

    /** \brief returns a user friendly string that list errors in the parsed document.
     * \return formatted error message with the list of errors with their location in
     *         the parsed document. an empty string is returned if no error occurred
     *         during parsing.
     */
    std::string getformatederrormessages () const;

private:
    enum tokentype
    {
        tokenendofstream = 0,
        tokenobjectbegin,
        tokenobjectend,
        tokenarraybegin,
        tokenarrayend,
        tokenstring,
        tokennumber,
        tokentrue,
        tokenfalse,
        tokennull,
        tokenarrayseparator,
        tokenmemberseparator,
        tokencomment,
        tokenerror
    };

    class token
    {
    public:
        tokentype type_;
        location start_;
        location end_;
    };

    class errorinfo
    {
    public:
        token token_;
        std::string message_;
        location extra_;
    };

    typedef std::deque<errorinfo> errors;

    bool expecttoken ( tokentype type, token& token, const char* message );
    bool readtoken ( token& token );
    void skipspaces ();
    bool match ( location pattern,
                 int patternlength );
    bool readcomment ();
    bool readcstylecomment ();
    bool readcppstylecomment ();
    bool readstring ();
    void readnumber ();
    bool readvalue ();
    bool readobject ( token& token );
    bool readarray ( token& token );
    bool decodenumber ( token& token );
    bool decodestring ( token& token );
    bool decodestring ( token& token, std::string& decoded );
    bool decodedouble ( token& token );
    bool decodeunicodecodepoint ( token& token,
                                  location& current,
                                  location end,
                                  unsigned int& unicode );
    bool decodeunicodeescapesequence ( token& token,
                                       location& current,
                                       location end,
                                       unsigned int& unicode );
    bool adderror ( std::string const& message,
                    token& token,
                    location extra = 0 );
    bool recoverfromerror ( tokentype skipuntiltoken );
    bool adderrorandrecover ( std::string const& message,
                              token& token,
                              tokentype skipuntiltoken );
    void skipuntilspace ();
    value& currentvalue ();
    char getnextchar ();
    void getlocationlineandcolumn ( location location,
                                    int& line,
                                    int& column ) const;
    std::string getlocationlineandcolumn ( location location ) const;
    void addcomment ( location begin,
                      location end,
                      commentplacement placement );
    void skipcommenttokens ( token& token );

    typedef std::stack<value*> nodes;
    nodes nodes_;
    errors errors_;
    std::string document_;
    location begin_;
    location end_;
    location current_;
    location lastvalueend_;
    value* lastvalue_;
    std::string commentsbefore_;
    features features_;
    bool collectcomments_;
};

/** \brief read from 'sin' into 'root'.

 always keep comments from the input json.

 this can be used to read a file into a particular sub-object.
 for example:
 \code
 json::value root;
 cin >> root["dir"]["file"];
 cout << root;
 \endcode
 result:
 \verbatim
 {
"dir": {
    "file": {
 // the input stream json would be nested here.
    }
}
 }
 \endverbatim
 \throw std::exception on parse error.
 \see json::operator<<()
*/
std::istream& operator>> ( std::istream&, value& );

} // namespace json

#endif // cpptl_json_reader_h_included
