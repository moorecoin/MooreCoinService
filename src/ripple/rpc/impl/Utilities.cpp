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

#include "boost/date_time/posix_time/posix_time.hpp"
#include <utility>

namespace ripple {
namespace rpc {

void
addpaymentdeliveredamount (
    json::value& meta,
    rpc::context& context,
    transaction::pointer transaction,
    transactionmetaset::pointer transactionmeta)
{
    sttx::pointer serializedtx;

    if (transaction)
        serializedtx = transaction->getstransaction ();

    if (serializedtx && serializedtx->gettxntype () == ttpayment)
    {
        // if the transaction explicitly specifies a deliveredamount in the
        // metadata then we use it.
        if (transactionmeta && transactionmeta->hasdeliveredamount ())
        {
            meta[jss::delivered_amount] =
                transactionmeta->getdeliveredamount ().getjson (1);
            return;
        }

        if (auto ledger = context.netops.getledgerbyseq (transaction->getledger ()))
        {
            // the first ledger where the deliveredamount flag appears is
            // which closed on 2014-jan-24 at 04:50:10. if the transaction we
            // are dealing with is in a ledger that closed after this date then
            // the absence of deliveredamount indicates that the correct amount
            // is in the amount field.

            boost::posix_time::ptime const cutoff (
                boost::posix_time::time_from_string ("2014-01-24 04:50:10"));

            if (ledger->getclosetime () >= cutoff)
            {
                meta[jss::delivered_amount] =
                    serializedtx->getfieldamount (sfamount).getjson (1);
                return;
            }
        }

        // otherwise we report "unavailable" which cannot be parsed into a
        // sensible amount.
        meta[jss::delivered_amount] = json::value ("unavailable");
    }
}

}
}
