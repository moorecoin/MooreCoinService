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

#ifndef ripple_peerfinder_logic_h_included
#define ripple_peerfinder_logic_h_included

#include <ripple/peerfinder/manager.h>
#include <ripple/peerfinder/impl/bootcache.h>
#include <ripple/peerfinder/impl/counts.h>
#include <ripple/peerfinder/impl/fixed.h>
#include <ripple/peerfinder/impl/iosformat.h>
#include <ripple/peerfinder/impl/handouts.h>
#include <ripple/peerfinder/impl/livecache.h>
#include <ripple/peerfinder/impl/reporting.h>
#include <ripple/peerfinder/impl/slotimp.h>
#include <ripple/peerfinder/impl/source.h>
#include <ripple/peerfinder/impl/store.h>
#include <beast/container/aged_container_utility.h>
#include <beast/smart_ptr/sharedptr.h>
#include <functional>
#include <map>
#include <set>

namespace ripple {
namespace peerfinder {

/** the logic for maintaining the list of slot addresses.
    we keep this in a separate class so it can be instantiated
    for unit tests.
*/
template <class checker>
class logic
{
public:
    // maps remote endpoints to slots. since a slot has a
    // remote endpoint upon construction, this holds all counts.
    //
    typedef std::map <beast::ip::endpoint,
        std::shared_ptr <slotimp>> slots;

    typedef std::map <beast::ip::endpoint, fixed> fixedslots;

    // a set of unique ripple public keys
    typedef std::set <ripplepublickey> keys;

    // a set of non-unique ipaddresses without ports, used
    // to filter duplicates when making outgoing connections.
    typedef std::multiset <beast::ip::endpoint> connectedaddresses;

    struct state
    {
        state (
            store* store,
            clock_type& clock,
            beast::journal journal)
            : stopping (false)
            , counts ()
            , livecache (clock, beast::journal (
                journal, reporting::livecache))
            , bootcache (*store, clock, beast::journal (
                journal, reporting::bootcache))
        {
        }

        // true if we are stopping.
        bool stopping;

        // the source we are currently fetching.
        // this is used to cancel i/o during program exit.
        beast::sharedptr <source> fetchsource;

        // configuration settings
        config config;

        // slot counts and other aggregate statistics.
        counts counts;

        // a list of slots that should always be connected
        fixedslots fixed;

        // live livecache from mtendpoints messages
        livecache <> livecache;

        // livecache of addresses suitable for gaining initial connections
        bootcache bootcache;

        // holds all counts
        slots slots;

        // the addresses (but not port) we are connected to. this includes
        // outgoing connection attempts. note that this set can contain
        // duplicates (since the port is not set)
        connectedaddresses connected_addresses;

        // set of public keys belonging to active peers
        keys keys;
    };

    typedef beast::shareddata <state> sharedstate;

    beast::journal m_journal;
    sharedstate m_state;
    clock_type& m_clock;
    store& m_store;
    checker& m_checker;

    // a list of dynamic sources to consult as a fallback
    std::vector <beast::sharedptr <source>> m_sources;

    clock_type::time_point m_whenbroadcast;

    connecthandouts::squelches m_squelches;

    //--------------------------------------------------------------------------

    logic (clock_type& clock, store& store,
            checker& checker, beast::journal journal)
        : m_journal (journal, reporting::logic)
        , m_state (&store, std::ref (clock), journal)
        , m_clock (clock)
        , m_store (store)
        , m_checker (checker)
        , m_whenbroadcast (m_clock.now())
        , m_squelches (m_clock)
    {
        config ({});
    }

    // load persistent state information from the store
    //
    void load ()
    {
        typename sharedstate::access state (m_state);

        state->bootcache.load ();
    }

    /** stop the logic.
        this will cancel the current fetch and set the stopping flag
        to `true` to prevent further fetches.
        thread safety:
            safe to call from any thread.
    */
    void stop ()
    {
        typename sharedstate::access state (m_state);
        state->stopping = true;
        if (state->fetchsource != nullptr)
            state->fetchsource->cancel ();
    }

    //--------------------------------------------------------------------------
    //
    // manager
    //
    //--------------------------------------------------------------------------

