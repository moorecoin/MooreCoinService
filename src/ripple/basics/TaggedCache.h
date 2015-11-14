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

#ifndef ripple_basics_taggedcache_h_included
#define ripple_basics_taggedcache_h_included

#include <ripple/basics/hardened_hash.h>
#include <ripple/basics/unorderedcontainers.h>
#include <beast/chrono/abstract_clock.h>
#include <beast/chrono/chrono_io.h>
#include <beast/insight.h>
#include <functional>
#include <mutex>
#include <vector>

namespace ripple {

// vfalco note deprecated
struct taggedcachelog;

/** map/cache combination.
    this class implements a cache and a map. the cache keeps objects alive
    in the map. the map allows multiple code paths that reference objects
    with the same tag to get the same actual object.

    so long as data is in the cache, it will stay in memory.
    if it stays in memory even after it is ejected from the cache,
    the map will track it.

    @note callers must not modify data objects that are stored in the cache
          unless they hold their own lock over all cache operations.
*/
// vfalco todo figure out how to pass through the allocator
template <
    class key,
    class t,
    class hash = hardened_hash <>,
    class keyequal = std::equal_to <key>,
    //class allocator = std::allocator <std::pair <key const, entry>>,
    class mutex = std::recursive_mutex
>
class taggedcache
{
public:
    typedef mutex mutex_type;
    // vfalco deprecated the caller can just use std::unique_lock <type>
    typedef std::unique_lock <mutex_type> scopedlocktype;
    typedef std::lock_guard <mutex_type> lock_guard;
    typedef key key_type;
    typedef t mapped_type;
    // vfalco todo use std::shared_ptr, std::weak_ptr
    typedef std::weak_ptr <mapped_type> weak_mapped_ptr;
    typedef std::shared_ptr <mapped_type> mapped_ptr;
    typedef beast::abstract_clock <std::chrono::steady_clock> clock_type;

public:
    // vfalco todo change expiration_seconds to clock_type::duration
    taggedcache (std::string const& name, int size,
        clock_type::rep expiration_seconds, clock_type& clock, beast::journal journal,
            beast::insight::collector::ptr const& collector = beast::insight::nullcollector::new ())
        : m_journal (journal)
        , m_clock (clock)
        , m_stats (name,
            std::bind (&taggedcache::collect_metrics, this),
                collector)
        , m_name (name)
        , m_target_size (size)
        , m_target_age (std::chrono::seconds (expiration_seconds))
        , m_cache_count (0)
        , m_hits (0)
        , m_misses (0)
    {
    }

public:
    /** return the clock associated with the cache. */
    clock_type& clock ()
    {
        return m_clock;
    }

    int gettargetsize () const
    {
        lock_guard lock (m_mutex);
        return m_target_size;
    }

    void settargetsize (int s)
    {
        lock_guard lock (m_mutex);
        m_target_size = s;

        if (s > 0)
            m_cache.rehash (static_cast<std::size_t> ((s + (s >> 2)) / m_cache.max_load_factor () + 1));

        if (m_journal.debug) m_journal.debug <<
            m_name << " target size set to " << s;
    }

    clock_type::rep gettargetage () const
    {
        lock_guard lock (m_mutex);
        return m_target_age.count();
    }

    void settargetage (clock_type::rep s)
    {
        lock_guard lock (m_mutex);
        m_target_age = std::chrono::seconds (s);
        if (m_journal.debug) m_journal.debug <<
            m_name << " target age set to " << m_target_age;
    }

    int getcachesize ()
    {
        lock_guard lock (m_mutex);
        return m_cache_count;
    }

    int gettracksize ()
    {
        lock_guard lock (m_mutex);
        return m_cache.size ();
    }

    float gethitrate ()
    {
        lock_guard lock (m_mutex);
        auto const total = static_cast<float> (m_hits + m_misses);
        return m_hits * (100.0f / std::max (1.0f, total));
    }

    void clearstats ()
    {
        lock_guard lock (m_mutex);
        m_hits = 0;
        m_misses = 0;
    }

    void clear ()
    {
        lock_guard lock (m_mutex);
        m_cache.clear ();
        m_cache_count = 0;
    }

