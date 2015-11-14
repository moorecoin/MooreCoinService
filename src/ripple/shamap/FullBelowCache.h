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

#ifndef ripple_shamap_fullbelowcache_h_included
#define ripple_shamap_fullbelowcache_h_included

#include <ripple/basics/base_uint.h>
#include <ripple/basics/keycache.h>
#include <beast/insight/collector.h>
#include <atomic>
#include <string>

namespace ripple {

namespace detail {

/** remembers which tree keys have all descendants resident.
    this optimizes the process of acquiring a complete tree.
*/
template <class key>
class basicfullbelowcache
{
private:
    typedef keycache <key> cachetype;

public:
    enum
    {
         defaultcachetargetsize = 0
        ,defaultcacheexpirationseconds = 120
    };

    typedef key key_type;
    typedef typename cachetype::size_type size_type;
    typedef typename cachetype::clock_type clock_type;

    /** construct the cache.

        @param name a label for diagnostics and stats reporting.
        @param collector the collector to use for reporting stats.
        @param targetsize the cache target size.
        @param targetexpirationseconds the expiration time for items.
    */
    basicfullbelowcache (std::string const& name, clock_type& clock,
        beast::insight::collector::ptr const& collector =
            beast::insight::nullcollector::new (),
        std::size_t target_size = defaultcachetargetsize,
        std::size_t expiration_seconds = defaultcacheexpirationseconds)
        : m_cache (name, clock, collector, target_size,
            expiration_seconds)
        , m_gen (1)
    {
    }

    /** return the clock associated with the cache. */
    clock_type& clock()
    {
        return m_cache.clock ();
    }

    /** return the number of elements in the cache.
        thread safety:
            safe to call from any thread.
    */
    size_type size () const
    {
        return m_cache.size ();
    }

    /** remove expired cache items.
        thread safety:
            safe to call from any thread.
    */
    void sweep ()
    {
        m_cache.sweep ();
    }

    /** refresh the last access time of an item, if it exists.
        thread safety:
            safe to call from any thread.
        @param key the key to refresh.
        @return `true` if the key exists.
    */
    bool touch_if_exists (key_type const& key)
    {
        return m_cache.touch_if_exists (key);
    }

    /** insert a key into the cache.
        if the key already exists, the last access time will still
        be refreshed.
        thread safety:
            safe to call from any thread.
        @param key the key to insert.
    */
    void insert (key_type const& key)
    {
        m_cache.insert (key);
    }

    /** generation determines whether cached entry is valid */
    std::uint32_t getgeneration (void) const
    {
        return m_gen;
    }

    void clear ()
    {
        m_cache.clear ();
        ++m_gen;
    }

private:
    keycache <key> m_cache;
    std::atomic <std::uint32_t> m_gen;
};

} // detail

using fullbelowcache = detail::basicfullbelowcache <uint256>;

}

#endif
