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
#include <ripple/crypto/randomnumbers.h>
#include <openssl/rand.h>
#include <cassert>
#include <random>
#include <stdexcept>

namespace ripple {

void add_entropy (void* buffer, int count)
{
    assert (buffer == nullptr || count != 0);

    // if we are passed data in we use it but conservatively estimate that it
    // contains only around 2 bits of entropy per byte.
    if (buffer != nullptr && count != 0)
        rand_add (buffer, count, count / 4.0);

    // try to add a bit more entropy from the system
    unsigned int rdbuf[32];

    std::random_device rd;

    for (auto& x : rdbuf)
        x = rd ();

    // in all our supported platforms, std::random_device is non-deterministic
    // but we conservatively estimate it has around 4 bits of entropy per byte.
    rand_add (rdbuf, sizeof (rdbuf), sizeof (rdbuf) / 2.0);
}

void random_fill (void* buffer, int count)
{
    assert (count > 0);

    if (rand_bytes (reinterpret_cast <unsigned char*> (buffer), count) != 1)
        throw std::runtime_error ("insufficient entropy in pool.");
}

}
