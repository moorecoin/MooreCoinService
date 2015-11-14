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

#ifndef websocketpp_utilities_hpp
#define websocketpp_utilities_hpp

#include <websocketpp/common/stdint.hpp>

#include <algorithm>
#include <string>
#include <locale>

namespace websocketpp {
/// generic non-websocket specific utility functions and data structures
namespace utility {

/// helper functor for case insensitive find
/**
 * based on code from
 * http://stackoverflow.com/questions/3152241/case-insensitive-stdstring-find
 *
 * templated version of my_equal so it could work with both char and wchar_t
 */
template<typename chart>
struct my_equal {
    /// construct the functor with the given locale
    /**
     * @param [in] loc the locale to use for determining the case of values
     */
    my_equal(std::locale const & loc ) : m_loc(loc) {}

    /// perform a case insensitive comparison
    /**
     * @param ch1 the first value to compare
     * @param ch2 the second value to compare
     * @return whether or not the two values are equal when both are converted
     *         to uppercase using the given locale.
     */
    bool operator()(chart ch1, chart ch2) {
        return std::toupper(ch1, m_loc) == std::toupper(ch2, m_loc);
    }
private:
    std::locale const & m_loc;
};

/// helper less than functor for case insensitive find
/**
 * based on code from
 * http://stackoverflow.com/questions/3152241/case-insensitive-stdstring-find
 */
struct ci_less : std::binary_function<std::string, std::string, bool> {
    // case-independent (ci) compare_less binary function
    struct nocase_compare
      : public std::binary_function<unsigned char,unsigned char,bool>
    {
        bool operator() (unsigned char const & c1, unsigned char const & c2) const {
            return tolower (c1) < tolower (c2);
        }
    };
    bool operator() (std::string const & s1, std::string const & s2) const {
        return std::lexicographical_compare
            (s1.begin (), s1.end (),   // source range
            s2.begin (), s2.end (),   // dest range
            nocase_compare ());  // comparison
    }
};

/// find substring (case insensitive)
/**
 * @param [in] haystack the string to search in
 * @param [in] needle the string to search for
 * @param [in] loc the locale to use for determining the case of values.
 *             defaults to the current locale.
 * @return an iterator to the first element of the first occurrance of needle in
 *         haystack. if the sequence is not found, the function returns
 *         haystack.end()
 */
template<typename t>
typename t::const_iterator ci_find_substr(t const & haystack, t const & needle,
    std::locale const & loc = std::locale())
{
    return std::search( haystack.begin(), haystack.end(),
        needle.begin(), needle.end(), my_equal<typename t::value_type>(loc) );
}

/// find substring (case insensitive)
/**
 * @todo is this still used? this method may not make sense.. should use
 * iterators or be less generic. as is it is too tightly coupled to std::string
 *
 * @param [in] haystack the string to search in
 * @param [in] needle the string to search for as a char array of values
 * @param [in] size length of needle
 * @param [in] loc the locale to use for determining the case of values.
 *             defaults to the current locale.
 * @return an iterator to the first element of the first occurrance of needle in
 *         haystack. if the sequence is not found, the function returns
 *         haystack.end()
 */
template<typename t>
typename t::const_iterator ci_find_substr(t const & haystack,
    typename t::value_type const * needle, typename t::size_type size,
    std::locale const & loc = std::locale())
{
    return std::search( haystack.begin(), haystack.end(),
        needle, needle+size, my_equal<typename t::value_type>(loc) );
}

/// convert a string to lowercase
/**
 * @param [in] in the string to convert
 * @return the converted string
 */
std::string to_lower(std::string const & in);

/// replace all occurrances of a substring with another
/**
 * @param [in] subject the string to search in
 * @param [in] search the string to search for
 * @param [in] replace the string to replace with
 * @return a copy of `subject` with all occurances of `search` replaced with
 *         `replace`
 */
std::string string_replace_all(std::string subject, std::string const & search,
                               std::string const & replace);

/// convert std::string to ascii printed string of hex digits
/**
 * @param [in] input the string to print
 * @return a copy of `input` converted to the printable representation of the
 *         hex values of its data.
 */
std::string to_hex(std::string const & input);

/// convert byte array (uint8_t) to ascii printed string of hex digits
/**
 * @param [in] input the byte array to print
 * @param [in] length the length of input
 * @return a copy of `input` converted to the printable representation of the
 *         hex values of its data.
 */
std::string to_hex(uint8_t const * input, size_t length);

/// convert char array to ascii printed string of hex digits
/**
 * @param [in] input the char array to print
 * @param [in] length the length of input
 * @return a copy of `input` converted to the printable representation of the
 *         hex values of its data.
 */
std::string to_hex(char const * input, size_t length);

} // namespace utility
} // namespace websocketpp

#include <websocketpp/impl/utilities_impl.hpp>

#endif // websocketpp_utilities_hpp
