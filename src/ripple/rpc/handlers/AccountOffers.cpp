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

namespace ripple {

// {
//   account: <account>|<account_public_key>
//   account_index: <number>        // optional, defaults to 0.
//   ledger_hash : <ledger>
//   ledger_index : <ledger_index>
//   limit: integer                 // optional
//   marker: opaque                 // optional, resume previous query
// }
json::value doaccountoffers (rpc::context& context)
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
    int const iindex (bindex ? params[jss::account_index].asuint () : 0);
    rippleaddress rippleaddress;

    json::value const jv (rpc::accountfromstring (ledger, rippleaddress, bindex,
        strident, iindex, false, context.netops));
    if (! jv.empty ())
    {
        for (json::value::const_iterator it (jv.begin ()); it != jv.end (); ++it)
            result[it.membername ()] = it.key ();

        return result;
    }

    // get info on account.
    result[jss::account] = rippleaddress.humanaccountid ();

    if (bindex)
        result[jss::account_index] = iindex;

    if (! ledger->hasaccount (rippleaddress))
        return rpcerror (rpcact_not_found);

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
            limit = std::max (rpc::tuning::minoffersperrequest,
                std::min (limit, rpc::tuning::maxoffersperrequest));
        }
    }
    else
    {
        limit = rpc::tuning::defaultoffersperrequest;
    }

    account const& raaccount (rippleaddress.getaccountid ());
    json::value& jsonoffers (result[jss::offers] = json::arrayvalue);
    std::vector <sle::pointer> offers;
    unsigned int reserve (limit);
    uint256 startafter;
    std::uint64_t starthint;

    if (params.ismember(jss::marker))
    {
        // we have a start point. use limit - 1 from the result and use the
        // very last one for the resume.
        json::value const& marker (params[jss::marker]);

        if (! marker.isstring ())
            return rpc::expected_field_error ("marker", "string");

        startafter.sethex (marker.asstring ());
        sle::pointer sleoffer (ledger->getslei (startafter));

        if (sleoffer == nullptr ||
            sleoffer->gettype () != ltoffer ||
            raaccount != sleoffer->getfieldaccount160 (sfaccount))
        {
            return rpcerror (rpcinvalid_params);
        }

        starthint = sleoffer->getfieldu64(sfownernode);

        // caller provided the first offer (startafter), add it as first result
        json::value& obj (jsonoffers.append (json::objectvalue));
        sleoffer->getfieldamount (sftakerpays).setjson (obj[jss::taker_pays]);
        sleoffer->getfieldamount (sftakergets).setjson (obj[jss::taker_gets]);
        obj[jss::seq] = sleoffer->getfieldu32 (sfsequence);
        obj[jss::flags] = sleoffer->getfieldu32 (sfflags);

        offers.reserve (reserve);
    }
    else
    {
        starthint = 0;
        // we have no start point, limit should be one higher than requested.
        offers.reserve (++reserve);
    }

    if (! ledger->visitaccountitems (raaccount, startafter, starthint, reserve,
        [&offers](sle::ref offer)
        {
            if (offer->gettype () == ltoffer)
            {
                offers.emplace_back (offer);
                return true;
            }

            return false;
        }))
    {
        return rpcerror (rpcinvalid_params);
    }

    if (offers.size () == reserve)
    {
        result[jss::limit] = limit;

        result[jss::marker] = to_string (offers.back ()->getindex ());
        offers.pop_back ();
    }

    for (auto const& offer : offers)
    {
        json::value& obj (jsonoffers.append (json::objectvalue));
        offer->getfieldamount (sftakerpays).setjson (obj[jss::taker_pays]);
        offer->getfieldamount (sftakergets).setjson (obj[jss::taker_gets]);
        obj[jss::seq] = offer->getfieldu32 (sfsequence);
        obj[jss::flags] = offer->getfieldu32 (sfflags);
    }

    context.loadtype = resource::feemediumburdenrpc;
    return result;
}

} // ripple
