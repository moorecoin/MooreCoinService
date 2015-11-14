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

#include <beastconfig.h>
#include <ripple/rpc/rpchandler.h>
#include <ripple/rpc/yield.h>
#include <ripple/rpc/impl/tuning.h>
#include <ripple/rpc/impl/context.h>
#include <ripple/rpc/impl/handler.h>
#include <ripple/rpc/impl/writejson.h>
#include <ripple/app/ledger/ledgermaster.h>
#include <ripple/app/misc/networkops.h>
#include <ripple/basics/log.h>
#include <ripple/core/config.h>
#include <ripple/core/jobqueue.h>
#include <ripple/json/to_string.h>
#include <ripple/net/infosub.h>
#include <ripple/net/rpcerr.h>
#include <ripple/protocol/jsonfields.h>
#include <ripple/rpc/impl/context.h>
#include <ripple/server/role.h>

namespace ripple {
namespace rpc {

namespace {

/**
   this code is called from both the http rpc handler and websockets.

   the form of the json returned is somewhat different between the two services.

   html:
     success:
        {
           "result" : {
              "ledger" : {
                 "accepted" : false,
                 "transaction_hash" : "..."
              },
              "ledger_index" : 10300865,
              "validated" : false,
              "status" : "success"  # status is inside the result.
           }
        }

     failure:
        {
           "result" : {
              "error" : "nonetwork",
              "error_code" : 16,
              "error_message" : "not synced to ripple network.",
              "request" : {
                 "command" : "ledger",
                 "ledger_index" : 10300865
              },
              "status" : "error"
           }
        }

   websocket:
     success:
        {
           "result" : {
              "ledger" : {
                 "accepted" : false,
                 "transaction_hash" : "..."
              },
              "ledger_index" : 10300865,
              "validated" : false
           }
           "type": "response",
           "status": "success",   # status is outside the result!
           "id": "client's id",   # optional
           "warning": 3.14        # optional
        }

     failure:
        {
          "error" : "nonetwork",
          "error_code" : 16,
          "error_message" : "not synced to ripple network.",
          "request" : {
             "command" : "ledger",
             "ledger_index" : 10300865
          },
          "type": "response",
          "status" : "error",
          "id": "client's id"   # optional
        }

 */

error_code_i fillhandler (context& context,
                          boost::optional<handler const&>& result)
{
    if (context.role != role::admin)
    {
        // vfalco note should we also add up the jtrpc jobs?
        //
        int jc = getapp().getjobqueue ().getjobcountge (jtclient);
        if (jc > tuning::maxjobqueueclients)
        {
            writelog (lsdebug, rpchandler) << "too busy for command: " << jc;
            return rpctoo_busy;
        }
    }

    if (!context.params.ismember ("command"))
        return rpccommand_missing;

    std::string strcommand  = context.params[jss::command].asstring ();

    writelog (lstrace, rpchandler) << "command:" << strcommand;
    writelog (lstrace, rpchandler) << "request:" << context.params;

    auto handler = gethandler(strcommand);

    if (!handler)
        return rpcunknown_command;

    if (handler->role_ == role::admin && context.role != role::admin)
        return rpcno_permission;

    if ((handler->condition_ & needs_network_connection) &&
        (context.netops.getoperatingmode () < networkops::omsyncing))
    {
        writelog (lsinfo, rpchandler)
            << "insufficient network mode for rpc: "
            << context.netops.stroperatingmode ();

        return rpcno_network;
    }

    if (!getconfig ().run_standalone
        && (handler->condition_ & needs_current_ledger)
        && (getapp().getledgermaster().getvalidatedledgerage() >
            tuning::maxvalidatedledgerage))
    {
        return rpcno_current;
    }

    if ((handler->condition_ & needs_closed_ledger) &&
        !context.netops.getclosedledger ())
    {
        return rpcno_closed;
    }

    result = *handler;
    return rpcsuccess;
}

template <class object, class method>
status callmethod (
    context& context, method method, std::string const& name, object& result)
{
    try
    {
        auto v = getapp().getjobqueue().getloadeventap(
            jtgeneric, "cmd:" + name);
        return method (context, result);
    }
    catch (std::exception& e)
    {
        writelog (lsinfo, rpchandler) << "caught throw: " << e.what ();

        if (context.loadtype == resource::feereferencerpc)
            context.loadtype = resource::feeexceptionrpc;

        inject_error (rpcinternal, result);
        return rpcinternal;
    }
}

template <class method, class object>
void getresult (
    context& context, method method, object& object, std::string const& name)
{
    auto&& result = addobject (object, jss::result);
    if (auto status = callmethod (context, method, name, result))
    {
        writelog (lsdebug, rpcerr) << "rpcerror: " << status.tostring();
        result[jss::status] = jss::error;
        result[jss::request] = context.params;
    }
    else
    {
        result[jss::status] = jss::success;
    }
}

} // namespace

status docommand (
    rpc::context& context, json::value& result, yieldstrategy const&)
{
    boost::optional <handler const&> handler;
    if (auto error = fillhandler (context, handler))
    {
        inject_error (error, result);
        return error;
    }

    if (auto method = handler->valuemethod_)
        return callmethod (context, method, handler->name_, result);

    return rpcunknown_command;
}

/** execute an rpc command and store the results in a string. */
void executerpc (
    rpc::context& context, std::string& output, yieldstrategy const& strategy)
{
    boost::optional <handler const&> handler;
    if (auto error = fillhandler (context, handler))
    {
        auto wo = stringwriterobject (output);
        auto&& sub = addobject (*wo, jss::result);
        inject_error (error, sub);
    }
    else if (auto method = handler->objectmethod_)
    {
        auto wo = stringwriterobject (output);
        getresult (context, method, *wo, handler->name_);
    }
    else if (auto method = handler->valuemethod_)
    {
        auto object = json::value (json::objectvalue);
        getresult (context, method, object, handler->name_);
        if (strategy.streaming == yieldstrategy::streaming::yes)
            output = jsonasstring (object);
        else
            output = to_string (object);
    }
    else
    {
        // can't ever get here.
        assert (false);
        throw rpc::jsonexception ("rpc handler with no method");
    }
}

} // rpc
} // ripple
