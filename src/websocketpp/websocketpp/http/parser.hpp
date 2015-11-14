/*
 * copyright (c) 2014, peter thorson. all rights reserved.
 *
 * redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * neither the name of the websocket++ project nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * this software is provided by the copyright holders and contributors "as is"
 * and any express or implied warranties, including, but not limited to, the
 * implied warranties of merchantability and fitness for a particular purpose
 * are disclaimed. in no event shall peter thorson be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential damages
 * (including, but not limited to, procurement of substitute goods or services;
 * loss of use, data, or profits; or business interruption) however caused and
 * on any theory of liability, whether in contract, strict liability, or tort
 * (including negligence or otherwise) arising in any way out of the use of this
 * software, even if advised of the possibility of such damage.
 *
 */

#ifndef http_parser_hpp
#define http_parser_hpp

#include <algorithm>
#include <iostream>
#include <map>

#include <websocketpp/utilities.hpp>
#include <websocketpp/http/constants.hpp>

namespace websocketpp {
namespace http {
namespace parser {

namespace state {
    enum value {
        method,
        resource,
        version,
        headers
    };
}

typedef std::map<std::string, std::string, utility::ci_less > header_list;

/// read and return the next token in the stream
/**
 * read until a non-token character is found and then return the token and
 * iterator to the next character to read
 *
 * @param begin an iterator to the beginning of the sequence
 * @param end an iterator to the end of the sequence
 * @return a pair containing the token and an iterator to the next character in
 * the stream
 */
template <typename inputiterator>
std::pair<std::string,inputiterator> extract_token(inputiterator begin,
    inputiterator end)
{
    inputiterator it = std::find_if(begin,end,&is_not_token_char);
    return std::make_pair(std::string(begin,it),it);
}

/// read and return the next quoted string in the stream
/**
 * read a double quoted string starting at `begin`. the quotes themselves are
 * stripped. the quoted value is returned along with an iterator to the next
 * character to read
 *
 * @param begin an iterator to the beginning of the sequence
 * @param end an iterator to the end of the sequence
 * @return a pair containing the string read and an iterator to the next
 * character in the stream
 */
template <typename inputiterator>
std::pair<std::string,inputiterator> extract_quoted_string(inputiterator begin,
    inputiterator end)
{
    std::string s;

    if (end == begin) {
        return std::make_pair(s,begin);
    }

    if (*begin != '"') {
        return std::make_pair(s,begin);
    }

    inputiterator cursor = begin+1;
    inputiterator marker = cursor;

    cursor = std::find(cursor,end,'"');

    while (cursor != end) {
        // either this is the end or a quoted string
        if (*(cursor-1) == '\\') {
            s.append(marker,cursor-1);
            s.append(1,'"');
            ++cursor;
            marker = cursor;
        } else {
            s.append(marker,cursor);
            ++cursor;
            return std::make_pair(s,cursor);
        }

        cursor = std::find(cursor,end,'"');
    }

    return std::make_pair("",begin);
}

/// read and discard one unit of linear whitespace
/**
 * read one unit of linear white space and return the iterator to the character
 * afterwards. if `begin` is returned, no whitespace was extracted.
 *
 * @param begin an iterator to the beginning of the sequence
 * @param end an iterator to the end of the sequence
 * @return an iterator to the character after the linear whitespace read
 */
template <typename inputiterator>
inputiterator extract_lws(inputiterator begin, inputiterator end) {
    inputiterator it = begin;

    // strip leading crlf
    if (end-begin > 2 && *begin == '\r' && *(begin+1) == '\n' &&
        is_whitespace_char(static_cast<unsigned char>(*(begin+2))))
    {
        it+=3;
    }

    it = std::find_if(it,end,&is_not_whitespace_char);
    return it;
}

/// read and discard linear whitespace
/**
 * read linear white space until a non-lws character is read and return an
 * iterator to that character. if `begin` is returned, no whitespace was
 * extracted.
 *
 * @param begin an iterator to the beginning of the sequence
 * @param end an iterator to the end of the sequence
 * @return an iterator to the character after the linear whitespace read
 */
template <typename inputiterator>
inputiterator extract_all_lws(inputiterator begin, inputiterator end) {
    inputiterator old_it;
    inputiterator new_it = begin;

    do {
        // pull value from previous iteration
        old_it = new_it;

        // look ahead another pass
        new_it = extract_lws(old_it,end);
    } while (new_it != end && old_it != new_it);

    return new_it;
}

/// extract http attributes
/**
 * an http attributes list is a semicolon delimited list of key value pairs in
 * the format: *( ";" attribute "=" value ) where attribute is a token and value
 * is a token or quoted string.
 *
 * attributes extracted are appended to the supplied attributes list
 * `attributes`.
 *
 * @param [in] begin an iterator to the beginning of the sequence
 * @param [in] end an iterator to the end of the sequence
 * @param [out] attributes a reference to the attributes list to append
 * attribute/value pairs extracted to
 * @return an iterator to the character after the last atribute read
 */
template <typename inputiterator>
inputiterator extract_attributes(inputiterator begin, inputiterator end,
    attribute_list & attributes)
{
    inputiterator cursor;
    bool first = true;

    if (begin == end) {
        return begin;
    }

    cursor = begin;
    std::pair<std::string,inputiterator> ret;

    while (cursor != end) {
        std::string name;

        cursor = http::parser::extract_all_lws(cursor,end);
        if (cursor == end) {
            break;
        }

        if (first) {
            // ignore this check for the very first pass
            first = false;
        } else {
            if (*cursor == ';') {
                // advance past the ';'
                ++cursor;
            } else {
                // non-semicolon in this position indicates end end of the
                // attribute list, break and return.
                break;
            }
        }

        cursor = http::parser::extract_all_lws(cursor,end);
        ret = http::parser::extract_token(cursor,end);

        if (ret.first == "") {
            // error: expected a token
            return begin;
        } else {
            name = ret.first;
            cursor = ret.second;
        }

        cursor = http::parser::extract_all_lws(cursor,end);
        if (cursor == end || *cursor != '=') {
            // if there is an equals sign, read the attribute value. otherwise
            // record a blank value and continue
            attributes[name] = "";
            continue;
        }

        // advance past the '='
        ++cursor;

        cursor = http::parser::extract_all_lws(cursor,end);
        if (cursor == end) {
            // error: expected a token or quoted string
            return begin;
        }

        ret = http::parser::extract_quoted_string(cursor,end);
        if (ret.second != cursor) {
            attributes[name] = ret.first;
            cursor = ret.second;
            continue;
        }

        ret = http::parser::extract_token(cursor,end);
        if (ret.first == "") {
            // error : expected token or quoted string
            return begin;
        } else {
            attributes[name] = ret.first;
            cursor = ret.second;
        }
    }

    return cursor;
}

/// extract http parameters
/**
 * an http parameters list is a comma delimited list of tokens followed by
 * optional semicolon delimited attributes lists.
 *
 * parameters extracted are appended to the supplied parameters list
 * `parameters`.
 *
 * @param [in] begin an iterator to the beginning of the sequence
 * @param [in] end an iterator to the end of the sequence
 * @param [out] parameters a reference to the parameters list to append
 * paramter values extracted to
 * @return an iterator to the character after the last parameter read
 */
template <typename inputiterator>
inputiterator extract_parameters(inputiterator begin, inputiterator end,
    parameter_list &parameters)
{
    inputiterator cursor;

    if (begin == end) {
        // error: expected non-zero length range
        return begin;
    }

    cursor = begin;
    std::pair<std::string,inputiterator> ret;

    /**
     * lws
     * token
     * lws
     * *(";" method-param)
     * lws
     * ,=loop again
     */
    while (cursor != end) {
        std::string parameter_name;
        attribute_list attributes;

        // extract any stray whitespace
        cursor = http::parser::extract_all_lws(cursor,end);
        if (cursor == end) {break;}

        ret = http::parser::extract_token(cursor,end);

        if (ret.first == "") {
            // error: expected a token
            return begin;
        } else {
            parameter_name = ret.first;
            cursor = ret.second;
        }

        // safe break point, insert parameter with blank attributes and exit
        cursor = http::parser::extract_all_lws(cursor,end);
        if (cursor == end) {
            //parameters[parameter_name] = attributes;
            parameters.push_back(std::make_pair(parameter_name,attributes));
            break;
        }

        // if there is an attribute list, read it in
        if (*cursor == ';') {
            inputiterator acursor;

            ++cursor;
            acursor = http::parser::extract_attributes(cursor,end,attributes);

            if (acursor == cursor) {
                // attribute extraction ended in syntax error
                return begin;
            }

            cursor = acursor;
        }

        // insert parameter into output list
        //parameters[parameter_name] = attributes;
        parameters.push_back(std::make_pair(parameter_name,attributes));

        cursor = http::parser::extract_all_lws(cursor,end);
        if (cursor == end) {break;}

        // if next char is ',' then read another parameter, else stop
        if (*cursor != ',') {
            break;
        }

        // advance past comma
        ++cursor;

        if (cursor == end) {
            // expected more bytes after a comma
            return begin;
        }
    }

    return cursor;
}

inline std::string strip_lws(std::string const & input) {
    std::string::const_iterator begin = extract_all_lws(input.begin(),input.end());
    if (begin == input.end()) {
        return std::string();
    }
    std::string::const_reverse_iterator end = extract_all_lws(input.rbegin(),input.rend());

    return std::string(begin,end.base());
}

/// base http parser
/**
 * includes methods and data elements common to all types of http messages such
 * as headers, versions, bodies, etc.
 */
class parser {
public:
    /// get the http version string
    /**
     * @return the version string for this parser
     */
    std::string const & get_version() const {
        return m_version;
    }

