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
#include <ripple/app/misc/amendmenttable.h>

namespace ripple {

static void texttime (
    std::string& text, int& seconds, const char* unitname, int unitval)
{
    int i = seconds / unitval;

    if (i == 0)
        return;

    seconds -= unitval * i;

    if (!text.empty ())
        text += ", ";

    text += beast::lexicalcastthrow <std::string> (i);
    text += " ";
    text += unitname;

    if (i > 1)
        text += "s";
}

json::value dofeature (rpc::context& context)
{
    if (!context.params.ismember ("feature"))
    {
        json::value jvreply = json::objectvalue;
        jvreply["features"] = getapp().getamendmenttable ().getjson(0);
        return jvreply;
    }

    uint256 ufeature
            = getapp().getamendmenttable ().get(
                context.params["feature"].asstring());

    if (ufeature.iszero ())
    {
        ufeature.sethex (context.params["feature"].asstring ());

        if (ufeature.iszero ())
            return rpcerror (rpcbad_feature);
    }

    if (!context.params.ismember ("vote"))
        return getapp().getamendmenttable ().getjson(ufeature);

    // writeme
    return rpcerror (rpcnot_supported);
}


} // ripple
