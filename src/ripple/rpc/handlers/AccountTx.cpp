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
//   account: account,
//   ledger_index_min: ledger_index  // optional, defaults to earliest
//   ledger_index_max: ledger_index, // optional, defaults to latest
//   binary: boolean,                // optional, defaults to false
//   forward: boolean,               // optional, defaults to false
//   limit: integer,                 // optional
//   marker: opaque                  // optional, resume previous query
// }
json::value doaccounttx (rpc::context& context)
{
    auto& params = context.params;

    rippleaddress   raaccount;
    int limit = params.ismember (jss::limit) ?
            params[jss::limit].asuint () : -1;
    bool bbinary = params.ismember ("binary") && params["binary"].asbool ();
    bool bforward = params.ismember ("forward") && params["forward"].asbool ();
    std::uint32_t   uledgermin;
    std::uint32_t   uledgermax;
    std::uint32_t   uvalidatedmin;
    std::uint32_t   uvalidatedmax;
    bool bvalidated = context.netops.getvalidatedrange (
        uvalidatedmin, uvalidatedmax);

    if (!bvalidated)
    {
        // don't have a validated ledger range.
        return rpcerror (rpclgr_idxs_invalid);
    }

    if (!params.ismember ("account"))
        return rpcerror (rpcinvalid_params);

    if (!raaccount.setaccountid (params["account"].asstring ()))
        return rpcerror (rpcact_malformed);

    context.loadtype = resource::feemediumburdenrpc;

    if (params.ismember ("ledger_index_min") ||
        params.ismember ("ledger_index_max"))
    {
        std::int64_t iledgermin  = params.ismember ("ledger_index_min")
                ? params["ledger_index_min"].asint () : -1;
        std::int64_t iledgermax  = params.ismember ("ledger_index_max")
                ? params["ledger_index_max"].asint () : -1;

        uledgermin  = iledgermin == -1 ? uvalidatedmin :
            ((iledgermin >= uvalidatedmin) ? iledgermin : uvalidatedmin);
        uledgermax  = iledgermax == -1 ? uvalidatedmax :
            ((iledgermax <= uvalidatedmax) ? iledgermax : uvalidatedmax);

        if (uledgermax < uledgermin)
            return rpcerror (rpclgr_idxs_invalid);
    }
    else
    {
        ledger::pointer l;
        json::value ret = rpc::lookupledger (params, l, context.netops);

        if (!l)
            return ret;

        uledgermin = uledgermax = l->getledgerseq ();
    }

    std::string txtype = "";
    if (params.ismember("tx_type"))
    {
        txtype = params["tx_type"].asstring();
        //check validation of tx_type, protect from sql injection
        try {
            txformats::getinstance().findtypebyname(txtype);
        } catch (...) {
            writelog (lswarning, accounttx) <<
            "invalide tx_type " << txtype;
            txtype = "";
            return rpcerror (rpcinvalid_params);
        }
    }
    
    json::value resumetoken;

    if (params.ismember(jss::marker))
         resumetoken = params[jss::marker];

#ifndef beast_debug

    try
    {
#endif
        json::value ret (json::objectvalue);

        ret["account"] = raaccount.humanaccountid ();
        json::value& jvtxns = (ret["transactions"] = json::arrayvalue);

        if (bbinary)
        {
            auto txns = context.netops.gettxsaccountb (
                raaccount, uledgermin, uledgermax, bforward, resumetoken, limit,
                context.role == role::admin, txtype);

            for (auto& it: txns)
            {
                json::value& jvobj = jvtxns.append (json::objectvalue);

                jvobj["tx_blob"] = std::get<0> (it);
                jvobj["meta"] = std::get<1> (it);

                std::uint32_t uledgerindex = std::get<2> (it);

                jvobj["ledger_index"] = uledgerindex;
                jvobj[jss::validated] = bvalidated &&
                    uvalidatedmin <= uledgerindex &&
                    uvalidatedmax >= uledgerindex;
            }
        }
        else
        {
            auto txns = context.netops.gettxsaccount (
                raaccount, uledgermin, uledgermax, bforward, resumetoken, limit,
                context.role == role::admin, txtype);

            for (auto& it: txns)
            {
                json::value& jvobj = jvtxns.append (json::objectvalue);

                if (it.first)
                    jvobj[jss::tx] = it.first->getjson (1);

                if (it.second)
                {
                    auto meta = it.second->getjson (1);
                    addpaymentdeliveredamount (meta, context, it.first, it.second);
                    jvobj[jss::meta] = meta;

                    std::uint32_t uledgerindex = it.second->getlgrseq ();

                    jvobj[jss::validated] = bvalidated &&
                        uvalidatedmin <= uledgerindex &&
                        uvalidatedmax >= uledgerindex;
                }

            }
        }

        //add information about the original query
        ret[jss::ledger_index_min] = uledgermin;
        ret[jss::ledger_index_max] = uledgermax;
        if (params.ismember (jss::limit))
            ret[jss::limit]        = limit;
        if (!resumetoken.isnull())
            ret[jss::marker] = resumetoken;

        return ret;
#ifndef beast_debug
    }
    catch (...)
    {
        return rpcerror (rpcinternal);
    }

#endif
}

} // ripple
