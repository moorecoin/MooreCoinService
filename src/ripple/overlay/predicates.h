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

#ifndef ripple_overlay_predicates_h_included
#define ripple_overlay_predicates_h_included

#include <ripple/overlay/message.h>
#include <ripple/overlay/peer.h>

#include <set>

namespace ripple {

/** sends a message to all peers */
struct send_always
{
    typedef void return_type;

    message::pointer const& msg;

    send_always(message::pointer const& m)
        : msg(m)
    { }

    void operator()(peer::ptr const& peer) const
    {
        peer->send (msg);
    }
};

//------------------------------------------------------------------------------

/** sends a message to match peers */
template <typename predicate>
struct send_if_pred
{
    typedef void return_type;

    message::pointer const& msg;
    predicate const& predicate;

    send_if_pred(message::pointer const& m, predicate const& p)
    : msg(m), predicate(p)
    { }

    void operator()(peer::ptr const& peer) const
    {
        if (predicate (peer))
            peer->send (msg);
    }
};

/** helper function to aid in type deduction */
template <typename predicate>
send_if_pred<predicate> send_if (
    message::pointer const& m,
        predicate const &f)
{
    return send_if_pred<predicate>(m, f);
}

//------------------------------------------------------------------------------

/** sends a message to non-matching peers */
template <typename predicate>
struct send_if_not_pred
{
    typedef void return_type;

    message::pointer const& msg;
    predicate const& predicate;

    send_if_not_pred(message::pointer const& m, predicate const& p)
        : msg(m), predicate(p)
    { }

    void operator()(peer::ptr const& peer) const
    {
        if (!predicate (peer))
            peer->send (msg);
    }
};

/** helper function to aid in type deduction */
template <typename predicate>
send_if_not_pred<predicate> send_if_not (
    message::pointer const& m,
        predicate const &f)
{
    return send_if_not_pred<predicate>(m, f);
}

//------------------------------------------------------------------------------

/** select the specific peer */
struct match_peer
{
    peer const* matchpeer;

    match_peer (peer const* match = nullptr)
        : matchpeer (match)
    { }

    bool operator() (peer::ptr const& peer) const
    {
        if(matchpeer && (peer.get () == matchpeer))
            return true;

        return false;
    }
};

//------------------------------------------------------------------------------

/** select all peers (except optional excluded) that are in our cluster */
struct peer_in_cluster
{
    match_peer skippeer;

    peer_in_cluster (peer const* skip = nullptr)
        : skippeer (skip)
    { }

    bool operator() (peer::ptr const& peer) const
    {
        if (skippeer (peer))
            return false;

        if (! peer->cluster())
            return false;

        return true;
    }
};

//------------------------------------------------------------------------------

/** select all peers that are in the specified set */
struct peer_in_set
{
    std::set <peer::id_t> const& peerset;

    peer_in_set (std::set<peer::id_t> const& peers)
        : peerset (peers)
    { }

    bool operator() (peer::ptr const& peer) const
    {
        if (peerset.count (peer->id ()) == 0)
            return false;

        return true;
    }
};

}

#endif
