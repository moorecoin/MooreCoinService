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
#include <ripple/basics/stringutilities.h>
#include <ripple/server/role.h>

namespace ripple {

// {
//   tx_json: <object>,
//   secret: <secret>
// }
json::value dosubmit (rpc::context& context)
{
    context.loadtype = resource::feemediumburdenrpc;

    if (!context.params.ismember ("tx_blob"))
    {
        bool bfailhard = context.params.ismember ("fail_hard")
                && context.params["fail_hard"].asbool ();
        return rpc::transactionsign (
            context.params, true, bfailhard, context.netops, context.role);
    }

    json::value                 jvresult;

    std::pair<blob, bool> ret(strunhex (context.params["tx_blob"].asstring ()));

    if (!ret.second || !ret.first.size ())
        return rpcerror (rpcinvalid_params);

    serializer                  strans (ret.first);
    serializeriterator          sittrans (strans);

    sttx::pointer stptrans;

    try
    {
        stptrans = std::make_shared<sttx> (std::ref (sittrans));
    }
    catch (std::exception& e)
    {
        jvresult[jss::error]           = "invalidtransaction";
        jvresult["error_exception"] = e.what ();

        return jvresult;
    }

    transaction::pointer            tptrans;

    try
    {
        tptrans     = std::make_shared<transaction> (stptrans, validate::yes);
    }
    catch (std::exception& e)
    {
        jvresult[jss::error]           = "internaltransaction";
        jvresult["error_exception"] = e.what ();

        return jvresult;
    }

    if (tptrans->getstatus() != new)
    {
        jvresult[jss::error]            = "invalidtransactions";
        jvresult["error_exception"] = "fails local checks";

        return jvresult;
    }

    try
    {
        (void) context.netops.processtransaction (
            tptrans, context.role == role::admin, true,
            context.params.ismember ("fail_hard")
            && context.params["fail_hard"].asbool ());
    }
    catch (std::exception& e)
    {
        jvresult[jss::error]           = "internalsubmit";
        jvresult[jss::error_exception] = e.what ();

        return jvresult;
    }


    try
    {
        jvresult[jss::tx_json] = tptrans->getjson (0);
        jvresult[jss::tx_blob] = strhex (
            tptrans->getstransaction ()->getserializer ().peekdata ());

        if (temuncertain != tptrans->getresult ())
        {
            std::string stoken;
            std::string shuman;

            transresultinfo (tptrans->getresult (), stoken, shuman);

            jvresult[jss::engine_result]           = stoken;
            jvresult[jss::engine_result_code]      = tptrans->getresult ();
            jvresult[jss::engine_result_message]   = shuman;
        }

        return jvresult;
    }
    catch (std::exception& e)
    {
        jvresult[jss::error]           = "internaljson";
        jvresult[jss::error_exception] = e.what ();

        return jvresult;
    }
}

} // ripple
