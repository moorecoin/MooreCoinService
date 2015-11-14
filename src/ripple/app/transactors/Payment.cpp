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
#include <ripple/app/paths/ripplecalc.h>
#include <ripple/app/transactors/transactor.h>
#include <ripple/basics/log.h>
#include <ripple/core/config.h>
#include <ripple/protocol/txflags.h>

namespace ripple {

// see https://ripple.com/wiki/transaction_format#payment_.280.29

class payment
    : public transactor
{
    /* the largest number of paths we allow */
    static std::size_t const maxpathsize = 6;

    /* the longest path we allow */
    static std::size_t const maxpathlength = 8;

public:
    payment (
        sttx const& txn,
        transactionengineparams params,
        transactionengine* engine)
        : transactor (
            txn,
            params,
            engine,
            deprecatedlogs().journal("payment"))
    {

    }
    
    void calculatefee ()
    {
        mfeedue = stamount (mengine->getledger ()->scalefeeload (
                        calculatebasefee (), mparams & tapadmin));

        std::uint64_t feebytrans = 0;
        
        account const udstaccountid (mtxn.getfieldaccount160 (sfdestination));
        auto const index = getaccountrootindex (udstaccountid);
        //dst account not exist yet, charge a fix amount of fee(0.01) for creating
        if (!mengine->entrycache (ltaccount_root, index))
        {
            feebytrans = getconfig().fee_default_create;
        }

        //if currency is native(vrp/vbc), charge 1/1000 of transfer amount,
        //otherwise charge a fix amount of fee(0.001)
        stamount const amount (mtxn.getfieldamount (sfamount));
        feebytrans += amount.isnative() ? std::max(int(amount.getnvalue() * getconfig().fee_default_rate_native), int(getconfig().fee_default_min_native)) : getconfig().fee_default_none_native;

        mfeedue = std::max(mfeedue, stamount(feebytrans, false));
    }

    ter doapply () override
    {
        // ripple if source or destination is non-native or if there are paths.
        std::uint32_t const utxflags = mtxn.getflags ();
        bool const partialpaymentallowed = utxflags & tfpartialpayment;
        bool const limitquality = utxflags & tflimitquality;
        bool const defaultpathsallowed = !(utxflags & tfnorippledirect);
        bool const bpaths = mtxn.isfieldpresent (sfpaths);
        bool const bmax = mtxn.isfieldpresent (sfsendmax);
        account const udstaccountid (mtxn.getfieldaccount160 (sfdestination));
        stamount const sadstamount (mtxn.getfieldamount (sfamount));
        stamount maxsourceamount;
        if (bmax)
            maxsourceamount = mtxn.getfieldamount (sfsendmax);
        else if (sadstamount.isnative ())
            maxsourceamount = sadstamount;
        else
          maxsourceamount = stamount (
              {sadstamount.getcurrency (), mtxnaccountid},
              sadstamount.mantissa(), sadstamount.exponent (),
              sadstamount < zero);
        auto const& usrccurrency = maxsourceamount.getcurrency ();
        auto const& udstcurrency = sadstamount.getcurrency ();

        // iszero() is xrp.  fix!
		bool const bxrpdirect = (usrccurrency.iszero() && udstcurrency.iszero()) || (isvbc(usrccurrency) && isvbc(udstcurrency));

        m_journal.trace <<
            "maxsourceamount=" << maxsourceamount.getfulltext () <<
            " sadstamount=" << sadstamount.getfulltext ();

        if (!islegalnet (sadstamount) || !islegalnet (maxsourceamount))
            return tembad_amount;

        if (utxflags & tfpaymentmask)
        {
            m_journal.trace <<
                "malformed transaction: invalid flags set.";

            return teminvalid_flag;
        }
        else if (!udstaccountid)
        {
            m_journal.trace <<
                "malformed transaction: payment destination account not specified.";

            return temdst_needed;
        }
        else if (bmax && maxsourceamount <= zero)
        {
            m_journal.trace <<
                "malformed transaction: bad max amount: " << maxsourceamount.getfulltext ();

            return tembad_amount;
        }
        else if (sadstamount <= zero)
        {
            m_journal.trace <<
                "malformed transaction: bad dst amount: " << sadstamount.getfulltext ();

            return tembad_amount;
        }
        else if (badcurrency() == usrccurrency || badcurrency() == udstcurrency)
        {
            m_journal.trace <<
                "malformed transaction: bad currency.";

            return tembad_currency;
        }
        else if (mtxnaccountid == udstaccountid && usrccurrency == udstcurrency && !bpaths)
        {
            // you're signing yourself a payment.
            // if bpaths is true, you might be trying some arbitrage.
            m_journal.trace <<
                "malformed transaction: redundant transaction:" <<
                " src=" << to_string (mtxnaccountid) <<
                " dst=" << to_string (udstaccountid) <<
                " src_cur=" << to_string (usrccurrency) <<
                " dst_cur=" << to_string (udstcurrency);

            return temredundant;
        }
        else if (bmax && maxsourceamount == sadstamount &&
                 maxsourceamount.getcurrency () == sadstamount.getcurrency ())
        {
            // consistent but redundant transaction.
            m_journal.trace <<
                "malformed transaction: redundant sendmax.";

            return temredundant_send_max;
        }
        else if (bxrpdirect && bmax)
        {
            // consistent but redundant transaction.
            m_journal.trace <<
                "malformed transaction: sendmax specified for xrp to xrp.";

            return tembad_send_xrp_max;
        }
        else if (bxrpdirect && bpaths)
        {
            // xrp is sent without paths.
            m_journal.trace <<
                "malformed transaction: paths specified for xrp to xrp.";

            return tembad_send_xrp_paths;
        }
        else if (bxrpdirect && partialpaymentallowed)
        {
            // consistent but redundant transaction.
            m_journal.trace <<
                "malformed transaction: partial payment specified for xrp to xrp.";

            return tembad_send_xrp_partial;
        }
        else if (bxrpdirect && limitquality)
        {
            // consistent but redundant transaction.
            m_journal.trace <<
                "malformed transaction: limit quality specified for xrp to xrp.";

            return tembad_send_xrp_limit;
        }
        else if (bxrpdirect && !defaultpathsallowed)
        {
            // consistent but redundant transaction.
            m_journal.trace <<
                "malformed transaction: no ripple direct specified for xrp to xrp.";

            return tembad_send_xrp_no_direct;
        }

        // additional checking for currency asset.
        if (assetcurrency() == udstcurrency) {
            if (sadstamount.getissuer() == udstaccountid) {
                // return asset to issuer is not allowed.
                m_journal.trace << "return asset to issuer is not allowed"
                                << " src=" << to_string(mtxnaccountid) << " dst=" << to_string(udstaccountid) << " src_cur=" << to_string(usrccurrency) << " dst_cur=" << to_string(udstcurrency);

                return temdisabled;
            }
            
            if (sadstamount < stamount(sadstamount.issue(), getconfig().asset_tx_min) || !sadstamount.ismathematicalinteger())
                return teminvalid;
        }

        if (assetcurrency() == usrccurrency) {
            if (bmax)
                return tembad_send_xrp_max;

            if (partialpaymentallowed)
                return tembad_send_xrp_partial;

            if (sadstamount.getissuer() == mtxnaccountid) {
                m_journal.trace << "asset payment from issuer is not allowed";
                return temdisabled;
            }
        }

        //
        // open a ledger for editing.
        auto const index = getaccountrootindex (udstaccountid);
        sle::pointer sledst (mengine->entrycache (ltaccount_root, index));

        if (!sledst)
        {
            // destination account does not exist.
            if (!sadstamount.isnative ())
            {
                m_journal.trace <<
                    "delay transaction: destination account does not exist.";

                // another transaction could create the account and then this
                // transaction would succeed.
                return tecno_dst;
            }
            else if (mparams & tapopen_ledger && partialpaymentallowed)
            {
                // you cannot fund an account with a partial payment.
                // make retry work smaller, by rejecting this.
                m_journal.trace <<
                    "delay transaction: partial payment not allowed to create account.";


                // another transaction could create the account and then this
                // transaction would succeed.
                return telno_dst_partial;
            }
            else if (sadstamount.getnvalue () < mengine->getledger ()->getreserve (0))
            {
                // getreserve() is the minimum amount that an account can have.
                // reserve is not scaled by load.
                m_journal.trace <<
                    "delay transaction: destination account does not exist. " <<
                    "insufficent payment to create account.";

                // todo: dedupe
                // another transaction could create the account and then this
                // transaction would succeed.
                return tecno_dst_insuf_xrp;
            }

            // create the account.
            auto const newindex = getaccountrootindex (udstaccountid);
            sledst = mengine->entrycreate (ltaccount_root, newindex);
            sledst->setfieldaccount (sfaccount, udstaccountid);
            sledst->setfieldu32 (sfsequence, 1);
        }
        else if ((sledst->getflags () & lsfrequiredesttag) &&
                 !mtxn.isfieldpresent (sfdestinationtag))
        {
            // the tag is basically account-specific information we don't
            // understand, but we can require someone to fill it in.

            // we didn't make this test for a newly-formed account because there's
            // no way for this field to be set.
            m_journal.trace << "malformed transaction: destinationtag required.";

            return tefdst_tag_needed;
        }
        else
        {
            // tell the engine that we are intending to change the the destination
            // account.  the source account gets always charged a fee so it's always
            // marked as modified.
            mengine->entrymodify (sledst);
        }

        ter terresult;

        bool const bripple = bpaths || bmax || !sadstamount.isnative ();
        // xxx should bmax be sufficient to imply ripple?

        if (bripple)
        {
            // ripple payment with at least one intermediate step and uses
            // transitive balances.

            // copy paths into an editable class.
            stpathset spspaths = mtxn.getfieldpathset (sfpaths);

            try
            {
                path::ripplecalc::input rcinput;
                rcinput.partialpaymentallowed = partialpaymentallowed;
                rcinput.defaultpathsallowed = defaultpathsallowed;
                rcinput.limitquality = limitquality;
                rcinput.deleteunfundedoffers = true;
                rcinput.isledgeropen = static_cast<bool>(mparams & tapopen_ledger);

                bool pathtoobig = spspaths.size () > maxpathsize;

                for (auto const& path : spspaths)
                    if (path.size () > maxpathlength)
                        pathtoobig = true;

                if (rcinput.isledgeropen && pathtoobig)
                {
                    terresult = telbad_path_count; // too many paths for proposed ledger.
                }
                else
                {
                    auto rc = path::ripplecalc::ripplecalculate (
                        mengine->view (),
                        maxsourceamount,
                        sadstamount,
                        udstaccountid,
                        mtxnaccountid,
                        spspaths,
                        &rcinput);

                    // todo: is this right?  if the amount is the correct amount, was
                    // the delivered amount previously set?
                    if (rc.result () == tessuccess && rc.actualamountout != sadstamount)
                        mengine->view ().setdeliveredamount (rc.actualamountout);

                    terresult = rc.result ();
                }

                // todo(tom): what's going on here?
                if (isterretry (terresult))
                    terresult = tecpath_dry;

            }
            catch (std::exception const& e)
            {
                m_journal.trace <<
                    "caught throw: " << e.what ();

                terresult = tefexception;
            }
        }
        else
        {
            // direct xrp payment.

            // uownercount is the number of entries in this legder for this account
            // that require a reserve.

            std::uint32_t const uownercount (mtxnaccount->getfieldu32 (sfownercount));

            // this is the total reserve in drops.
            // todo(tom): there should be a class for this.
            std::uint64_t const ureserve (mengine->getledger ()->getreserve (uownercount));

            // mpriorbalance is the balance on the sending account before the fees were charged.
            //
            // make sure have enough reserve to send. allow final spend to use
            // reserve for fee.
            auto const mmm = std::max(ureserve, mtxn.gettransactionfee ().getnvalue ());
            bool isvbctransaction = isvbc(sadstamount);
            if (mpriorbalance < (isvbctransaction?0:sadstamount) + mmm
                || (isvbctransaction && mtxnaccount->getfieldamount (sfbalancevbc) < sadstamount) )
            {
                // vote no.
                // however, transaction might succeed, if applied in a different order.
                m_journal.trace << "delay transaction: insufficient funds: " <<
                    " " << mpriorbalance.gettext () <<
                    " / " << (sadstamount + ureserve).gettext () <<
                    " (" << ureserve << ")";

                terresult = tecunfunded_payment;
            }
            else
            {
                // the source account does have enough money, so do the arithmetic
                // for the transfer and make the ledger change.
                m_journal.info << "moorecoin: deduct coin "
                    << isvbctransaction << msourcebalance << sadstamount;

                if (isvbctransaction)
				{
					mtxnaccount->setfieldamount(sfbalancevbc, mtxnaccount->getfieldamount (sfbalancevbc) - sadstamount);
					sledst->setfieldamount(sfbalancevbc, sledst->getfieldamount(sfbalancevbc) + sadstamount);
				}
				else
				{
					mtxnaccount->setfieldamount(sfbalance, msourcebalance - sadstamount);
					sledst->setfieldamount(sfbalance, sledst->getfieldamount(sfbalance) + sadstamount);
				}



                // re-arm the password change fee if we can and need to.
                if ((sledst->getflags () & lsfpasswordspent))
                    sledst->clearflag (lsfpasswordspent);

                terresult = tessuccess;
            }
        }

        std::string strtoken;
        std::string strhuman;

        if (transresultinfo (terresult, strtoken, strhuman))
        {
            m_journal.trace <<
                strtoken << ": " << strhuman;
        }
        else
        {
            assert (false);
        }

        return terresult;
    }
};

ter
transact_payment (
    sttx const& txn,
    transactionengineparams params,
    transactionengine* engine)
{
    return payment(txn, params, engine).apply ();
}

}  // ripple