    void sweep ()
    {
        int cacheremovals = 0;
        int mapremovals = 0;
        int cc = 0;

        // keep references to all the stuff we sweep
        // so that we can destroy them outside the lock.
        //
        std::vector <mapped_ptr> stufftosweep;

        {
            clock_type::time_point const now (m_clock.now());
            clock_type::time_point when_expire;

            lock_guard lock (m_mutex);

            if (m_target_size == 0 ||
                (static_cast<int> (m_cache.size ()) <= m_target_size))
            {
                when_expire = now - m_target_age;
            }
            else
            {
                when_expire = now - clock_type::duration (
                    m_target_age.count() * m_target_size / m_cache.size ());

                clock_type::duration const minimumage (
                    std::chrono::seconds (1));
                if (when_expire > (now - minimumage))
                    when_expire = now - minimumage;

                if (m_journal.trace) m_journal.trace <<
                    m_name << " is growing fast " << m_cache.size () << " of " << m_target_size <<
                        " aging at " << (now - when_expire) << " of " << m_target_age;
            }

            stufftosweep.reserve (m_cache.size ());

            cache_iterator cit = m_cache.begin ();

            while (cit != m_cache.end ())
            {
                if (cit->second.isweak ())
                {
                    // weak
                    if (cit->second.isexpired ())
                    {
                        ++mapremovals;
                        cit = m_cache.erase (cit);
                    }
                    else
                    {
                        ++cit;
                    }
                }
                else if (cit->second.last_access <= when_expire)
                {
                    // strong, expired
                    --m_cache_count;
                    ++cacheremovals;
                    if (cit->second.ptr.unique ())
                    {
                        stufftosweep.push_back (cit->second.ptr);
                        ++mapremovals;
                        cit = m_cache.erase (cit);
                    }
                    else
                    {
                        // remains weakly cached
                        cit->second.ptr.reset ();
                        ++cit;
                    }
                }
                else
                {
                    // strong, not expired
                    ++cc;
                    ++cit;
                }
            }
        }

        if (m_journal.trace && (mapremovals || cacheremovals)) m_journal.trace <<
            m_name << ": cache = " << m_cache.size () << "-" << cacheremovals <<
                ", map-=" << mapremovals;

        // at this point stufftosweep will go out of scope outside the lock
        // and decrement the reference count on each strong pointer.
    }

    bool del (const key_type& key, bool valid)
    {
        // remove from cache, if !valid, remove from map too. returns true if removed from cache
        lock_guard lock (m_mutex);

        cache_iterator cit = m_cache.find (key);

        if (cit == m_cache.end ())
            return false;

        entry& entry = cit->second;

        bool ret = false;

        if (entry.iscached ())
        {
            --m_cache_count;
            entry.ptr.reset ();
            ret = true;
        }

        if (!valid || entry.isexpired ())
            m_cache.erase (cit);

        return ret;
    }

    /** replace aliased objects with originals.

        due to concurrency it is possible for two separate objects with
        the same content and referring to the same unique "thing" to exist.
        this routine eliminates the duplicate and performs a replacement
        on the callers shared pointer if needed.

        @param key the key corresponding to the object
        @param data a shared pointer to the data corresponding to the object.
        @param replace `true` if `data` is the up to date version of the object.

        @return `true` if the key already existed.
    */
    bool canonicalize (const key_type& key, std::shared_ptr<t>& data, bool replace = false)
    {
        // return canonical value, store if needed, refresh in cache
        // return values: true=we had the data already
        lock_guard lock (m_mutex);

        cache_iterator cit = m_cache.find (key);

        if (cit == m_cache.end ())
        {
            m_cache.emplace (std::piecewise_construct,
                std::forward_as_tuple(key),
                std::forward_as_tuple(m_clock.now(), data));
            ++m_cache_count;
            return false;
        }

        entry& entry = cit->second;
        entry.touch (m_clock.now());

        if (entry.iscached ())
        {
            if (replace)
            {
                entry.ptr = data;
                entry.weak_ptr = data;
            }
            else
            {
                data = entry.ptr;
            }

            return true;
        }

        mapped_ptr cacheddata = entry.lock ();

        if (cacheddata)
        {
            if (replace)
            {
                entry.ptr = data;
                entry.weak_ptr = data;
            }
            else
            {
                entry.ptr = cacheddata;
                data = cacheddata;
            }

            ++m_cache_count;
            return true;
        }

        entry.ptr = data;
        entry.weak_ptr = data;
        ++m_cache_count;

        return false;
    }

    std::shared_ptr<t> fetch (const key_type& key)
    {
        // fetch us a shared pointer to the stored data object
        lock_guard lock (m_mutex);

        cache_iterator cit = m_cache.find (key);

        if (cit == m_cache.end ())
        {
            ++m_misses;
            return mapped_ptr ();
        }

        entry& entry = cit->second;
        entry.touch (m_clock.now());

        if (entry.iscached ())
        {
            ++m_hits;
            return entry.ptr;
        }

        entry.ptr = entry.lock ();

        if (entry.iscached ())
        {
            // independent of cache size, so not counted as a hit
            ++m_cache_count;
            return entry.ptr;
        }

        m_cache.erase (cit);
        ++m_misses;
        return mapped_ptr ();
    }

