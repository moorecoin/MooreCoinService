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

// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2011 the bitcoin developers
// distributed under the mit/x11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.
//
// why base-58 instead of standard base-64 encoding?
// - don't want 0oil characters that look the same in some fonts and
//      could be used to create visually identical looking account numbers.
// - a string with non-alphanumeric characters is not as easily accepted as an account number.
// - e-mail usually won't line-break if there's no punctuation to break at.
// - doubleclicking selects the whole number as one word if it's all alphanumeric.
//
#ifndef ripple_crypto_base58_h
#define ripple_crypto_base58_h

#include <ripple/basics/blob.h>
#include <array>
#include <cassert>
#include <iterator>
#include <string>
#include <type_traits>
#include <vector>

namespace ripple {

/** performs base 58 encoding and decoding. */
class base58
{
public:
    class alphabet
    {
    public:
        // chars may not contain high-ascii values
        explicit alphabet (char const* chars)
            : m_chars (chars)
        {
            assert (m_inverse.size () >= m_chars.length ());

            m_inverse.fill (-1);

            int i = 0;
            for (auto c : m_chars)
            {
                assert ((c >= 0) && (c < m_inverse.size ()));
                assert (m_inverse[c] == -1);

                m_inverse[c] = i++;
            }
        }

        char const* chars () const
            { return m_chars.c_str (); }

        char to_char (int digit) const
            { return m_chars [digit]; }

        char operator[] (int digit) const
            { return to_char (digit); }

        int from_char (char c) const
            { return m_inverse [c]; }

    private:
        std::string const m_chars;
        std::array <int, 256> m_inverse;
    };

    static alphabet const& getbitcoinalphabet ();
    static alphabet const& getripplealphabet ();

    static std::string raw_encode (unsigned char const* begin,
        unsigned char const* end, alphabet const& alphabet);

    static void fourbyte_hash256 (void* out, void const* in, std::size_t bytes);

    template <class inputit>
    static std::string encode (inputit first, inputit last,
        alphabet const& alphabet, bool withcheck)
    {
        typedef typename std::iterator_traits<inputit>::value_type value_type;
        std::vector <typename std::remove_const <value_type>::type> v;
        std::size_t const size (std::distance (first, last));
        if (withcheck)
        {
            v.reserve (size + 1 + 4);
            v.insert (v.begin(), first, last);
            unsigned char hash [4];
            fourbyte_hash256 (hash, &v.front(), v.size());
            v.resize (0);
            // place the hash reversed in the front
            std::copy (std::reverse_iterator <unsigned char const*> (hash+4),
                       std::reverse_iterator <unsigned char const*> (hash),
                       std::back_inserter (v));
        }
        else
        {
            v.reserve (size + 1);
        }
        // append input little endian
        std::copy (std::reverse_iterator <inputit> (last),
                   std::reverse_iterator <inputit> (first),
                   std::back_inserter (v));
        // pad zero to make the bignum positive
        v.push_back (0);
        return raw_encode (&v.front(), &v.back()+1, alphabet);
    }

    template <class container>
    static std::string encode (container const& container)
    {
        return encode (container.container.begin(), container.end(),
            getripplealphabet(), false);
    }

    template <class container>
    static std::string encodewithcheck (container const& container)
    {
        return encode (&container.front(), &container.back()+1,
            getripplealphabet(), true);
    }

    static std::string encode (const unsigned char* pbegin, const unsigned char* pend)
    {
        return encode (pbegin, pend, getripplealphabet(), false);
    }

    //--------------------------------------------------------------------------

    // raw decoder leaves the check bytes in place if present

    static bool raw_decode (char const* first, char const* last,
        void* dest, std::size_t size, bool checked, alphabet const& alphabet);

    static bool decode (const char* psz, blob& vchret, alphabet const& alphabet = getripplealphabet ());
    static bool decode (std::string const& str, blob& vchret);
    static bool decodewithcheck (const char* psz, blob& vchret, alphabet const& alphabet = getripplealphabet());
    static bool decodewithcheck (std::string const& str, blob& vchret, alphabet const& alphabet = getripplealphabet());
};

}

#endif
