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

#ifndef ripple_resource_logic_h_included
#define ripple_resource_logic_h_included

#include <ripple/resource/fees.h>
#include <ripple/resource/gossip.h>
#include <ripple/resource/impl/import.h>
#include <ripple/basics/unorderedcontainers.h>
#include <ripple/json/json_value.h>
#include <beast/chrono/abstract_clock.h>
#include <beast/insight.h>
#include <beast/threads/shareddata.h>
#include <beast/utility/propertystream.h>

namespace ripple {
namespace resource {

class logic
{
private:
    typedef beast::abstract_clock <std::chrono::steady_clock> clock_type;
    typedef hash_map <std::string, import> imports;
    typedef hash_map <key, entry, key::hasher, key::key_equal> table;
    typedef beast::list <entry> entryintrusivelist;

    struct state
    {
        // table of all entries
        table table;

        // because the following are intrusive lists, a given entry may be in
        // at most list at a given instant.  the entry must be removed from
        // one list before placing it in another.

        // list of all active inbound entries
        entryintrusivelist inbound;

        // list of all active outbound entries
        entryintrusivelist outbound;

        // list of all active admin entries
        entryintrusivelist admin;

        // list of all inactve entries
        entryintrusivelist inactive;

        // all imported gossip data
        imports import_table;
    };

    typedef beast::shareddata <state> sharedstate;

    struct stats
    {
        stats (beast::insight::collector::ptr const& collector)
        {
            warn = collector->make_meter ("warn");
            drop = collector->make_meter ("drop");
        }

        beast::insight::meter warn;
        beast::insight::meter drop;
    };

    sharedstate m_state;
    stats m_stats;
    beast::abstract_clock <std::chrono::steady_clock>& m_clock;
    beast::journal m_journal;

    //--------------------------------------------------------------------------
public:

    logic (beast::insight::collector::ptr const& collector,
        clock_type& clock, beast::journal journal)
        : m_stats (collector)
        , m_clock (clock)
        , m_journal (journal)
    {
    }

    ~logic ()
    {
        // these have to be cleared before the logic is destroyed
        // since their destructors call back into the class.
        // order matters here as well, the import table has to be
        // destroyed before the consumer table.
        //
        sharedstate::unlockedaccess state (m_state);
        state->import_table.clear();
        state->table.clear();
    }

    consumer newinboundendpoint (beast::ip::endpoint const& address)
    {
        if (iswhitelisted (address))
            return newadminendpoint (to_string (address));

        entry* entry (nullptr);

        {
            sharedstate::access state (m_state);
            std::pair <table::iterator, bool> result (
                state->table.emplace (std::piecewise_construct,
                    std::make_tuple (kindinbound, address.at_port (0)), // key
                    std::make_tuple (m_clock.now())));                  // entry

            entry = &result.first->second;
            entry->key = &result.first->first;
            ++entry->refcount;
            if (entry->refcount == 1)
            {
                if (! result.second)
                {
                    state->inactive.erase (
                        state->inactive.iterator_to (*entry));
                }
                state->inbound.push_back (*entry);
            }
        }

        m_journal.debug <<
            "new inbound endpoint " << *entry;

        return consumer (*this, *entry);
    }

    consumer newoutboundendpoint (beast::ip::endpoint const& address)
    {
        if (iswhitelisted (address))
            return newadminendpoint (to_string (address));

        entry* entry (nullptr);

        {
            sharedstate::access state (m_state);
            std::pair <table::iterator, bool> result (
                state->table.emplace (std::piecewise_construct,
                    std::make_tuple (kindoutbound, address),            // key
                    std::make_tuple (m_clock.now())));                  // entry

            entry = &result.first->second;
            entry->key = &result.first->first;
            ++entry->refcount;
            if (entry->refcount == 1)
            {
                if (! result.second)
                    state->inactive.erase (
                        state->inactive.iterator_to (*entry));
                state->outbound.push_back (*entry);
            }
        }

        m_journal.debug <<
            "new outbound endpoint " << *entry;

        return consumer (*this, *entry);
    }

    consumer newadminendpoint (std::string const& name)
    {
        entry* entry (nullptr);

        {
            sharedstate::access state (m_state);
            std::pair <table::iterator, bool> result (
                state->table.emplace (std::piecewise_construct,
                    std::make_tuple (kindadmin, name),                  // key
                    std::make_tuple (m_clock.now())));                  // entry

            entry = &result.first->second;
            entry->key = &result.first->first;
            ++entry->refcount;
            if (entry->refcount == 1)
            {
                if (! result.second)
                    state->inactive.erase (
                        state->inactive.iterator_to (*entry));
                state->admin.push_back (*entry);
            }
        }

        m_journal.debug <<
            "new admin endpoint " << *entry;

        return consumer (*this, *entry);
    }