    /** insert the element into the container.
        if the key already exists, nothing happens.
        @return `true` if the element was inserted
    */
    bool insert (key_type const& key, t const& value)
    {
        mapped_ptr p (std::make_shared <t> (
            std::cref (value)));
        return canonicalize (key, p);
    }

    // vfalco note it looks like this returns a copy of the data in
    //             the output parameter 'data'. this could be expensive.
    //             perhaps it should work like standard containers, which
    //             simply return an iterator.
    //
    bool retrieve (const key_type& key, t& data)
    {
        // retrieve the value of the stored data
        mapped_ptr entry = fetch (key);

        if (!entry)
            return false;

        data = *entry;
        return true;
    }

    /** refresh the expiration time on a key.

        @param key the key to refresh.
        @return `true` if the key was found and the object is cached.
    */
    bool refreshifpresent (const key_type& key)
    {
        bool found = false;

        // if present, make current in cache
        lock_guard lock (m_mutex);

        cache_iterator cit = m_cache.find (key);

        if (cit != m_cache.end ())
        {
            entry& entry = cit->second;

            if (! entry.iscached ())
            {
                // convert weak to strong.
                entry.ptr = entry.lock ();

                if (entry.iscached ())
                {
                    // we just put the object back in cache
                    ++m_cache_count;
                    entry.touch (m_clock.now());
                    found = true;
                }
                else
                {
                    // couldn't get strong pointer,
                    // object fell out of the cache so remove the entry.
                    m_cache.erase (cit);
                }
            }
            else
            {
                // it's cached so update the timer
                entry.touch (m_clock.now());
                found = true;
            }
        }
        else
        {
            // not present
        }

        return found;
    }

    mutex_type& peekmutex ()
    {
        return m_mutex;
    }

    std::vector <key_type> getkeys ()
    {
        std::vector <key_type> v;

        {
            lock_guard lock (m_mutex);
            v.reserve (m_cache.size());
            for (auto const& _ : m_cache)
                v.push_back (_.first);
        }

        return v;
    }

private:
    void collect_metrics ()
    {
        m_stats.size.set (getcachesize ());

        {
            beast::insight::gauge::value_type hit_rate (0);
            {
                lock_guard lock (m_mutex);
                auto const total (m_hits + m_misses);
                if (total != 0)
                    hit_rate = (m_hits * 100) / total;
            }
            m_stats.hit_rate.set (hit_rate);
        }
    }

private:
    struct stats
    {
        template <class handler>
        stats (std::string const& prefix, handler const& handler,
            beast::insight::collector::ptr const& collector)
            : hook (collector->make_hook (handler))
            , size (collector->make_gauge (prefix, "size"))
            , hit_rate (collector->make_gauge (prefix, "hit_rate"))
            { }

        beast::insight::hook hook;
        beast::insight::gauge size;
        beast::insight::gauge hit_rate;
    };

    class entry
    {
    public:
        mapped_ptr ptr;
        weak_mapped_ptr weak_ptr;
        clock_type::time_point last_access;

        entry (clock_type::time_point const& last_access_,
            mapped_ptr const& ptr_)
            : ptr (ptr_)
            , weak_ptr (ptr_)
            , last_access (last_access_)
        {
        }

        bool isweak () const { return ptr == nullptr; }
        bool iscached () const { return ptr != nullptr; }
        bool isexpired () const { return weak_ptr.expired (); }
        mapped_ptr lock () { return weak_ptr.lock (); }
        void touch (clock_type::time_point const& now) { last_access = now; }
    };

    typedef hardened_hash_map <key_type, entry, hash, keyequal> cache_type;
    typedef typename cache_type::iterator cache_iterator;

    beast::journal m_journal;
    clock_type& m_clock;
    stats m_stats;

    mutex_type mutable m_mutex;

    // used for logging
    std::string m_name;

    // desired number of cache entries (0 = ignore)
    int m_target_size;

    // desired maximum cache age
    clock_type::duration m_target_age;

    // number of items cached
    int m_cache_count;
    cache_type m_cache;  // hold strong reference to recent objects
    std::uint64_t m_hits;
    std::uint64_t m_misses;
};

}

#endif
