//------------------------------------------------------------------------------
/*
    this file is part of rippled: https://github.com/ripple/rippled
    copyright (c) 2012-2014 ripple labs inc.

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

namespace ripple {

// {
//   ledger_hash : <ledger>,
//   ledger_index : <ledger_index>
// }
//
// xxx in this case, not specify either ledger does not mean ledger current. it
// means any ledger.
json::value dotransactionentry (rpc::context& context)
{
    ledger::pointer     lpledger;
    json::value jvresult = rpc::lookupledger (
        context.params,
        lpledger,
        context.netops);

    if (!lpledger)
        return jvresult;

    if (!context.params.ismember ("tx_hash"))
    {
        jvresult["error"]   = "fieldnotfoundtransaction";
    }
    else if (!context.params.ismember ("ledger_hash")
             && !context.params.ismember ("ledger_index"))
    {
        // we don't work on ledger current.

        // xxx we don't support any transaction yet.
        jvresult["error"]   = "notyetimplemented";
    }
    else
    {
        uint256 utransid;
        // xxx relying on trusted wss client. would be better to have a strict
        // routine, returning success or failure.
        utransid.sethex (context.params["tx_hash"].asstring ());

        if (!lpledger)
        {
            jvresult["error"]   = "ledgernotfound";
        }
        else
        {
            transaction::pointer        tptrans;
            transactionmetaset::pointer tmtrans;

            if (!lpledger->gettransaction (utransid, tptrans, tmtrans))
            {
                jvresult["error"]   = "transactionnotfound";
            }
            else
            {
                jvresult["tx_json"]     = tptrans->getjson (0);
                if (tmtrans)
                    jvresult["metadata"]    = tmtrans->getjson (0);
                // 'accounts'
                // 'engine_...'
                // 'ledger_...'
            }
        }
    }

    return jvresult;
}

} // ripple
