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
#include <ripple/rpc/impl/tuning.h>
#include <ripple/app/paths/ripplestate.h>
#include <ripple/protocol/indexes.h>

namespace ripple {

struct visitdata
{
    std::vector <ripplestate::pointer> items;
    account const& accountid;
    rippleaddress const& rippleaddresspeer;
    account const& rapeeraccount;
};

void addline (json::value& jsonlines, ripplestate const& line, ledger::pointer ledger)
{
    stamount sabalance (line.getbalance ());
    stamount const& salimit (line.getlimit ());
    stamount const& salimitpeer (line.getlimitpeer ());
    json::value& jpeer (jsonlines.append (json::objectvalue));

    jpeer[jss::account] = to_string (line.getaccountidpeer ());
    if (assetcurrency() == sabalance.getcurrency()) {
        // calculate released & reserved balance for asset.
        auto lpledger = std::make_shared<ledger> (std::ref (*ledger), false);
        ledgerentryset les(lpledger, tapnone);
        auto sleripplestate = les.entrycache(ltripple_state, getripplestateindex(line.getaccountid(), line.getaccountidpeer(), assetcurrency()));
        les.assetrelease(line.getaccountid(), line.getaccountidpeer(), assetcurrency(), sleripplestate);
        stamount reserve = sleripplestate->getfieldamount(sfreserve);
        stamount balance = sleripplestate->getfieldamount(sfbalance);
        if (line.getaccountid() == sleripplestate->getfieldamount(sfhighlimit).getissuer()) {
            reserve.negate();
            balance.negate();
        }
        jpeer[jss::reserve] = reserve.gettext();
        jpeer[jss::balance] = balance.gettext();
    } else {
        // amount reported is positive if current account holds other
        // account's ious.
        //
        // amount reported is negative if other account holds current
        // account's ious.
        jpeer[jss::balance] = sabalance.gettext();
    }
    jpeer[jss::currency] = sabalance.gethumancurrency ();
    jpeer[jss::limit] = salimit.gettext ();
    jpeer[jss::limit_peer] = salimitpeer.gettext ();
    jpeer[jss::quality_in]
        = static_cast<json::uint> (line.getqualityin ());
    jpeer[jss::quality_out]
        = static_cast<json::uint> (line.getqualityout ());
    if (line.getauth ())
        jpeer[jss::authorized] = true;
    if (line.getauthpeer ())
        jpeer[jss::peer_authorized] = true;
    if (line.getnoripple ())
        jpeer[jss::no_ripple] = true;
    if (line.getnoripplepeer ())
        jpeer[jss::no_ripple_peer] = true;
    if (line.getfreeze ())
        jpeer[jss::freeze] = true;
    if (line.getfreezepeer ())
        jpeer[jss::freeze_peer] = true;
}

// {
//   account: <account>|<account_public_key>
//   account_index: <number>        // optional, defaults to 0.
//   ledger_hash : <ledger>
//   ledger_index : <ledger_index>
//   limit: integer                 // optional
//   marker: opaque                 // optional, resume previous query
// }
json::value doaccountlines (rpc::context& context)
{
    auto const& params (context.params);
    if (! params.ismember (jss::account))
        return rpc::missing_field_error ("account");

    ledger::pointer ledger;
    json::value result (rpc::lookupledger (params, ledger, context.netops));
    if (! ledger)
        return result;

    std::string strident (params[jss::account].asstring ());
    bool bindex (params.ismember (jss::account_index));
    int iindex (bindex ? params[jss::account_index].asuint () : 0);
    rippleaddress rippleaddress;

    json::value const jv (rpc::accountfromstring (ledger, rippleaddress, bindex,
        strident, iindex, false, context.netops));
    if (! jv.empty ())
    {
        for (json::value::const_iterator it (jv.begin ()); it != jv.end (); ++it)
            result[it.membername ()] = it.key ();

        return result;
    }

    if (! ledger->hasaccount (rippleaddress))
        return rpcerror (rpcact_not_found);

    std::string strpeer (params.ismember (jss::peer)
        ? params[jss::peer].asstring () : "");
    bool bpeerindex (params.ismember (jss::peer_index));
    int ipeerindex (bindex ? params[jss::peer_index].asuint () : 0);

    rippleaddress rippleaddresspeer;

    if (! strpeer.empty ())
    {
        result[jss::peer] = rippleaddress.humanaccountid ();

        if (bpeerindex)
            result[jss::peer_index] = ipeerindex;

        result = rpc::accountfromstring (ledger, rippleaddresspeer, bpeerindex,
            strpeer, ipeerindex, false, context.netops);

        if (! result.empty ())
            return result;
    }

    account rapeeraccount;
    if (rippleaddresspeer.isvalid ())
        rapeeraccount = rippleaddresspeer.getaccountid ();

    unsigned int limit;
    if (params.ismember (jss::limit))
    {
        auto const& jvlimit (params[jss::limit]);
        if (! jvlimit.isintegral ())
            return rpc::expected_field_error ("limit", "unsigned integer");

        limit = jvlimit.isuint () ? jvlimit.asuint () :
            std::max (0, jvlimit.asint ());

        if (context.role != role::admin)
        {
            limit = std::max (rpc::tuning::minlinesperrequest,
                std::min (limit, rpc::tuning::maxlinesperrequest));
        }
    }
    else
    {
        limit = rpc::tuning::defaultlinesperrequest;
    }

    json::value& jsonlines (result[jss::lines] = json::arrayvalue);
    account const& raaccount(rippleaddress.getaccountid ());
    visitdata visitdata = { {}, raaccount, rippleaddresspeer, rapeeraccount };
    unsigned int reserve (limit);
    uint256 startafter;
    std::uint64_t starthint;

    if (params.ismember (jss::marker))
    {
        // we have a start point. use limit - 1 from the result and use the
        // very last one for the resume.
        json::value const& marker (params[jss::marker]);

        if (! marker.isstring ())
            return rpc::expected_field_error ("marker", "string");

        startafter.sethex (marker.asstring ());
        sle::pointer sleline (ledger->getslei (startafter));

        if (sleline == nullptr || sleline->gettype () != ltripple_state)
            return rpcerror (rpcinvalid_params);

        if (sleline->getfieldamount (sflowlimit).getissuer () == raaccount)
            starthint = sleline->getfieldu64 (sflownode);
        else if (sleline->getfieldamount (sfhighlimit).getissuer () == raaccount)
            starthint = sleline->getfieldu64 (sfhighnode);
        else
            return rpcerror (rpcinvalid_params);

        // caller provided the first line (startafter), add it as first result
        auto const line (ripplestate::makeitem (raaccount, sleline));
        if (line == nullptr)
            return rpcerror (rpcinvalid_params);

        addline (jsonlines, *line, ledger);
        visitdata.items.reserve (reserve);
    }
    else
    {
        starthint = 0;
        // we have no start point, limit should be one higher than requested.
        visitdata.items.reserve (++reserve);
    }

    if (! ledger->visitaccountitems (raaccount, startafter, starthint, reserve,
        [&visitdata](sle::ref slecur)
        {
            auto const line (ripplestate::makeitem (visitdata.accountid, slecur));
            if (line != nullptr &&
                (! visitdata.rippleaddresspeer.isvalid () ||
                visitdata.rapeeraccount == line->getaccountidpeer ()))
            {
                visitdata.items.emplace_back (line);
                return true;
            }

            return false;
        }))
    {
        return rpcerror (rpcinvalid_params);
    }

    if (visitdata.items.size () == reserve)
    {
        result[jss::limit] = limit;

        ripplestate::pointer line (visitdata.items.back ());
        result[jss::marker] = to_string (line->peeksle ().getindex ());
        visitdata.items.pop_back ();
    }

    result[jss::account] = rippleaddress.humanaccountid ();

    for (auto const& item : visitdata.items)
        addline (jsonlines, *item.get (), ledger);

    context.loadtype = resource::feemediumburdenrpc;
    return result;
}

} // ripple
