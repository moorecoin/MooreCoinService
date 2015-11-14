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

#ifndef ripple_server_role_h_included
#define ripple_server_role_h_included

#include <ripple/server/port.h>
#include <ripple/json/json_value.h>
#include <beast/net/ipendpoint.h>
#include <vector>

namespace ripple {

/** indicates the level of administrative permission to grant. */
enum class role
{
    guest,
    user,
    admin,
    forbid
};

/** return the allowed privilege role.
    jsonrpc must meet the requirements of the json-rpc
    specification. it must be of type object, containing the key params
    which is an array with at least one object. inside this object
    are the optional keys 'admin_user' and 'admin_password' used to
    validate the credentials.
*/
role
adminrole (http::port const& port, json::value const& jsonrpc,
    beast::ip::endpoint const& remoteip,
        std::vector<beast::ip::endpoint> const& admin_allow);

} // ripple

#endif
