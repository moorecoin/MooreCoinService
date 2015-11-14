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

#ifndef ripple_ledgerconsensus_h
#define ripple_ledgerconsensus_h

#include <ripple/app/ledger/ledger.h>
#include <ripple/app/ledger/ledgerproposal.h>
#include <ripple/app/misc/canonicaltxset.h>
#include <ripple/app/misc/feevote.h>
#include <ripple/app/tx/localtxs.h>
#include <ripple/json/json_value.h>
#include <ripple/overlay/peer.h>
#include <ripple/protocol/rippleledgerhash.h>
#include <beast/chrono/abstract_clock.h>
#include <chrono>
#include <ripple/app/misc/dividendvote.h>

namespace ripple {

/** manager for achieving consensus on the next ledger.

    this object is created when the consensus process starts, and
    is destroyed when the process is complete.
*/
class ledgerconsensus
{
public:
    typedef beast::abstract_clock <std::chrono::steady_clock> clock_type;

    virtual ~ledgerconsensus() = 0;

    virtual int startup () = 0;

    virtual json::value getjson (bool full) = 0;

    virtual ledger::ref peekpreviousledger () = 0;

    virtual uint256 getlcl () = 0;

    virtual shamap::pointer gettransactiontree (uint256 const& hash,
        bool doacquire) = 0;

    virtual void mapcomplete (uint256 const& hash, shamap::ref map,
        bool acquired) = 0;

    virtual bool stillneedtxset (uint256 const& hash) = 0;

    virtual void checklcl () = 0;

    virtual void handlelcl (uint256 const& lclhash) = 0;

    virtual void timerentry () = 0;

    // state handlers
    virtual void statepreclose () = 0;
    virtual void stateestablish () = 0;
    virtual void statefinished () = 0;
    virtual void stateaccepted () = 0;

    virtual bool haveconsensus (bool forreal) = 0;

    virtual bool peerposition (ledgerproposal::ref) = 0;

    virtual bool peerhasset (peer::ptr const& peer, uint256 const& set,
        protocol::txsetstatus status) = 0;

    virtual shamapaddnode peergavenodes (peer::ptr const& peer,
        uint256 const& sethash,
        const std::list<shamapnodeid>& nodeids,
        const std::list< blob >& nodedata) = 0;

    virtual bool isourpubkey (const rippleaddress & k) = 0;

    // test/debug
    virtual void simulate () = 0;
};

std::shared_ptr <ledgerconsensus>
make_ledgerconsensus (ledgerconsensus::clock_type& clock, localtxs& localtx,
    ledgerhash const & prevlclhash, ledger::ref previousledger,
        std::uint32_t closetime, feevote& feevote, dividendvote& dividendvote);

void
applytransactions(shamap::ref set, ledger::ref applyledger,
                  ledger::ref checkledger,
                  canonicaltxset& retriabletransactions, bool openlgr);

} // ripple

#endif
