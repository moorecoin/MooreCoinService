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
#include <ripple/app/ledger/transactionstatesf.h>
#include <ripple/app/main/application.h>
#include <ripple/app/misc/networkops.h>
#include <ripple/app/tx/transactionmaster.h>
#include <ripple/nodestore/database.h>
#include <ripple/protocol/hashprefix.h>

namespace ripple {

transactionstatesf::transactionstatesf()
{
}

void transactionstatesf::gotnode (bool fromfilter,
                                  shamapnodeid const& id,
                                  uint256 const& nodehash,
                                  blob& nodedata,
                                  shamaptreenode::tntype type)
{
    // vfalco shamapsync filters should be passed the shamap, the
    //        shamap should provide an accessor to get the injected database,
    //        and this should use that database instad of getnodestore
    getapp().getnodestore ().store (
        (type == shamaptreenode::tntransaction_nm) ? hottransaction : hottransaction_node,
        std::move (nodedata),
        nodehash);
}

bool transactionstatesf::havenode (shamapnodeid const& id,
                                   uint256 const& nodehash,
                                   blob& nodedata)
{
    return getapp().getops ().getfetchpack (nodehash, nodedata);
}

} // ripple
