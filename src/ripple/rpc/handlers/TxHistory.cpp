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
#include <ripple/server/role.h>

namespace ripple {

// {
//   start: <index>
// }
json::value dotxhistory (rpc::context& context)
{
    context.loadtype = resource::feemediumburdenrpc;

    if (!context.params.ismember ("start"))
        return rpcerror (rpcinvalid_params);

    unsigned int startindex = context.params["start"].asuint ();

    if ((startindex > 10000) &&  (context.role != role::admin))
        return rpcerror (rpcno_permission);

    json::value obj;
    json::value txs;

    obj["index"] = startindex;

    std::string sql =
        boost::str (boost::format (
            "select * from transactions order by ledgerseq desc limit %u,20")
                    % startindex);

    {
        auto db = getapp().gettxndb ().getdb ();
        auto sl (getapp().gettxndb ().lock ());

        sql_foreach (db, sql)
        {
            if (auto trans = transaction::transactionfromsql (db, validate::no))
                txs.append (trans->getjson (0));
        }
    }

    obj["txs"] = txs;

    return obj;
}

} // ripple
