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
#include <ripple/app/tx/transactionmaster.h>
#include <ripple/app/main/application.h>
#include <ripple/basics/log.h>
#include <ripple/basics/seconds_clock.h>

namespace ripple {

transactionmaster::transactionmaster ()
    : mcache ("transactioncache", 65536, 1800, get_seconds_clock (),
        deprecatedlogs().journal("taggedcache"))
{
}

bool transactionmaster::inledger (uint256 const& hash, std::uint32_t ledger)
{
    transaction::pointer txn = mcache.fetch (hash);

    if (!txn)
        return false;

    txn->setstatus (committed, ledger);
    return true;
}

transaction::pointer transactionmaster::fetch (uint256 const& txnid, bool checkdisk)
{
    transaction::pointer txn = mcache.fetch (txnid);

    if (!checkdisk || txn)
        return txn;

    txn = transaction::load (txnid);

    if (!txn)
        return txn;

    mcache.canonicalize (txnid, txn);

    return txn;
}

sttx::pointer transactionmaster::fetch (shamapitem::ref item,
        shamaptreenode::tntype type,
        bool checkdisk, std::uint32_t ucommitledger)
{
    sttx::pointer  txn;
    transaction::pointer            itx = getapp().getmastertransaction ().fetch (item->gettag (), false);

    if (!itx)
    {

        if (type == shamaptreenode::tntransaction_nm)
        {
            serializeriterator sit (item->peekserializer ());
            txn = std::make_shared<sttx> (std::ref (sit));
        }
        else if (type == shamaptreenode::tntransaction_md)
        {
            serializer s;
            int length;
            item->peekserializer ().getvl (s.moddata (), 0, length);
            serializeriterator sit (s);

            txn = std::make_shared<sttx> (std::ref (sit));
        }
    }
    else
    {
        if (ucommitledger)
            itx->setstatus (committed, ucommitledger);

        txn = itx->getstransaction ();
    }

    return txn;
}

bool transactionmaster::canonicalize (transaction::pointer* ptransaction)
{
    transaction::pointer txn (*ptransaction);

    uint256 tid = txn->getid ();

    if (!tid)
        return false;

    // vfalco note canonicalize can change the value of txn!
    if (mcache.canonicalize (tid, txn))
    {
        *ptransaction = txn;
        return true;
    }

    // vfalco note i am unsure if this is necessary but better safe than sorry.
    *ptransaction = txn;
    return false;
}

void transactionmaster::sweep (void)
{
    mcache.sweep ();
}

taggedcache <uint256, transaction>& transactionmaster::getcache()
{
    return mcache;
}

} // ripple
