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
#include <ripple/core/config.h>
#include <ripple/app/transactors/transactor.h>
#include <ripple/protocol/indexes.h>
#include <ripple/basics/log.h>

namespace ripple {

ter transact_payment (sttx const& txn, transactionengineparams params, transactionengine* engine);
ter transact_setaccount (sttx const& txn, transactionengineparams params, transactionengine* engine);
ter transact_setregularkey (sttx const& txn, transactionengineparams params, transactionengine* engine);
ter transact_settrust (sttx const& txn, transactionengineparams params, transactionengine* engine);
ter transact_createoffer (sttx const& txn, transactionengineparams params, transactionengine* engine);
ter transact_canceloffer (sttx const& txn, transactionengineparams params, transactionengine* engine);
ter transact_addwallet (sttx const& txn, transactionengineparams params, transactionengine* engine);
ter transact_change (sttx const& txn, transactionengineparams params, transactionengine* engine);
ter transact_dividend (sttx const& txn, transactionengineparams params, transactionengine* engine);
ter transact_createticket (sttx const& txn, transactionengineparams params, transactionengine* engine);
ter transact_cancelticket (sttx const& txn, transactionengineparams params, transactionengine* engine);
ter transact_addreferee(sttx const& txn, transactionengineparams params, transactionengine* engine);
ter transact_activeaccount(sttx const& txn, transactionengineparams params, transactionengine* engine);
ter transact_issue(sttx const& txn, transactionengineparams params, transactionengine* engine);

ter
transactor::transact (
    sttx const& txn,
    transactionengineparams params,
    transactionengine* engine)
{
    writelog(lsdebug, transactor)
        << "applying transaction";

    switch (txn.gettxntype ())
    {
    case ttpayment:
        return transact_payment (txn, params, engine);

    case ttaddreferee:
        return transact_addreferee(txn, params, engine);
            
    case ttissue:
        return transact_issue(txn, params, engine);

    case ttactiveaccount:
        return transact_activeaccount(txn, params, engine);
        
    case ttaccount_set:
        return transact_setaccount (txn, params, engine);

    case ttregular_key_set:
        return transact_setregularkey (txn, params, engine);

    case tttrust_set:
        return transact_settrust (txn, params, engine);

    case ttoffer_create:
        return transact_createoffer (txn, params, engine);

    case ttoffer_cancel:
        return transact_canceloffer (txn, params, engine);

    case ttwallet_add:
        return transact_addwallet (txn, params, engine);

    case ttamendment:
    case ttfee:
        return transact_change (txn, params, engine);
        
    case ttdividend:
		return transact_dividend (txn, params, engine);

    case ttticket_create:
        return transact_createticket (txn, params, engine);

    case ttticket_cancel:
        return transact_cancelticket (txn, params, engine);

    default:
        return temunknown;
    }
}

transactor::transactor (
    sttx const& txn,
    transactionengineparams params,
    transactionengine* engine,
    beast::journal journal)
    : mtxn (txn)
    , mengine (engine)
    , mparams (params)
    , mhasauthkey (false)
    , msigmaster (false)
    , m_journal (journal)
{
}

void transactor::calculatefee ()
{
    mfeedue = stamount (mengine->getledger ()->scalefeeload (
        calculatebasefee (), mparams & tapadmin));
}

std::uint64_t transactor::calculatebasefee ()
{
    // returns the fee in fee units
    return getconfig ().transaction_fee_base;
}

ter transactor::payfee ()
{
    stamount sapaid = mtxn.gettransactionfee ();

    if (!islegalnet (sapaid))
        return tembad_amount;

    // only check fee is sufficient when the ledger is open.
    if ((mparams & tapopen_ledger) && sapaid < mfeedue)
    {
        m_journal.trace << "insufficient fee paid: " <<
            sapaid.gettext () << "/" << mfeedue.gettext ();

        return telinsuf_fee_p;
    }

    if (sapaid < zero || !sapaid.isnative ())
        return tembad_fee;

    if (!sapaid)
        return tessuccess;

    if (msourcebalance < sapaid)
    {
        m_journal.trace << "insufficient balance:" <<
            " balance=" << msourcebalance.gettext () <<
            " paid=" << sapaid.gettext ();

        if ((msourcebalance > zero) && (!(mparams & tapopen_ledger)))
        {
            // closed ledger, non-zero balance, less than fee
            msourcebalance.clear ();
            mtxnaccount->setfieldamount (sfbalance, msourcebalance);
            return tecinsuff_fee;
        }

        return terinsuf_fee_b;
    }

    // deduct the fee, so it's not available during the transaction.
    // will only write the account back, if the transaction succeeds.

    msourcebalance -= sapaid;
    mtxnaccount->setfieldamount (sfbalance, msourcebalance);

    return tessuccess;
}

ter transactor::checksig ()
{
    // consistency: check signature
    // verify the transaction's signing public key is the key authorized for signing.
    if (msigningpubkey.getaccountid () == mtxnaccountid)
    {
        // authorized to continue.
        msigmaster = true;
        if (mtxnaccount->isflag(lsfdisablemaster))
        return tefmaster_disabled;
    }
    else if (mhasauthkey && msigningpubkey.getaccountid () == mtxnaccount->getfieldaccount160 (sfregularkey))
    {
        // authorized to continue.
    }
    else if (mhasauthkey)
    {
        m_journal.trace << "applytransaction: delay: not authorized to use account.";
        return tefbad_auth;
    }
    else
    {
        m_journal.trace << "applytransaction: invalid: not authorized to use account.";

        return tembad_auth_master;
    }

    return tessuccess;
}

ter transactor::checkseq ()
{
    std::uint32_t t_seq = mtxn.getsequence ();
    std::uint32_t a_seq = mtxnaccount->getfieldu32 (sfsequence);

    m_journal.trace << "aseq=" << a_seq << ", tseq=" << t_seq;

    if (t_seq != a_seq)
    {
        if (a_seq < t_seq)
        {
            m_journal.trace << "apply: transaction has future sequence number";

            return terpre_seq;
        }
        else
        {
            if (mengine->getledger ()->hastransaction (mtxn.gettransactionid ()))
                return tefalready;
        }

        m_journal.warning << "apply: transaction has past sequence number";

        return tefpast_seq;
    }

    // deprecated: do not use
    if (mtxn.isfieldpresent (sfprevioustxnid) &&
            (mtxnaccount->getfieldh256 (sfprevioustxnid) != mtxn.getfieldh256 (sfprevioustxnid)))
        return tefwrong_prior;

    if (mtxn.isfieldpresent (sfaccounttxnid) &&
            (mtxnaccount->getfieldh256 (sfaccounttxnid) != mtxn.getfieldh256 (sfaccounttxnid)))
        return tefwrong_prior;

    if (mtxn.isfieldpresent (sflastledgersequence) &&
            (mengine->getledger()->getledgerseq() > mtxn.getfieldu32 (sflastledgersequence)))
        return tefmax_ledger;

    mtxnaccount->setfieldu32 (sfsequence, t_seq + 1);

    if (mtxnaccount->isfieldpresent (sfaccounttxnid))
        mtxnaccount->setfieldh256 (sfaccounttxnid, mtxn.gettransactionid ());

    return tessuccess;
}

// check stuff before you bother to lock the ledger
ter transactor::precheck ()
{
    mtxnaccountid   = mtxn.getsourceaccount ().getaccountid ();

    if (!mtxnaccountid)
    {
        m_journal.warning << "apply: bad transaction source id";
        return tembad_src_account;
    }

    // extract signing key
    // transactions contain a signing key.  this allows us to trivially verify a
    // transaction has at least been properly signed without going to disk.
    // each transaction also notes a source account id. this is used to verify
    // that the signing key is associated with the account.
    // xxx this could be a lot cleaner to prevent unnecessary copying.
    msigningpubkey = rippleaddress::createaccountpublic (mtxn.getsigningpubkey ());

    // consistency: really signed.
    if (!mtxn.isknowngood ())
    {
        if (mtxn.isknownbad () ||
            (!(mparams & tapno_check_sign) && !mtxn.checksign()))
        {
            mtxn.setbad ();
            m_journal.warning << "apply: invalid transaction (bad signature)";
            return teminvalid;
        }

        mtxn.setgood ();
    }

    return tessuccess;
}

ter transactor::apply ()
{
    ter terresult (precheck ());

    if (terresult != tessuccess)
        return (terresult);

    writelog(lsdebug, transactor)
        << "begin to apply";
    mtxnaccount = mengine->entrycache (ltaccount_root,
        getaccountrootindex (mtxnaccountid));
    calculatefee ();

    // find source account
    // if are only forwarding, due to resource limitations, we might verifying
    // only some transactions, this would be probabilistic.

    if (!mtxnaccount)
    {
        if (musthavevalidaccount ())
        {
            m_journal.trace <<
                "apply: delay transaction: source account does not exist " <<
                mtxn.getsourceaccount ().humanaccountid ();
            return terno_account;
        }
    }
    else
    {
        mpriorbalance   = mtxnaccount->getfieldamount (sfbalance);
        msourcebalance  = mpriorbalance;
        mhasauthkey     = mtxnaccount->isfieldpresent (sfregularkey);
    }

    terresult = checkseq ();

    if (terresult != tessuccess) return (terresult);

    terresult = payfee ();

    if (terresult != tessuccess) return (terresult);

    terresult = checksig ();

    if (terresult != tessuccess) return (terresult);

    if (mtxnaccount)
        mengine->entrymodify (mtxnaccount);

    return doapply ();
}

}
