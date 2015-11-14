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

#ifndef ripple_peerfinder_livecache_h_included
#define ripple_peerfinder_livecache_h_included

#include <ripple/peerfinder/manager.h>
#include <ripple/peerfinder/impl/iosformat.h>
#include <ripple/peerfinder/impl/tuning.h>
#include <beast/chrono/chrono_io.h>
#include <beast/container/aged_map.h>
#include <beast/utility/maybe_const.h>
#include <boost/intrusive/list.hpp>
#include <boost/iterator/transform_iterator.hpp>

namespace ripple {
namespace peerfinder {

template <class>
class livecache;

namespace detail {

class livecachebase
{
protected:
    struct element
        : boost::intrusive::list_base_hook <>
    {
        element (endpoint const& endpoint_)
            : endpoint (endpoint_)
        {
        }

        endpoint endpoint;
    };

    typedef boost::intrusive::make_list <element,
        boost::intrusive::constant_time_size <false>
            >::type list_type;

public:
    /** a list of endpoint at the same hops
        this is a lightweight wrapper around a reference to the underlying
        container.
    */
    template <bool isconst>
    class hop
    {
    public:
        // iterator transformation to extract the endpoint from element
        struct transform
            : public std::unary_function <element, endpoint>
        {
            endpoint const& operator() (element const& e) const
            {
                return e.endpoint;
            }
        };

    public:
        typedef boost::transform_iterator <transform,
            typename list_type::const_iterator> iterator;

        typedef iterator const_iterator;

        typedef boost::transform_iterator <transform,
            typename list_type::const_reverse_iterator> reverse_iterator;

        typedef reverse_iterator const_reverse_iterator;

        iterator begin () const
        {
            return iterator (m_list.get().cbegin(),
                transform());
        }

        iterator cbegin () const
        {
            return iterator (m_list.get().cbegin(),
                transform());
        }

        iterator end () const
        {
            return iterator (m_list.get().cend(),
                transform());
        }

        iterator cend () const
        {
            return iterator (m_list.get().cend(),
                transform());
        }

        reverse_iterator rbegin () const
        {
            return reverse_iterator (m_list.get().crbegin(),
                transform());
        }

        reverse_iterator crbegin () const
        {
            return reverse_iterator (m_list.get().crbegin(),
                transform());
        }

        reverse_iterator rend () const
        {
            return reverse_iterator (m_list.get().crend(),
                transform());
        }

        reverse_iterator crend () const
        {
            return reverse_iterator (m_list.get().crend(),
                transform());
        }

        // move the element to the end of the container
        void move_back (const_iterator pos)
        {
            auto& e (const_cast <element&>(*pos.base()));
            m_list.get().erase (m_list.get().iterator_to (e));
            m_list.get().push_back (e);
        }

    private:
        explicit hop (typename beast::maybe_const <
            isconst, list_type>::type& list)
            : m_list (list)
        {
        }

        friend class livecachebase;

        std::reference_wrapper <typename beast::maybe_const <
            isconst, list_type>::type> m_list;
    };

protected:
    // work-around to call hop's private constructor from livecache
    template <bool isconst>
    static hop <isconst> make_hop (typename beast::maybe_const <
        isconst, list_type>::type& list)
    {
        return hop <isconst> (list);
    }
};

}

//------------------------------------------------------------------------------

/** the livecache holds the short-lived relayed endpoint messages.

    since peers only advertise themselves when they have open slots,
    we want these messags to expire rather quickly after the peer becomes
    full.

    addresses added to the cache are not connection-tested to see if
    they are connectible (with one small exception regarding neighbors).
    therefore, these addresses are not suitable for persisting across
    launches or for bootstrapping, because they do not have verifiable
    and locally observed uptime and connectibility information.
*/
template <class allocator = std::allocator <char>>
class livecache : protected detail::livecachebase
{
private:
    typedef beast::aged_map <beast::ip::endpoint, element,
        std::chrono::steady_clock, std::less <beast::ip::endpoint>,
            allocator> cache_type;

    beast::journal m_journal;
    cache_type m_cache;

public:
    typedef allocator allocator_type;

    /** create the cache. */
    livecache (
        clock_type& clock,
        beast::journal journal,
        allocator alloc = allocator());

    //
    // iteration by hops
    //
    // the range [begin, end) provides a sequence of list_type
    // where each list contains endpoints at a given hops.
    //

