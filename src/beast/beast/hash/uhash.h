//------------------------------------------------------------------------------
/*
    this file is part of beast: https://github.com/vinniefalco/beast
    copyright 2014, howard hinnant <howard.hinnant@gmail.com>,
        vinnie falco <vinnie.falco@gmail.com

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

#ifndef beast_hash_uhash_h_included
#define beast_hash_uhash_h_included

#include <beast/hash/spooky.h>

namespace beast {

// universal hash function
template <class hasher = spooky>
struct uhash
{
    using result_type = typename hasher::result_type;

    template <class t>
    result_type
    operator()(t const& t) const noexcept
    {
        hasher h;
        hash_append (h, t);
        return static_cast<result_type>(h);
    }
};

} // beast

#endif
