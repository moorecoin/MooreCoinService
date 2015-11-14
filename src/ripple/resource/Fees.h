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

#ifndef ripple_resource_fees_h_included
#define ripple_resource_fees_h_included

#include <ripple/resource/charge.h>

namespace ripple {
namespace resource {

/** schedule of fees charged for imposing load on the server. */
/** @{ */
extern charge const feeinvalidrequest;        // a request that we can immediately tell is invalid
extern charge const feerequestnoreply;        // a request that we cannot satisfy
extern charge const feeinvalidsignature;      // an object whose signature we had to check and it failed
extern charge const feeunwanteddata;          // data we have no use for
extern charge const feebaddata;               // data we have to verify before rejecting

// rpc loads
extern charge const feeinvalidrpc;            // an rpc request that we can immediately tell is invalid.
extern charge const feereferencerpc;          // a default "reference" unspecified load
extern charge const feeexceptionrpc;          // an rpc load that causes an exception
extern charge const feelightrpc;              // a normal rpc command
extern charge const feelowburdenrpc;          // a slightly burdensome rpc load
extern charge const feemediumburdenrpc;       // a somewhat burdensome rpc load
extern charge const feehighburdenrpc;         // a very burdensome rpc load
extern charge const feepathfindupdate;        // an update to an existing pf request

// good things
extern charge const feenewtrustednote;        // a new transaction/validation/proposal we trust
extern charge const feenewvalidtx;            // a new, valid transaction
extern charge const feesatisfiedrequest;      // data we requested

// requests
extern charge const feerequesteddata;         // a request that is hard to satisfy, disk access
extern charge const feecheapquery;            // a query that is trivial, cached data

// administrative
extern charge const feewarning;               // the cost of receiving a warning
extern charge const feedrop;                  // the cost of being dropped for excess load
/** @} */

}
}

#endif
