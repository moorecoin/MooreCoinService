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

#ifndef ripple_net_rpc_rpccall_h_included
#define ripple_net_rpc_rpccall_h_included

#include <ripple/json/json_value.h>
#include <boost/asio/io_service.hpp>
#include <functional>
#include <string>
#include <vector>

namespace ripple {

//
// this a trusted interface, the user is expected to provide valid input to perform valid requests.
// error catching and reporting is not a requirement of this command line interface.
//
// improvements to be more strict and to provide better diagnostics are welcome.
//

/** processes ripple rpc calls.
*/
class rpccall
{
public:

    static int fromcommandline (const std::vector<std::string>& vcmd);

    static void fromnetwork (
        boost::asio::io_service& io_service,
        std::string const& strip, const int iport,
        std::string const& strusername, std::string const& strpassword,
        std::string const& strpath, std::string const& strmethod,
        json::value const& jvparams, const bool bssl,
        std::function<void (json::value const& jvinput)> callbackfuncp = std::function<void (json::value const& jvinput)> ());
};

} // ripple

#endif
