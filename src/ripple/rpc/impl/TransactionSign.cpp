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
#include <ripple/app/paths/findpaths.h>
#include <ripple/basics/stringutilities.h>
#include <ripple/json/json_reader.h>
#include <ripple/protocol/txflags.h>
#include <ripple/rpc/impl/transactionsign.h>
#include <beast/unit_test.h>

namespace ripple {

//------------------------------------------------------------------------------

namespace rpc {
namespace rpcdetail {

// ledgerfacade methods

void ledgerfacade::snapshotaccountstate (rippleaddress const& accountid)
{
    if (!netops_) // unit testing.
        return;

    ledger_ = netops_->getcurrentledger ();
    accountid_ = accountid;
    accountstate_ = netops_->getaccountstate (ledger_, accountid_);
}

bool ledgerfacade::isvalidaccount () const
{
    if (!ledger_) // unit testing.
        return true;

    return static_cast <bool> (accountstate_);
}

std::uint32_t ledgerfacade::getseq () const
{
    if (!ledger_) // unit testing.
        return 0;

    return accountstate_->getseq ();
}

transaction::pointer ledgerfacade::submittransactionsync (
    transaction::ref tptrans,
    bool badmin,
    bool blocal,
    bool bfailhard,
    bool bsubmit)
{
    if (!netops_) // unit testing.
        return tptrans;

    return netops_->submittransactionsync (
        tptrans, badmin, blocal, bfailhard, bsubmit);
}

bool ledgerfacade::findpathsforoneissuer (
    rippleaddress const& dstaccountid,
    issue const& srcissue,
    stamount const& dstamount,
    int searchlevel,
    unsigned int const maxpaths,
    stpathset& pathsout,
    stpath& fullliquiditypath) const
{
    if (!ledger_) // unit testing.
        // note that unit tests don't (yet) need pathsout or fullliquiditypath.
        return true;

    auto cache = std::make_shared<ripplelinecache> (ledger_);
    return ripple::findpathsforoneissuer (
        cache,
        accountid_.getaccountid (),
        dstaccountid.getaccountid (),
        srcissue,
        dstamount,
        searchlevel,
        maxpaths,
        pathsout,
        fullliquiditypath);
}

std::uint64_t ledgerfacade::scalefeebase (std::uint64_t fee) const
{
    if (!ledger_) // unit testing.
        return fee;

    return ledger_->scalefeebase (fee);
}

std::uint64_t ledgerfacade::scalefeeload (std::uint64_t fee, bool badmin) const
{
    if (!ledger_) // unit testing.
        return fee;

    return ledger_->scalefeeload (fee, badmin);
}

bool ledgerfacade::hasaccountroot () const
{
    if (!netops_) // unit testing.
        return true;

    sle::pointer const sleaccountroot =
        netops_->getslei (ledger_, getaccountrootindex (accountid_));

    return static_cast <bool> (sleaccountroot);
}

bool ledgerfacade::isaccountexist (const account& account) const
{
    if (!ledger_)
    {
        return false;
    }
    
    return ledger_->getaccountroot(account) != nullptr;
}
    
bool ledgerfacade::accountmasterdisabled () const
{
    if (!accountstate_) // unit testing.
        return false;

    stledgerentry const& sle = accountstate_->peeksle ();
    return sle.isflag(lsfdisablemaster);
}

bool ledgerfacade::accountmatchesregularkey (account account) const
{
    if (!accountstate_) // unit testing.
        return true;

    stledgerentry const& sle = accountstate_->peeksle ();
    return ((sle.isfieldpresent (sfregularkey)) &&
        (account == sle.getfieldaccount160 (sfregularkey)));
}

int ledgerfacade::getvalidatedledgerage () const
{
    if (!netops_) // unit testing.
        return 0;

    return getapp( ).getledgermaster ().getvalidatedledgerage ();
}

bool ledgerfacade::isloadedcluster () const
{
    if (!netops_) // unit testing.
        return false;

    return getapp().getfeetrack().isloadedcluster();
}
} // namespace rpcdetail

//------------------------------------------------------------------------------

/** fill in the fee on behalf of the client.
    this is called when the client does not explicitly specify the fee.
    the client may also put a ceiling on the amount of the fee. this ceiling
    is expressed as a multiplier based on the current ledger's fee schedule.

    json fields

    "fee"   the fee paid by the transaction. omitted when the client
            wants the fee filled in.

    "fee_mult_max"  a multiplier applied to the current ledger's transaction
                    fee that caps the maximum the fee server should auto fill.
                    if this optional field is not specified, then a default
                    multiplier is used.

    @param tx       the json corresponding to the transaction to fill in
    @param ledger   a ledger for retrieving the current fee schedule
    @param result   a json object for injecting error results, if any
    @param admin    `true` if this is called by an administrative endpoint.
*/
static void autofill_fee (
    json::value& request,
    rpcdetail::ledgerfacade& ledgerfacade,
    json::value& result,
    bool admin)
{
    json::value& tx (request["tx_json"]);
    if (tx.ismember ("fee"))
        return;

    std::uint64_t feebytrans = 0;
    if (tx.ismember("transactiontype") && tx["transactiontype"].asstring() == "payment")
    {
        if (!tx.ismember("destination"))
        {
            rpc::inject_error (rpcinvalid_params, "no destination account", result);
            return;
        }
        std::string dstaccountid = tx["destination"].asstring();
        rippleaddress dstaddress;
        if (!dstaddress.setaccountid(dstaccountid))
        {
            rpc::inject_error (rpcinvalid_params, "invalid account id", result);
            return;
        }

        //dst account not exist yet, charge a fix amount of fee(0.01) for creating
        if (!ledgerfacade.isaccountexist(dstaddress.getaccountid()))
        {
            feebytrans = getconfig().fee_default_create;
        }

        //if currency is native(vrp/vbc), charge 1/1000 of transfer amount,
        //otherwise charge a fix ' of fee(0.001)
        if (tx.ismember("amount"))
        {
            stamount amount;
            if (!amountfromjsonnothrow(amount, tx["amount"]))
            {
                rpc::inject_error (rpcinvalid_params, "wrong amount format", result);
                return;
            }
            feebytrans += amount.isnative() ? std::max(int(amount.getnvalue() * getconfig().fee_default_rate_native), int(getconfig().fee_default_min_native)) : getconfig().fee_default_none_native;
        }
    }
    
    /*
        add compat for transaction type activeaccount
    */
    else if (tx.ismember("transactiontype") && tx["transactiontype"].asstring() == "activeaccount")
    {
        // referee is source, reference is destination
        if (!tx.ismember("referee"))
        {
            rpc::inject_error (rpcinvalid_params, "no referee account", result);
            return;
        }
        else if (!tx.ismember("reference"))
        {
            rpc::inject_error (rpcinvalid_params, "no reference account", result);
            return;
        }
        config d;
        std::string referenceaccountid = tx["reference"].asstring();
        rippleaddress referenceaddress;
        
        if (!referenceaddress.setaccountid(referenceaccountid))
        {
            rpc::inject_error (rpcinvalid_params, "invalid reference account id", result);
            return;
        }

        //dst account not exist yet, charge a fix amount of fee(0.01) for creating
        if (!ledgerfacade.isaccountexist(referenceaddress.getaccountid()))
        {
            feebytrans = d.fee_default_create;
        }

        //if currency is native(vrp/vbc), charge 1/1000 of transfer amount,
        //otherwise charge a fix amount of fee(0.001)
        if (tx.ismember("amount"))
        {
            stamount amount;
            if (!amountfromjsonnothrow(amount, tx["amount"]))
            {
                rpc::inject_error (rpcinvalid_params, "wrong amount format", result);
                return;
            }
            feebytrans += amount.isnative() ? std::max(int(amount.getnvalue() * getconfig().fee_default_rate_native), int(getconfig().fee_default_min_native)) : getconfig().fee_default_none_native;
        }
        else
        {
            feebytrans += getconfig().fee_default_min_native;
        }
    }
    
    int mult = tuning::defaultautofillfeemultiplier;
    if (request.ismember ("fee_mult_max"))
    {
        if (request["fee_mult_max"].isnumeric ())
        {
            mult = request["fee_mult_max"].asint();
        }
        else
        {
            rpc::inject_error (rpchigh_fee, rpc::expected_field_message (
                "fee_mult_max", "a number"), result);
            return;
        }
    }

    // default fee in fee units.
    std::uint64_t const feedefault = getconfig().transaction_fee_base;

    // administrative endpoints are exempt from local fees.
    std::uint64_t const fee = ledgerfacade.scalefeeload (feedefault, admin);
    std::uint64_t const limit = mult * ledgerfacade.scalefeebase (feedefault);

    if (fee > limit)
    {
        std::stringstream ss;
        ss <<
            "fee of " << fee <<
            " exceeds the requested tx limit of " << limit;
        rpc::inject_error (rpchigh_fee, ss.str(), result);
        return;
    }
    
    std::stringstream ss;
    ss << std::max(fee, feebytrans);
    tx["fee"] = ss.str();
}

static json::value signpayment(
    json::value const& params,
    json::value& tx_json,
    rippleaddress const& rasrcaddressid,
    rpcdetail::ledgerfacade& ledgerfacade,
    role role)
{
    rippleaddress dstaccountid;

    if (!tx_json.ismember ("amount"))
        return rpc::missing_field_error ("tx_json.amount");

    stamount amount;

    if (! amountfromjsonnothrow (amount, tx_json ["amount"]))
        return rpc::invalid_field_error ("tx_json.amount");

    if (!tx_json.ismember ("destination"))
        return rpc::missing_field_error ("tx_json.destination");

    if (!dstaccountid.setaccountid (tx_json["destination"].asstring ()))
        return rpc::invalid_field_error ("tx_json.destination");

    if (tx_json.ismember ("paths") && params.ismember ("build_path"))
        return rpc::make_error (rpcinvalid_params,
            "cannot specify both 'tx_json.paths' and 'build_path'");

    if (!tx_json.ismember ("paths")
        && tx_json.ismember ("amount")
        && params.ismember ("build_path"))
    {
        // need a ripple path.
        currency usrccurrencyid;
        account usrcissuerid;

        stamount    sasendmax;

        if (tx_json.ismember ("sendmax"))
        {
            if (! amountfromjsonnothrow (sasendmax, tx_json ["sendmax"]))
                return rpc::invalid_field_error ("tx_json.sendmax");
        }
        else
        {
            // if no sendmax, default to amount with sender as issuer.
            sasendmax = amount;
            sasendmax.setissuer (rasrcaddressid.getaccountid ());
        }

        if (sasendmax.isnative () && amount.isnative ())
            return rpc::make_error (rpcinvalid_params,
                "cannot build xrp to xrp paths.");

        {
            legacypathfind lpf (role == role::admin);
            if (!lpf.isok ())
                return rpcerror (rpctoo_busy);

            stpathset spspaths;
            stpath fullliquiditypath;
            bool valid = ledgerfacade.findpathsforoneissuer (
                dstaccountid,
                sasendmax.issue (),
                amount,
                getconfig ().path_search_old,
                4,  // imaxpaths
                spspaths,
                fullliquiditypath);


            if (!valid)
            {
                writelog (lsdebug, rpchandler)
                        << "transactionsign: build_path: no paths found.";
                return rpcerror (rpcno_path);
            }
            writelog (lsdebug, rpchandler)
                    << "transactionsign: build_path: "
                    << spspaths.getjson (0);

            if (!spspaths.empty ())
                tx_json["paths"] = spspaths.getjson (0);
        }
    }
    return json::value();
}

//------------------------------------------------------------------------------

// vfalco todo this function should take a reference to the params, modify it
//             as needed, and then there should be a separate function to
//             submit the transaction.
//
json::value
transactionsign (
    json::value params,
    bool bsubmit,
    bool bfailhard,
    rpcdetail::ledgerfacade& ledgerfacade,
    role role)
{
    json::value jvresult;

    writelog (lsdebug, rpchandler) << "transactionsign: " << params;

    if (! params.ismember ("secret"))
        return rpc::missing_field_error ("secret");

    if (! params.ismember ("tx_json"))
        return rpc::missing_field_error ("tx_json");

    rippleaddress naseed;

    if (! naseed.setseedgeneric (params["secret"].asstring ()))
        return rpc::make_error (rpcbad_seed,
            rpc::invalid_field_message ("secret"));

    json::value& tx_json (params ["tx_json"]);

    if (! tx_json.isobject ())
        return rpc::object_field_error ("tx_json");

    if (! tx_json.ismember ("transactiontype"))
        return rpc::missing_field_error ("tx_json.transactiontype");

    std::string const stype = tx_json ["transactiontype"].asstring ();

    if (! tx_json.ismember ("account"))
        return rpc::make_error (rpcsrc_act_missing,
            rpc::missing_field_message ("tx_json.account"));

    rippleaddress rasrcaddressid;

    if (! rasrcaddressid.setaccountid (tx_json["account"].asstring ()))
        return rpc::make_error (rpcsrc_act_malformed,
            rpc::invalid_field_message ("tx_json.account"));

    bool const verify = !(params.ismember ("offline")
                          && params["offline"].asbool ());

    if (!tx_json.ismember ("sequence") && !verify)
        return rpc::missing_field_error ("tx_json.sequence");

    // check for current ledger.
    if (verify && !getconfig ().run_standalone &&
        (ledgerfacade.getvalidatedledgerage () > 120))
        return rpcerror (rpcno_current);

    // check for load.
    if (ledgerfacade.isloadedcluster () && (role != role::admin))
        return rpcerror (rpctoo_busy);

    ledgerfacade.snapshotaccountstate (rasrcaddressid);

    if (verify) {
        if (!ledgerfacade.isvalidaccount ())
        {
            // if not offline and did not find account, error.
            writelog (lsdebug, rpchandler)
                << "transactionsign: failed to find source account "
                << "in current ledger: "
                << rasrcaddressid.humanaccountid ();

            return rpcerror (rpcsrc_act_not_found);
        }
    }

    autofill_fee (params, ledgerfacade, jvresult, role == role::admin);
    if (rpc::contains_error (jvresult))
        return jvresult;

    if ("payment" == stype)
    {
        auto e = signpayment(
            params,
            tx_json,
            rasrcaddressid,
            ledgerfacade,
            role);
        if (contains_error(e))
            return e;
    }

    if (!tx_json.ismember ("sequence"))
        tx_json["sequence"] = ledgerfacade.getseq ();

    if (!tx_json.ismember ("flags"))
        tx_json["flags"] = tffullycanonicalsig;

    if (verify)
    {
        if (!ledgerfacade.hasaccountroot ())
            // xxx ignore transactions for accounts not created.
            return rpcerror (rpcsrc_act_not_found);
    }

    rippleaddress secret = rippleaddress::createseedgeneric (
        params["secret"].asstring ());
    rippleaddress mastergenerator = rippleaddress::creategeneratorpublic (
        secret);
    rippleaddress masteraccountpublic = rippleaddress::createaccountpublic (
        mastergenerator, 0);

    if (verify)
    {
        writelog (lstrace, rpchandler) <<
                "verify: " << masteraccountpublic.humanaccountid () <<
                " : " << rasrcaddressid.humanaccountid ();

        auto const secretaccountid = masteraccountpublic.getaccountid();
        if (rasrcaddressid.getaccountid () == secretaccountid)
        {
            if (ledgerfacade.accountmasterdisabled ())
                return rpcerror (rpcmaster_disabled);
        }
        else if (!ledgerfacade.accountmatchesregularkey (secretaccountid))
        {
            return rpcerror (rpcbad_secret);
        }
    }

    stparsedjsonobject parsed ("tx_json", tx_json);
    if (!parsed.object.get())
    {
        jvresult ["error"] = parsed.error ["error"];
        jvresult ["error_code"] = parsed.error ["error_code"];
        jvresult ["error_message"] = parsed.error ["error_message"];
        return jvresult;
    }
    std::unique_ptr<stobject> soptrans = std::move(parsed.object);
    soptrans->setfieldvl (
        sfsigningpubkey,
        masteraccountpublic.getaccountpublic ());

    sttx::pointer stptrans;

    try
    {
        stptrans = std::make_shared<sttx> (*soptrans);
        //writelog(lsinfo, rpchandler) << "moorecoin: before sign " << stptrans->getfieldamount(sfamount);
    }
    catch (std::exception&)
    {
        return rpc::make_error (rpcinternal,
            "exception occurred during transaction");
    }

    std::string reason;
    if (!passeslocalchecks (*stptrans, reason))
        return rpc::make_error (rpcinvalid_params, reason);

    if (params.ismember ("debug_signing"))
    {
        jvresult["tx_unsigned"] = strhex (
            stptrans->getserializer ().peekdata ());
        jvresult["tx_signing_hash"] = to_string (stptrans->getsigninghash ());
    }

    // fixme: for performance, transactions should not be signed in this code
    // path.
    rippleaddress naaccountprivate = rippleaddress::createaccountprivate (
        mastergenerator, secret, 0);

    stptrans->sign (naaccountprivate);

    transaction::pointer tptrans;

    try
    {
        //writelog(lsinfo, rpchandler) << "moorecoin: after sign " << stptrans->getfieldamount(sfamount);
        tptrans = std::make_shared<transaction>(stptrans, validate::no);
        //writelog(lsinfo, rpchandler) << "moorecoin: after copy" << tptrans->getstransaction()->getfieldamount(sfamount);
    }
    catch (std::exception&)
    {
        return rpc::make_error (rpcinternal,
            "exception occurred during transaction");
    }

    try
    {
        // fixme: for performance, should use asynch interface.
        tptrans = ledgerfacade.submittransactionsync (tptrans,
            role == role::admin, true, bfailhard, bsubmit);

        if (!tptrans)
        {
            return rpc::make_error (rpcinternal,
                "unable to sterilize transaction.");
        }
    }
    catch (std::exception&)
    {
        return rpc::make_error (rpcinternal,
            "exception occurred during transaction submission.");
    }

    try
    {
        jvresult["tx_json"] = tptrans->getjson (0);
        jvresult["tx_blob"] = strhex (
            tptrans->getstransaction ()->getserializer ().peekdata ());

        if (temuncertain != tptrans->getresult ())
        {
            std::string stoken;
            std::string shuman;

            transresultinfo (tptrans->getresult (), stoken, shuman);

            jvresult["engine_result"]           = stoken;
            jvresult["engine_result_code"]      = tptrans->getresult ();
            jvresult["engine_result_message"]   = shuman;
        }

        return jvresult;
    }
    catch (std::exception&)
    {
        return rpc::make_error (rpcinternal,
            "exception occurred during json handling.");
    }
}

} // rpc
} // ripple
