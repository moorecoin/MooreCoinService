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
#include <ripple/app/main/application.h>
#include <ripple/app/misc/amendmenttable.h>
#include <ripple/app/misc/networkops.h>
#include <ripple/app/transactors/transactor.h>
#include <ripple/basics/log.h>
#include <ripple/protocol/indexes.h>

namespace ripple {

class change
    : public transactor
{
public:
    change (
        sttx const& txn,
        transactionengineparams params,
        transactionengine* engine)
        : transactor (
            txn,
            params,
            engine,
            deprecatedlogs().journal("change"))
    {
    }

    ter doapply () override
    {
        if (mtxn.gettxntype () == ttamendment)
            return applyamendment ();

        if (mtxn.gettxntype () == ttfee)
            return applyfee ();

        return temunknown;
    }

    ter checksig () override
    {
        if (mtxn.getfieldaccount160 (sfaccount).isnonzero ())
        {
            m_journal.warning << "bad source account";
            return tembad_src_account;
        }

        if (!mtxn.getsigningpubkey ().empty () || !mtxn.getsignature ().empty ())
        {
            m_journal.warning << "bad signature";
            return tembad_signature;
        }

        return tessuccess;
    }

    ter checkseq () override
    {
        if ((mtxn.getsequence () != 0) || mtxn.isfieldpresent (sfprevioustxnid))
        {
            m_journal.warning << "bad sequence";
            return tembad_sequence;
        }

        return tessuccess;
    }

    ter payfee () override
    {
        if (mtxn.gettransactionfee () != stamount ())
        {
            m_journal.warning << "non-zero fee";
            return tembad_fee;
        }

        return tessuccess;
    }

    ter precheck () override
    {
        mtxnaccountid = mtxn.getsourceaccount ().getaccountid ();

        if (mtxnaccountid.isnonzero ())
        {
            m_journal.warning << "bad source id";

            return tembad_src_account;
        }

        if (mparams & tapopen_ledger)
        {
            m_journal.warning << "change transaction against open ledger";
            return teminvalid;
        }

        return tessuccess;
    }

private:
    ter applyamendment ()
    {
        uint256 amendment (mtxn.getfieldh256 (sfamendment));

        auto const index = getledgeramendmentindex ();

        sle::pointer amendmentobject (mengine->entrycache (ltamendments, index));

        if (!amendmentobject)
            amendmentobject = mengine->entrycreate(ltamendments, index);

        stvector256 amendments (amendmentobject->getfieldv256 (sfamendments));

        if (std::find (amendments.begin(), amendments.end(),
            amendment) != amendments.end ())
        {
            return tefalready;
        }

        amendments.push_back (amendment);
        amendmentobject->setfieldv256 (sfamendments, amendments);
        mengine->entrymodify (amendmentobject);

        getapp().getamendmenttable ().enable (amendment);

        if (!getapp().getamendmenttable ().issupported (amendment))
            getapp().getops ().setamendmentblocked ();

        return tessuccess;
    }

    ter applyfee ()
    {
        auto const index = getledgerfeeindex ();

        sle::pointer feeobject = mengine->entrycache (ltfee_settings, index);

        if (!feeobject)
            feeobject = mengine->entrycreate (ltfee_settings, index);

        // vfalco-fixme this generates errors
        // m_journal.trace <<
        //     "previous fee object: " << feeobject->getjson (0);

        feeobject->setfieldu64 (
            sfbasefee, mtxn.getfieldu64 (sfbasefee));
        feeobject->setfieldu32 (
            sfreferencefeeunits, mtxn.getfieldu32 (sfreferencefeeunits));
        feeobject->setfieldu32 (
            sfreservebase, mtxn.getfieldu32 (sfreservebase));
        feeobject->setfieldu32 (
            sfreserveincrement, mtxn.getfieldu32 (sfreserveincrement));

        mengine->entrymodify (feeobject);

        // vfalco-fixme this generates errors
        // m_journal.trace <<
        //     "new fee object: " << feeobject->getjson (0);
        m_journal.warning << "fees have been changed";
        return tessuccess;
    }

    // vfalco todo can this be removed?
    bool musthavevalidaccount () override
    {
        return false;
    }
};


ter
transact_change (
    sttx const& txn,
    transactionengineparams params,
    transactionengine* engine)
{
    return change (txn, params, engine).apply ();
}

}
