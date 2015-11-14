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

#ifndef rippled_ripple_rpc_handlers_ledger_h
#define rippled_ripple_rpc_handlers_ledger_h

#include <ripple/app/ledger/ledgertojson.h>
#include <ripple/core/loadfeetrack.h>
#include <ripple/rpc/impl/jsonobject.h>
#include <ripple/server/role.h>

namespace ripple {
namespace rpc {

class object;

// ledger [id|index|current|closed] [full]
// {
//    ledger: 'current' | 'closed' | <uint256> | <number>,  // optional
//    full: true | false    // optional, defaults to false.
// }

class ledgerhandler {
public:
    explicit ledgerhandler (context&);

    status check ();

    template <class object>
    void writeresult (object&);

    static const char* const name()
    {
        return "ledger";
    }

    static role role()
    {
        return role::user;
    }

    static condition condition()
    {
        return needs_network_connection;
    }

private:
    context& context_;
    ledger::pointer ledger_;
    json::value result_;
    int options_ = 0;
};

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//
// implementation.

template <class object>
void ledgerhandler::writeresult (object& value)
{
    if (ledger_)
    {
        rpc::copyfrom (value, result_);
        addjson (value, {*ledger_, options_, context_.yield});
    }
    else
    {
        auto& master = getapp().getledgermaster ();
        auto& yield = context_.yield;
        {
            auto&& closed = rpc::addobject (value, jss::closed);
            addjson (closed, {*master.getclosedledger(), 0, yield});
        }
        {
            auto&& open = rpc::addobject (value, jss::open);
            addjson (open, {*master.getcurrentledger(), 0, yield});
        }
    }
}

} // rpc
} // ripple

#endif
