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
#include <ripple/protocol/indexes.h>

namespace ripple {

// {
//   ledger_hash : <ledger>
//   ledger_index : <ledger_index>
//   ...
// }
json::value doledgerentry (rpc::context& context)
{
    ledger::pointer lpledger;
    json::value jvresult = rpc::lookupledger (
        context.params, lpledger, context.netops);

    if (!lpledger)
        return jvresult;

    uint256     unodeindex;
    bool        bnodebinary = false;

    if (context.params.ismember ("index"))
    {
        // xxx needs to provide proof.
        unodeindex.sethex (context.params["index"].asstring ());
        bnodebinary = true;
    }
    else if (context.params.ismember ("account_root"))
    {
        rippleaddress   naaccount;

        if (!naaccount.setaccountid (
                context.params["account_root"].asstring ())
            || !naaccount.getaccountid ())
        {
            jvresult["error"]   = "malformedaddress";
        }
        else
        {
            unodeindex
                    = getaccountrootindex (naaccount.getaccountid ());
        }
    }
    else if (context.params.ismember ("directory"))
    {
        if (!context.params["directory"].isobject ())
        {
            unodeindex.sethex (context.params["directory"].asstring ());
        }
        else if (context.params["directory"].ismember ("sub_index")
                 && !context.params["directory"]["sub_index"].isintegral ())
        {
            jvresult["error"]   = "malformedrequest";
        }
        else
        {
            std::uint64_t  usubindex
                    = context.params["directory"].ismember ("sub_index")
                    ? context.params["directory"]["sub_index"].asuint () : 0;

            if (context.params["directory"].ismember ("dir_root"))
            {
                uint256 udirroot;

                udirroot.sethex (context.params["dir_root"].asstring ());

                unodeindex  = getdirnodeindex (udirroot, usubindex);
            }
            else if (context.params["directory"].ismember ("owner"))
            {
                rippleaddress   naownerid;

                if (!naownerid.setaccountid (
                        context.params["directory"]["owner"].asstring ()))
                {
                    jvresult["error"]   = "malformedaddress";
                }
                else
                {
                    uint256 udirroot
                            = getownerdirindex (
                                naownerid.getaccountid ());
                    unodeindex  = getdirnodeindex (udirroot, usubindex);
                }
            }
            else
            {
                jvresult["error"]   = "malformedrequest";
            }
        }
    }
    else if (context.params.ismember ("generator"))
    {
        rippleaddress   nageneratorid;

        if (!context.params["generator"].isobject ())
        {
            unodeindex.sethex (context.params["generator"].asstring ());
        }
        else if (!context.params["generator"].ismember ("regular_seed"))
        {
            jvresult["error"]   = "malformedrequest";
        }
        else if (!nageneratorid.setseedgeneric (
            context.params["generator"]["regular_seed"].asstring ()))
        {
            jvresult["error"]   = "malformedaddress";
        }
        else
        {
            rippleaddress na0public;      // to find the generator's index.
            rippleaddress nagenerator
                    = rippleaddress::creategeneratorpublic (nageneratorid);

            na0public.setaccountpublic (nagenerator, 0);

            unodeindex  = getgeneratorindex (na0public.getaccountid ());
        }
    }
    else if (context.params.ismember ("offer"))
    {
        rippleaddress   naaccountid;

        if (!context.params["offer"].isobject ())
        {
            unodeindex.sethex (context.params["offer"].asstring ());
        }
        else if (!context.params["offer"].ismember ("account")
                 || !context.params["offer"].ismember ("seq")
                 || !context.params["offer"]["seq"].isintegral ())
        {
            jvresult["error"]   = "malformedrequest";
        }
        else if (!naaccountid.setaccountid (
            context.params["offer"]["account"].asstring ()))
        {
            jvresult["error"]   = "malformedaddress";
        }
        else
        {
            unodeindex  = getofferindex (naaccountid.getaccountid (),
                context.params["offer"]["seq"].asuint ());
        }
    }
    else if (context.params.ismember ("ripple_state"))
    {
        rippleaddress   naa;
        rippleaddress   nab;
        currency         ucurrency;
        json::value     jvripplestate   = context.params["ripple_state"];

        if (!jvripplestate.isobject ()
            || !jvripplestate.ismember ("currency")
            || !jvripplestate.ismember ("accounts")
            || !jvripplestate["accounts"].isarray ()
            || 2 != jvripplestate["accounts"].size ()
            || !jvripplestate["accounts"][0u].isstring ()
            || !jvripplestate["accounts"][1u].isstring ()
            || (jvripplestate["accounts"][0u].asstring ()
                == jvripplestate["accounts"][1u].asstring ())
           )
        {
            jvresult["error"]   = "malformedrequest";
        }
        else if (!naa.setaccountid (
                     jvripplestate["accounts"][0u].asstring ())
                 || !nab.setaccountid (
                     jvripplestate["accounts"][1u].asstring ()))
        {
            jvresult["error"]   = "malformedaddress";
        }
        else if (!to_currency (
            ucurrency, jvripplestate["currency"].asstring ()))
        {
            jvresult["error"]   = "malformedcurrency";
        }
        else
        {
            unodeindex  = getripplestateindex (
                naa.getaccountid (), nab.getaccountid (), ucurrency);
        }
    }
    else if (context.params.ismember ("dividend"))
    {
        unodeindex = getledgerdividendindex();
    }
    else if (context.params.ismember ("account_refer"))
    {
        rippleaddress   naaccount;
        
        if (!naaccount.setaccountid (context.params["account_refer"].asstring())
            || !naaccount.getaccountid ())
        {
            jvresult["error"]   = "malformedaddress";
        }
        else
        {
            unodeindex = getaccountreferindex(naaccount.getaccountid());
        }
    }
    else if (context.params.ismember("asset")) {
        rippleaddress naaccount;
        currency ucurrency;
        json::value jvasset = context.params["asset"];

        if (!jvasset.isobject()
            || !jvasset.ismember("currency")
            || !jvasset.ismember("account")
            || !jvasset["account"].isstring()) {
            jvresult["error"] = "malformedrequest";
        } else if (!naaccount.setaccountid(
                       jvasset["account"].asstring())) {
            jvresult["error"] = "malformedaddress";
        } else if (!to_currency(
                       ucurrency, jvasset["currency"].asstring())) {
            jvresult["error"] = "malformedcurrency";
        } else {
            unodeindex = getassetindex(naaccount.getaccountid(), ucurrency);
        }
    } else if (context.params.ismember("asset_state")) {
        rippleaddress naa;
        rippleaddress nab;
        currency ucurrency;
        json::value jvassetstate = context.params["asset_state"];

        if (!jvassetstate.isobject()
            || !jvassetstate.ismember("currency")
            || !jvassetstate.ismember("accounts")
            || !jvassetstate["accounts"].isarray()
            || 2 != jvassetstate["accounts"].size()
            || !jvassetstate["accounts"][0u].isstring()
            || !jvassetstate["accounts"][1u].isstring()
            || (jvassetstate["accounts"][0u].asstring() == jvassetstate["accounts"][1u].asstring())) {
            jvresult["error"] = "malformedrequest";
        } else if (!naa.setaccountid(
                       jvassetstate["accounts"][0u].asstring()) ||
                   !nab.setaccountid(
                       jvassetstate["accounts"][1u].asstring())) {
            jvresult["error"] = "malformedaddress";
        } else if (!to_currency(
                       ucurrency, jvassetstate["currency"].asstring())) {
            jvresult["error"] = "malformedcurrency";
        } else {
            std::uint32_t udate = jvassetstate.ismember (jss::date) ? jvassetstate[jss::date].asuint () : 0;
            unodeindex = getqualityindex (
                getassetstateindex (naa.getaccountid (), nab.getaccountid (), ucurrency),
                udate);
        }
    }
    else
    {
        jvresult["error"]   = "unknownoption";
    }

    if (unodeindex.isnonzero ())
    {
        auto slenode = context.netops.getslei (lpledger, unodeindex);

        if (context.params.ismember("binary"))
            bnodebinary = context.params["binary"].asbool();

        if (!slenode)
        {
            // not found.
            // xxx should also provide proof.
            jvresult["error"]       = "entrynotfound";
        }
        else if (bnodebinary)
        {
            // xxx should also provide proof.
            serializer s;

            slenode->add (s);

            jvresult["node_binary"] = strhex (s.peekdata ());
            jvresult["index"]       = to_string (unodeindex);
        }
        else
        {
            jvresult["node"]        = slenode->getjson (0);
            jvresult["index"]       = to_string (unodeindex);
        }
    }

    return jvresult;
}

} // ripple
