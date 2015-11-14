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

#ifndef ripple_rpc_handler
#define ripple_rpc_handler

#include <ripple/core/config.h>
#include <ripple/rpc/rpchandler.h>
#include <ripple/rpc/status.h>

namespace ripple {
namespace rpc {

class object;

// under what condition can we call this rpc?
enum condition {
    no_condition     = 0,
    needs_network_connection  = 1,
    needs_current_ledger  = 2 + needs_network_connection,
    needs_closed_ledger   = 4 + needs_network_connection,
};

struct handler
{
    template <class jsonvalue>
    using method = std::function <status (context&, jsonvalue&)>;

    const char* name_;
    method<json::value> valuemethod_;
    role role_;
    rpc::condition condition_;
    method<object> objectmethod_;
};

const handler* gethandler (std::string const&);

/** return a json::objectvalue with a single entry. */
template <class value>
json::value makeobjectvalue (
    value const& value, json::staticstring const& field = jss::message)
{
    json::value result (json::objectvalue);
    result[field] = value;
    return result;
}

} // rpc
} // ripple

#endif