    void
    config (config const& c)
    {
        typename sharedstate::access state (m_state);
        state->config = c;
        state->counts.onconfig (state->config);
    }

    config
    config()
    {
        typename sharedstate::access state (m_state);
        return state->config;
    }

    void
    addfixedpeer (std::string const& name,
        std::vector <beast::ip::endpoint> const& addresses)
    {
        typename sharedstate::access state (m_state);

        if (addresses.empty ())
        {
            if (m_journal.info) m_journal.info <<
                "could not resolve fixed slot '" << name << "'";
            return;
        }

        for (auto const& remote_address : addresses)
        {
            auto result (state->fixed.emplace (std::piecewise_construct,
                std::forward_as_tuple (remote_address),
                    std::make_tuple (std::ref (m_clock))));

            if (result.second)
            {
                if (m_journal.debug) m_journal.debug << beast::leftw (18) <<
                    "logic add fixed '" << name <<
                    "' at " << remote_address;
                return;
            }
        }
    }

    //--------------------------------------------------------------------------

    // called when the checker completes a connectivity test
    void checkcomplete (beast::ip::endpoint const& remoteaddress,
        beast::ip::endpoint const& checkedaddress,
            boost::system::error_code ec)
    {
        if (ec == boost::asio::error::operation_aborted)
            return;

        typename sharedstate::access state (m_state);
        slots::iterator const iter (state->slots.find (remoteaddress));
        if (iter == state->slots.end())
        {
            // the slot disconnected before we finished the check
            if (m_journal.debug) m_journal.debug << beast::leftw (18) <<
                "logic tested " << checkedaddress <<
                " but the connection was closed";
            return;
        }

        slotimp& slot (*iter->second);
        slot.checked = true;
        slot.connectivitycheckinprogress = false;

        if (ec)
        {
            // vfalco todo should we retry depending on the error?
            slot.canaccept = false;
            if (m_journal.error) m_journal.error << beast::leftw (18) <<
                "logic testing " << iter->first << " with error, " <<
                ec.message();
            state->bootcache.on_failure (checkedaddress);
            return;
        }

        slot.canaccept = true;
        if (m_journal.debug) m_journal.debug << beast::leftw (18) <<
            "logic testing " << checkedaddress << " succeeded";
    }

    //--------------------------------------------------------------------------

    slotimp::ptr new_inbound_slot (beast::ip::endpoint const& local_endpoint,
        beast::ip::endpoint const& remote_endpoint)
    {
        if (m_journal.debug) m_journal.debug << beast::leftw (18) <<
            "logic accept" << remote_endpoint <<
            " on local " << local_endpoint;

        typename sharedstate::access state (m_state);

        // check for duplicate connection
        {
            auto const iter = state->connected_addresses.find (remote_endpoint);
            if (iter != state->connected_addresses.end())
            {
                if (m_journal.warning) m_journal.warning << beast::leftw (18) <<
                    "logic dropping inbound " << remote_endpoint <<
                    " as duplicate";
                return slotimp::ptr();
            }
        }

        // check for self-connect by address
        // this is disabled because otherwise we couldn't connect to
        // ourselves for testing purposes. eventually a self-connect will
        // be dropped if the public key is the same. and if it's different,
        // we want to allow the self-connect.
        /*
        {
            auto const iter (state->slots.find (local_endpoint));
            if (iter != state->slots.end ())
            {
                slot::ptr const& self (iter->second);
                bool const consistent ((
                    self->local_endpoint() == boost::none) ||
                        (*self->local_endpoint() == remote_endpoint));
                if (! consistent)
                {
                    m_journal.fatal << "\n" <<
                        "local endpoint mismatch\n" <<
                        "local_endpoint=" << local_endpoint <<
                            ", remote_endpoint=" << remote_endpoint << "\n" <<
                        "self->local_endpoint()=" << *self->local_endpoint() <<
                            ", self->remote_endpoint()=" << self->remote_endpoint();
                }
                // this assert goes off
                //assert (consistent);
                if (m_journal.warning) m_journal.warning << beast::leftw (18) <<
                    "logic dropping " << remote_endpoint <<
                    " as self connect";
                return slotimp::ptr ();
            }
        }
        */

        // create the slot
        slotimp::ptr const slot (std::make_shared <slotimp> (local_endpoint,
            remote_endpoint, fixed (remote_endpoint.address (), state),
                m_clock));
        // add slot to table
        auto const result (state->slots.emplace (
            slot->remote_endpoint (), slot));
        // remote address must not already exist
        assert (result.second);
        // add to the connected address list
        state->connected_addresses.emplace (remote_endpoint.at_port (0));

        // update counts
        state->counts.add (*slot);

        return result.first->second;
    }

