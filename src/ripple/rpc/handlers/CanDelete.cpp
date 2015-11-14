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
#include <ripple/app/misc/shamapstore.h>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/format.hpp>

namespace ripple {

// can_delete [<ledgerid>|<ledgerhash>|now|always|never]
json::value docandelete (rpc::context& context)
{
    if (! getapp().getshamapstore().advisorydelete())
        return rpc::make_error(rpcnot_enabled);

    json::value ret (json::objectvalue);

    if (context.params.ismember("can_delete"))
    {
        json::value candelete  = context.params.get(jss::can_delete, 0);
        std::uint32_t candeleteseq = 0;

        if (candelete.isuint())
        {
            candeleteseq = candelete.asuint();
        }
        else
        {
            std::string candeletestr = candelete.asstring();
            boost::to_lower (candeletestr);

            if (candeletestr.find_first_not_of ("0123456789") ==
                std::string::npos)
            {
                candeleteseq =
                        beast::lexicalcast <std::uint32_t>(candeletestr);
            }
            else if (candeletestr == "never")
            {
                candeleteseq = 0;
            }
            else if (candeletestr == "always")
            {
                candeleteseq = std::numeric_limits <std::uint32_t>::max();
            }
            else if (candeletestr == "now")
            {
                candeleteseq = getapp().getshamapstore().getlastrotated();
                if (!candeleteseq)
                    return rpc::make_error (rpcnot_ready);            }
                else if (candeletestr.size() == 64 &&
                    candeletestr.find_first_not_of("0123456789abcdef") ==
                    std::string::npos)
            {
                uint256 ledgerhash (candeletestr);
                ledger::pointer ledger =
                        context.netops.getledgerbyhash (ledgerhash);
                if (!ledger)
                    return rpc::make_error(rpclgr_not_found, "ledgernotfound");

                candeleteseq = ledger->getledgerseq();
            }
            else
            {
                return rpc::make_error (rpcinvalid_params);
            }
        }

        ret["can_delete"] =
                getapp().getshamapstore().setcandelete (candeleteseq);
    }
    else
    {
        ret["can_delete"] = getapp().getshamapstore().getcandelete();
    }

    return ret;
}

} // ripple
