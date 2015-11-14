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
//   ledger_index_min: ledger_index,
//   ledger_index_max: ledger_index,
//   binary: boolean,              // optional, defaults to false
//   count: boolean,               // optional, defaults to false
//   descending: boolean,          // optional, defaults to false
//   offset: integer,              // optional, defaults to 0
//   limit: integer                // optional
// }
json::value doaccounttxold (rpc::context& context)
{
    rippleaddress   raaccount;
    std::uint32_t offset
            = context.params.ismember ("offset")
            ? context.params["offset"].asuint () : 0;
    int limit = context.params.ismember ("limit")
            ? context.params["limit"].asuint () : -1;
    bool bbinary = context.params.ismember ("binary")
            && context.params["binary"].asbool ();
    bool bdescending = context.params.ismember ("descending")
            && context.params["descending"].asbool ();
    bool bcount = context.params.ismember ("count")
            && context.params["count"].asbool ();
    std::uint32_t   uledgermin;
    std::uint32_t   uledgermax;
    std::uint32_t   uvalidatedmin;
    std::uint32_t   uvalidatedmax;
    bool bvalidated  = context.netops.getvalidatedrange (
        uvalidatedmin, uvalidatedmax);

    if (!context.params.ismember ("account"))
        return rpcerror (rpcinvalid_params);

    if (!raaccount.setaccountid (context.params["account"].asstring ()))
        return rpcerror (rpcact_malformed);

    if (offset > 3000)
        return rpcerror (rpcatx_deprecated);

    context.loadtype = resource::feehighburdenrpc;

    // deprecated
    if (context.params.ismember ("ledger_min"))
    {
        context.params["ledger_index_min"]   = context.params["ledger_min"];
        bdescending = true;
    }

    // deprecated
    if (context.params.ismember ("ledger_max"))
    {
        context.params["ledger_index_max"]   = context.params["ledger_max"];
        bdescending = true;
    }

    if (context.params.ismember ("ledger_index_min")
        || context.params.ismember ("ledger_index_max"))
    {
        std::int64_t iledgermin  = context.params.ismember ("ledger_index_min")
                ? context.params["ledger_index_min"].asint () : -1;
        std::int64_t iledgermax  = context.params.ismember ("ledger_index_max")
                ? context.params["ledger_index_max"].asint () : -1;

        if (!bvalidated && (iledgermin == -1 || iledgermax == -1))
        {
            // don't have a validated ledger range.
            return rpcerror (rpclgr_idxs_invalid);
        }

        uledgermin  = iledgermin == -1 ? uvalidatedmin : iledgermin;
        uledgermax  = iledgermax == -1 ? uvalidatedmax : iledgermax;

        if (uledgermax < uledgermin)
        {
            return rpcerror (rpclgr_idxs_invalid);
        }
    }
    else
    {
        ledger::pointer l;
        json::value ret = rpc::lookupledger (context.params, l, context.netops);

        if (!l)
            return ret;

        uledgermin = uledgermax = l->getledgerseq ();
    }

    int count = 0;

#ifndef beast_debug

    try
    {
#endif

        json::value ret (json::objectvalue);

        ret["account"] = raaccount.humanaccountid ();
        json::value& jvtxns = (ret["transactions"] = json::arrayvalue);

        if (bbinary)
        {
            auto txns = context.netops.getaccounttxsb (
                raaccount, uledgermin, uledgermax, bdescending, offset, limit,
                context.role == role::admin);

            for (auto it = txns.begin (), end = txns.end (); it != end; ++it)
            {
                ++count;
                json::value& jvobj = jvtxns.append (json::objectvalue);

                std::uint32_t  uledgerindex = std::get<2> (*it);
                jvobj["tx_blob"]            = std::get<0> (*it);
                jvobj["meta"]               = std::get<1> (*it);
                jvobj["ledger_index"]       = uledgerindex;
                jvobj["validated"]
                        = bvalidated
                        && uvalidatedmin <= uledgerindex
                        && uvalidatedmax >= uledgerindex;

            }
        }
        else
        {
            auto txns = context.netops.getaccounttxs (
                raaccount, uledgermin, uledgermax, bdescending, offset, limit,
                context.role == role::admin);

            for (auto it = txns.begin (), end = txns.end (); it != end; ++it)
            {
                ++count;
                json::value&    jvobj = jvtxns.append (json::objectvalue);

                if (it->first)
                    jvobj["tx"]             = it->first->getjson (1);

                if (it->second)
                {
                    std::uint32_t uledgerindex = it->second->getlgrseq ();

                    jvobj["meta"]           = it->second->getjson (0);
                    jvobj["validated"]
                            = bvalidated
                            && uvalidatedmin <= uledgerindex
                            && uvalidatedmax >= uledgerindex;
                }

            }
        }

        //add information about the original query
        ret["ledger_index_min"] = uledgermin;
        ret["ledger_index_max"] = uledgermax;
        ret["validated"]
                = bvalidated
                && uvalidatedmin <= uledgermin
                && uvalidatedmax >= uledgermax;
        ret["offset"]           = offset;

        // we no longer return the full count but only the count of returned
        // transactions. computing this count was two expensive and this api is
        // deprecated anyway.
        if (bcount)
            ret["count"]        = count;

        if (context.params.ismember ("limit"))
            ret["limit"]        = limit;


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