    entry& elevatetoadminendpoint (entry& prior, std::string const& name)
    {
        m_journal.info <<
            "elevate " << prior << " to " << name;

        entry* entry (nullptr);

        {
            sharedstate::access state (m_state);
            std::pair <table::iterator, bool> result (
                state->table.emplace (std::piecewise_construct,
                    std::make_tuple (kindadmin, name),                  // key
                    std::make_tuple (m_clock.now())));                  // entry

            entry = &result.first->second;
            entry->key = &result.first->first;
            ++entry->refcount;
            if (entry->refcount == 1)
            {
                if (! result.second)
                    state->inactive.erase (
                        state->inactive.iterator_to (*entry));
                state->admin.push_back (*entry);
            }

            release (prior, state);
        }

        return *entry;
    }

    json::value getjson ()
    {
        return getjson (warningthreshold);
    }

    /** returns a json::objectvalue. */
    json::value getjson (int threshold)
    {
        clock_type::time_point const now (m_clock.now());

        json::value ret (json::objectvalue);
        sharedstate::access state (m_state);

        for (auto& inboundentry : state->inbound)
        {
            int localbalance = inboundentry.local_balance.value (now);
            if ((localbalance + inboundentry.remote_balance) >= threshold)
            {
                json::value& entry = (ret[inboundentry.to_string()] = json::objectvalue);
                entry["local"] = localbalance;
                entry["remote"] = inboundentry.remote_balance;
                entry["type"] = "outbound";
            }

        }
        for (auto& outboundentry : state->outbound)
        {
            int localbalance = outboundentry.local_balance.value (now);
            if ((localbalance + outboundentry.remote_balance) >= threshold)
            {
                json::value& entry = (ret[outboundentry.to_string()] = json::objectvalue);
                entry["local"] = localbalance;
                entry["remote"] = outboundentry.remote_balance;
                entry["type"] = "outbound";
            }

        }
        for (auto& adminentry : state->admin)
        {
            int localbalance = adminentry.local_balance.value (now);
            if ((localbalance + adminentry.remote_balance) >= threshold)
            {
                json::value& entry = (ret[adminentry.to_string()] = json::objectvalue);
                entry["local"] = localbalance;
                entry["remote"] = adminentry.remote_balance;
                entry["type"] = "admin";
            }

        }

        return ret;
    }

    gossip exportconsumers ()
    {
        clock_type::time_point const now (m_clock.now());

        gossip gossip;
        sharedstate::access state (m_state);

        gossip.items.reserve (state->inbound.size());

        for (auto& inboundentry : state->inbound)
        {
            gossip::item item;
            item.balance = inboundentry.local_balance.value (now);
            if (item.balance >= minimumgossipbalance)
            {
                item.address = inboundentry.key->address;
                gossip.items.push_back (item);
            }
        }

        return gossip;
    }

    //--------------------------------------------------------------------------

    void importconsumers (std::string const& origin, gossip const& gossip)
    {
        clock_type::rep const elapsed (m_clock.elapsed());
        {
            sharedstate::access state (m_state);
            std::pair <imports::iterator, bool> result (
                state->import_table.emplace (std::piecewise_construct,
                    std::make_tuple(origin),                  // key
                    std::make_tuple(m_clock.elapsed())));     // import

            if (result.second)
            {
                // this is a new import
                import& next (result.first->second);
                next.whenexpires = elapsed + gossipexpirationseconds;
                next.items.reserve (gossip.items.size());

                for (auto const& gossipitem : gossip.items)
                {
                    import::item item;
                    item.balance = gossipitem.balance;
                    item.consumer = newinboundendpoint (gossipitem.address);
                    item.consumer.entry().remote_balance += item.balance;
                    next.items.push_back (item);
                }
            }
            else
            {
                // previous import exists so add the new remote
                // balances and then deduct the old remote balances.

                import next;
                next.whenexpires = elapsed + gossipexpirationseconds;
                next.items.reserve (gossip.items.size());
                for (auto const& gossipitem : gossip.items)
                {
                    import::item item;
                    item.balance = gossipitem.balance;
                    item.consumer = newinboundendpoint (gossipitem.address);
                    item.consumer.entry().remote_balance += item.balance;
                    next.items.push_back (item);
                }

                import& prev (result.first->second);
                for (auto& item : prev.items)
                {
                    item.consumer.entry().remote_balance -= item.balance;
                }

                std::swap (next, prev);
            }
        }
    }

    //--------------------------------------------------------------------------

    bool iswhitelisted (beast::ip::endpoint const& address)
    {
        if (! is_public (address))
            return true;

        return false;
    }

    // called periodically to expire entries and groom the table.
    //
    void periodicactivity ()
    {
        sharedstate::access state (m_state);

        clock_type::rep const elapsed (m_clock.elapsed());

        for (auto iter (state->inactive.begin()); iter != state->inactive.end();)
        {
            if (iter->whenexpires <= elapsed)
            {
                m_journal.debug << "expired " << *iter;
                table::iterator table_iter (
                    state->table.find (*iter->key));
                ++iter;
                erase (table_iter, state);
            }
            else
            {
                break;
            }
        }

        imports::iterator iter (state->import_table.begin());
        while (iter != state->import_table.end())
        {
            import& import (iter->second);
            if (iter->second.whenexpires <= elapsed)
            {
                for (auto item_iter (import.items.begin());
                    item_iter != import.items.end(); ++item_iter)
                {
                    item_iter->consumer.entry().remote_balance -= item_iter->balance;
                }

                iter = state->import_table.erase (iter);
            }
            else
                ++iter;
        }
    }

