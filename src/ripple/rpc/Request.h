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

#ifndef ripple_rpc_request_h_included
#define ripple_rpc_request_h_included

#include <ripple/resource/charge.h>
#include <ripple/resource/fees.h>
#include <ripple/json/json_value.h>
#include <beast/utility/journal.h>

namespace ripple {

class application;

namespace rpc {

struct request
{
    explicit request (
        beast::journal journal_,
        std::string const& method_,
        json::value& params_,
        application& app_)
        : journal (journal_)
        , method (method_)
        , params (params_)
        , fee (resource::feereferencerpc)
        , app (app_)
    {
    }

    // [in] the journal for logging
    beast::journal journal;

    // [in] the json-rpc method
    std::string method;

    // [in] the ripple-specific "params" object
    json::value params;

    // [in, out] the resource cost for the command
    resource::charge fee;

    // [out] the json-rpc response
    json::value result;

    // [in] the application instance
    application& app;

private:
    request& operator= (request const&);
};

}
}

#endif
