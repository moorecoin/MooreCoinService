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
#include <ripple/app/tx/transaction.h>
#include <ripple/basics/log.h>
#include <ripple/app/data/databasecon.h>
#include <ripple/app/ledger/ledgermaster.h>
#include <ripple/app/main/application.h>

namespace ripple {

transaction::transaction (sttx::ref sit, validate validate)
    : minledger (0),
      mstatus (invalid),
      mresult (temuncertain),
      mtransaction (sit)
{
    try
    {
        mfrompubkey.setaccountpublic (mtransaction->getsigningpubkey ());
        mtransactionid  = mtransaction->gettransactionid ();
        maccountfrom    = mtransaction->getsourceaccount ();
    }
    catch (...)
    {
        return;
    }

    if (validate == validate::no ||
        (passeslocalchecks (*mtransaction) && checksign ()))
    {
        mstatus = new;
    }
}

transaction::pointer transaction::sharedtransaction (
    blob const& vuctransaction, validate validate)
{
    try
    {
        serializer s (vuctransaction);
        serializeriterator sit (s);
//        writelog(lsinfo, ledger) << "moorecoin: pass initialization ";
//        for (auto it = vuctransaction.begin(); it != vuctransaction.end(); ++it) {
//            writelog(lsinfo, ledger) << "moorecoin: blob " << (int)(*it);
//        }

        return std::make_shared<transaction> (
            std::make_shared<sttx> (sit),
            validate);
    }
    catch (...)
    {
        writelog (lswarning, ledger) << "exception constructing transaction";
        return std::shared_ptr<transaction> ();
    }
}

//
// misc.
//

bool transaction::checksign () const
{
    if (mfrompubkey.isvalid ())
        return mtransaction->checksign();

    writelog (lswarning, ledger) << "transaction has bad source public key";
    return false;

}

void transaction::setstatus (transstatus ts, std::uint32_t lseq)
{
    mstatus     = ts;
    minledger   = lseq;
}

transaction::pointer transaction::transactionfromsql (
    database* db, validate validate)
{
    serializer rawtxn;
    std::string status;
    std::uint32_t inledger;

    int txsize = 2048;
    rawtxn.resize (txsize);

    db->getstr ("status", status);
    inledger = db->getint ("ledgerseq");
    txsize = db->getbinary ("rawtxn", &*rawtxn.begin (), rawtxn.getlength ());

    if (txsize > rawtxn.getlength ())
    {
        rawtxn.resize (txsize);
        db->getbinary ("rawtxn", &*rawtxn.begin (), rawtxn.getlength ());
    }

    rawtxn.resize (txsize);

    serializeriterator it (rawtxn);
    auto txn = std::make_shared<sttx> (it);
    auto tr = std::make_shared<transaction> (txn, validate);

    transstatus st (invalid);

    switch (status[0])
    {
    case txn_sql_new:
        st = new;
        break;

    case txn_sql_conflict:
        st = conflicted;
        break;

    case txn_sql_held:
        st = held;
        break;

    case txn_sql_validated:
        st = committed;
        break;

    case txn_sql_included:
        st = included;
        break;

    case txn_sql_unknown:
        break;

    default:
        assert (false);
    }

    tr->setstatus (st);
    tr->setledger (inledger);
    return tr;
}

// david: would you rather duplicate this code or keep the lock longer?
transaction::pointer transaction::transactionfromsql (std::string const& sql)
{
    serializer rawtxn;
    std::string status;
    std::uint32_t inledger;

    int txsize = 2048;
    rawtxn.resize (txsize);

    {
        auto sl (getapp().gettxndb ().lock ());
        auto db = getapp().gettxndb ().getdb ();

        if (!db->executesql (sql, true) || !db->startiterrows ())
            return transaction::pointer ();

        db->getstr ("status", status);
        inledger = db->getint ("ledgerseq");
        txsize = db->getbinary (
            "rawtxn", &*rawtxn.begin (), rawtxn.getlength ());

        if (txsize > rawtxn.getlength ())
        {
            rawtxn.resize (txsize);
            db->getbinary ("rawtxn", &*rawtxn.begin (), rawtxn.getlength ());
        }

        db->enditerrows ();
    }
    rawtxn.resize (txsize);

    serializeriterator it (rawtxn);
    auto txn = std::make_shared<sttx> (it);
    auto tr = std::make_shared<transaction> (txn, validate::yes);

    transstatus st (invalid);

    switch (status[0])
    {
    case txn_sql_new:
        st = new;
        break;

    case txn_sql_conflict:
        st = conflicted;
        break;

    case txn_sql_held:
        st = held;
        break;

    case txn_sql_validated:
        st = committed;
        break;

    case txn_sql_included:
        st = included;
        break;

    case txn_sql_unknown:
        break;

    default:
        assert (false);
    }

    tr->setstatus (st);
    tr->setledger (inledger);
    return tr;
}


transaction::pointer transaction::load (uint256 const& id)
{
    std::string sql = "select ledgerseq,status,rawtxn "
            "from transactions where transid='";
    sql.append (to_string (id));
    sql.append ("';");
    return transactionfromsql (sql);
}

// options 1 to include the date of the transaction
json::value transaction::getjson (int options, bool binary) const
{
    json::value ret (mtransaction->getjson (0, binary));

    if (minledger)
    {
        ret["inledger"] = minledger;        // deprecated.
        ret["ledger_index"] = minledger;

        if (options == 1)
        {
            auto ledger = getapp().getledgermaster ().
                    getledgerbyseq (minledger);
            if (ledger)
                ret["date"] = ledger->getclosetimenc ();
        }
    }

    return ret;
}

bool transaction::ishextxid (std::string const& txid)
{
    if (txid.size () != 64)
        return false;

    auto const ret = std::find_if (txid.begin (), txid.end (),
        [](std::string::value_type c)
        {
            return !std::isxdigit (c);
        });

    return (ret == txid.end ());
}

} // ripple
