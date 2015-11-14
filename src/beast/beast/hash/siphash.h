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

#ifndef beast_hash_siphash_h_included
#define beast_hash_siphash_h_included

#include <beast/utility/noexcept.h>
#include <cstddef>
#include <cstdint>

namespace beast {

// see https://131002.net/siphash/
class siphash
{
private:
    std::uint64_t v0_ = 0x736f6d6570736575ull;
    std::uint64_t v1_ = 0x646f72616e646f6dull;
    std::uint64_t v2_ = 0x6c7967656e657261ull;
    std::uint64_t v3_ = 0x7465646279746573ull;
    unsigned char buf_[8];
    unsigned bufsize_ = 0;
    unsigned total_length_ = 0;

public:
    using result_type = std::size_t;

    siphash() = default;

    explicit
    siphash (std::uint64_t k0, std::uint64_t k1 = 0) noexcept;

    void
    append (void const* key, std::size_t len) noexcept;

    explicit
    operator std::size_t() noexcept;
};

} // beast

#endif
