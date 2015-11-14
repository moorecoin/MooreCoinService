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
#include <ripple/app/data/databasecon.h>
#include <ripple/app/data/sqlitedatabase.h>
#include <ripple/app/ledger/acceptedledger.h>
#include <ripple/basics/uptimetimer.h>
#include <ripple/nodestore/database.h>
#include <boost/foreach.hpp>

namespace ripple {

// {
//   min_count: <number>  // optional, defaults to 10
// }
json::value dogetcounts (rpc::context& context)
{
    auto lock = getapp().masterlock();

    int mincount = 10;

    if (context.params.ismember ("min_count"))
        mincount = context.params["min_count"].asuint ();

    auto objectcounts = countedobjects::getinstance ().getcounts (mincount);

    json::value ret (json::objectvalue);

    boost_foreach (countedobjects::entry& it, objectcounts)
    {
        ret [it.first] = it.second;
    }

    application& app = getapp();

    int dbkb = app.getledgerdb ().getdb ()->getkbusedall ();

    if (dbkb > 0)
        ret["dbkbtotal"] = dbkb;

    dbkb = app.getledgerdb ().getdb ()->getkbuseddb ();

    if (dbkb > 0)
        ret["dbkbledger"] = dbkb;

    dbkb = app.gettxndb ().getdb ()->getkbuseddb ();

    if (dbkb > 0)
        ret["dbkbtransaction"] = dbkb;

    {
        std::size_t c = app.getops().getlocaltxcount ();
        if (c > 0)
            ret["local_txs"] = static_cast<json::uint> (c);
    }

    ret["write_load"] = app.getnodestore ().getwriteload ();

    ret["sle_hit_rate"] = app.getslecache ().gethitrate ();
    ret["node_hit_rate"] = app.getnodestore ().getcachehitrate ();
    ret["ledger_hit_rate"] = app.getledgermaster ().getcachehitrate ();
    ret["al_hit_rate"] = acceptedledger::getcachehitrate ();

    ret["fullbelow_size"] = static_cast<int>(app.getfullbelowcache().size());
    ret["treenode_cache_size"] = app.gettreenodecache().getcachesize();
    ret["treenode_track_size"] = app.gettreenodecache().gettracksize();

    std::string uptime;
    int s = uptimetimer::getinstance ().getelapsedseconds ();
    ret["uptime"] = s;
    texttime (uptime, s, "year", 365 * 24 * 60 * 60);
    texttime (uptime, s, "day", 24 * 60 * 60);
    texttime (uptime, s, "hour", 60 * 60);
    texttime (uptime, s, "minute", 60);
    texttime (uptime, s, "second", 1);
    ret["uptime_human"] = uptime;

    ret["node_writes"] = app.getnodestore().getstorecount();
    ret["node_reads_total"] = app.getnodestore().getfetchtotalcount();
    ret["node_reads_hit"] = app.getnodestore().getfetchhitcount();
    ret["node_written_bytes"] = app.getnodestore().getstoresize();
    ret["node_read_bytes"] = app.getnodestore().getfetchsize();

    return ret;
}

} // ripple
