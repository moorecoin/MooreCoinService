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

#ifndef ripple_protocol_uint160_h_included
#define ripple_protocol_uint160_h_included

#include <ripple/basics/base_uint.h>
#include <ripple/basics/strhex.h>
#include <boost/functional/hash.hpp>
#include <functional>

namespace ripple {

template <class tag>
uint256 to256 (base_uint<160, tag> const& a)
{
    uint256 m;
    memcpy (m.begin (), a.begin (), a.size ());
    return m;
}

}

//------------------------------------------------------------------------------

namespace std {

template <>
struct hash <ripple::uint160> : ripple::uint160::hasher
{
    // vfalco note broken in vs2012
    //using ripple::uint160::hasher::hasher; // inherit ctors
};

}

//------------------------------------------------------------------------------

namespace boost {

template <>
struct hash <ripple::uint160> : ripple::uint160::hasher
{
    // vfalco note broken in vs2012
    //using ripple::uint160::hasher::hasher; // inherit ctors
};

}

#endif
