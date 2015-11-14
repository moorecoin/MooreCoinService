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
#include <ripple/app/ledger/inboundledgers.h>
#include <ripple/app/ledger/ledgertojson.h>

namespace ripple {

// {
//   ledger_hash : <ledger>
//   ledger_index : <ledger_index>
// }
json::value doledgerrequest (rpc::context& context)
{
    auto const hashash = context.params.ismember (jss::ledger_hash);
    auto const hasindex = context.params.ismember (jss::ledger_index);

    auto& ledgermaster = getapp().getledgermaster();
    ledgerhash ledgerhash;

    if ((hashash && hasindex) || !(hashash || hasindex))
    {
        return rpc::make_param_error(
            "exactly one of ledger_hash and ledger_index can be set.");
    }

    if (hashash)
    {
        auto const& jsonhash = context.params[jss::ledger_hash];
        if (!jsonhash.isstring() || !ledgerhash.sethex (jsonhash.asstring ()))
            return rpc::invalid_field_message ("ledger_hash");
    } else {
        auto const& jsonindex = context.params[jss::ledger_index];
        if (!jsonindex.isnumeric ())
            return rpc::invalid_field_message ("ledger_index");

        // we need a validated ledger to get the hash from the sequence
        if (ledgermaster.getvalidatedledgerage() > 120)
            return rpcerror (rpcno_current);

        auto ledgerindex = jsonindex.asint();
        auto ledger = ledgermaster.getvalidatedledger();

        if (ledgerindex >= ledger->getledgerseq())
            return rpc::make_param_error("ledger index too large");

        // try to get the hash of the desired ledger from the validated ledger
        ledgerhash = ledger->getledgerhash (ledgerindex);

        if (ledgerhash == zero)
        {
            // find a ledger more likely to have the hash of the desired ledger
            auto refindex = (ledgerindex + 255) & (~255);
            auto refhash = ledger->getledgerhash (refindex);
            assert (refhash.isnonzero ());

            ledger = ledgermaster.getledgerbyhash (refhash);
            if (!ledger)
            {
                // we don't have the ledger we need to figure out which ledger
                // they want. try to get it.

                if (auto il = getapp().getinboundledgers().findcreate (
                        refhash, refindex, inboundledger::fcgeneric))
                {
                    json::value jvresult = il->getjson (0);

                    jvresult[jss::error] = "ledgernotfound";
                    return jvresult;
                }

                // findcreate failed to return an inbound ledger. app is likely shutting down
                return json::value();
            }

            ledgerhash = ledger->getledgerhash (ledgerindex);
            assert (ledgerhash.isnonzero ());
        }
    }

    auto ledger = ledgermaster.getledgerbyhash (ledgerhash);
    if (ledger)
    {
        // we already have the ledger they want
        json::value jvresult;
        jvresult[jss::ledger_index] = ledger->getledgerseq();
        addjson (jvresult, {*ledger, 0});
        return jvresult;
    }
    else
    {
        // try to get the desired ledger
        if (auto il = getapp ().getinboundledgers ().findcreate (
                ledgerhash, 0, inboundledger::fcgeneric))
        {
            return il->getjson (0);
        }
        return rpc::make_error (
            rpcnot_ready, "findcreate failed to return an inbound ledger");
    }
}

} // ripple
