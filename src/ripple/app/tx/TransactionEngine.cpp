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
#include <ripple/app/tx/transactionengine.h>
#include <ripple/app/transactors/transactor.h>
#include <ripple/basics/log.h>
#include <ripple/json/to_string.h>
#include <ripple/protocol/indexes.h>
#include <boost/foreach.hpp>
#include <cassert>

namespace ripple {

//
// xxx make sure all fields are recognized in transactions.
//

void transactionengine::txnwrite ()
{
    // write back the account states
    typedef std::map<uint256, ledgerentrysetentry>::value_type u256_les_pair;
    boost_foreach (u256_les_pair & it, mnodes)
    {
        sle::ref    sleentry    = it.second.mentry;

        switch (it.second.maction)
        {
        case taanone:
            assert (false);
            break;

        case taacached:
            break;

        case taacreate:
        {
            writelog (lsdebug, transactionengine) << "applytransaction: taacreate: " << sleentry->gettext ();

            if (mledger->writeback (lepcreate, sleentry) & leperror)
                assert (false);
        }
        break;

        case taamodify:
        {
            writelog (lsdebug, transactionengine) << "applytransaction: taamodify: " << sleentry->gettext ();

            if (mledger->writeback (lepnone, sleentry) & leperror)
                assert (false);
        }
        break;

        case taadelete:
        {
            writelog (lsdebug, transactionengine) << "applytransaction: taadelete: " << sleentry->gettext ();

            if (!mledger->peekaccountstatemap ()->delitem (it.first))
                assert (false);
        }
        break;
        }
    }
}

ter transactionengine::applytransaction (
    sttx const& txn,
    transactionengineparams params,
    bool& didapply)
{
    writelog (lstrace, transactionengine) << "applytransaction>";
    didapply = false;
    assert (mledger);
    uint256 const& txid = txn.gettransactionid ();
    mnodes.init (mledger, txid, mledger->getledgerseq (), params);

#ifdef beast_debug
    if (1)
    {
        serializer ser;
        txn.add (ser);
        serializeriterator sit (ser);
        sttx s2 (sit);

        if (!s2.isequivalent (txn))
        {
            writelog (lsfatal, transactionengine) <<
                "transaction serdes mismatch";
            writelog (lsinfo, transactionengine) << txn.getjson (0);
            writelog (lsfatal, transactionengine) << s2.getjson (0);
            assert (false);
        }
    }
#endif

    if (!txid)
    {
        writelog (lswarning, transactionengine) <<
            "applytransaction: invalid transaction id";
        return teminvalid;
    }

    ter terresult = transactor::transact (txn, params, this);

    if (terresult == temunknown)
    {
        writelog (lswarning, transactionengine) <<
            "applytransaction: invalid transaction: unknown transaction type";
        return temunknown;
    }

    if (shouldlog (lsdebug, transactionengine))
    {
        std::string strtoken;
        std::string strhuman;

        transresultinfo (terresult, strtoken, strhuman);

        writelog (lsdebug, transactionengine) <<
            "applytransaction: terresult=" << strtoken <<
            " : " << terresult <<
            " : " << strhuman;
    }

    if (istessuccess (terresult))
        didapply = true;
    else if (istecclaim (terresult) && !(params & tapretry))
    {
        // only claim the transaction fee
        writelog (lsdebug, transactionengine) << "reprocessing to only claim fee";
        mnodes.clear ();

        sle::pointer txnacct = entrycache (ltaccount_root,
            getaccountrootindex (txn.getsourceaccount ()));

        if (!txnacct)
            terresult = terno_account;
        else
        {
            std::uint32_t t_seq = txn.getsequence ();
            std::uint32_t a_seq = txnacct->getfieldu32 (sfsequence);

            if (a_seq < t_seq)
                terresult = terpre_seq;
            else if (a_seq > t_seq)
                terresult = tefpast_seq;
            else
            {
                stamount fee        = txn.gettransactionfee ();
                stamount balance    = txnacct->getfieldamount (sfbalance);
                stamount balancevbc = txnacct->getfieldamount(sfbalancevbc);

                // we retry/reject the transaction if the account
                // balance is zero or we're applying against an open
                // ledger and the balance is less than the fee
                if ((balance == zero) || (balancevbc.getnvalue() == 0) ||
                    ((params & tapopen_ledger) && (balance < fee)))
                {
                    // account has no funds or ledger is open
                    terresult = terinsuf_fee_b;
                }
                else
                {
                    if (fee > balance)
                        fee = balance;
                    txnacct->setfieldamount (sfbalance, balance - fee);
                    txnacct->setfieldamount(sfbalancevbc, balancevbc);
                    txnacct->setfieldu32 (sfsequence, t_seq + 1);
                    entrymodify (txnacct);
                    didapply = true;
                }
            }
        }
    }
    else
        writelog (lsdebug, transactionengine) << "not applying transaction " << txid;

    if (didapply)
    {
        if (!checkinvariants (terresult, txn, params))
        {
            writelog (lsfatal, transactionengine) <<
                "transaction violates invariants";
            writelog (lsfatal, transactionengine) <<
                txn.getjson (0);
            writelog (lsfatal, transactionengine) <<
                transtoken (terresult) << ": " << transhuman (terresult);
            writelog (lsfatal, transactionengine) <<
                mnodes.getjson (0);
            didapply = false;
            terresult = tefinternal;
        }
        else
        {
            // transaction succeeded fully or (retries are not allowed and the
            // transaction could claim a fee)
            serializer m;
            mnodes.calcrawmeta (m, terresult, mtxnseq++);

            txnwrite ();

            serializer s;
            txn.add (s);

            if (params & tapopen_ledger)
            {
                if (!mledger->addtransaction (txid, s))
                {
                    writelog (lsfatal, transactionengine) <<
                        "tried to add transaction to open ledger that already had it";
                    assert (false);
                    throw std::runtime_error ("duplicate transaction applied");
                }
            }
            else
            {
                if (!mledger->addtransaction (txid, s, m))
                {
                    writelog (lsfatal, transactionengine) <<
                        "tried to add transaction to ledger that already had it";
                    assert (false);
                    throw std::runtime_error ("duplicate transaction applied to closed ledger");
                }

                // charge whatever fee they specified.
                stamount sapaid = txn.gettransactionfee ();
                mledger->destroycoins (sapaid.getnvalue ());
            }
        }
    }

    mtxnaccount.reset ();
    mnodes.clear ();

    if (!(params & tapopen_ledger) && istemmalformed (terresult))
    {
        // xxx malformed or failed transaction in closed ledger must bow out.
    }

    return terresult;
}

} // ripple