    /// set http parser version
    /**
     * input should be in format: http/x.y where x and y are positive integers.
     * @todo does this method need any validation?
     *
     * @param [in] version the value to set the http version to.
     */
    void set_version(std::string const & version);

    /// get the value of an http header
    /**
     * @todo make this method case insensitive.
     *
     * @param [in] key the name/key of the header to get.
     * @return the value associated with the given http header key.
     */
    std::string const & get_header(std::string const & key) const;

    /// extract an http parameter list from a parser header.
    /**
     * if the header requested doesn't exist or exists and is empty the
     * parameter list is valid (but empty).
     *
     * @param [in] key the name/key of the http header to use as input.
     * @param [out] out the parameter list to store extracted parameters in.
     * @return whether or not the input was a valid parameter list.
     */
    bool get_header_as_plist(std::string const & key, parameter_list & out)
        const;

    /// append a value to an existing http header
    /**
     * this method will set the value of the http header `key` with the
     * indicated value. if a header with the name `key` already exists, `val`
     * will be appended to the existing value.
     *
     * @todo make this method case insensitive.
     * @todo should there be any restrictions on which keys are allowed?
     * @todo exception free varient
     *
     * @see replace_header
     *
     * @param [in] key the name/key of the header to append to.
     * @param [in] val the value to append.
     */
    void append_header(std::string const & key, std::string const & val);

