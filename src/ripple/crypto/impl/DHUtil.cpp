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

#include <beastconfig.h>
#include <ripple/crypto/dhutil.h>

namespace ripple {

std::string dh_der_gen (int ikeylength)
{
    dh*         dh  = 0;
    int         icodes;
    std::string strder;

    do
    {
        dh  = dh_generate_parameters (ikeylength, dh_generator_5, nullptr, nullptr);
        icodes  = 0;
        dh_check (dh, &icodes);
    }
    while (icodes & (dh_check_p_not_prime | dh_check_p_not_safe_prime | dh_unable_to_check_generator | dh_not_suitable_generator));

    strder.resize (i2d_dhparams (dh, nullptr));

    unsigned char* next = reinterpret_cast<unsigned char*> (&strder[0]);

    (void) i2d_dhparams (dh, &next);

    return strder;
}

dh* dh_der_load (std::string const& strder)
{
    const unsigned char* pbuf   = reinterpret_cast<const unsigned char*> (&strder[0]);

    return d2i_dhparams (nullptr, &pbuf, strder.size ());
}

}
