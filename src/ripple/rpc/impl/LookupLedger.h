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

#ifndef ripple_rpc_lookupledger_h_included
#define ripple_rpc_lookupledger_h_included

#include <ripple/rpc/status.h>

namespace ripple {
namespace rpc {

/** look up a ledger from a request and fill a json::result with either
    an error, or data representing a ledger.

    if there is no error in the return value, then the ledger pointer will have
    been filled.
*/
json::value lookupledger (
    json::value const& request, ledger::pointer&, networkops&);

/** look up a ledger from a request and fill a json::result with the data
    representing a ledger.

    if the returned status is ok, the ledger pointer will have been filled. */
status lookupledger (
    json::value const& request,
    ledger::pointer&,
    networkops&,
    json::value& result);

} // rpc
} // ripple

#endif