    /// set a value for an http header, replacing an existing value
    /**
     * this method will set the value of the http header `key` with the
     * indicated value. if a header with the name `key` already exists, `val`
     * will replace the existing value.
     *
     * @todo make this method case insensitive.
     * @todo should there be any restrictions on which keys are allowed?
     * @todo exception free varient
     *
     * @see append_header
     *
     * @param [in] key the name/key of the header to append to.
     * @param [in] val the value to append.
     */
    void replace_header(std::string const & key, std::string const & val);

    /// remove a header from the parser
    /**
     * removes the header entirely from the parser. this is different than
     * setting the value of the header to blank.
     *
     * @todo make this method case insensitive.
     *
     * @param [in] key the name/key of the header to remove.
     */
    void remove_header(std::string const & key);

    /// set http body
    /**
     * sets the body of the http object and fills in the appropriate content
     * length header.
     *
     * @param [in] value the value to set the body to.
     */
    std::string const & get_body() const {
        return m_body;
    }

    /// set body content
    /**
     * set the body content of the http response to the parameter string. note
     * set_body will also set the content-length http header to the appropriate
     * value. if you want the content-length header to be something else, do so
     * via replace_header("content-length") after calling set_body()
     *
     * @param value string data to include as the body content.
     */
    void set_body(std::string const & value);

    /// extract an http parameter list from a string.
    /**
     * @param [in] in the input string.
     * @param [out] out the parameter list to store extracted parameters in.
     * @return whether or not the input was a valid parameter list.
     */
    bool parse_parameter_list(std::string const & in, parameter_list & out)
        const;
protected:
    /// parse headers from an istream
    /**
     * @deprecated use process_header instead.
     *
     * @param [in] s the istream to extract headers from.
     */
    bool parse_headers(std::istream & s);

    /// process a header line
    /**
     * @todo update this method to be exception free.
     *
     * @param [in] begin an iterator to the beginning of the sequence.
     * @param [in] end an iterator to the end of the sequence.
     */
    void process_header(std::string::iterator begin, std::string::iterator end);

    /// generate and return the http headers as a string
    /**
     * each headers will be followed by the \r\n sequence including the last one.
     * a second \r\n sequence (blank header) is not appended by this method
     *
     * @return the http headers as a string.
     */
    std::string raw_headers() const;

    std::string m_version;
    header_list m_headers;
    std::string m_body;
};

} // namespace parser
} // namespace http
} // namespace websocketpp

#include <websocketpp/http/impl/parser.hpp>

#endif // http_parser_hpp
