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
#include <ripple/app/ledger/consensustranssetsf.h>
#include <ripple/app/main/application.h>
#include <ripple/app/misc/networkops.h>
#include <ripple/app/tx/transactionmaster.h>
#include <ripple/basics/log.h>
#include <ripple/core/jobqueue.h>
#include <ripple/nodestore/database.h>
#include <ripple/protocol/hashprefix.h>

namespace ripple {

consensustranssetsf::consensustranssetsf (nodecache& nodecache)
    : m_nodecache (nodecache)
{
}

void consensustranssetsf::gotnode (bool fromfilter, const shamapnodeid& id, uint256 const& nodehash,
                                   blob& nodedata, shamaptreenode::tntype type)
{
    if (fromfilter)
        return;

    m_nodecache.insert (nodehash, nodedata);

    if ((type == shamaptreenode::tntransaction_nm) && (nodedata.size () > 16))
    {
        // this is a transaction, and we didn't have it
        writelog (lsdebug, transactionacquire) << "node on our acquiring tx set is txn we may not have";

        try
        {
            serializer s (nodedata.begin () + 4, nodedata.end ()); // skip prefix
            serializeriterator sit (s);
            sttx::pointer stx = std::make_shared<sttx> (std::ref (sit));
            assert (stx->gettransactionid () == nodehash);
            getapp().getjobqueue ().addjob (
                jttransaction, "txs->txn",
                std::bind (&networkops::submittransaction, &getapp().getops (),
                           std::placeholders::_1, stx,
                           networkops::stcallback ()));
        }
        catch (...)
        {
            writelog (lswarning, transactionacquire) << "fetched invalid transaction in proposed set";
        }
    }
}

bool consensustranssetsf::havenode (const shamapnodeid& id, uint256 const& nodehash,
                                    blob& nodedata)
{
    if (m_nodecache.retrieve (nodehash, nodedata))
        return true;

    // vfalco todo use a dependency injection here
    transaction::pointer txn = getapp().getmastertransaction().fetch(nodehash, false);

    if (txn)
    {
        // this is a transaction, and we have it
        writelog (lsdebug, transactionacquire) << "node in our acquiring tx set is txn we have";
        serializer s;
        s.add32 (hashprefix::transactionid);
        txn->getstransaction ()->add (s, true);
        assert (s.getsha512half () == nodehash);
        nodedata = s.peekdata ();
        return true;
    }

    return false;
}

} // ripple
