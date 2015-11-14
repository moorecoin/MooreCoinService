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

#ifndef beast_container_aged_unordered_multiset_h_included
#define beast_container_aged_unordered_multiset_h_included

#include <beast/container/detail/aged_unordered_container.h>

#include <chrono>
#include <functional>
#include <memory>

namespace beast {

template <
    class key,
    class clock = std::chrono::steady_clock,
    class hash = std::hash <key>,
    class keyequal = std::equal_to <key>,
    class allocator = std::allocator <key>
>
using aged_unordered_multiset = detail::aged_unordered_container <
    true, false, key, void, clock, hash, keyequal, allocator>;

}

#endif
