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
#include <ripple/app/book/quality.h>
#include <ripple/app/transactors/transactor.h>
#include <ripple/basics/log.h>
#include <ripple/protocol/indexes.h>
#include <ripple/protocol/txflags.h>

namespace ripple {

class settrust
    : public transactor
{
public:
    settrust (
        sttx const& txn,
        transactionengineparams params,
        transactionengine* engine)
        : transactor (
            txn,
            params,
            engine,
            deprecatedlogs().journal("settrust"))
    {
    }

    ter doapply () override
    {
        ter terresult = tessuccess;

        stamount const salimitamount (mtxn.getfieldamount (sflimitamount));
        bool const bqualityin (mtxn.isfieldpresent (sfqualityin));
        bool const bqualityout (mtxn.isfieldpresent (sfqualityout));

        currency const currency (salimitamount.getcurrency ());
        account udstaccountid (salimitamount.getissuer ());

        // true, iff current is high account.
        bool const bhigh = mtxnaccountid > udstaccountid;

        std::uint32_t const uownercount (mtxnaccount->getfieldu32 (sfownercount));

        // the reserve required to create the line. note that we allow up to
        // two trust lines without requiring a reserve because being able to
        // exchange currencies is a powerful ripple feature.
        //
        // this is also a security feature: if you're a gateway and you want to
        // be able to let someone use your services, you would otherwise have to
        // give them enough xrp to cover the incremental reserve for their trust
        // line. if they had no intention of using your services, they could use
        // the xrp for their own purposes. so we make it possible for gateways
        // to fund accounts in a way where there's no incentive to trick them
        // into creating an account you have no intention of using.

        std::uint64_t const ureservecreate = (uownercount < 2)
            ? 0
            : mengine->getledger ()->getreserve (uownercount + 1);

        std::uint32_t uqualityin (bqualityin ? mtxn.getfieldu32 (sfqualityin) : 0);
        std::uint32_t uqualityout (bqualityout ? mtxn.getfieldu32 (sfqualityout) : 0);

        if (!islegalnet (salimitamount))
            return tembad_amount;

        if (bqualityout && quality_one == uqualityout)
            uqualityout = 0;

        std::uint32_t const utxflags = mtxn.getflags ();

        if (utxflags & tftrustsetmask)
        {
            m_journal.trace <<
                "malformed transaction: invalid flags set.";
            return teminvalid_flag;
        }

        bool const bsetauth = (utxflags & tfsetfauth);
        bool const bclearnoripple  = (utxflags & tfclearnoripple);
        bool const bsetnoripple = (utxflags & tfsetnoripple);
        bool const bsetfreeze = (utxflags & tfsetfreeze);
        bool const bclearfreeze = (utxflags & tfclearfreeze);

        if (bsetauth && !(mtxnaccount->getfieldu32 (sfflags) & lsfrequireauth))
        {
            m_journal.trace <<
                "retry: auth not required.";
            return tefno_auth_required;
        }

        if (salimitamount.isnative ())
        {
            m_journal.trace <<
                "malformed transaction: native credit limit: " <<
                salimitamount.getfulltext ();
            return tembad_limit;
        }

        if (salimitamount < zero)
        {
            m_journal.trace <<
                "malformed transaction: negative credit limit.";
            return tembad_limit;
        }

        // check if destination makes sense.
        if (!udstaccountid || udstaccountid == noaccount())
        {
            m_journal.trace <<
                "malformed transaction: destination account not specified.";

            return temdst_needed;
        }

        if (mtxnaccountid == udstaccountid)
        {
            sle::pointer seldelete (
                mengine->entrycache (ltripple_state,
                    getripplestateindex (
                        mtxnaccountid, udstaccountid, currency)));

            if (seldelete)
            {
                m_journal.warning <<
                    "clearing redundant line.";

                return mengine->view ().trustdelete (
                    seldelete, mtxnaccountid, udstaccountid);
            }
            else
            {
                m_journal.trace <<
                    "malformed transaction: can not extend credit to self.";
                return temdst_is_src;
            }
        }

        sle::pointer sledst (mengine->entrycache (
            ltaccount_root, getaccountrootindex (udstaccountid)));

        if (!sledst)
        {
            m_journal.trace <<
                "delay transaction: destination account does not exist.";
            return tecno_dst;
        }

        stamount salimitallow = salimitamount;
        salimitallow.setissuer (mtxnaccountid);

        sle::pointer sleripplestate (mengine->entrycache (ltripple_state,
            getripplestateindex (mtxnaccountid, udstaccountid, currency)));

        if (sleripplestate)
        {
            stamount        salowbalance;
            stamount        salowlimit;
            stamount        sahighbalance;
            stamount        sahighlimit;
            std::uint32_t   ulowqualityin;
            std::uint32_t   ulowqualityout;
            std::uint32_t   uhighqualityin;
            std::uint32_t   uhighqualityout;
            auto const& ulowaccountid   = !bhigh ? mtxnaccountid : udstaccountid;
            auto const& uhighaccountid  =  bhigh ? mtxnaccountid : udstaccountid;
            sle::ref        slelowaccount   = !bhigh ? mtxnaccount : sledst;
            sle::ref        slehighaccount  =  bhigh ? mtxnaccount : sledst;

            //
            // balances
            //

            salowbalance    = sleripplestate->getfieldamount (sfbalance);
            sahighbalance   = -salowbalance;

            //
            // limits
            //

            sleripplestate->setfieldamount (!bhigh ? sflowlimit : sfhighlimit, salimitallow);

            salowlimit  = !bhigh ? salimitallow : sleripplestate->getfieldamount (sflowlimit);
            sahighlimit =  bhigh ? salimitallow : sleripplestate->getfieldamount (sfhighlimit);

            //
            // quality in
            //

            if (!bqualityin)
            {
                // not setting. just get it.

                ulowqualityin   = sleripplestate->getfieldu32 (sflowqualityin);
                uhighqualityin  = sleripplestate->getfieldu32 (sfhighqualityin);
            }
            else if (uqualityin)
            {
                // setting.

                sleripplestate->setfieldu32 (!bhigh ? sflowqualityin : sfhighqualityin, uqualityin);

                ulowqualityin   = !bhigh ? uqualityin : sleripplestate->getfieldu32 (sflowqualityin);
                uhighqualityin  =  bhigh ? uqualityin : sleripplestate->getfieldu32 (sfhighqualityin);
            }
            else
            {
                // clearing.

                sleripplestate->makefieldabsent (!bhigh ? sflowqualityin : sfhighqualityin);

                ulowqualityin   = !bhigh ? 0 : sleripplestate->getfieldu32 (sflowqualityin);
                uhighqualityin  =  bhigh ? 0 : sleripplestate->getfieldu32 (sfhighqualityin);
            }

            if (quality_one == ulowqualityin)   ulowqualityin   = 0;

            if (quality_one == uhighqualityin)  uhighqualityin  = 0;

            //
            // quality out
            //

            if (!bqualityout)
            {
                // not setting. just get it.

                ulowqualityout  = sleripplestate->getfieldu32 (sflowqualityout);
                uhighqualityout = sleripplestate->getfieldu32 (sfhighqualityout);
            }
            else if (uqualityout)
            {
                // setting.

                sleripplestate->setfieldu32 (!bhigh ? sflowqualityout : sfhighqualityout, uqualityout);

                ulowqualityout  = !bhigh ? uqualityout : sleripplestate->getfieldu32 (sflowqualityout);
                uhighqualityout =  bhigh ? uqualityout : sleripplestate->getfieldu32 (sfhighqualityout);
            }
            else
            {
                // clearing.

                sleripplestate->makefieldabsent (!bhigh ? sflowqualityout : sfhighqualityout);

                ulowqualityout  = !bhigh ? 0 : sleripplestate->getfieldu32 (sflowqualityout);
                uhighqualityout =  bhigh ? 0 : sleripplestate->getfieldu32 (sfhighqualityout);
            }

            std::uint32_t const uflagsin (sleripplestate->getfieldu32 (sfflags));
            std::uint32_t uflagsout (uflagsin);

            if (bsetnoripple && !bclearnoripple && (bhigh ? sahighbalance : salowbalance) >= zero)
            {
                uflagsout |= (bhigh ? lsfhighnoripple : lsflownoripple);
            }
            else if (bclearnoripple && !bsetnoripple)
            {
                uflagsout &= ~(bhigh ? lsfhighnoripple : lsflownoripple);
            }

            if (bsetfreeze && !bclearfreeze && !mtxnaccount->isflag  (lsfnofreeze))
            {
                uflagsout           |= (bhigh ? lsfhighfreeze : lsflowfreeze);
            }
            else if (bclearfreeze && !bsetfreeze)
            {
                uflagsout           &= ~(bhigh ? lsfhighfreeze : lsflowfreeze);
            }

            if (quality_one == ulowqualityout)  ulowqualityout  = 0;

            if (quality_one == uhighqualityout) uhighqualityout = 0;


            bool const  blowreserveset      = ulowqualityin || ulowqualityout ||
                                              (uflagsout & lsflownoripple) ||
                                              (uflagsout & lsflowfreeze) ||
                                              salowlimit || salowbalance > zero;
            bool const  blowreserveclear    = !blowreserveset;

            bool const  bhighreserveset     = uhighqualityin || uhighqualityout ||
                                              (uflagsout & lsfhighnoripple) ||
                                              (uflagsout & lsfhighfreeze) ||
                                              sahighlimit || sahighbalance > zero;
            bool const  bhighreserveclear   = !bhighreserveset;

            bool const  bdefault            = blowreserveclear && bhighreserveclear;

            bool const  blowreserved = (uflagsin & lsflowreserve);
            bool const  bhighreserved = (uflagsin & lsfhighreserve);

            bool        breserveincrease    = false;

            if (assetcurrency() == currency && blowreserveclear
                && bclearnoripple && sahighbalance <= zero && !sahighlimit
                && !uhighqualityin && !uhighqualityout)
            {
                uint256 baseindex = getassetstateindex(mtxnaccountid, salimitamount.issue());
                uint256 assetstateindex = getqualityindex(baseindex);
                uint256 assetstateend = getqualitynext(assetstateindex);
                bool bisassetstateempty = true;
                for(;;)
                {
                    // check assetstate is totally empty
                    auto const& sleassetstate = mengine->entrycache(ltasset_state, assetstateindex);
                    if (sleassetstate)
                    {
                        bisassetstateempty = false;
                        break;
                    }
                    uint256 nextassetstateindex = mengine->getledger()->getnextledgerindex(assetstateindex, assetstateend);
                    if (nextassetstateindex.iszero())
                        break;
                    assetstateindex = nextassetstateindex;
                }
                
                if (bisassetstateempty)
                {
                    mengine->view ().decrementownercount (slelowaccount);
                    mengine->view ().decrementownercount (slehighaccount);
                    return mengine->view ().trustdelete (sleripplestate, ulowaccountid, uhighaccountid); 
                }
                else
                {
                    return temdisabled;
                }
            }
            else if (assetcurrency() == currency && bclearnoripple) {
                m_journal.trace << "malformed transaction: tfclearnoripple is not allowed on asset";
                return temdisabled;
            }
            
            if (bsetauth)
            {
                uflagsout |= (bhigh ? lsfhighauth : lsflowauth);
            }

            if (blowreserveset && !blowreserved)
            {
                // set reserve for low account.
                mengine->view ().incrementownercount (slelowaccount);
                uflagsout |= lsflowreserve;

                if (!bhigh)
                    breserveincrease = true;
            }

            if (blowreserveclear && blowreserved)
            {
                // clear reserve for low account.
                mengine->view ().decrementownercount (slelowaccount);
                uflagsout &= ~lsflowreserve;
            }

            if (bhighreserveset && !bhighreserved)
            {
                // set reserve for high account.
                mengine->view ().incrementownercount (slehighaccount);
                uflagsout |= lsfhighreserve;

                if (bhigh)
                    breserveincrease    = true;
            }

            if (bhighreserveclear && bhighreserved)
            {
                // clear reserve for high account.
                mengine->view ().decrementownercount (slehighaccount);
                uflagsout &= ~lsfhighreserve;
            }

            if (uflagsin != uflagsout)
                sleripplestate->setfieldu32 (sfflags, uflagsout);

            if (bdefault || badcurrency() == currency)
            {
                // delete.

                terresult = mengine->view ().trustdelete (sleripplestate, ulowaccountid, uhighaccountid);
            }
            else if (breserveincrease
                     && mpriorbalance.getnvalue () < ureservecreate) // reserve is not scaled by load.
            {
                m_journal.trace <<
                    "delay transaction: insufficent reserve to add trust line.";

                // another transaction could provide xrp to the account and then
                // this transaction would succeed.
                terresult = tecinsuf_reserve_line;
            }
            else
            {
                mengine->entrymodify (sleripplestate);

                m_journal.trace << "modify ripple line";
            }
        }
        // line does not exist.
        else if (!salimitamount                       // setting default limit.
                 && (!bqualityin || !uqualityin)      // not setting quality in or setting default quality in.
                 && (!bqualityout || !uqualityout))   // not setting quality out or setting default quality out.
        {
            m_journal.trace <<
                "redundant: setting non-existent ripple line to defaults.";
            return tecno_line_redundant;
        }
        else if (mpriorbalance.getnvalue () < ureservecreate) // reserve is not scaled by load.
        {
            m_journal.trace <<
                "delay transaction: line does not exist. insufficent reserve to create line.";

            // another transaction could create the account and then this transaction would succeed.
            terresult = tecno_line_insuf_reserve;
        }
        else if (badcurrency() == currency)
        {
            terresult = tembad_currency;
        }
        else if (assetcurrency () == currency && bclearnoripple)
        {
            terresult = temdisabled;
        }
        else
        {
            // zero balance in currency.
            stamount sabalance ({currency, noaccount()});

            uint256 index (getripplestateindex (
                mtxnaccountid, udstaccountid, currency));

            m_journal.trace <<
                "dotrustset: creating ripple line: " <<
                to_string (index);

            // create a new ripple line.
            terresult = mengine->view ().trustcreate (
                              bhigh,
                              mtxnaccountid,
                              udstaccountid,
                              index,
                              mtxnaccount,
                              bsetauth,
                              bsetnoripple && !bclearnoripple,
                              bsetfreeze && !bclearfreeze,
                              sabalance,
                              salimitallow,       // limit for who is being charged.
                              uqualityin,
                              uqualityout);
        }

        return terresult;
    }
};

ter
transact_settrust (
    sttx const& txn,
    transactionengineparams params,
    transactionengine* engine)
{
    return settrust (txn, params, engine).apply ();
}

}
