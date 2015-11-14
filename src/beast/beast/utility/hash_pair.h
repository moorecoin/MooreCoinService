//------------------------------------------------------------------------------
/*
    this file is part of beast: https://github.com/vinniefalco/beast
    copyright 2013, vinnie falco <vinnie.falco@gmail.com>

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

#ifndef beast_utility_hash_pair_h_included
#define beast_utility_hash_pair_h_included

#include <functional>
#include <utility>

#include <boost/functional/hash.hpp>
#include <boost/utility/base_from_member.hpp>

namespace std {

/** specialization of std::hash for any std::pair type. */
template <class first, class second>
struct hash <std::pair <first, second>>
    : private boost::base_from_member <std::hash <first>, 0>
    , private boost::base_from_member <std::hash <second>, 1>
{
private:
    typedef boost::base_from_member <std::hash <first>, 0> first_hash;
    typedef boost::base_from_member <std::hash <second>, 1> second_hash;

public:
    hash ()
    {
    }

    hash (std::hash <first> const& first_hash_,
          std::hash <second> const& second_hash_)
          : first_hash (first_hash_)
          , second_hash (second_hash_)
    {
    }

    std::size_t operator() (std::pair <first, second> const& value)
    {
        std::size_t result (first_hash::member (value.first));
        boost::hash_combine (result, second_hash::member (value.second));
        return result;
    }

    std::size_t operator() (std::pair <first, second> const& value) const
    {
        std::size_t result (first_hash::member (value.first));
        boost::hash_combine (result, second_hash::member (value.second));
        return result;
    }
};

}

#endif
