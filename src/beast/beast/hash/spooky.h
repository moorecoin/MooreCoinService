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

#ifndef beast_hash_spooky_h_included
#define beast_hash_spooky_h_included

#include <beast/hash/impl/spookyv2.h>

namespace beast {

// see http://burtleburtle.net/bob/hash/spooky.html
class spooky
{
private:
    spookyhash state_;

public:
    using result_type = std::size_t;

    spooky (std::size_t seed1 = 1, std::size_t seed2 = 2) noexcept
    {
        state_.init (seed1, seed2);
    }

    void
    append (void const* key, std::size_t len) noexcept
    {
        state_.update (key, len);
    }

    explicit
    operator std::size_t() noexcept
    {
        std::uint64_t h1, h2;
        state_.final (&h1, &h2);
        return static_cast <std::size_t> (h1);
    }
};

} // beast

#endif