    // can't check for self-connect because we don't know the local endpoint
    slotimp::ptr
    new_outbound_slot (beast::ip::endpoint const& remote_endpoint)
    {
        if (m_journal.debug) m_journal.debug << beast::leftw (18) <<
            "logic connect " << remote_endpoint;

        typename sharedstate::access state (m_state);

        // check for duplicate connection
        if (state->slots.find (remote_endpoint) !=
            state->slots.end ())
        {
            if (m_journal.warning) m_journal.warning << beast::leftw (18) <<
                "logic dropping " << remote_endpoint <<
                " as duplicate connect";
            return slotimp::ptr ();
        }

        // create the slot
        slotimp::ptr const slot (std::make_shared <slotimp> (
            remote_endpoint, fixed (remote_endpoint, state), m_clock));

        // add slot to table
        std::pair <slots::iterator, bool> result (
            state->slots.emplace (slot->remote_endpoint (),
                slot));
        // remote address must not already exist
        assert (result.second);

        // add to the connected address list
        state->connected_addresses.emplace (remote_endpoint.at_port (0));

        // update counts
        state->counts.add (*slot);

        return result.first->second;
    }

    bool
    onconnected (slotimp::ptr const& slot,
        beast::ip::endpoint const& local_endpoint)
    {
        if (m_journal.trace) m_journal.trace << beast::leftw (18) <<
            "logic connected" << slot->remote_endpoint () <<
            " on local " << local_endpoint;

        typename sharedstate::access state (m_state);

        // the object must exist in our table
        assert (state->slots.find (slot->remote_endpoint ()) !=
            state->slots.end ());
        // assign the local endpoint now that it's known
        slot->local_endpoint (local_endpoint);

        // check for self-connect by address
        {
            auto const iter (state->slots.find (local_endpoint));
            if (iter != state->slots.end ())
            {
                assert (iter->second->local_endpoint ()
                        == slot->remote_endpoint ());
                if (m_journal.warning) m_journal.warning << beast::leftw (18) <<
                    "logic dropping " << slot->remote_endpoint () <<
                    " as self connect";
                return false;
            }
        }

        // update counts
        state->counts.remove (*slot);
        slot->state (slot::connected);
        state->counts.add (*slot);
        return true;
    }

    result
    activate (slotimp::ptr const& slot,
        ripplepublickey const& key, bool cluster)
    {
        if (m_journal.debug) m_journal.debug << beast::leftw (18) <<
            "logic handshake " << slot->remote_endpoint () <<
            " with " << (cluster ? "clustered " : "") << "key " << key;

        typename sharedstate::access state (m_state);

        // the object must exist in our table
        assert (state->slots.find (slot->remote_endpoint ()) !=
            state->slots.end ());
        // must be accepted or connected
        assert (slot->state() == slot::accept ||
            slot->state() == slot::connected);

        // check for duplicate connection by key
        if (state->keys.find (key) != state->keys.end())
            return result::duplicate;

        // see if we have an open space for this slot
        if (! state->counts.can_activate (*slot))
        {
            if (! slot->inbound())
                state->bootcache.on_success (
                    slot->remote_endpoint());
            return result::full;
        }

        // set key and cluster right before adding to the map
        // otherwise we could assert later when erasing the key.
        state->counts.remove (*slot);
        slot->public_key (key);
        slot->cluster (cluster);
        state->counts.add (*slot);

        // add the public key to the active set
        std::pair <keys::iterator, bool> const result (
            state->keys.insert (key));
        // public key must not already exist
        assert (result.second);
        (void) result.second;

        // change state and update counts
        state->counts.remove (*slot);
        slot->activate (m_clock.now ());
        state->counts.add (*slot);

        if (! slot->inbound())
            state->bootcache.on_success (
                slot->remote_endpoint());

        // mark fixed slot success
        if (slot->fixed() && ! slot->inbound())
        {
            auto iter (state->fixed.find (slot->remote_endpoint()));
            assert (iter != state->fixed.end ());
            iter->second.success (m_clock.now ());
            if (m_journal.trace) m_journal.trace << beast::leftw (18) <<
                "logic fixed " << slot->remote_endpoint () << " success";
        }

        return result::success;
    }

