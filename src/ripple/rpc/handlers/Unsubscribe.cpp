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

// fixme: this leaks rpcsub objects for json-rpc.  shouldn't matter for anyone
// sane.
json::value dounsubscribe (rpc::context& context)
{
    auto lock = getapp().masterlock();

    infosub::pointer ispsub;
    json::value jvresult (json::objectvalue);

    if (!context.infosub && !context.params.ismember ("url"))
    {
        // must be a json-rpc call.
        return rpcerror (rpcinvalid_params);
    }

    if (context.params.ismember ("url"))
    {
        if (context.role != role::admin)
            return rpcerror (rpcno_permission);

        std::string strurl  = context.params["url"].asstring ();
        ispsub  = context.netops.findrpcsub (strurl);

        if (!ispsub)
            return jvresult;
    }
    else
    {
        ispsub  = context.infosub;
    }

    if (context.params.ismember ("streams"))
    {
        for (auto& it: context.params["streams"])
        {
            if (it.isstring ())
            {
                std::string streamname = it.asstring ();

                if (streamname == "server")
                    context.netops.unsubserver (ispsub->getseq ());

                else if (streamname == "ledger")
                    context.netops.unsubledger (ispsub->getseq ());

                else if (streamname == "transactions")
                    context.netops.unsubtransactions (ispsub->getseq ());

                else if (streamname == "transactions_proposed"
                         || streamname == "rt_transactions") // deprecated
                    context.netops.unsubrttransactions (ispsub->getseq ());

                else
                    jvresult["error"] = "unknown stream: " + streamname;
            }
            else
            {
                jvresult["error"]   = "malformedsteam";
            }
        }
    }

    if (context.params.ismember ("accounts_proposed")
        || context.params.ismember ("rt_accounts"))
    {
        auto accounts  = rpc::parseaccountids (
                    context.params.ismember ("accounts_proposed")
                    ? context.params["accounts_proposed"]
                    : context.params["rt_accounts"]); // deprecated

        if (accounts.empty ())
            jvresult["error"]   = "malformedaccount";
        else
            context.netops.unsubaccount (ispsub->getseq (), accounts, true);
    }

    if (context.params.ismember ("accounts"))
    {
        auto accounts  = rpc::parseaccountids (context.params["accounts"]);

        if (accounts.empty ())
            jvresult["error"]   = "malformedaccount";
        else
            context.netops.unsubaccount (ispsub->getseq (), accounts, false);
    }

    if (!context.params.ismember ("books"))
    {
    }
    else if (!context.params["books"].isarray ())
    {
        return rpcerror (rpcinvalid_params);
    }
    else
    {
        for (auto& jv: context.params["books"])
        {
            if (!jv.isobject ()
                    || !jv.ismember ("taker_pays")
                    || !jv.ismember ("taker_gets")
                    || !jv["taker_pays"].isobject ()
                    || !jv["taker_gets"].isobject ())
                return rpcerror (rpcinvalid_params);

            bool bboth = (jv.ismember ("both") && jv["both"].asbool ()) ||
                    (jv.ismember ("both_sides") && jv["both_sides"].asbool ());
            // both_sides is deprecated.

            json::value taker_pays = jv["taker_pays"];
            json::value taker_gets = jv["taker_gets"];

            book book;

            // parse mandatory currency.
            if (!taker_pays.ismember ("currency")
                || !to_currency (
                    book.in.currency, taker_pays["currency"].asstring ()))
            {
                writelog (lsinfo, rpchandler) << "bad taker_pays currency.";
                return rpcerror (rpcsrc_cur_malformed);
            }
            // parse optional issuer.
            else if (((taker_pays.ismember ("issuer"))
                      && (!taker_pays["issuer"].isstring ()
                          || !to_issuer (
                              book.in.account, taker_pays["issuer"].asstring ())))
                     // don't allow illegal issuers.
                     || !isconsistent (book.in)
                     || noaccount() == book.in.account)
            {
                writelog (lsinfo, rpchandler) << "bad taker_pays issuer.";

                return rpcerror (rpcsrc_isr_malformed);
            }

            // parse mandatory currency.
            if (!taker_gets.ismember ("currency")
                    || !to_currency (book.out.currency,
                                     taker_gets["currency"].asstring ()))
            {
                writelog (lsinfo, rpchandler) << "bad taker_pays currency.";

                return rpcerror (rpcsrc_cur_malformed);
            }
            // parse optional issuer.
            else if (((taker_gets.ismember ("issuer"))
                      && (!taker_gets["issuer"].isstring ()
                          || !to_issuer (book.out.account,
                                         taker_gets["issuer"].asstring ())))
                     // don't allow illegal issuers.
                     || !isconsistent (book.out)
                     || noaccount() == book.out.account)
            {
                writelog (lsinfo, rpchandler) << "bad taker_gets issuer.";

                return rpcerror (rpcdst_isr_malformed);
            }

            if (book.in == book.out)
            {
                writelog (lsinfo, rpchandler)
                    << "taker_gets same as taker_pays.";
                return rpcerror (rpcbad_market);
            }

            context.netops.unsubbook (ispsub->getseq (), book);

            if (bboth)
                context.netops.unsubbook (ispsub->getseq (), book);
        }
    }

    return jvresult;
}

} // ripple