    class hops_t
    {
    private:
        // an endpoint at hops=0 represents the local node.
        // endpoints coming in at maxhops are stored at maxhops +1,
        // but not given out (since they would exceed maxhops). they
        // are used for automatic connection attempts.
        //
        typedef std::array <int, 1 + tuning::maxhops + 1> histogram;
        typedef std::array <list_type, 1 + tuning::maxhops + 1> lists_type;

        template <bool isconst>
        struct transform
            : public std::unary_function <
                typename lists_type::value_type, hop <isconst>>
        {
            hop <isconst> operator() (typename beast::maybe_const <
                isconst, typename lists_type::value_type>::type& list) const
            {
                return make_hop <isconst> (list);
            }
        };

    public:
        typedef boost::transform_iterator <transform <false>,
            typename lists_type::iterator> iterator;

        typedef boost::transform_iterator <transform <true>,
            typename lists_type::const_iterator> const_iterator;

        typedef boost::transform_iterator <transform <false>,
            typename lists_type::reverse_iterator> reverse_iterator;

        typedef boost::transform_iterator <transform <true>,
            typename lists_type::const_reverse_iterator> const_reverse_iterator;

        iterator begin ()
        {
            return iterator (m_lists.begin(),
                transform <false>());
        }

        const_iterator begin () const
        {
            return const_iterator (m_lists.cbegin(),
                transform <true>());
        }

        const_iterator cbegin () const
        {
            return const_iterator (m_lists.cbegin(),
                transform <true>());
        }

        iterator end ()
        {
            return iterator (m_lists.end(),
                transform <false>());
        }

        const_iterator end () const
        {
            return const_iterator (m_lists.cend(),
                transform <true>());
        }

        const_iterator cend () const
        {
            return const_iterator (m_lists.cend(),
                transform <true>());
        }

        reverse_iterator rbegin ()
        {
            return reverse_iterator (m_lists.rbegin(),
                transform <false>());
        }

        const_reverse_iterator rbegin () const
        {
            return const_reverse_iterator (m_lists.crbegin(),
                transform <true>());
        }

        const_reverse_iterator crbegin () const
        {
            return const_reverse_iterator (m_lists.crbegin(),
                transform <true>());
        }

        reverse_iterator rend ()
        {
            return reverse_iterator (m_lists.rend(),
                transform <false>());
        }

        const_reverse_iterator rend () const
        {
            return const_reverse_iterator (m_lists.crend(),
                transform <true>());
        }

        const_reverse_iterator crend () const
        {
            return const_reverse_iterator (m_lists.crend(),
                transform <true>());
        }

        /** shuffle each hop list. */
        void shuffle ();

        std::string histogram() const;

    private:
        explicit hops_t (allocator const& alloc);

        void insert (element& e);

        // reinsert e at a new hops
        void reinsert (element& e, int hops);

        void remove (element& e);

        friend class livecache;
        lists_type m_lists;
        histogram m_hist;
    } hops;

    /** returns `true` if the cache is empty. */
    bool empty () const
    {
        return m_cache.empty ();
    }

    /** returns the number of entries in the cache. */
    typename cache_type::size_type size() const
    {
        return m_cache.size();
    }

    /** erase entries whose time has expired. */
    void expire ();

    /** creates or updates an existing element based on a new message. */
    void insert (endpoint const& ep);

    /** produce diagnostic output. */
    void dump (beast::journal::scopedstream& ss) const;