    /** return a list of addresses suitable for redirection.
        this is a legacy function, redirects should be returned in
        the http handshake and not via tmendpoints.
    */
    std::vector <endpoint>
    redirect (slotimp::ptr const& slot)
    {
        typename sharedstate::access state (m_state);
        redirecthandouts h (slot);
        state->livecache.hops.shuffle();
        handout (&h, (&h)+1,
            state->livecache.hops.begin(),
                state->livecache.hops.end());
        return std::move(h.list());
    }

    /** create new outbound connection attempts as needed.
        this implements peerfinder's "outbound connection strategy"
    */
    // vfalco todo this should add the returned addresses to the
    //             squelch list in one go once the list is built,
    //             rather than having each module add to the squelch list.
    std::vector <beast::ip::endpoint>
    autoconnect()
    {
        std::vector <beast::ip::endpoint> const none;

        typename sharedstate::access state (m_state);

        // count how many more outbound attempts to make
        //
        auto needed (state->counts.attempts_needed ());
        if (needed == 0)
            return none;

        connecthandouts h (needed, m_squelches);

        // make sure we don't connect to already-connected entries.
        squelch_slots (state);

        // 1. use fixed if:
        //    fixed active count is below fixed count and
        //      ( there are eligible fixed addresses to try or
        //        any outbound attempts are in progress)
        //
        if (state->counts.fixed_active() < state->fixed.size ())
        {
            get_fixed (needed, h.list(), m_squelches, state);

            if (! h.list().empty ())
            {
                if (m_journal.debug) m_journal.debug << beast::leftw (18) <<
                    "logic connect " << h.list().size() << " fixed";
                return h.list();
            }

            if (state->counts.attempts() > 0)
            {
                if (m_journal.debug) m_journal.debug << beast::leftw (18) <<
                    "logic waiting on " <<
                        state->counts.attempts() << " attempts";
                return none;
            }
        }

        // only proceed if auto connect is enabled and we
        // have less than the desired number of outbound slots
        //
        if (! state->config.autoconnect ||
            state->counts.out_active () >= state->counts.out_max ())
            return none;

        // 2. use livecache if:
        //    there are any entries in the cache or
        //    any outbound attempts are in progress
        //
        {
            state->livecache.hops.shuffle();
            handout (&h, (&h)+1,
                state->livecache.hops.rbegin(),
                    state->livecache.hops.rend());
            if (! h.list().empty ())
            {
                if (m_journal.debug) m_journal.debug << beast::leftw (18) <<
                    "logic connect " << h.list().size () << " live " <<
                    ((h.list().size () > 1) ? "endpoints" : "endpoint");
                return h.list();
            }
            else if (state->counts.attempts() > 0)
            {
                if (m_journal.debug) m_journal.debug << beast::leftw (18) <<
                    "logic waiting on " <<
                        state->counts.attempts() << " attempts";
                return none;
            }
        }

        /*  3. bootcache refill
            if the bootcache is empty, try to get addresses from the current
            set of sources and add them into the bootstrap cache.

            pseudocode:
                if (    domainnames.count() > 0 and (
                           unusedbootstrapips.count() == 0
                        or activenameresolutions.count() > 0) )
                    foroneormore (domainname that hasn't been resolved recently)
                        contact domainname and add entries to the unusedbootstrapips
                    return;
        */

        // 4. use bootcache if:
        //    there are any entries we haven't tried lately
        //
        for (auto iter (state->bootcache.begin());
            ! h.full() && iter != state->bootcache.end(); ++iter)
            h.try_insert (*iter);

        if (! h.list().empty ())
        {
            if (m_journal.debug) m_journal.debug << beast::leftw (18) <<
                "logic connect " << h.list().size () << " boot " <<
                ((h.list().size () > 1) ? "addresses" : "address");
            return h.list();
        }

        // if we get here we are stuck
        return none;
    }

