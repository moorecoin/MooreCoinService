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
#include <ripple/app/tx/transactionmaster.h>

namespace ripple {

// {
//   transaction: <hex>
// }
json::value dotx (rpc::context& context)
{
    if (!context.params.ismember (jss::transaction))
        return rpcerror (rpcinvalid_params);

    bool binary = context.params.ismember (jss::binary)
            && context.params[jss::binary].asbool ();

    auto const txid  = context.params[jss::transaction].asstring ();

    if (!transaction::ishextxid (txid))
        return rpcerror (rpcnot_impl);

    auto txn = getapp().getmastertransaction ().fetch (uint256 (txid), true);

    if (!txn)
        return rpcerror (rpctxn_not_found);

    json::value ret = txn->getjson (1, binary);

    if (txn->getledger () == 0)
        return ret;

    if (auto lgr = context.netops.getledgerbyseq (txn->getledger ()))
    {
        bool okay = false;

        if (binary)
        {
            std::string meta;

            if (lgr->getmetahex (txn->getid (), meta))
            {
                ret[jss::meta] = meta;
                okay = true;
            }
        }
        else
        {
            transactionmetaset::pointer txmeta;

            if (lgr->gettransactionmeta (txn->getid (), txmeta))
            {
                okay = true;
                auto meta = txmeta->getjson (0);
                addpaymentdeliveredamount (meta, context, txn, txmeta);
                ret[jss::meta] = meta;
            }
        }

        if (okay)
            ret[jss::validated] = context.netops.isvalidated (lgr);
    }

    return ret;
}

} // ripple
