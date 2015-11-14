//------------------------------------------------------------------------------
/*
    this file is part of rippled: https://github.com/ripple/rippled
    copyright (c) 2012, 2013 ripple labs inc.

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

#ifndef rippled_ripple_app_ledger_ledgertojson_h
#define rippled_ripple_app_ledger_ledgertojson_h

#include <ripple/app/ledger/ledger.h>
#include <ripple/basics/time.h>
#include <ripple/protocol/jsonfields.h>
#include <ripple/protocol/sttx.h>
#include <ripple/rpc/yield.h>
#include <ripple/rpc/impl/jsonobject.h>
#include <boost/date_time/posix_time/posix_time.hpp>

namespace ripple {

struct ledgerfill
{
    ledgerfill (ledger const& l,
                int o = 0,
                rpc::yield const& y = {},
                rpc::yieldstrategy const& ys = {})
            : ledger (l),
              options (o),
              yield (y),
              yieldstrategy (ys)
    {
    }

    ledger const& ledger;
    int options;
    rpc::yield yield;
    rpc::yieldstrategy yieldstrategy;
};

/** given a ledger, options, and a generic object that has json semantics,
    fill the object with a description of the ledger.
*/
template <class object>
void filljson (object&, ledgerfill const&);

/** add json to an existing generic object. */
template <class object>
void addjson (object&, ledgerfill const&);

/** return a new json::value representing the ledger with given options.*/
json::value getjson (ledgerfill const&);

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// implementations.

template <typename object>
void filljson (object& json, ledgerfill const& fill)
{
    auto const& ledger = fill.ledger;

    bool const bfull (fill.options & ledger_json_full);
    bool const bexpand (fill.options & ledger_json_expand);
    bool const bfilldividend (fill.options & ledger_json_dump_txdiv);

    // deprecated
    json[jss::seqnum]       = to_string (ledger.getledgerseq());
    json[jss::parent_hash]  = to_string (ledger.getparenthash());
    json[jss::ledger_index] = to_string (ledger.getledgerseq());

    if (ledger.isclosed() || bfull)
    {
        if (ledger.isclosed())
            json[jss::closed] = true;

        // deprecated
        json[jss::hash] = to_string (ledger.getrawhash());

        // deprecated
        json[jss::totalcoins]        = to_string (ledger.gettotalcoins());
        json[jss::totalcoinsvbc]     = to_string (ledger.gettotalcoinsvbc());
        json[jss::ledger_hash]       = to_string (ledger.getrawhash());
        json[jss::transaction_hash]  = to_string (ledger.gettranshash());
        json[jss::account_hash]      = to_string (ledger.getaccounthash());
        json[jss::accepted]          = ledger.isaccepted();
        json[jss::total_coins]       = to_string (ledger.gettotalcoins());
        json[jss::total_coinsvbc]    = to_string (ledger.gettotalcoinsvbc());

        auto closetime = ledger.getclosetimenc();
        if (closetime != 0)
        {
            json[jss::close_time]            = closetime;
            json[jss::close_time_human]
                    = boost::posix_time::to_simple_string (
                        ptfromseconds (closetime));
            json[jss::close_time_resolution] = ledger.getcloseresolution();

            if (!ledger.getcloseagree())
                json[jss::close_time_estimated] = true;
        }
    }
    else
    {
        json[jss::closed] = false;
    }

    auto &transactionmap = ledger.peektransactionmap();
    if (transactionmap && (bfull || fill.options & ledger_json_dump_txrp))
    {
        auto&& txns = rpc::addarray (json, jss::transactions);
        shamaptreenode::tntype type;

        rpc::countedyield count (
            fill.yieldstrategy.transactionyieldcount, fill.yield);
        for (auto item = transactionmap->peekfirstitem (type); item;
             item = transactionmap->peeknextitem (item->gettag (), type))
        {
            count.yield();
            if (bfull || bexpand)
            {
                if (type == shamaptreenode::tntransaction_nm)
                {
                    serializeriterator sit (item->peekserializer ());
                    sttx txn (sit);
                    if (!bfilldividend && txn.gettxntype()==ttdividend)
                        continue;
                    txns.append (txn.getjson (0));
                }
                else if (type == shamaptreenode::tntransaction_md)
                {
                    serializeriterator sit (item->peekserializer ());
                    serializer stxn (sit.getvl ());

                    serializeriterator tsit (stxn);
                    sttx txn (tsit);
                    if (!bfilldividend && txn.gettxntype()==ttdividend)
                        continue;

                    transactionmetaset meta (
                        item->gettag (), ledger.getledgerseq(), sit.getvl ());
                    json::value txjson = txn.getjson (0);
                    txjson[jss::metadata] = meta.getjson (0);
                    txns.append (txjson);
                }
                else
                {
                    json::value error = json::objectvalue;
                    error[to_string (item->gettag ())] = type;
                    txns.append (error);
                }
            }
            else txns.append (to_string (item->gettag ()));
        }
    }

    auto& accountstatemap = ledger.peekaccountstatemap();
    if (accountstatemap && (bfull || fill.options & ledger_json_dump_state))
    {
        auto&& array = rpc::addarray (json, jss::accountstate);
        rpc::countedyield count (
            fill.yieldstrategy.accountyieldcount, fill.yield);
        if (bfull || bexpand)
        {
            ledger.visitstateitems (
                [&array, &count] (sle::ref sle)
                {
                    count.yield();
                    array.append (sle->getjson(0));
                });
        }
        else
        {
            accountstatemap->visitleaves(
                [&array, &count] (shamapitem::ref smi)
                {
                    count.yield();
                    array.append (to_string(smi->gettag ()));
                });
        }
    }
}

/** add json to an existing generic object. */
template <class object>
void addjson (object& json, ledgerfill const& fill)
{
    auto&& object = rpc::addobject (json, jss::ledger);
    filljson (object, fill);
}

inline
json::value getjson (ledgerfill const& fill)
{
    json::value json;
    filljson (json, fill);
    return json;
}

} // ripple

#endif
