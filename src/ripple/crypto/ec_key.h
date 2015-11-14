//------------------------------------------------------------------------------
/*
    this file is part of rippled: https://github.com/ripple/rippled
    copyright (c) 2014 ripple labs inc.

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

#ifndef ripple_eckey_h
#define ripple_eckey_h

#include <ripple/basics/base_uint.h>
#include <cstdint>

namespace ripple {
namespace openssl {

class ec_key
{
public:
    typedef struct opaque_ec_key* pointer_t;

private:
    pointer_t ptr;

    void destroy();

    ec_key (pointer_t raw) : ptr(raw)
    {
    }

public:
    static const ec_key invalid;

    static ec_key acquire (pointer_t raw)  { return ec_key (raw); }

    //ec_key() : ptr() {}

    ec_key            (const ec_key&);
    ec_key& operator= (const ec_key&) = delete;

    ~ec_key()
    {
        destroy();
    }

    pointer_t get() const  { return ptr; }

    pointer_t release()
    {
        pointer_t released = ptr;

        ptr = nullptr;

        return released;
    }

    bool valid() const  { return ptr != nullptr; }

    uint256 get_private_key() const;

    static std::size_t get_public_key_max_size()  { return 33; }

    std::size_t get_public_key_size() const;

    std::uint8_t get_public_key (std::uint8_t* buffer) const;
};

} // openssl
} // ripple

#endif