    //--------------------------------------------------------------------------

    // returns the disposition based on the balance and thresholds
    static disposition disposition (int balance)
    {
        if (balance >= dropthreshold)
            return disposition::drop;

        if (balance >= warningthreshold)
            return disposition::warn;

        return disposition::ok;
    }

    void acquire (entry& entry, sharedstate::access& state)
    {
        ++entry.refcount;
    }

    void release (entry& entry, sharedstate::access& state)
    {
        if (--entry.refcount == 0)
        {
            m_journal.debug <<
                "inactive " << entry;

            switch (entry.key->kind)
            {
            case kindinbound:
                state->inbound.erase (
                    state->inbound.iterator_to (entry));
                break;
            case kindoutbound:
                state->outbound.erase (
                    state->outbound.iterator_to (entry));
                break;
            case kindadmin:
                state->admin.erase (
                    state->admin.iterator_to (entry));
                break;
            default:
                bassertfalse;
                break;
            }
            state->inactive.push_back (entry);
            entry.whenexpires = m_clock.elapsed() + secondsuntilexpiration;
        }
    }

    void erase (table::iterator iter, sharedstate::access& state)
    {
        entry& entry (iter->second);
        bassert (entry.refcount == 0);
        state->inactive.erase (
            state->inactive.iterator_to (entry));
        state->table.erase (iter);
    }

    disposition charge (entry& entry, charge const& fee, sharedstate::access& state)
    {
        clock_type::time_point const now (m_clock.now());
        int const balance (entry.add (fee.cost(), now));
        m_journal.trace <<
            "charging " << entry << " for " << fee;
        return disposition (balance);
    }

    bool warn (entry& entry, sharedstate::access& state)
    {
        bool notify (false);
        clock_type::rep const elapsed (m_clock.elapsed());
        if (entry.balance (m_clock.now()) >= warningthreshold &&
            elapsed != entry.lastwarningtime)
        {
            charge (entry, feewarning, state);
            notify = true;
            entry.lastwarningtime = elapsed;
        }

        if (notify)
            m_journal.info <<
                "load warning: " << entry;

        if (notify)
            ++m_stats.warn;

        return notify;
    }

    bool disconnect (entry& entry, sharedstate::access& state)
    {
        bool drop (false);
        clock_type::time_point const now (m_clock.now());
        int const balance (entry.balance (now));
        if (balance >= dropthreshold)
        {
            m_journal.warning <<
                "consumer entry " << entry <<
                " dropped with balance " << balance <<
                " at or above drop threshold " << dropthreshold;

            // adding feedrop at this point keeps the dropped connection
            // from re-connecting for at least a little while after it is
            // dropped.
            charge (entry, feedrop, state);
            ++m_stats.drop;
            drop = true;
        }
        return drop;
    }

    int balance (entry& entry, sharedstate::access& state)
    {
        return entry.balance (m_clock.now());
    }

    //--------------------------------------------------------------------------

    void acquire (entry& entry)
    {
        sharedstate::access state (m_state);
        acquire (entry, state);
    }

    void release (entry& entry)
    {
        sharedstate::access state (m_state);
        release (entry, state);
    }

    disposition charge (entry& entry, charge const& fee)
    {
        sharedstate::access state (m_state);
        return charge (entry, fee, state);
    }

    bool warn (entry& entry)
    {
        if (entry.admin())
            return false;

        sharedstate::access state (m_state);
        return warn (entry, state);
    }

    bool disconnect (entry& entry)
    {
        if (entry.admin())
            return false;

        sharedstate::access state (m_state);
        return disconnect (entry, state);
    }

    int balance (entry& entry)
    {
        sharedstate::access state (m_state);
        return balance (entry, state);
    }

    //--------------------------------------------------------------------------

    void writelist (
        clock_type::time_point const now,
            beast::propertystream::set& items,
                entryintrusivelist& list)
    {
        for (auto& entry : list)
        {
            beast::propertystream::map item (items);
            if (entry.refcount != 0)
                item ["count"] = entry.refcount;
            item ["name"] = entry.to_string();
            item ["balance"] = entry.balance(now);
            if (entry.remote_balance != 0)
                item ["remote_balance"] = entry.remote_balance;
        }
    }

    void onwrite (beast::propertystream::map& map)
    {
        clock_type::time_point const now (m_clock.now());

        sharedstate::access state (m_state);

        {
            beast::propertystream::set s ("inbound", map);
            writelist (now, s, state->inbound);
        }

        {
            beast::propertystream::set s ("outbound", map);
            writelist (now, s, state->outbound);
        }

        {
            beast::propertystream::set s ("admin", map);
            writelist (now, s, state->admin);
        }

        {
            beast::propertystream::set s ("inactive", map);
            writelist (now, s, state->inactive);
        }
    }
};

}
}

#endif
