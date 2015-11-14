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
#include <ripple/peerfinder/impl/bootcache.h>
#include <ripple/peerfinder/impl/iosformat.h>
#include <ripple/peerfinder/impl/tuning.h>

namespace ripple {
namespace peerfinder {

bootcache::bootcache (
    store& store,
    clock_type& clock,
    beast::journal journal)
    : m_store (store)
    , m_clock (clock)
    , m_journal (journal)
    , m_whenupdate (m_clock.now ())
    , m_needsupdate (false)
{
}

bootcache::~bootcache ()
{
    update();
}

bool
bootcache::empty() const
{
    return m_map.empty();
}

bootcache::map_type::size_type
bootcache::size() const
{
    return m_map.size();
}

bootcache::const_iterator
bootcache::begin() const
{
    return const_iterator (m_map.right.begin());
}

bootcache::const_iterator
bootcache::cbegin() const
{
    return const_iterator (m_map.right.begin());
}

bootcache::const_iterator
bootcache::end() const
{
    return const_iterator (m_map.right.end());
}

bootcache::const_iterator
bootcache::cend() const
{
    return const_iterator (m_map.right.end());
}

void
bootcache::clear()
{
    m_map.clear();
    m_needsupdate = true;
}

//--------------------------------------------------------------------------

void
bootcache::load ()
{
    clear();
    auto const n (m_store.load (
        [this](beast::ip::endpoint const& endpoint, int valence)
        {
            auto const result (this->m_map.insert (
                value_type (endpoint, valence)));
            if (! result.second)
            {
                if (this->m_journal.error)
                    this->m_journal.error << beast::leftw (18) <<
                    "bootcache discard " << endpoint;
            }
        }));

    if (n > 0)
    {
        if (m_journal.info) m_journal.info << beast::leftw (18) <<
            "bootcache loaded " << n <<
                ((n > 1) ? " addresses" : " address");
        prune ();
    }
}

bool
bootcache::insert (beast::ip::endpoint const& endpoint)
{
    auto const result (m_map.insert (
        value_type (endpoint, 0)));
    if (result.second)
    {
        if (m_journal.trace) m_journal.trace << beast::leftw (18) <<
            "bootcache insert " << endpoint;
        prune ();
        flagforupdate();
    }
    return result.second;
}

void
bootcache::on_success (beast::ip::endpoint const& endpoint)
{
    auto result (m_map.insert (
        value_type (endpoint, 1)));
    if (result.second)
    {
        prune ();
    }
    else
    {
        entry entry (result.first->right);
        if (entry.valence() < 0)
            entry.valence() = 0;
        ++entry.valence();
        m_map.erase (result.first);
        result = m_map.insert (
            value_type (endpoint, entry));
        assert (result.second);
    }
    entry const& entry (result.first->right);
    if (m_journal.info) m_journal.info << beast::leftw (18) <<
        "bootcache connect " << endpoint <<
        " with " << entry.valence() <<
        ((entry.valence() > 1) ? " successes" : " success");
    flagforupdate();
}

void
bootcache::on_failure (beast::ip::endpoint const& endpoint)
{
    auto result (m_map.insert (
        value_type (endpoint, -1)));
    if (result.second)
    {
        prune();
    }
    else
    {
        entry entry (result.first->right);
        if (entry.valence() > 0)
            entry.valence() = 0;
        --entry.valence();
        m_map.erase (result.first);
        result = m_map.insert (
            value_type (endpoint, entry));
        assert (result.second);
    }
    entry const& entry (result.first->right);
    auto const n (std::abs (entry.valence()));
    if (m_journal.debug) m_journal.debug << beast::leftw (18) <<
        "bootcache failed " << endpoint <<
        " with " << n <<
        ((n > 1) ? " attempts" : " attempt");
    flagforupdate();
}

void
bootcache::periodicactivity ()
{
    checkupdate();
}

//--------------------------------------------------------------------------

void
bootcache::onwrite (beast::propertystream::map& map)
{
    map ["entries"] = std::uint32_t (m_map.size());
}

    // checks the cache size and prunes if its over the limit.
void
bootcache::prune ()
{
    if (size() <= tuning::bootcachesize)
        return;

    // calculate the amount to remove
    auto count ((size() *
        tuning::bootcacheprunepercent) / 100);
    decltype(count) pruned (0);

    // work backwards because bimap doesn't handle
    // erasing using a reverse iterator very well.
    //
    for (auto iter (m_map.right.end());
        count-- > 0 && iter != m_map.right.begin(); ++pruned)
    {
        --iter;
        beast::ip::endpoint const& endpoint (iter->get_left());
        entry const& entry (iter->get_right());
        if (m_journal.trace) m_journal.trace << beast::leftw (18) <<
            "bootcache pruned" << endpoint <<
            " at valence " << entry.valence();
        iter = m_map.right.erase (iter);
    }

    if (m_journal.debug) m_journal.debug << beast::leftw (18) <<
        "bootcache pruned " << pruned << " entries total";
}

// updates the store with the current set of entries if needed.
void
bootcache::update ()
{
    if (! m_needsupdate)
        return;
    std::vector <store::entry> list;
    list.reserve (m_map.size());
    for (auto const& e : m_map)
    {
        store::entry se;
        se.endpoint = e.get_left();
        se.valence = e.get_right().valence();
        list.push_back (se);
    }
    m_store.save (list);
    // reset the flag and cooldown timer
    m_needsupdate = false;
    m_whenupdate = m_clock.now() + tuning::bootcachecooldowntime;
}

// checks the clock and calls update if we are off the cooldown.
void
bootcache::checkupdate ()
{
    if (m_needsupdate && m_whenupdate < m_clock.now())
        update ();
}

// called when changes to an entry will affect the store.
void
bootcache::flagforupdate ()
{
    m_needsupdate = true;
    checkupdate ();
}

}
}
