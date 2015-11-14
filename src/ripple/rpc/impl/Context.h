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

#ifndef ripple_rpc_context
#define ripple_rpc_context

#include <ripple/core/config.h>
#include <ripple/net/infosub.h>
#include <ripple/rpc/yield.h>
#include <ripple/server/role.h>

namespace ripple {

class networkops;

namespace rpc {

/** the context of information needed to call an rpc. */
struct context
{
    json::value params;
    resource::charge& loadtype;
    networkops& netops;
    role role;
    infosub::pointer infosub;
    rpc::yield yield;
};

} // rpc
} // ripple



#endif
