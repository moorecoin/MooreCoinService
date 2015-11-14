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

class createticket
    : public transactor
{
public:
    createticket (
        sttx const& txn,
        transactionengineparams params,
        transactionengine* engine)
        : transactor (
            txn,
            params,
            engine,
            deprecatedlogs().journal("createticket"))
    {

    }

    ter doapply () override
    {
        assert (mtxnaccount);

        // a ticket counts against the reserve of the issuing account, but we check
        // the starting balance because we want to allow dipping into the reserve to
        // pay fees.
        auto const accountreserve (mengine->getledger ()->getreserve (
            mtxnaccount->getfieldu32 (sfownercount) + 1));

        if (mpriorbalance.getnvalue () < accountreserve)
            return tecinsufficient_reserve;

        std::uint32_t expiration (0);

        if (mtxn.isfieldpresent (sfexpiration))
        {
            expiration = mtxn.getfieldu32 (sfexpiration);

            if (!expiration)
            {
                m_journal.warning <<
                    "malformed ticket requestion: bad expiration";

                return tembad_expiration;
            }

            if (mengine->getledger ()->getparentclosetimenc () >= expiration)
                return tessuccess;
        }

        sle::pointer sleticket = mengine->entrycreate (ltticket,
            getticketindex (mtxnaccountid, mtxn.getsequence ()));

        sleticket->setfieldaccount (sfaccount, mtxnaccountid);
        sleticket->setfieldu32 (sfsequence, mtxn.getsequence ());

        if (expiration != 0)
            sleticket->setfieldu32 (sfexpiration, expiration);

        if (mtxn.isfieldpresent (sftarget))
        {
            account const target_account (mtxn.getfieldaccount160 (sftarget));

            sle::pointer sletarget = mengine->entrycache (ltaccount_root,
                getaccountrootindex (target_account));

            // destination account does not exist.
            if (!sletarget)
                return tecno_target;

            // the issuing account is the default account to which the ticket
            // applies so don't bother saving it if that's what's specified.
            if (target_account != mtxnaccountid)
                sleticket->setfieldaccount (sftarget, target_account);
        }

        std::uint64_t hint;

        auto describer = [&](sle::pointer p, bool b)
        {
            ledger::ownerdirdescriber(p, b, mtxnaccountid);
        };
        ter result = mengine->view ().diradd (
            hint,
            getownerdirindex (mtxnaccountid),
            sleticket->getindex (),
            describer);

        m_journal.trace <<
            "creating ticket " << to_string (sleticket->getindex ()) <<
            ": " << transhuman (result);

        if (result != tessuccess)
            return result;

        sleticket->setfieldu64(sfownernode, hint);

        // if we succeeded, the new entry counts agains the creator's reserve.
        mengine->view ().incrementownercount (mtxnaccount);

        return result;
    }
};

ter
transact_createticket (
    sttx const& txn,
    transactionengineparams params,
    transactionengine* engine)
{
#if ripple_enable_tickets
    return createticket (txn, params, engine).apply ();
#else
    return temdisabled;
#endif
}

}
