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
#include <ripple/net/rpcsub.h>
#include <ripple/rpc/impl/parseaccountids.h>
#include <ripple/server/role.h>

namespace ripple {

json::value dosubscribe (rpc::context& context)
{
    auto lock = getapp().masterlock();

    // fixme: this needs to release the master lock immediately
    // subscriptions need to be protected by their own lock

    infosub::pointer ispsub;
    json::value jvresult (json::objectvalue);
    std::uint32_t uledgerindex = context.params.ismember (jss::ledger_index)
            && context.params[jss::ledger_index].isnumeric ()
            ? context.params[jss::ledger_index].asuint ()
            : 0;

    if (!context.infosub && !context.params.ismember ("url"))
    {
        // must be a json-rpc call.
        writelog (lsinfo, rpchandler)
            << "dosubscribe: rpc subscribe requires a url";

        return rpcerror (rpcinvalid_params);
    }

    if (context.params.ismember ("url"))
    {
        if (context.role != role::admin)
            return rpcerror (rpcno_permission);

        std::string strurl      = context.params["url"].asstring ();
        std::string strusername = context.params.ismember ("url_username") ?
                context.params["url_username"].asstring () : "";
        std::string strpassword = context.params.ismember ("url_password") ?
                context.params["url_password"].asstring () : "";

        // deprecated
        if (context.params.ismember ("username"))
            strusername = context.params["username"].asstring ();

        // deprecated
        if (context.params.ismember ("password"))
            strpassword = context.params["password"].asstring ();

        ispsub  = context.netops.findrpcsub (strurl);

        if (!ispsub)
        {
            writelog (lsdebug, rpchandler)
                << "dosubscribe: building: " << strurl;

            rpcsub::pointer rspsub = rpcsub::new (getapp ().getops (),
                getapp ().getioservice (), getapp ().getjobqueue (),
                    strurl, strusername, strpassword);
            ispsub  = context.netops.addrpcsub (
                strurl, std::dynamic_pointer_cast<infosub> (rspsub));
        }
        else
        {
            writelog (lstrace, rpchandler)
                << "dosubscribe: reusing: " << strurl;

            if (context.params.ismember ("username"))
                dynamic_cast<rpcsub*> (&*ispsub)->setusername (strusername);

            if (context.params.ismember ("password"))
                dynamic_cast<rpcsub*> (&*ispsub)->setpassword (strpassword);
        }
    }
    else
    {
        ispsub  = context.infosub;
    }

    if (!context.params.ismember ("streams"))
    {
    }
    else if (!context.params["streams"].isarray ())
    {
        writelog (lsinfo, rpchandler)
            << "dosubscribe: streams requires an array.";

        return rpcerror (rpcinvalid_params);
    }
    else
    {
        for (auto& it: context.params["streams"])
        {
            if (it.isstring ())
            {
                std::string streamname = it.asstring ();

                if (streamname == "server")
                {
                    context.netops.subserver (ispsub, jvresult,
                        context.role == role::admin);
                }
                else if (streamname == "ledger")
                {
                    context.netops.subledger (ispsub, jvresult);
                }
                else if (streamname == "transactions")
                {
                    context.netops.subtransactions (ispsub);
                }
                else if (streamname == "transactions_proposed"
                         || streamname == "rt_transactions") // deprecated
                {
                    context.netops.subrttransactions (ispsub);
                }
                else
                {
                    jvresult[jss::error]   = "unknownstream";
                }
            }
            else
            {
                jvresult[jss::error]   = "malformedstream";
            }
        }
    }

    std::string straccountsproposed =
               context.params.ismember ("accounts_proposed")
               ? "accounts_proposed" : "rt_accounts";  // deprecated

    if (!context.params.ismember (straccountsproposed))
    {
    }
    else if (!context.params[straccountsproposed].isarray ())
    {
        return rpcerror (rpcinvalid_params);
    }
    else
    {
        auto ids  = rpc::parseaccountids (context.params[straccountsproposed]);

        if (ids.empty ())
            jvresult[jss::error] = "malformedaccount";
        else
            context.netops.subaccount (ispsub, ids, uledgerindex, true);
    }

    if (!context.params.ismember ("accounts"))
    {
    }
    else if (!context.params["accounts"].isarray ())
    {
        return rpcerror (rpcinvalid_params);
    }
    else
    {
        auto ids  = rpc::parseaccountids (context.params["accounts"]);

        if (ids.empty ())
        {
            jvresult[jss::error]   = "malformedaccount";
        }
        else
        {
            context.netops.subaccount (ispsub, ids, uledgerindex, false);
            writelog (lsdebug, rpchandler)
                << "dosubscribe: accounts: " << ids.size ();
        }
    }

    bool bhavemasterlock = true;
    if (!context.params.ismember ("books"))
    {
    }
    else if (!context.params["books"].isarray ())
    {
        return rpcerror (rpcinvalid_params);
    }
    else
    {
        for (auto& j: context.params["books"])
        {
            if (!j.isobject ()
                    || !j.ismember (jss::taker_pays)
                    || !j.ismember (jss::taker_gets)
                    || !j[jss::taker_pays].isobject ()
                    || !j[jss::taker_gets].isobject ())
                return rpcerror (rpcinvalid_params);

            book book;
            bool bboth =
                    (j.ismember ("both") && j["both"].asbool ()) ||
                    (j.ismember ("both_sides") && j["both_sides"].asbool ());
            bool bsnapshot =
                    (j.ismember ("snapshot") && j["snapshot"].asbool ()) ||
                    (j.ismember ("state_now") && j["state_now"].asbool ());
            // todo(tom): both_sides and state_now are apparently deprecated...
            // where is this documented?

            json::value taker_pays = j[jss::taker_pays];
            json::value taker_gets = j[jss::taker_gets];

            // parse mandatory currency.
            if (!taker_pays.ismember (jss::currency)
                    || !to_currency (book.in.currency,
                                     taker_pays[jss::currency].asstring ()))
            {
                writelog (lsinfo, rpchandler) << "bad taker_pays currency.";

                return rpcerror (rpcsrc_cur_malformed);
            }
            // parse optional issuer.
            else if (((taker_pays.ismember (jss::issuer))
                      && (!taker_pays[jss::issuer].isstring ()
                          || !to_issuer (book.in.account,
                                         taker_pays[jss::issuer].asstring ())))
                     // don't allow illegal issuers.
                     || (!book.in.currency != !book.in.account)
                     || noaccount() == book.in.account)
            {
                writelog (lsinfo, rpchandler) << "bad taker_pays issuer.";

                return rpcerror (rpcsrc_isr_malformed);
            }

            // parse mandatory currency.
            if (!taker_gets.ismember (jss::currency)
                    || !to_currency (book.out.currency,
                                     taker_gets[jss::currency].asstring ()))
            {
                writelog (lsinfo, rpchandler) << "bad taker_pays currency.";

                return rpcerror (rpcsrc_cur_malformed);
            }
            // parse optional issuer.
            else if (((taker_gets.ismember (jss::issuer))
                      && (!taker_gets[jss::issuer].isstring ()
                          || !to_issuer (book.out.account,
                                         taker_gets[jss::issuer].asstring ())))
                     // don't allow illegal issuers.
                     || (!book.out.currency != !book.out.account)
                     || noaccount() == book.out.account)
            {
                writelog (lsinfo, rpchandler) << "bad taker_gets issuer.";

                return rpcerror (rpcdst_isr_malformed);
            }

            if (book.in.currency == book.out.currency
                    && book.in.account == book.out.account)
            {
                writelog (lsinfo, rpchandler)
                    << "taker_gets same as taker_pays.";
                return rpcerror (rpcbad_market);
            }

            rippleaddress   ratakerid;

            if (!j.ismember ("taker"))
                ratakerid.setaccountid (noaccount());
            else if (!ratakerid.setaccountid (j["taker"].asstring ()))
                return rpcerror (rpcbad_issuer);

            if (!isconsistent (book))
            {
                writelog (lswarning, rpchandler) << "bad market: " << book;
                return rpcerror (rpcbad_market);
            }

            context.netops.subbook (ispsub, book);

            if (bboth)
                context.netops.subbook (ispsub, reversed (book));

            if (bsnapshot)
            {
                if (bhavemasterlock)
                {
                    lock->unlock ();
                    bhavemasterlock = false;
                }

                context.loadtype = resource::feemediumburdenrpc;
                auto lpledger = getapp().getledgermaster ().
                        getpublishedledger ();
                if (lpledger)
                {
                    const json::value jvmarker = json::value (json::nullvalue);
                    json::value jvoffers (json::objectvalue);

                    auto add = [&](json::staticstring field)
                    {
                        context.netops.getbookpage (context.role == role::admin,
                            lpledger, field == jss::asks ? reversed (book) : book,
                            ratakerid.getaccountid(), false, 0, jvmarker,
                            jvoffers);

                        if (jvresult.ismember (field))
                        {
                            json::value& results (jvresult[field]);
                            for (auto const& e : jvoffers[jss::offers])
                                results.append (e);
                        }
                        else
                        {
                            jvresult[field] = jvoffers[jss::offers];
                        }
                    };

                    if (bboth)
                    {
                        add (jss::bids);
                        add (jss::asks);
                    }
                    else
                    {
                        add (jss::offers);
                    }
                }
            }
        }
    }

    return jvresult;
}

} // ripple