    std::vector<std::pair<slot::ptr, std::vector<endpoint>>>
    buildendpointsforpeers()
    {
        std::vector<std::pair<slot::ptr, std::vector<endpoint>>> result;

        typename sharedstate::access state (m_state);

        clock_type::time_point const now = m_clock.now();
        if (m_whenbroadcast <= now)
        {
            std::vector <slothandouts> targets;

            {
                // build list of active slots
                std::vector <slotimp::ptr> slots;
                slots.reserve (state->slots.size());
                std::for_each (state->slots.cbegin(), state->slots.cend(),
                    [&slots](slots::value_type const& value)
                    {
                        if (value.second->state() == slot::active)
                            slots.emplace_back (value.second);
                    });
                std::random_shuffle (slots.begin(), slots.end());

                // build target vector
                targets.reserve (slots.size());
                std::for_each (slots.cbegin(), slots.cend(),
                    [&targets](slotimp::ptr const& slot)
                    {
                        targets.emplace_back (slot);
                    });
            }

            /* vfalco note
                this is a temporary measure. once we know our own ip
                address, the correct solution is to put it into the livecache
                at hops 0, and go through the regular handout path. this way
                we avoid handing our address out too frequenty, which this code
                suffers from.
            */
            // add an entry for ourselves if:
            // 1. we want incoming
            // 2. we have slots
            // 3. we haven't failed the firewalled test
            //
            if (state->config.wantincoming &&
                state->counts.inboundslots() > 0)
            {
                endpoint ep;
                ep.hops = 0;
                ep.address = beast::ip::endpoint (
                    beast::ip::addressv4 ()).at_port (
                        state->config.listeningport);
                for (auto& t : targets)
                    t.insert (ep);
            }

            // build sequence of endpoints by hops
            state->livecache.hops.shuffle();
            handout (targets.begin(), targets.end(),
                state->livecache.hops.begin(),
                    state->livecache.hops.end());

            // broadcast
            for (auto const& t : targets)
            {
                slotimp::ptr const& slot = t.slot();
                auto const& list = t.list();
                if (m_journal.trace) m_journal.trace << beast::leftw (18) <<
                    "logic sending " << slot->remote_endpoint() <<
                    " with " << list.size() <<
                    ((list.size() == 1) ? " endpoint" : " endpoints");
                result.push_back (std::make_pair (slot, list));
            }

            m_whenbroadcast = now + tuning::secondspermessage;
        }

        return result;
    }

    void once_per_second()
    {
        typename sharedstate::access state (m_state);

        // expire the livecache
        state->livecache.expire ();

        // expire the recent cache in each slot
        for (auto const& entry : state->slots)
            entry.second->expire();

        // expire the recent attempts table
        beast::expire (m_squelches,
            tuning::recentattemptduration);

        state->bootcache.periodicactivity ();
    }

    //--------------------------------------------------------------------------

    // validate and clean up the list that we received from the slot.
    void preprocess (slotimp::ptr const& slot, endpoints& list,
        typename sharedstate::access& state)
    {
        bool neighbor (false);
        for (auto iter (list.begin()); iter != list.end();)
        {
            endpoint& ep (*iter);

            // enforce hop limit
            if (ep.hops > tuning::maxhops)
            {
                if (m_journal.warning) m_journal.warning << beast::leftw (18) <<
                    "endpoints drop " << ep.address <<
                    " for excess hops " << ep.hops;
                iter = list.erase (iter);
                continue;
            }

            // see if we are directly connected
            if (ep.hops == 0)
            {
                if (! neighbor)
                {
                    // fill in our neighbors remote address
                    neighbor = true;
                    ep.address = slot->remote_endpoint().at_port (
                        ep.address.port ());
                }
                else
                {
                    if (m_journal.warning) m_journal.warning << beast::leftw (18) <<
                        "endpoints drop " << ep.address <<
                        " for extra self";
                    iter = list.erase (iter);
                    continue;
                }
            }

            // discard invalid addresses
            if (! is_valid_address (ep.address))
            {
                if (m_journal.warning) m_journal.warning << beast::leftw (18) <<
                    "endpoints drop " << ep.address <<
                    " as invalid";
                iter = list.erase (iter);
                continue;
            }

            // filter duplicates
            if (std::any_of (list.begin(), iter,
                [ep](endpoints::value_type const& other)
                {
                    return ep.address == other.address;
                }))
            {
                if (m_journal.warning) m_journal.warning << beast::leftw (18) <<
                    "endpoints drop " << ep.address <<
                    " as duplicate";
                iter = list.erase (iter);
                continue;
            }

            // increment hop count on the incoming message, so
            // we store it at the hop count we will send it at.
            //
            ++ep.hops;

            ++iter;
        }
    }

