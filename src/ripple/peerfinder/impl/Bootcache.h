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

#ifndef ripple_peerfinder_bootcache_h_included
#define ripple_peerfinder_bootcache_h_included

#include <ripple/peerfinder/manager.h>
#include <ripple/peerfinder/impl/store.h>
#include <beast/utility/journal.h>
#include <beast/utility/propertystream.h>
#include <boost/bimap.hpp>
#include <boost/bimap/multiset_of.hpp>
#include <boost/bimap/unordered_set_of.hpp>
#include <boost/iterator/transform_iterator.hpp>

namespace ripple {
namespace peerfinder {

/** stores ip addresses useful for gaining initial connections.

    this is one of the caches that is consulted when additional outgoing
    connections are needed. along with the address, each entry has this
    additional metadata:

    valence
        a signed integer which represents the number of successful
        consecutive connection attempts when positive, and the number of
        failed consecutive connection attempts when negative.

    when choosing addresses from the boot cache for the purpose of
    establishing outgoing connections, addresses are ranked in decreasing
    order of high uptime, with valence as the tie breaker.
*/
class bootcache
{
private:
    class entry
    {
    public:
        entry (int valence)
            : m_valence (valence)
        {
        }

        int& valence ()
        {
            return m_valence;
        }

        int valence () const
        {
            return m_valence;
        }

        friend bool operator< (entry const& lhs, entry const& rhs)
        {
            if (lhs.valence() > rhs.valence())
                return true;
            return false;
        }

    private:
        int m_valence;
    };

    typedef boost::bimaps::unordered_set_of <beast::ip::endpoint> left_t;
    typedef boost::bimaps::multiset_of <entry> right_t;
    typedef boost::bimap <left_t, right_t> map_type;
    typedef map_type::value_type value_type;

    struct transform : std::unary_function <
        map_type::right_map::const_iterator::value_type const&,
            beast::ip::endpoint const&>
    {
        beast::ip::endpoint const& operator() (
            map_type::right_map::
                const_iterator::value_type const& v) const
        {
            return v.get_left();
        }
    };

private:
    map_type m_map;

    store& m_store;
    clock_type& m_clock;
    beast::journal m_journal;

    // time after which we can update the database again
    clock_type::time_point m_whenupdate;

    // set to true when a database update is needed
    bool m_needsupdate;

public:
    typedef boost::transform_iterator <transform,
        map_type::right_map::const_iterator> iterator;

    typedef iterator const_iterator;

    bootcache (
        store& store,
        clock_type& clock,
        beast::journal journal);

    ~bootcache ();

    /** returns `true` if the cache is empty. */
    bool empty() const;

    /** returns the number of entries in the cache. */
    map_type::size_type size() const;

    /** ip::endpoint iterators that traverse in decreasing valence. */
    /** @{ */
    const_iterator begin() const;
    const_iterator cbegin() const;
    const_iterator end() const;
    const_iterator cend() const;
    void clear();
    /** @} */

    /** load the persisted data from the store into the container. */
    void load ();

    /** add the address to the cache. */
    bool insert (beast::ip::endpoint const& endpoint);

    /** called when an outbound connection handshake completes. */
    void on_success (beast::ip::endpoint const& endpoint);

    /** called when an outbound connection attempt fails to handshake. */
    void on_failure (beast::ip::endpoint const& endpoint);

    /** stores the cache in the persistent database on a timer. */
    void periodicactivity ();

    /** write the cache state to the property stream. */
    void onwrite (beast::propertystream::map& map);

private:
    void prune ();
    void update ();
    void checkupdate ();
    void flagforupdate ();
};

}
}

#endif
