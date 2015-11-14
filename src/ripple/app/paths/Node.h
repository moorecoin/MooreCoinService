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

#ifndef ripple_app_path_node_h
#define ripple_app_path_node_h

#include <ripple/app/paths/nodedirectory.h>
#include <ripple/app/paths/types.h>
#include <ripple/protocol/uinttypes.h>

namespace ripple {
namespace path {

struct node
{
    typedef std::vector<node> list;

    inline bool isaccount() const
    {
        return (uflags & stpathelement::typeaccount);
    }

    json::value getjson () const;

    bool operator == (node const&) const;

    std::uint16_t uflags;       // --> from path.

    account account_;           // --> accounts: receiving/sending account.

    issue issue_;               // --> accounts: receive and send, offers: send.
                                // --- for offer's next has currency out.

    stamount transferrate_;    // transfer rate for issuer.

    // computed by reverse.
    stamount sarevredeem;        // <-- amount to redeem to next.
    stamount sarevissue;         // <-- amount to issue to next, limited by
                                 //     credit and outstanding ious.  issue
                                 //     isn't used by offers.
    stamount sarevdeliver;       // <-- amount to deliver to next regardless of
                                 // fee.

    // computed by forward.
    stamount safwdredeem;        // <-- amount node will redeem to next.
    stamount safwdissue;         // <-- amount node will issue to next.
                                 //     issue isn't used by offers.
    stamount safwddeliver;       // <-- amount to deliver to next regardless of
                                 // fee.

    // for offers:

    stamount saratemax;

    // the nodes are partitioned into a buckets called "directories".
    //
    // each "directory" contains nodes with exactly the same "quality" (meaning
    // the conversion rate between one corrency and the next).
    //
    // the "directories" are ordered in "increasing" "quality" value, which
    // means that the first "directory" has the "best" (i.e. numerically least)
    // "quality".
    // https://ripple.com/wiki/ledger_format#prioritizing_a_continuous_key_space

    nodedirectory directory;

    stamount saofrrate;          // for correct ratio.

    // paymentnode
    bool bentryadvance;          // need to advance entry.
    unsigned int uentry;
    uint256 offerindex_;
    sle::pointer sleoffer;
    account offerowneraccount_;

    // do we need to refresh saofferfunds, satakerpays, & satakergets?
    bool bfundsdirty;
    stamount saofferfunds;
    stamount satakerpays;
    stamount satakergets;

    /** clear input and output amounts. */
    void clear()
    {
        sarevredeem.clear ();
        sarevissue.clear ();
        sarevdeliver.clear ();
        safwddeliver.clear ();
    }
};

} // path
} // ripple

#endif
