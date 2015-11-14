//------------------------------------------------------------------------------
/*
    this file is part of rippled: https://github.com/ripple/rippled
    copyright (c) 2014 ripple labs inc.

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
#include <ripple/app/transactors/transactor.h>
#include <ripple/basics/log.h>
#include <ripple/protocol/indexes.h>

namespace ripple {

class cancelticket
    : public transactor
{
public:
    cancelticket (
        sttx const& txn,
        transactionengineparams params,
        transactionengine* engine)
        : transactor (
            txn,
            params,
            engine,
            deprecatedlogs().journal("cancelticket"))
    {

    }

    ter doapply () override
    {
        assert (mtxnaccount);

        uint256 const ticketid = mtxn.getfieldh256 (sfticketid);

        sle::pointer sleticket = mengine->view ().entrycache (ltticket, ticketid);

        if (!sleticket)
            return tecno_entry;

        account const ticket_owner (sleticket->getfieldaccount160 (sfaccount));

        bool authorized (mtxnaccountid == ticket_owner);

        // the target can also always remove a ticket
        if (!authorized && sleticket->isfieldpresent (sftarget))
            authorized = (mtxnaccountid == sleticket->getfieldaccount160 (sftarget));

        // and finally, anyone can remove an expired ticket
        if (!authorized && sleticket->isfieldpresent (sfexpiration))
        {
            std::uint32_t const expiration = sleticket->getfieldu32 (sfexpiration);

            if (mengine->getledger ()->getparentclosetimenc () >= expiration)
                authorized = true;
        }

        if (!authorized)
            return tecno_permission;

        std::uint64_t const hint (sleticket->getfieldu64 (sfownernode));

        ter const result = mengine->view ().dirdelete (false, hint,
            getownerdirindex (ticket_owner), ticketid, false, (hint == 0));

        mengine->view ().decrementownercount (mtxnaccount);
        mengine->view ().entrydelete (sleticket);

        return result;
    }
};

ter
transact_cancelticket (
    sttx const& txn,
    transactionengineparams params,
    transactionengine* engine)
{
#if ripple_enable_tickets
    return cancelticket (txn, params, engine).apply ();
#else
    return temdisabled;
#endif
}


}
