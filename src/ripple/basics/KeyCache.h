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

#ifndef ripple_basics_keycache_h_included
#define ripple_basics_keycache_h_included

#include <ripple/basics/hardened_hash.h>
#include <ripple/basics/unorderedcontainers.h>
#include <beast/chrono/abstract_clock.h>
#include <beast/chrono/chrono_io.h>
#include <beast/insight.h>
#include <mutex>

namespace ripple {

/** maintains a cache of keys with no associated data.

    the cache has a target size and an expiration time. when cached items become
    older than the maximum age they are eligible for removal during a
    call to @ref sweep.
*/
// vfalco todo figure out how to pass through the allocator
template <
    class key,
    class hash = hardened_hash <>,
    class keyequal = std::equal_to <key>,
    //class allocator = std::allocator <std::pair <key const, entry>>,
    class mutex = std::mutex
>
class keycache
{
public:
    typedef key key_type;
    typedef beast::abstract_clock <std::chrono::steady_clock> clock_type;

private:
    struct stats
    {
        template <class handler>
        stats (std::string const& prefix, handler const& handler,
            beast::insight::collector::ptr const& collector)
            : hook (collector->make_hook (handler))
            , size (collector->make_gauge (prefix, "size"))
            , hit_rate (collector->make_gauge (prefix, "hit_rate"))
            , hits (0)
            , misses (0)
            { }

        beast::insight::hook hook;
        beast::insight::gauge size;
        beast::insight::gauge hit_rate;

        std::size_t hits;
        std::size_t misses;
    };

    struct entry
    {
        explicit entry (clock_type::time_point const& last_access_)
            : last_access (last_access_)
        {
        }

        clock_type::time_point last_access;
    };

    typedef hardened_hash_map <key_type, entry, hash, keyequal> map_type;
    typedef typename map_type::iterator iterator;
    typedef std::lock_guard <mutex> lock_guard;

public:
    typedef typename map_type::size_type size_type;

private:
    mutex mutable m_mutex;
    map_type m_map;
    stats mutable m_stats;
    clock_type& m_clock;
    std::string const m_name;
    size_type m_target_size;
    clock_type::duration m_target_age;

public:
    /** construct with the specified name.

        @param size the initial target size.
        @param age  the initial expiration time.
    */
    keycache (std::string const& name, clock_type& clock,
        beast::insight::collector::ptr const& collector, size_type target_size = 0,
            clock_type::rep expiration_seconds = 120)
        : m_stats (name,
            std::bind (&keycache::collect_metrics, this),
                collector)
        , m_clock (clock)
        , m_name (name)
        , m_target_size (target_size)
        , m_target_age (std::chrono::seconds (expiration_seconds))
    {
    }

    // vfalco todo use a forwarding constructor call here
    keycache (std::string const& name, clock_type& clock,
        size_type target_size = 0, clock_type::rep expiration_seconds = 120)
        : m_stats (name,
            std::bind (&keycache::collect_metrics, this),
                beast::insight::nullcollector::new ())
        , m_clock (clock)
        , m_name (name)
        , m_target_size (target_size)
        , m_target_age (std::chrono::seconds (expiration_seconds))
    {
    }

    //--------------------------------------------------------------------------

    /** retrieve the name of this object. */
    std::string const& name () const
    {
        return m_name;
    }

    /** return the clock associated with the cache. */
    clock_type& clock ()
    {
        return m_clock;
    }

    /** returns the number of items in the container. */
    size_type size () const
    {
        lock_guard lock (m_mutex);
        return m_map.size ();
    }

    /** empty the cache */
    void clear ()
    {
        lock_guard lock (m_mutex);
        m_map.clear ();
    }

    void settargetsize (size_type s)
    {
        lock_guard lock (m_mutex);
        m_target_size = s;
    }

    void settargetage (size_type s)
    {
        lock_guard lock (m_mutex);
        m_target_age = std::chrono::seconds (s);
    }

    /** returns `true` if the key was found.
        does not update the last access time.
    */
    template <class keycomparable>
    bool exists (keycomparable const& key) const
    {
        lock_guard lock (m_mutex);
        typename map_type::const_iterator const iter (m_map.find (key));
        if (iter != m_map.end ())
        {
            ++m_stats.hits;
            return true;
        }
        ++m_stats.misses;
        return false;
    }

    /** insert the specified key.
        the last access time is refreshed in all cases.
        @return `true` if the key was newly inserted.
    */
    bool insert (key const& key)
    {
        lock_guard lock (m_mutex);
        clock_type::time_point const now (m_clock.now ());
        std::pair <iterator, bool> result (m_map.emplace (
            std::piecewise_construct, std::forward_as_tuple (key),
                std::forward_as_tuple (now)));
        if (! result.second)
        {
            result.first->second.last_access = now;
            return false;
        }
        return true;
    }

    /** refresh the last access time on a key if present.
        @return `true` if the key was found.
    */
    template <class keycomparable>
    bool touch_if_exists (keycomparable const& key)
    {
        lock_guard lock (m_mutex);
        iterator const iter (m_map.find (key));
        if (iter == m_map.end ())
        {
            ++m_stats.misses;
            return false;
        }
        iter->second.last_access = m_clock.now ();
        ++m_stats.hits;
        return true;
    }

    /** remove the specified cache entry.
        @param key the key to remove.
        @return `false` if the key was not found.
    */
    bool erase (key_type const& key)
    {
        lock_guard lock (m_mutex);
        if (m_map.erase (key) > 0)
        {
            ++m_stats.hits;
            return true;
        }
        ++m_stats.misses;
        return false;
    }

    /** remove stale entries from the cache. */
    void sweep ()
    {
        clock_type::time_point const now (m_clock.now ());
        clock_type::time_point when_expire;

        lock_guard lock (m_mutex);

        if (m_target_size == 0 ||
            (m_map.size () <= m_target_size))
        {
            when_expire = now - m_target_age;
        }
        else
        {
            when_expire = now - clock_type::duration (
                m_target_age.count() * m_target_size / m_map.size ());

            clock_type::duration const minimumage (
                std::chrono::seconds (1));
            if (when_expire > (now - minimumage))
                when_expire = now - minimumage;
        }

        iterator it = m_map.begin ();

        while (it != m_map.end ())
        {
            if (it->second.last_access > now)
            {
                it->second.last_access = now;
                ++it;
            }
            else if (it->second.last_access <= when_expire)
            {
                it = m_map.erase (it);
            }
            else
            {
                ++it;
            }
        }
    }

private:
    void collect_metrics ()
    {
        m_stats.size.set (size ());

        {
            beast::insight::gauge::value_type hit_rate (0);
            {
                lock_guard lock (m_mutex);
                auto const total (m_stats.hits + m_stats.misses);
                if (total != 0)
                    hit_rate = (m_stats.hits * 100) / total;
            }
            m_stats.hit_rate.set (hit_rate);
        }
    }
};

}

#endif
