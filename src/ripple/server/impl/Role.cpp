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
#include <ripple/server/role.h>

namespace ripple {

role
adminrole (http::port const& port, json::value const& params,
    beast::ip::endpoint const& remoteip,
        std::vector<beast::ip::endpoint> const& admin_allow)
{
    role role (role::forbid);

    bool const bpasswordsupplied =
        params.ismember ("admin_user") ||
        params.ismember ("admin_password");

    bool const bpasswordrequired =
        ! port.admin_user.empty() || ! port.admin_password.empty();

    bool bpasswordwrong;

    if (bpasswordsupplied)
    {
        if (bpasswordrequired)
        {
            // required, and supplied, check match
            bpasswordwrong =
                (port.admin_user !=
                    (params.ismember ("admin_user") ? params["admin_user"].asstring () : ""))
                ||
                (port.admin_password !=
                    (params.ismember ("admin_user") ? params["admin_password"].asstring () : ""));
        }
        else
        {
            // not required, but supplied
            bpasswordwrong = false;
        }
    }
    else
    {
        // required but not supplied,
        bpasswordwrong = bpasswordrequired;
    }

    // meets ip restriction for admin.
    beast::ip::endpoint const remote_addr (remoteip.at_port (0));
    bool badminip = false;

    // vfalco todo don't use this!
    for (auto const& allow_addr : admin_allow)
    {
        if (allow_addr == remote_addr)
        {
            badminip = true;
            break;
        }
    }

    if (bpasswordwrong                          // wrong
            || (bpasswordsupplied && !badminip))    // supplied and doesn't meet ip filter.
    {
        role   = role::forbid;
    }
    // if supplied, password is correct.
    else
    {
        // allow admin, if from admin ip and no password is required or it was supplied and correct.
        role = badminip && (!bpasswordrequired || bpasswordsupplied) ? role::admin : role::guest;
    }

    return role;
}

}
