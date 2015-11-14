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

#ifndef ripple_peerfinder_counts_h_included
#define ripple_peerfinder_counts_h_included

#include <ripple/peerfinder/manager.h>
#include <ripple/peerfinder/slot.h>
#include <ripple/peerfinder/impl/tuning.h>
#include <random>

namespace ripple {
namespace peerfinder {

/** manages the count of available connections for the various slots. */
class counts
{
public:
    counts ()
        : m_attempts (0)
        , m_active (0)
        , m_in_max (0)
        , m_in_active (0)
        , m_out_max (0)
        , m_out_active (0)
        , m_fixed (0)
        , m_fixed_active (0)
        , m_cluster (0)

        , m_acceptcount (0)
        , m_closingcount (0)
    {
#if 1
        std::random_device rd;
        std::mt19937 gen (rd());
        m_roundingthreshold =
            std::generate_canonical <double, 10> (gen);
#else
        m_roundingthreshold = random::getsystemrandom().nextdouble();
#endif
    }

    //--------------------------------------------------------------------------

    /** adds the slot state and properties to the slot counts. */
    void add (slot const& s)
    {
        adjust (s, 1);
    }

    /** removes the slot state and properties from the slot counts. */
    void remove (slot const& s)
    {
        adjust (s, -1);
    }

    /** returns `true` if the slot can become active. */
    bool can_activate (slot const& s) const
    {
        // must be handshaked and in the right state
        assert (s.state() == slot::connected || s.state() == slot::accept);

        if (s.fixed () || s.cluster ())
            return true;

        if (s.inbound ())
            return m_in_active < m_in_max;

        return m_out_active < m_out_max;
    }

    /** returns the number of attempts needed to bring us to the max. */
    std::size_t attempts_needed () const
    {
        if (m_attempts >= tuning::maxconnectattempts)
            return 0;
        return tuning::maxconnectattempts - m_attempts;
    }

    /** returns the number of outbound connection attempts. */
    std::size_t attempts () const
    {
        return m_attempts;
    };

    /** returns the total number of outbound slots. */
    int out_max () const
    {
        return m_out_max;
    }

    /** returns the number of outbound peers assigned an open slot.
        fixed peers do not count towards outbound slots used.
    */
    int out_active () const
    {
        return m_out_active;
    }

    /** returns the number of fixed connections. */
    std::size_t fixed () const
    {
        return m_fixed;
    }

    /** returns the number of active fixed connections. */
    std::size_t fixed_active () const
    {
        return m_fixed_active;
    }

    //--------------------------------------------------------------------------

    /** called when the config is set or changed. */
    void onconfig (config const& config)
    {
        // calculate the number of outbound peers we want. if we dont want or can't
        // accept incoming, this will simply be equal to maxpeers. otherwise
        // we calculate a fractional amount based on percentages and pseudo-randomly
        // round up or down.
        //
        if (config.wantincoming)
        {
            // round outpeers upwards using a bernoulli distribution
            m_out_max = std::floor (config.outpeers);
            if (m_roundingthreshold < (config.outpeers - m_out_max))
                ++m_out_max;
        }
        else
        {
            m_out_max = config.maxpeers;
        }

        // calculate the largest number of inbound connections we could take.
        if (config.maxpeers >= m_out_max)
            m_in_max = config.maxpeers - m_out_max;
        else
            m_in_max = 0;
    }

    /** returns the number of accepted connections that haven't handshaked. */
    int acceptcount() const
    {
        return m_acceptcount;
    }

    /** returns the number of connection attempts currently active. */
    int connectcount() const
    {
        return m_attempts;
    }

    /** returns the number of connections that are gracefully closing. */
    int closingcount () const
    {
        return m_closingcount;
    }

    /** returns the total number of inbound slots. */
    int inboundslots () const
    {
        return m_in_max;
    }

    /** returns the number of inbound peers assigned an open slot. */
    int inboundactive () const
    {
        return m_in_active;
    }

    /** returns the total number of active peers excluding fixed peers. */
    int totalactive () const
    {
        return m_in_active + m_out_active;
    }

    /** returns the number of unused inbound slots.
        fixed peers do not deduct from inbound slots or count towards totals.
    */
    int inboundslotsfree () const
    {
        if (m_in_active < m_in_max)
            return m_in_max - m_in_active;
        return 0;
    }

    /** returns the number of unused outbound slots.
        fixed peers do not deduct from outbound slots or count towards totals.
    */
    int outboundslotsfree () const
    {
        if (m_out_active < m_out_max)
            return m_out_max - m_out_active;
        return 0;
    }

    //--------------------------------------------------------------------------

    /** returns true if the slot logic considers us "connected" to the network. */
    bool isconnectedtonetwork () const
    {
        // we will consider ourselves connected if we have reached
        // the number of outgoing connections desired, or if connect
        // automatically is false.
        //
        // fixed peers do not count towards the active outgoing total.

        if (m_out_max > 0)
            return false;

        return true;
    }

    /** output statistics. */
    void onwrite (beast::propertystream::map& map)
    {
        map ["accept"]  = acceptcount ();
        map ["connect"] = connectcount ();
        map ["close"]   = closingcount ();
        map ["in"]      << m_in_active << "/" << m_in_max;
        map ["out"]     << m_out_active << "/" << m_out_max;
        map ["fixed"]   = m_fixed_active;
        map ["cluster"] = m_cluster;
        map ["total"]   = m_active;
    }

    /** records the state for diagnostics. */
    std::string state_string () const
    {
        std::stringstream ss;
        ss <<
            m_out_active << "/" << m_out_max << " out, " <<
            m_in_active << "/" << m_in_max << " in, " <<
            connectcount() << " connecting, " <<
            closingcount() << " closing"
            ;
        return ss.str();
    }

    //--------------------------------------------------------------------------
private:
    // adjusts counts based on the specified slot, in the direction indicated.
    void adjust (slot const& s, int const n)
    {
        if (s.fixed ())
            m_fixed += n;

        if (s.cluster ())
            m_cluster += n;

        switch (s.state ())
        {
        case slot::accept:
            assert (s.inbound ());
            m_acceptcount += n;
            break;

        case slot::connect:
        case slot::connected:
            assert (! s.inbound ());
            m_attempts += n;
            break;

        case slot::active:
            if (s.fixed ())
                m_fixed_active += n;
            if (! s.fixed () && ! s.cluster ())
            {
                if (s.inbound ())
                    m_in_active += n;
                else
                    m_out_active += n;
            }
            m_active += n;
            break;

        case slot::closing:
            m_closingcount += n;
            break;

        default:
            assert (false);
            break;
        };
    }

private:
    /** outbound connection attempts. */
    int m_attempts;

    /** active connections, including fixed and cluster. */
    std::size_t m_active;

    /** total number of inbound slots. */
    std::size_t m_in_max;

    /** number of inbound slots assigned to active peers. */
    std::size_t m_in_active;

    /** maximum desired outbound slots. */
    std::size_t m_out_max;

    /** active outbound slots. */
    std::size_t m_out_active;

    /** fixed connections. */
    std::size_t m_fixed;

    /** active fixed connections. */
    std::size_t m_fixed_active;

    /** cluster connections. */
    std::size_t m_cluster;




    // number of inbound connections that are
    // not active or gracefully closing.
    int m_acceptcount;

    // number of connections that are gracefully closing.
    int m_closingcount;

    /** fractional threshold below which we round down.
        this is used to round the value of config::outpeers up or down in
        such a way that the network-wide average number of outgoing
        connections approximates the recommended, fractional value.
    */
    double m_roundingthreshold;
};

}
}

#endif
