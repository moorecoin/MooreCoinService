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

#include <beastconfig.h>
#include <ripple/crypto/ec_key.h>
#include <openssl/ec.h>

namespace ripple  {
namespace openssl {

static inline ec_key* get_ec_key (const ec_key& that)
{
    return (ec_key*) that.get();
}

const ec_key ec_key::invalid = ec_key::acquire (nullptr);

ec_key::ec_key (const ec_key& that)
{
    if (that.ptr == nullptr)
    {
        ptr = nullptr;
        return;
    }

    ptr = (pointer_t) ec_key_dup (get_ec_key (that));

    if (ptr == nullptr)
    {
        throw std::runtime_error ("ec_key::ec_key() : ec_key_dup failed");
    }

    ec_key_set_conv_form (get_ec_key (*this), point_conversion_compressed);
}

void ec_key::destroy()
{
    if (ptr != nullptr)
    {
        ec_key_free (get_ec_key (*this));
        ptr = nullptr;
    }
}

uint256 ec_key::get_private_key() const
{
    uint256 result;
    result.zero();

    if (valid())
    {
        const bignum* bn = ec_key_get0_private_key (get_ec_key (*this));

        if (bn == nullptr)
        {
            throw std::runtime_error ("ec_key::get_private_key: ec_key_get0_private_key failed");
        }

        bn_bn2bin (bn, result.end() - bn_num_bytes (bn));
    }

    return result;
}

std::size_t ec_key::get_public_key_size() const
{
    int const size = i2o_ecpublickey (get_ec_key (*this), nullptr);

    if (size == 0)
    {
        throw std::runtime_error ("ec_key::get_public_key_size() : i2o_ecpublickey failed");
    }

    if (size > get_public_key_max_size())
    {
        throw std::runtime_error ("ec_key::get_public_key_size() : i2o_ecpublickey() result too big");
    }

    return size;
}

std::uint8_t ec_key::get_public_key (std::uint8_t* buffer) const
{
    std::uint8_t* begin = buffer;

    int const size = i2o_ecpublickey (get_ec_key (*this), &begin);

    assert (size == get_public_key_size());

    return std::uint8_t (size);
}

} // openssl
} // ripple