    void on_endpoints (slotimp::ptr const& slot, endpoints list)
    {
        if (m_journal.trace) m_journal.trace << beast::leftw (18) <<
            "endpoints from " << slot->remote_endpoint () <<
            " contained " << list.size () <<
            ((list.size() > 1) ? " entries" : " entry");

        typename sharedstate::access state (m_state);

        // the object must exist in our table
        assert (state->slots.find (slot->remote_endpoint ()) !=
            state->slots.end ());

        // must be handshaked!
        assert (slot->state() == slot::active);

        preprocess (slot, list, state);

        clock_type::time_point const now (m_clock.now());

        for (auto const& ep : list)
        {
            assert (ep.hops != 0);

            slot->recent.insert (ep.address, ep.hops);

            // note hops has been incremented, so 1
            // means a directly connected neighbor.
            //
            if (ep.hops == 1)
            {
                if (slot->connectivitycheckinprogress)
                {
                    if (m_journal.warning) m_journal.warning << beast::leftw (18) <<
                        "logic testing " << ep.address << " already in progress";
                    continue;
                }

                if (! slot->checked)
                {
                    // mark that a check for this slot is now in progress.
                    slot->connectivitycheckinprogress = true;

                    // test the slot's listening port before
                    // adding it to the livecache for the first time.
                    //
                    m_checker.async_connect (ep.address, std::bind (
                        &logic::checkcomplete, this, slot->remote_endpoint(),
                            ep.address, std::placeholders::_1));

                    // note that we simply discard the first endpoint
                    // that the neighbor sends when we perform the
                    // listening test. they will just send us another
                    // one in a few seconds.

                    continue;
                }

                // if they failed the test then skip the address
                if (! slot->canaccept)
                    continue;
            }

            // we only add to the livecache if the neighbor passed the
            // listening test, else we silently drop their messsage
            // since their listening port is misconfigured.
            //
            state->livecache.insert (ep);
            state->bootcache.insert (ep.address);
        }

        slot->whenacceptendpoints = now + tuning::secondspermessage;
    }

    //--------------------------------------------------------------------------

    void on_legacy_endpoints (ipaddresses const& list)
    {
        // ignoring them also seems a valid choice.
        typename sharedstate::access state (m_state);
        for (ipaddresses::const_iterator iter (list.begin());
            iter != list.end(); ++iter)
            state->bootcache.insert (*iter);
    }

    void remove (slotimp::ptr const& slot, typename sharedstate::access& state)
    {
        slots::iterator const iter (state->slots.find (
            slot->remote_endpoint ()));
        // the slot must exist in the table
        assert (iter != state->slots.end ());
        // remove from slot by ip table
        state->slots.erase (iter);
        // remove the key if present
        if (slot->public_key () != boost::none)
        {
            keys::iterator const iter (state->keys.find (*slot->public_key()));
            // key must exist
            assert (iter != state->keys.end ());
            state->keys.erase (iter);
        }
        // remove from connected address table
        {
            auto const iter (state->connected_addresses.find (
                slot->remote_endpoint().at_port (0)));
            // address must exist
            assert (iter != state->connected_addresses.end ());
            state->connected_addresses.erase (iter);
        }

        // update counts
        state->counts.remove (*slot);
    }

