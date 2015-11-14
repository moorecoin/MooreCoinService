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

#ifndef ripple_basics_unordered_containers_h_included
#define ripple_basics_unordered_containers_h_included

#include <ripple/basics/hardened_hash.h>
#include <beast/hash/hash_append.h>
#include <beast/hash/uhash.h>
#include <beast/hash/xxhasher.h>
#include <unordered_map>
#include <unordered_set>

/**
* use hash_* containers for keys that do not need a cryptographically secure
* hashing algorithm.
*
* use hardened_hash_* containers for keys that do need a secure hashing algorithm.
*
* the cryptographic security of containers where a hash function is used as a
* template parameter depends entirely on that hash function and not at all on
* what container it is.
*/

namespace ripple
{

// hash containers

template <class key, class value, class hash = beast::uhash<>,
          class pred = std::equal_to<key>,
          class allocator = std::allocator<std::pair<key const, value>>>
using hash_map = std::unordered_map <key, value, hash, pred, allocator>;

template <class key, class value, class hash = beast::uhash<>,
          class pred = std::equal_to<key>,
          class allocator = std::allocator<std::pair<key const, value>>>
using hash_multimap = std::unordered_multimap <key, value, hash, pred, allocator>;

template <class value, class hash = beast::uhash<>,
          class pred = std::equal_to<value>,
          class allocator = std::allocator<value>>
using hash_set = std::unordered_set <value, hash, pred, allocator>;

template <class value, class hash = beast::uhash<>,
          class pred = std::equal_to<value>,
          class allocator = std::allocator<value>>
using hash_multiset = std::unordered_multiset <value, hash, pred, allocator>;

// hardened_hash containers

using strong_hash = beast::xxhasher;

template <class key, class value, class hash = hardened_hash<strong_hash>,
          class pred = std::equal_to<key>,
          class allocator = std::allocator<std::pair<key const, value>>>
using hardened_hash_map = std::unordered_map <key, value, hash, pred, allocator>;

template <class key, class value, class hash = hardened_hash<strong_hash>,
          class pred = std::equal_to<key>,
          class allocator = std::allocator<std::pair<key const, value>>>
using hardened_hash_multimap = std::unordered_multimap <key, value, hash, pred, allocator>;

template <class value, class hash = hardened_hash<strong_hash>,
          class pred = std::equal_to<value>,
          class allocator = std::allocator<value>>
using hardened_hash_set = std::unordered_set <value, hash, pred, allocator>;

template <class value, class hash = hardened_hash<strong_hash>,
          class pred = std::equal_to<value>,
          class allocator = std::allocator<value>>
using hardened_hash_multiset = std::unordered_multiset <value, hash, pred, allocator>;

} // ripple

#endif