    /** output statistics. */
    void onwrite (beast::propertystream::map& map);
};

//------------------------------------------------------------------------------

template <class allocator>
livecache <allocator>::livecache (
    clock_type& clock,
    beast::journal journal,
    allocator alloc)
    : m_journal (journal)
    , m_cache (clock, alloc)
    , hops (alloc)
{
}

template <class allocator>
void
livecache <allocator>::expire()
{
    std::size_t n (0);
    typename cache_type::time_point const expired (
        m_cache.clock().now() - tuning::livecachesecondstolive);
    for (auto iter (m_cache.chronological.begin());
        iter != m_cache.chronological.end() && iter.when() <= expired;)
    {
        element& e (iter->second);
        hops.remove (e);
        iter = m_cache.erase (iter);
        ++n;
    }
    if (n > 0)
    {
        if (m_journal.debug) m_journal.debug << beast::leftw (18) <<
            "livecache expired " << n <<
            ((n > 1) ? " entries" : " entry");
    }
}

template <class allocator>
void livecache <allocator>::insert (endpoint const& ep)
{
    // the caller already incremented hop, so if we got a
    // message at maxhops we will store it at maxhops + 1.
    // this means we won't give out the address to other peers
    // but we will use it to make connections and hand it out
    // when redirecting.
    //
    assert (ep.hops <= (tuning::maxhops + 1));
    std::pair <typename cache_type::iterator, bool> result (
        m_cache.emplace (ep.address, ep));
    element& e (result.first->second);
    if (result.second)
    {
        hops.insert (e);
        if (m_journal.debug) m_journal.debug << beast::leftw (18) <<
            "livecache insert " << ep.address <<
            " at hops " << ep.hops;
        return;
    }
    else if (! result.second && (ep.hops > e.endpoint.hops))
    {
        // drop duplicates at higher hops
        std::size_t const excess (
            ep.hops - e.endpoint.hops);
        if (m_journal.trace) m_journal.trace << beast::leftw(18) <<
            "livecache drop " << ep.address <<
            " at hops +" << excess;
        return;
    }

    m_cache.touch (result.first);

    // address already in the cache so update metadata
    if (ep.hops < e.endpoint.hops)
    {
        hops.reinsert (e, ep.hops);
        if (m_journal.debug) m_journal.debug << beast::leftw (18) <<
            "livecache update " << ep.address <<
            " at hops " << ep.hops;
    }
    else
    {
        if (m_journal.trace) m_journal.trace << beast::leftw (18) <<
            "livecache refresh " << ep.address <<
            " at hops " << ep.hops;
    }
}

template <class allocator>
void
livecache <allocator>::dump (beast::journal::scopedstream& ss) const
{
    ss << std::endl << std::endl <<
        "livecache (size " << m_cache.size() << ")";
    for (auto const& entry : m_cache)
    {
        auto const& e (entry.second);
        ss << std::endl <<
            e.endpoint.address << ", " <<
            e.endpoint.hops << " hops";
    }
}

template <class allocator>
void
livecache <allocator>::onwrite (beast::propertystream::map& map)
{
    typename cache_type::time_point const expired (
        m_cache.clock().now() - tuning::livecachesecondstolive);
    map ["size"] = size ();
    map ["hist"] = hops.histogram();
    beast::propertystream::set set ("entries", map);
    for (auto iter (m_cache.cbegin()); iter != m_cache.cend(); ++iter)
    {
        auto const& e (iter->second);
        beast::propertystream::map item (set);
        item ["hops"] = e.endpoint.hops;
        item ["address"] = e.endpoint.address.to_string ();
        std::stringstream ss;
        ss << iter.when() - expired;
        item ["expires"] = ss.str();
    }
}

//------------------------------------------------------------------------------

template <class allocator>
void
livecache <allocator>::hops_t::shuffle()
{
    for (auto& list : m_lists)
    {
        std::vector <std::reference_wrapper <element>> v;
        v.reserve (list.size());
        std::copy (list.begin(), list.end(),
            std::back_inserter (v));
        std::random_shuffle (v.begin(), v.end());
        list.clear();
        for (auto& e : v)
            list.push_back (e);
    }
}

template <class allocator>
std::string
livecache <allocator>::hops_t::histogram() const
{
    std::stringstream ss;
    for (typename decltype(m_hist)::size_type i (0);
        i < m_hist.size(); ++i)
    {
        ss << m_hist[i] <<
            ((i < tuning::maxhops + 1) ? ", " : "");
    }
    return ss.str();
}

template <class allocator>
livecache <allocator>::hops_t::hops_t (allocator const& alloc)
{
    std::fill (m_hist.begin(), m_hist.end(), 0);
}

template <class allocator>
void
livecache <allocator>::hops_t::insert (element& e)
{
    assert (e.endpoint.hops >= 0 &&
        e.endpoint.hops <= tuning::maxhops + 1);
    // this has security implications without a shuffle
    m_lists [e.endpoint.hops].push_front (e);
    ++m_hist [e.endpoint.hops];
}

template <class allocator>
void
livecache <allocator>::hops_t::reinsert (element& e, int hops)
{
    assert (hops >= 0 && hops <= tuning::maxhops + 1);
    list_type& list (m_lists [e.endpoint.hops]);
    list.erase (list.iterator_to (e));
    --m_hist [e.endpoint.hops];

    e.endpoint.hops = hops;
    insert (e);
}

template <class allocator>
void
livecache <allocator>::hops_t::remove (element& e)
{
    --m_hist [e.endpoint.hops];
    list_type& list (m_lists [e.endpoint.hops]);
    list.erase (list.iterator_to (e));
}

}
}

#endif
