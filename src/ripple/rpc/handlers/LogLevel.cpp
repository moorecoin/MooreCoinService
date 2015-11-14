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
#include <boost/algorithm/string/predicate.hpp>

namespace ripple {

json::value dologlevel (rpc::context& context)
{
    // log_level
    if (!context.params.ismember ("severity"))
    {
        // get log severities
        json::value ret (json::objectvalue);
        json::value lev (json::objectvalue);

        lev["base"] =
                logs::tostring(logs::fromseverity(deprecatedlogs().severity()));
        std::vector< std::pair<std::string, std::string> > logtable (
            deprecatedlogs().partition_severities());
        typedef std::map<std::string, std::string>::value_type stringpair;
        boost_foreach (const stringpair & it, logtable)
            lev[it.first] = it.second;

        ret["levels"] = lev;
        return ret;
    }

    logseverity const sv (
        logs::fromstring (context.params["severity"].asstring ()));

    if (sv == lsinvalid)
        return rpcerror (rpcinvalid_params);

    auto severity = logs::toseverity(sv);
    // log_level severity
    if (!context.params.ismember ("partition"))
    {
        // set base log severity
        deprecatedlogs().severity(severity);
        return json::objectvalue;
    }

    // log_level partition severity base?
    if (context.params.ismember ("partition"))
    {
        // set partition severity
        std::string partition (context.params["partition"].asstring ());

        if (boost::iequals (partition, "base"))
            deprecatedlogs().severity (severity);
        else
            deprecatedlogs().get(partition).severity(severity);

        return json::objectvalue;
    }

    return rpcerror (rpcinvalid_params);
}

} // ripple
