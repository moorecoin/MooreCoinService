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
#include <ripple/app/paths/accountcurrencies.h>
#include <ripple/app/paths/findpaths.h>
#include <ripple/app/paths/ripplecalc.h>
#include <ripple/core/loadfeetrack.h>
#include <ripple/protocol/stparsedjson.h>
#include <ripple/rpc/impl/legacypathfind.h>
#include <ripple/server/role.h>

namespace ripple {

// this interface is deprecated.
json::value doripplepathfind (rpc::context& context)
{
    rpc::legacypathfind lpf (context.role == role::admin);
    if (!lpf.isok ())
        return rpcerror (rpctoo_busy);

    context.loadtype = resource::feehighburdenrpc;

    rippleaddress rasrc;
    rippleaddress radst;
    stamount sadstamount;
    ledger::pointer lpledger;

    json::value jvresult;

    if (getconfig().run_standalone ||
        context.params.ismember(jss::ledger) ||
        context.params.ismember(jss::ledger_index) ||
        context.params.ismember(jss::ledger_hash))
    {
        // the caller specified a ledger
        jvresult = rpc::lookupledger (
            context.params, lpledger, context.netops);
        if (!lpledger)
            return jvresult;
    }

    if (!context.params.ismember ("source_account"))
    {
        jvresult = rpcerror (rpcsrc_act_missing);
    }
    else if (!context.params["source_account"].isstring ()
             || !rasrc.setaccountid (
                 context.params["source_account"].asstring ()))
    {
        jvresult = rpcerror (rpcsrc_act_malformed);
    }
    else if (!context.params.ismember ("destination_account"))
    {
        jvresult = rpcerror (rpcdst_act_missing);
    }
    else if (!context.params["destination_account"].isstring ()
             || !radst.setaccountid (
                 context.params["destination_account"].asstring ()))
    {
        jvresult = rpcerror (rpcdst_act_malformed);
    }
    else if (
        // parse sadstamount.
        !context.params.ismember ("destination_amount")
        || ! amountfromjsonnothrow(sadstamount, context.params["destination_amount"])
        || sadstamount <= zero
		|| (!isnative(sadstamount.getcurrency())
            && (!sadstamount.getissuer () ||
                noaccount() == sadstamount.getissuer ())))
    {
        writelog (lsinfo, rpchandler) << "bad destination_amount.";
        jvresult    = rpcerror (rpcinvalid_params);
    }
    else if (
        // checks on source_currencies.
        context.params.ismember ("source_currencies")
        && (!context.params["source_currencies"].isarray ()
            || !context.params["source_currencies"].size ())
        // don't allow empty currencies.
    )
    {
        writelog (lsinfo, rpchandler) << "bad source_currencies.";
        jvresult    = rpcerror (rpcinvalid_params);
    }
    else
    {
        context.loadtype = resource::feehighburdenrpc;
        ripplelinecache::pointer cache;

        if (lpledger)
        {
            // the caller specified a ledger
            lpledger = std::make_shared<ledger> (std::ref (*lpledger), false);
            cache = std::make_shared<ripplelinecache>(lpledger);
        }
        else
        {
            // the closed ledger is recent and any nodes made resident
            // have the best chance to persist
            lpledger = context.netops.getclosedledger();
            cache = getapp().getpathrequests().getlinecache(lpledger, false);
        }

        json::value     jvsrccurrencies;

        if (context.params.ismember ("source_currencies"))
        {
            jvsrccurrencies = context.params["source_currencies"];
        }
        else
        {
            auto currencies = accountsourcecurrencies (rasrc, cache, true);
            jvsrccurrencies = json::value (json::arrayvalue);

            for (auto const& ucurrency: currencies)
            {
                json::value jvcurrency (json::objectvalue);
                jvcurrency["currency"] = to_string(ucurrency);
                jvsrccurrencies.append (jvcurrency);
            }
        }

        // fill in currencies destination will accept
        json::value jvdestcur (json::arrayvalue);

        // todo(tom): this could be optimized the same way that
        // pathrequest::doupdate() is - if we don't obsolete this code first.
        auto usdestcurrid = accountdestcurrencies (radst, cache, true);
        for (auto const& ucurrency: usdestcurrid)
                jvdestcur.append (to_string (ucurrency));

        jvresult["destination_currencies"] = jvdestcur;
        jvresult["destination_account"] = radst.humanaccountid ();

        json::value jvarray (json::arrayvalue);

        int level = getconfig().path_search_old;
        if ((getconfig().path_search_max > level)
            && !getapp().getfeetrack().isloadedlocal())
        {
            ++level;
        }

        if (context.params.ismember("search_depth")
            && context.params["search_depth"].isintegral())
        {
            int rlev = context.params["search_depth"].asint ();
            if ((rlev < level) || (context.role == role::admin))
                level = rlev;
        }

        findpaths fp (
            cache,
            rasrc.getaccountid(),
            radst.getaccountid(),
            sadstamount,
            level,
            4); // max paths

        for (unsigned int i = 0; i != jvsrccurrencies.size (); ++i)
        {
            json::value jvsource        = jvsrccurrencies[i];

            currency usrccurrencyid;
            account usrcissuerid;

            if (!jvsource.isobject ())
                return rpcerror (rpcinvalid_params);

            // parse mandatory currency.
            if (!jvsource.ismember ("currency")
                || !to_currency (
                    usrccurrencyid, jvsource["currency"].asstring ()))
            {
                writelog (lsinfo, rpchandler) << "bad currency.";

                return rpcerror (rpcsrc_cur_malformed);
            }

            if (usrccurrencyid == assetcurrency())
                continue;

            if (isvbc(usrccurrencyid))
                usrcissuerid = vbcaccount();
            else if (usrccurrencyid.isnonzero ())
                usrcissuerid = rasrc.getaccountid ();

            // parse optional issuer.
            if (jvsource.ismember ("issuer") &&
                ((!jvsource["issuer"].isstring () ||
                  !to_issuer (usrcissuerid, jvsource["issuer"].asstring ())) ||
                 (usrcissuerid.iszero () != usrccurrencyid.iszero ()) ||
                 (isvbc(usrcissuerid) != isvbc(usrccurrencyid)) ||
                 (noaccount() == usrcissuerid)))
            {
                writelog (lsinfo, rpchandler) << "bad issuer.";
                return rpcerror (rpcsrc_isr_malformed);
            }

            stpathset spscomputed;
            if (context.params.ismember("paths"))
            {
                json::value pathset = json::objectvalue;
                pathset["paths"] = context.params["paths"];
                stparsedjsonobject paths ("pathset", pathset);
                if (paths.object.get() == nullptr)
                    return paths.error;
                else
                {
                    spscomputed = paths.object.get()->getfieldpathset (sfpaths);
                    writelog (lstrace, rpchandler) << "ripple_path_find: paths: " << spscomputed.getjson (0);
                }
            }

            stpath fullliquiditypath;
            auto valid = fp.findpathsforissue (
                {usrccurrencyid, usrcissuerid},
                spscomputed,
                fullliquiditypath);
            if (!valid)
            {
                writelog (lswarning, rpchandler)
                    << "ripple_path_find: no paths found.";
            }
            else
            {
                auto& issuer =
                    isxrp (usrcissuerid) ?
                        isxrp (usrccurrencyid) ? // default to source account.
                            xrpaccount() :
                            account (rasrc.getaccountid ())
                    : (isvbc(usrcissuerid) ?
                        (isvbc(usrccurrencyid) ?
                            vbcaccount() :
                            account (rasrc.getaccountid ()))
                        : usrcissuerid);            // use specifed issuer.

                stamount samaxamount ({usrccurrencyid, issuer}, 1);
                samaxamount.negate ();

                ledgerentryset lessandbox (lpledger, tapnone);

                auto rc = path::ripplecalc::ripplecalculate (
                    lessandbox,
                    samaxamount,            // --> amount to send is unlimited
                                            //     to get an estimate.
                    sadstamount,            // --> amount to deliver.
                    radst.getaccountid (),  // --> account to deliver to.
                    rasrc.getaccountid (),  // --> account sending from.
                    spscomputed);           // --> path set.

                writelog (lswarning, rpchandler)
                    << "ripple_path_find:"
                    << " samaxamount=" << samaxamount
                    << " sadstamount=" << sadstamount
                    << " samaxamountact=" << rc.actualamountin
                    << " sadstamountact=" << rc.actualamountout;

                if (fullliquiditypath.size() > 0 &&
                    (rc.result() == terno_line || rc.result() == tecpath_partial))
                {
                    writelog (lsdebug, pathrequest)
                        << "trying with an extra path element";

                    spscomputed.push_back (fullliquiditypath);
                    lessandbox.clear ();
                    rc = path::ripplecalc::ripplecalculate (
                        lessandbox,
                        samaxamount,            // --> amount to send is unlimited
                        //     to get an estimate.
                        sadstamount,            // --> amount to deliver.
                        radst.getaccountid (),  // --> account to deliver to.
                        rasrc.getaccountid (),  // --> account sending from.
                        spscomputed);         // --> path set.
                    writelog (lsdebug, pathrequest)
                        << "extra path element gives "
                        << transhuman (rc.result ());
                }

                if (rc.result () == tessuccess)
                {
                    json::value jventry (json::objectvalue);

                    stpathset   spscanonical;

                    // reuse the expanded as it would need to be calcuated
                    // anyway to produce the canonical.  (at least unless we
                    // make a direct canonical.)

                    jventry["source_amount"] = rc.actualamountin.getjson (0);
                    jventry["paths_canonical"]  = json::arrayvalue;
                    jventry["paths_computed"]   = spscomputed.getjson (0);

                    jvarray.append (jventry);
                }
                else
                {
                    std::string strtoken;
                    std::string strhuman;

                    transresultinfo (rc.result (), strtoken, strhuman);

                    writelog (lsdebug, rpchandler)
                        << "ripple_path_find: "
                        << strtoken << " "
                        << strhuman << " "
                        << spscomputed.getjson (0);
                }
            }
        }

        // each alternative differs by source currency.
        jvresult["alternatives"] = jvarray;
    }

    writelog (lsdebug, rpchandler)
            << boost::str (boost::format ("ripple_path_find< %s")
                           % jvresult);

    return jvresult;
}

} // ripple
