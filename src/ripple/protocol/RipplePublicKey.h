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

#ifndef ripple_protocol_ripplepublickey_h_included
#define ripple_protocol_ripplepublickey_h_included

#include <ripple/crypto/base58.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <iterator>
#include <ostream>
#include <stdexcept>
#include <beast/cxx14/type_traits.h> // <type_traits>

namespace ripple {

// simplified public key that avoids the complexities of rippleaddress
class ripplepublickey
{
private:
    std::array<std::uint8_t, 33> data_;

public:
    /** construct from a range of unsigned char. */
    template <class fwdit, class = std::enable_if_t<std::is_same<unsigned char,
        typename std::iterator_traits<fwdit>::value_type>::value>>
    ripplepublickey (fwdit first, fwdit last);

    template <class = void>
    std::string
    to_string() const;

    friend
    bool
    operator< (ripplepublickey const& lhs, ripplepublickey const& rhs)
    {
        return lhs.data_ < rhs.data_;
    }

    friend
    bool
    operator== (ripplepublickey const& lhs, ripplepublickey const& rhs)
    {
        return lhs.data_ == rhs.data_;
    }
};

template <class fwdit, class>
ripplepublickey::ripplepublickey (fwdit first, fwdit last)
{
    assert(std::distance(first, last) == data_.size());
    // vfalco todo use 4-arg copy from c++14
    std::copy (first, last, data_.begin());
}

template <class>
std::string
ripplepublickey::to_string() const
{
    // the expanded form of the key is:
    //  <type> <key> <checksum>
    std::array <std::uint8_t, 1 + 33 + 4> e;
    e[0] = 28; // node public key type
    std::copy (data_.begin(), data_.end(), e.begin() + 1);
    base58::fourbyte_hash256 (&*(e.begin() + 34), e.data(), 34);
    // convert key + checksum to little endian with an extra pad byte
    std::array <std::uint8_t, 4 + 33 + 1 + 1> le;
    std::reverse_copy (e.begin(), e.end(), le.begin());
    le.back() = 0; // make bignum positive
    return base58::raw_encode (le.data(),
        le.data() + le.size(), base58::getripplealphabet());
}

inline
std::ostream&
operator<< (std::ostream& os, ripplepublickey const& k)
{
    return os << k.to_string();
}

}

#endif
