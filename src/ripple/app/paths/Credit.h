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

#ifndef rippled_ripple_app_paths_credit_h
#define rippled_ripple_app_paths_credit_h

#include <ripple/app/book/types.h>
#include <ripple/protocol/stamount.h>

namespace ripple {

/** calculate the maximum amount of ious that an account can hold
    @param ledger the ledger to check against.
    @param account the account of interest.
    @param issuer the issuer of the iou.
    @param currency the iou to check.
    @return the maximum amount that can be held.
*/
stamount creditlimit (
    ledgerentryset& ledger,
    account const& account,
    account const& issuer,
    currency const& currency);

/** returns the amount of ious issued by issuer that are held by an account
    @param ledger the ledger to check against.
    @param account the account of interest.
    @param issuer the issuer of the iou.
    @param currency the iou to check.
*/
stamount creditbalance (
    ledgerentryset& ledger,
    account const& account,
    account const& issuer,
    currency const& currency);

} // ripple

#endif
