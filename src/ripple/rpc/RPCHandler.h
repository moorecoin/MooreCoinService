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

#ifndef ripple_app_rpc_handler
#define ripple_app_rpc_handler

#include <ripple/core/config.h>
#include <ripple/net/infosub.h>
#include <ripple/rpc/status.h>
#include <ripple/rpc/impl/context.h>

namespace ripple {
namespace rpc {

struct context;
struct yieldstrategy;

/** execute an rpc command and store the results in a json::value. */
status docommand (rpc::context&, json::value&, yieldstrategy const& s = {});

/** execute an rpc command and store the results in an std::string. */
void executerpc (rpc::context&, std::string&, yieldstrategy const& s = {});

/** temporary flag to enable rpcs. */
auto const streamingrpc = false;

} // rpc
} // ripple

#endif