    void on_closed (slotimp::ptr const& slot)
    {
        typename sharedstate::access state (m_state);

        remove (slot, state);

        // mark fixed slot failure
        if (slot->fixed() && ! slot->inbound() && slot->state() != slot::active)
        {
            auto iter (state->fixed.find (slot->remote_endpoint()));
            assert (iter != state->fixed.end ());
            iter->second.failure (m_clock.now ());
            if (m_journal.debug) m_journal.debug << beast::leftw (18) <<
                "logic fixed " << slot->remote_endpoint () << " failed";
        }

        // do state specific bookkeeping
        switch (slot->state())
        {
        case slot::accept:
            if (m_journal.trace) m_journal.trace << beast::leftw (18) <<
                "logic accept " << slot->remote_endpoint () << " failed";
            break;

        case slot::connect:
        case slot::connected:
            state->bootcache.on_failure (slot->remote_endpoint ());
            // vfalco todo if the address exists in the ephemeral/live
            //             endpoint livecache then we should mark the failure
            // as if it didn't pass the listening test. we should also
            // avoid propagating the address.
            break;

        case slot::active:
            if (m_journal.trace) m_journal.trace << beast::leftw (18) <<
                "logic close " << slot->remote_endpoint();
            break;

        case slot::closing:
            if (m_journal.trace) m_journal.trace << beast::leftw (18) <<
                "logic finished " << slot->remote_endpoint();
            break;

        default:
            assert (false);
            break;
        }
    }

    // insert a set of redirect ip addresses into the bootcache
    template <class fwditer>
    void
    onredirects (fwditer first, fwditer last,
        boost::asio::ip::tcp::endpoint const& remote_address);

    //--------------------------------------------------------------------------

    // returns `true` if the address matches a fixed slot address
    bool fixed (beast::ip::endpoint const& endpoint, typename sharedstate::access& state) const
    {
        for (auto const& entry : state->fixed)
            if (entry.first == endpoint)
                return true;
        return false;
    }

    // returns `true` if the address matches a fixed slot address
    // note that this does not use the port information in the ip::endpoint
    bool fixed (beast::ip::address const& address, typename sharedstate::access& state) const
    {
        for (auto const& entry : state->fixed)
            if (entry.first.address () == address)
                return true;
        return false;
    }

    //--------------------------------------------------------------------------
    //
    // connection strategy
    //
    //--------------------------------------------------------------------------

    /** adds eligible fixed addresses for outbound attempts. */
    template <class container>
    void get_fixed (std::size_t needed, container& c,
        typename connecthandouts::squelches& squelches,
        typename sharedstate::access& state)
    {
        auto const now (m_clock.now());
        for (auto iter = state->fixed.begin ();
            needed && iter != state->fixed.end (); ++iter)
        {
            auto const& address (iter->first.address());
            if (iter->second.when() <= now && squelches.find(address) ==
                    squelches.end() && std::none_of (
                        state->slots.cbegin(), state->slots.cend(),
                    [address](slots::value_type const& v)
                    {
                        return address == v.first.address();
                    }))
            {
                squelches.insert(iter->first.address());
                c.push_back (iter->first);
                --needed;
            }
        }
    }

    //--------------------------------------------------------------------------

    // adds slot addresses to the squelched set
    void squelch_slots (typename sharedstate::access& state)
    {
        for (auto const& s : state->slots)
        {
            auto const result (m_squelches.insert (
                s.second->remote_endpoint().address()));
            if (! result.second)
                m_squelches.touch (result.first);
        }
    }

    //--------------------------------------------------------------------------

    void
    addstaticsource (beast::sharedptr <source> const& source)
    {
        fetch (source);
    }

    void
    addsource (beast::sharedptr <source> const& source)
    {
        m_sources.push_back (source);
    }

    //--------------------------------------------------------------------------
    //
    // bootcache livecache sources
    //
    //--------------------------------------------------------------------------

    // add one address.
    // returns `true` if the address is new.
    //
    bool addbootcacheaddress (beast::ip::endpoint const& address,
        typename sharedstate::access& state)
    {
        return state->bootcache.insert (address);
    }

    // add a set of addresses.
    // returns the number of addresses added.
    //
    int addbootcacheaddresses (ipaddresses const& list)
    {
        int count (0);
        typename sharedstate::access state (m_state);
        for (auto addr : list)
        {
            if (addbootcacheaddress (addr, state))
                ++count;
        }
        return count;
    }

