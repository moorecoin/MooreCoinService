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

#ifndef ripple_crypto_randomnumbers_h_included
#define ripple_crypto_randomnumbers_h_included

#include <beast/cxx14/type_traits.h> // <type_traits>

namespace ripple {

/** adds entropy to the rng pool.

    @param buffer an optional buffer that contains random data.
    @param count the size of the buffer, in bytes (or 0).

    this can be called multiple times to stir entropy into the pool
    without any locks.
*/
void add_entropy (void* buffer = nullptr, int count = 0);

/** generate random bytes, suitable for cryptography. */
/**@{*/
/**
    @param buffer the place to store the bytes.
    @param count the number of bytes to generate.
*/
void random_fill (void* buffer, int count);

/** fills a pod object with random data */
template <class t, class = std::enable_if_t<std::is_pod<t>::value>>
void
random_fill (t* object)
{
    random_fill (object, sizeof (t));
}
/**@}*/

}

#endif
