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

#ifndef ripple_ripplecalc_h
#define ripple_ripplecalc_h

#include <ripple/app/ledger/ledgerentryset.h>
#include <ripple/app/paths/pathstate.h>
#include <ripple/protocol/stamount.h>
#include <ripple/protocol/ter.h>

namespace ripple {
namespace path {

/** ripplecalc calculates the quality of a payment path.

    quality is the amount of input required to produce a given output along a
    specified path - another name for this is exchange rate.
*/
class ripplecalc
{
public:
    struct input
    {
        bool partialpaymentallowed = false;
        bool defaultpathsallowed = true;
        bool limitquality = false;
        bool deleteunfundedoffers = false;
        bool isledgeropen = true;
    };
    struct output
    {
        // the computed input amount.
        stamount actualamountin;

        // the computed output amount.
        stamount actualamountout;

        // expanded path with all the actual nodes in it.
        // a path starts with the source account, ends with the destination account
        // and goes through other acounts or order books.
        pathstate::list pathstatelist;

    private:
        ter calculationresult_;

    public:
        ter result () const
        {
            return calculationresult_;
        }
        void setresult (ter const value)
        {
            calculationresult_ = value;
        }

    };

    static output ripplecalculate (
        ledgerentryset& activeledger,

        // compute paths using this ledger entry set.  up to caller to actually
        // apply to ledger.

        // issuer:
        //      xrp: xrpaccount()
        //  non-xrp: usrcaccountid (for any issuer) or another account with
        //           trust node.
        stamount const& samaxamountreq,             // --> -1 = no limit.

        // issuer:
        //      xrp: xrpaccount()
        //  non-xrp: udstaccountid (for any issuer) or another account with
        //           trust node.
        stamount const& sadstamountreq,

        account const& udstaccountid,
        account const& usrcaccountid,

        // a set of paths that are included in the transaction that we'll
        // explore for liquidity.
        stpathset const& spspaths,
        input const* const pinputs = nullptr);

    /** the active ledger. */
    ledgerentryset& mactiveledger;

    // if the transaction fails to meet some constraint, still need to delete
    // unfunded offers.
    //
    // offers that were found unfunded.
    path::offerset permanentlyunfundedoffers_;

    // first time working in reverse a funding source was mentioned.  source may
    // only be used there.

    // map of currency, issuer to node index.
    accountissuetonodeindex mumsource_;

private:
    ripplecalc (
        ledgerentryset& activeledger,
        stamount const& samaxamountreq,             // --> -1 = no limit.
        stamount const& sadstamountreq,

        account const& udstaccountid,
        account const& usrcaccountid,
        stpathset const& spspaths)
            : mactiveledger (activeledger),
              sadstamountreq_(sadstamountreq),
              samaxamountreq_(samaxamountreq),
              udstaccountid_(udstaccountid),
              usrcaccountid_(usrcaccountid),
              spspaths_(spspaths)
    {
    }

    /** compute liquidity through these path sets. */
    ter ripplecalculate ();

    /** add a single pathstate.  returns true on success.*/
    bool addpathstate(stpath const&, ter&);

    stamount const& sadstamountreq_;
    stamount const& samaxamountreq_;
    account const& udstaccountid_;
    account const& usrcaccountid_;
    stpathset const& spspaths_;

    // the computed input amount.
    stamount actualamountin_;

    // the computed output amount.
    stamount actualamountout_;

    // expanded path with all the actual nodes in it.
    // a path starts with the source account, ends with the destination account
    // and goes through other acounts or order books.
    pathstate::list pathstatelist_;

    input inputflags;
};

} // path
} // ripple

#endif