    // fetch bootcache addresses from the specified source.
    void fetch (beast::sharedptr <source> const& source)
    {
        source::results results;

        {
            {
                typename sharedstate::access state (m_state);
                if (state->stopping)
                    return;
                state->fetchsource = source;
            }

            // vfalco note the fetch is synchronous,
            //             not sure if that's a good thing.
            //
            source->fetch (results, m_journal);

            {
                typename sharedstate::access state (m_state);
                if (state->stopping)
                    return;
                state->fetchsource = nullptr;
            }
        }

        if (! results.error)
        {
            int const count (addbootcacheaddresses (results.addresses));
            if (m_journal.info) m_journal.info << beast::leftw (18) <<
                "logic added " << count <<
                " new " << ((count == 1) ? "address" : "addresses") <<
                " from " << source->name();
        }
        else
        {
            if (m_journal.error) m_journal.error << beast::leftw (18) <<
                "logic failed " << "'" << source->name() << "' fetch, " <<
                results.error.message();
        }
    }

    //--------------------------------------------------------------------------
    //
    // endpoint message handling
    //
    //--------------------------------------------------------------------------

    // returns true if the ip::endpoint contains no invalid data.
    bool is_valid_address (beast::ip::endpoint const& address)
    {
        if (is_unspecified (address))
            return false;
        if (! is_public (address))
            return false;
        if (address.port() == 0)
            return false;
        return true;
    }

    //--------------------------------------------------------------------------
    //
    // propertystream
    //
    //--------------------------------------------------------------------------

    void writeslots (beast::propertystream::set& set, slots const& slots)
    {
        for (auto const& entry : slots)
        {
            beast::propertystream::map item (set);
            slotimp const& slot (*entry.second);
            if (slot.local_endpoint () != boost::none)
                item ["local_address"] = to_string (*slot.local_endpoint ());
            item ["remote_address"]   = to_string (slot.remote_endpoint ());
            if (slot.inbound())
                item ["inbound"]    = "yes";
            if (slot.fixed())
                item ["fixed"]      = "yes";
            if (slot.cluster())
                item ["cluster"]    = "yes";

            item ["state"] = statestring (slot.state());
        }
    }

    void onwrite (beast::propertystream::map& map)
    {
        typename sharedstate::access state (m_state);

        // vfalco note these ugly casts are needed because
        //             of how std::size_t is declared on some linuxes
        //
        map ["bootcache"]   = std::uint32_t (state->bootcache.size());
        map ["fixed"]       = std::uint32_t (state->fixed.size());

        {
            beast::propertystream::set child ("peers", map);
            writeslots (child, state->slots);
        }

        {
            beast::propertystream::map child ("counts", map);
            state->counts.onwrite (child);
        }

        {
            beast::propertystream::map child ("config", map);
            state->config.onwrite (child);
        }

        {
            beast::propertystream::map child ("livecache", map);
            state->livecache.onwrite (child);
        }

        {
            beast::propertystream::map child ("bootcache", map);
            state->bootcache.onwrite (child);
        }
    }

    //--------------------------------------------------------------------------
    //
    // diagnostics
    //
    //--------------------------------------------------------------------------

    state const& state () const
    {
        return *typename sharedstate::constaccess (m_state);
    }

    counts const& counts () const
    {
        return typename sharedstate::constaccess (m_state)->counts;
    }

    static std::string statestring (slot::state state)
    {
        switch (state)
        {
        case slot::accept:     return "accept";
        case slot::connect:    return "connect";
        case slot::connected:  return "connected";
        case slot::active:     return "active";
        case slot::closing:    return "closing";
        default:
            break;
        };
        return "?";
    }
};

//------------------------------------------------------------------------------

template <class checker>
template <class fwditer>
void
logic<checker>::onredirects (fwditer first, fwditer last,
    boost::asio::ip::tcp::endpoint const& remote_address)
{
    typename sharedstate::access state (m_state);
    std::size_t n = 0;
    for(;first != last && n < tuning::maxredirects; ++first, ++n)
        state->bootcache.insert(
            beast::ipaddressconversion::from_asio(*first));
    if (n > 0)
        if (m_journal.trace) m_journal.trace << beast::leftw (18) <<
            "logic add " << n << " redirect ips from " << remote_address;
}

}
}

#endif
