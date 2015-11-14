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
#include <ripple/server/role.h>

namespace ripple {

// get state nodes from a ledger
//   inputs:
//     limit:        integer, maximum number of entries
//     marker:       opaque, resume point
//     binary:       boolean, format
//   outputs:
//     ledger_hash:  chosen ledger's hash
//     ledger_index: chosen ledger's index
//     state:        array of state nodes
//     marker:       resume point, if any
json::value doledgerdata (rpc::context& context)
{
    int const binary_page_length = 2048;
    int const json_page_length = 256;

    ledger::pointer lpledger;
    auto const& params = context.params;

    json::value jvresult = rpc::lookupledger (params, lpledger, context.netops);
    if (!lpledger)
        return jvresult;

    uint256 resumepoint;
    if (params.ismember ("marker"))
    {
        json::value const& jmarker = params["marker"];
        if (!jmarker.isstring ())
            return rpc::expected_field_error ("marker", "valid");
        if (!resumepoint.sethex (jmarker.asstring ()))
            return rpc::expected_field_error ("marker", "valid");
    }

    bool isbinary = params["binary"].asbool();

    int limit = -1;
    int maxlimit = isbinary ? binary_page_length : json_page_length;

    if (params.ismember ("limit"))
    {
        json::value const& jlimit = params["limit"];
        if (!jlimit.isintegral ())
            return rpc::expected_field_error ("limit", "integer");

        limit = jlimit.asint ();
    }

    if ((limit < 0) || ((limit > maxlimit) && (context.role != role::admin)))
        limit = maxlimit;

    jvresult["ledger_hash"] = to_string (lpledger->gethash());
    jvresult["ledger_index"] = std::to_string( lpledger->getledgerseq ());

    json::value& nodes = (jvresult["state"] = json::arrayvalue);
    shamap& map = *(lpledger->peekaccountstatemap ());

    for (;;)
    {
       shamapitem::pointer item = map.peeknextitem (resumepoint);
       if (!item)
           break;
       resumepoint = item->gettag();

       if (limit-- <= 0)
       {
           --resumepoint;
           jvresult["marker"] = to_string (resumepoint);
           break;
       }

       if (isbinary)
       {
           json::value& entry = nodes.append (json::objectvalue);
           entry["data"] = strhex (
               item->peekdata().begin(), item->peekdata().size());
           entry["index"] = to_string (item->gettag ());
       }
       else
       {
           sle sle (item->peekserializer(), item->gettag ());
           json::value& entry = nodes.append (sle.getjson (0));
           entry["index"] = to_string (item->gettag ());
       }
    }

    return jvresult;
}

} // ripple
