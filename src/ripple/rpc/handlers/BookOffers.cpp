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

namespace ripple {

json::value dobookoffers (rpc::context& context)
{
    // vfalco todo here is a terrible place for this kind of business
    //             logic. it needs to be moved elsewhere and documented,
    //             and encapsulated into a function.
    if (getapp().getjobqueue ().getjobcountge (jtclient) > 200)
        return rpcerror (rpctoo_busy);

    ledger::pointer lpledger;
    json::value jvresult (
        rpc::lookupledger (context.params, lpledger, context.netops));

    if (!lpledger)
        return jvresult;

    if (!context.params.ismember ("taker_pays"))
        return rpc::missing_field_error ("taker_pays");

    if (!context.params.ismember ("taker_gets"))
        return rpc::missing_field_error ("taker_gets");

    if (!context.params["taker_pays"].isobject ())
        return rpc::object_field_error ("taker_pays");

    if (!context.params["taker_gets"].isobject ())
        return rpc::object_field_error ("taker_gets");

    json::value const& taker_pays (context.params["taker_pays"]);

    if (!taker_pays.ismember ("currency"))
        return rpc::missing_field_error ("taker_pays.currency");

    if (! taker_pays ["currency"].isstring ())
        return rpc::expected_field_error ("taker_pays.currency", "string");

    json::value const& taker_gets = context.params["taker_gets"];

    if (! taker_gets.ismember ("currency"))
        return rpc::missing_field_error ("taker_gets.currency");

    if (! taker_gets ["currency"].isstring ())
        return rpc::expected_field_error ("taker_gets.currency", "string");

    currency pay_currency;

    if (!to_currency (pay_currency, taker_pays ["currency"].asstring ()))
    {
        writelog (lsinfo, rpchandler) << "bad taker_pays currency.";
        return rpc::make_error (rpcsrc_cur_malformed,
            "invalid field 'taker_pays.currency', bad currency.");
    }

    currency get_currency;

    if (!to_currency (get_currency, taker_gets ["currency"].asstring ()))
    {
        writelog (lsinfo, rpchandler) << "bad taker_gets currency.";
        return rpc::make_error (rpcdst_amt_malformed,
            "invalid field 'taker_gets.currency', bad currency.");
    }

    account pay_issuer;

    if (taker_pays.ismember ("issuer"))
    {
        if (! taker_pays ["issuer"].isstring())
            return rpc::expected_field_error ("taker_pays.issuer", "string");

        if (!to_issuer(
            pay_issuer, taker_pays ["issuer"].asstring ()))
            return rpc::make_error (rpcsrc_isr_malformed,
                "invalid field 'taker_pays.issuer', bad issuer.");

        if (pay_issuer == noaccount ())
            return rpc::make_error (rpcsrc_isr_malformed,
                "invalid field 'taker_pays.issuer', bad issuer account one.");
    }
    else
    {
		if (isxrp(pay_currency))
			pay_issuer = xrpaccount();
		else
			pay_issuer = vbcaccount();
    }

    if (isxrp (pay_currency) && ! isxrp (pay_issuer))
        return rpc::make_error (
            rpcsrc_isr_malformed, "unneeded field 'taker_pays.issuer' for "
            "xrp currency specification.");

    if (!isxrp (pay_currency) && isxrp (pay_issuer))
        return rpc::make_error (rpcsrc_isr_malformed,
            "invalid field 'taker_pays.issuer', expected non-xrp issuer.");

	if (isvbc (pay_currency) && !isvbc(pay_issuer))
		return rpc::make_error(
		rpcsrc_isr_malformed, "unneeded field 'taker_pays.issuer' for "
		"vbc currency specification.");

	if (!isvbc (pay_currency) && isvbc(pay_issuer))
		return rpc::make_error(rpcsrc_isr_malformed,
		"invalid field 'taker_pays.issuer', expected non-vbc issuer.");

    account get_issuer;

    if (taker_gets.ismember ("issuer"))
    {
        if (! taker_gets ["issuer"].isstring())
            return rpc::expected_field_error ("taker_gets.issuer", "string");

        if (! to_issuer (
            get_issuer, taker_gets ["issuer"].asstring ()))
            return rpc::make_error (rpcdst_isr_malformed,
                "invalid field 'taker_gets.issuer', bad issuer.");

        if (get_issuer == noaccount ())
            return rpc::make_error (rpcdst_isr_malformed,
                "invalid field 'taker_gets.issuer', bad issuer account one.");
    }
    else
    {
		if (isxrp(get_currency))
			get_issuer = xrpaccount();
		else
			get_issuer = vbcaccount();
    }


    if (isxrp (get_currency) && ! isxrp (get_issuer))
        return rpc::make_error (rpcdst_isr_malformed,
            "unneeded field 'taker_gets.issuer' for "
                               "xrp currency specification.");

    if (!isxrp (get_currency) && isxrp (get_issuer))
        return rpc::make_error (rpcdst_isr_malformed,
            "invalid field 'taker_gets.issuer', expected non-xrp issuer.");

	if (isvbc (get_currency) && !isvbc(get_issuer))
		return rpc::make_error(rpcdst_isr_malformed,
		"unneeded field 'taker_gets.issuer' for "
		"vbc currency specification.");

	if (!isvbc(get_currency) && isvbc(get_issuer))
		return rpc::make_error(rpcdst_isr_malformed,
		"invalid field 'taker_gets.issuer', expected non-vbc issuer.");

    rippleaddress ratakerid;

    if (context.params.ismember ("taker"))
    {
        if (! context.params ["taker"].isstring ())
            return rpc::expected_field_error ("taker", "string");

        if (! ratakerid.setaccountid (context.params ["taker"].asstring ()))
            return rpc::invalid_field_error ("taker");
    }
    else
    {
        ratakerid.setaccountid (noaccount());
    }

    if (pay_currency == get_currency && pay_issuer == get_issuer)
    {
        writelog (lsinfo, rpchandler) << "taker_gets same as taker_pays.";
        return rpc::make_error (rpcbad_market);
    }

    unsigned int ilimit;
    if (context.params.ismember (jss::limit))
    {
        auto const& jvlimit (context.params[jss::limit]);

        if (! jvlimit.isintegral ())
            return rpc::expected_field_error ("limit", "unsigned integer");

        ilimit = jvlimit.isuint () ? jvlimit.asuint () :
            std::max (0, jvlimit.asint ());
    }
    else
    {
        ilimit = 0;
    }

    bool const bproof (context.params.ismember ("proof"));

    json::value const jvmarker (context.params.ismember ("marker")
        ? context.params["marker"]
        : json::value (json::nullvalue));

    lpledger = std::make_shared<ledger> (std::ref (*lpledger), false);
    context.netops.getbookpage (
        context.role == role::admin,
        lpledger,
        {{pay_currency, pay_issuer}, {get_currency, get_issuer}},
        ratakerid.getaccountid (), bproof, ilimit, jvmarker, jvresult);

    context.loadtype = resource::feemediumburdenrpc;

    return jvresult;
}

} // ripple
