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
#include <ripple/app/paths/ripplecalc.h>
#include <ripple/app/paths/cursor/pathcursor.h>
#include <ripple/basics/log.h>

namespace ripple {
namespace path {

namespace {

ter deleteoffers (
    ledgerentryset& activeledger, offerset& offers)
{
    for (auto& o: offers)
    {
        if (ter r = activeledger.offerdelete (o))
            return r;
    }
    return tessuccess;
}

} // namespace

ripplecalc::output ripplecalc::ripplecalculate (
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
    input const* const pinputs)
{
    ripplecalc rc (
        activeledger,
        samaxamountreq,
        sadstamountreq,
        udstaccountid,
        usrcaccountid,
        spspaths);
    if (pinputs != nullptr)
    {
        rc.inputflags = *pinputs;
    }

    auto result = rc.ripplecalculate ();
    output output;
    output.setresult (result);
    output.actualamountin = rc.actualamountin_;
    output.actualamountout = rc.actualamountout_;
    output.pathstatelist = rc.pathstatelist_;

    return output;
}

bool ripplecalc::addpathstate(stpath const& path, ter& resultcode)
{
    auto pathstate = std::make_shared<pathstate> (
        sadstamountreq_, samaxamountreq_);

    if (!pathstate)
    {
        resultcode = temunknown;
        return false;
    }

    pathstate->expandpath (
        mactiveledger,
        path,
        udstaccountid_,
        usrcaccountid_);

    if (pathstate->status() == tessuccess)
        pathstate->checknoripple (udstaccountid_, usrcaccountid_);

    if (pathstate->status() == tessuccess && mactiveledger.enforcefreeze ())
        pathstate->checkfreeze ();

    pathstate->setindex (pathstatelist_.size ());

    writelog (lsdebug, ripplecalc)
        << "ripplecalc: build direct:"
        << " status: " << transtoken (pathstate->status());

    // return if malformed.
    if (istemmalformed (pathstate->status()))
    {
        resultcode = pathstate->status();
        return false;
    }

    if (pathstate->status () == tessuccess)
    {
        resultcode = pathstate->status();
        pathstatelist_.push_back (pathstate);
    }
    else if (pathstate->status () != terno_line)
    {
        resultcode = pathstate->status();
    }

    return true;
}

// optimize: when calculating path increment, note if increment consumes all
// liquidity. no need to revisit path in the future if all liquidity is used.

// <-- ter: only returns teppath_partial if partialpaymentallowed.
ter ripplecalc::ripplecalculate ()
{
    assert (mactiveledger.isvalid ());
    writelog (lstrace, ripplecalc)
        << "ripplecalc>"
        << " samaxamountreq_:" << samaxamountreq_
        << " sadstamountreq_:" << sadstamountreq_;

    ter resultcode = temuncertain;
    permanentlyunfundedoffers_.clear ();
    mumsource_.clear ();

    // yyy might do basic checks on src and dst validity as per dopayment.

    // incrementally search paths.
    if (inputflags.defaultpathsallowed)
    {
        if (!addpathstate (stpath(), resultcode))
            return resultcode;
    }
    else if (spspaths_.empty ())
    {
        writelog (lsdebug, ripplecalc)
            << "ripplecalc: invalid transaction:"
            << "no paths and direct ripple not allowed.";

        return temripple_empty;
    }

    // build a default path.  use sadstamountreq_ and samaxamountreq_ to imply
    // nodes.
    // xxx might also make a xrp bridge by default.

    writelog (lstrace, ripplecalc)
        << "ripplecalc: paths in set: " << spspaths_.size ();

    // now expand the path state.
    for (auto const& sppath: spspaths_)
    {
        if (!addpathstate (sppath, resultcode))
            return resultcode;
    }

    if (resultcode != tessuccess)
        return (resultcode == temuncertain) ? terno_line : resultcode;

    resultcode = temuncertain;

    actualamountin_ = samaxamountreq_.zeroed();
    actualamountout_ = sadstamountreq_.zeroed();

    // when processing, we don't want to complicate directory walking with
    // deletion.
    const std::uint64_t uqualitylimit = inputflags.limitquality ?
            getrate (sadstamountreq_, samaxamountreq_) : 0;

    // offers that became unfunded.
    offerset unfundedoffersfrombestpaths;

    int ipass = 0;

    while (resultcode == temuncertain)
    {
        int ibest = -1;
        ledgerentryset lescheckpoint = mactiveledger;
        int idry = 0;

        // true, if ever computed multi-quality.
        bool multiquality = false;

        // find the best path.
        for (auto pathstate : pathstatelist_)
        {
            if (pathstate->quality())
                // only do active paths.
            {
                // if computing the only non-dry path, compute multi-quality.
                multiquality = ((pathstatelist_.size () - idry) == 1);

                // update to current amount processed.
                pathstate->reset (actualamountin_, actualamountout_);

                // error if done, output met.
                pathcursor pc(*this, *pathstate, multiquality);
                pc.nextincrement (lescheckpoint);

                // compute increment.
                writelog (lsdebug, ripplecalc)
                    << "ripplecalc: after:"
                    << " mindex=" << pathstate->index()
                    << " uquality=" << pathstate->quality()
                    << " rate=" << amountfromrate (pathstate->quality());

                if (!pathstate->quality())
                {
                    // path was dry.

                    ++idry;
                }
                else if (pathstate->outpass() == zero)
                {
                    // path is not dry, but moved no funds
                    // this should never happen. consider the path dry

                    writelog (lswarning, ripplecalc)
                        << "rippelcalc: non-dry path moves no funds";

                    assert (false);

                    pathstate->setquality (0);
                    ++idry;
                }
                else
                {
                    condlog (!pathstate->inpass() || !pathstate->outpass(),
                             lsdebug, ripplecalc)
                        << "ripplecalc: better:"
                        << " uquality="
                        << amountfromrate (pathstate->quality())
                        << " inpass()=" << pathstate->inpass()
                        << " saoutpass=" << pathstate->outpass();

                    assert (pathstate->inpass() && pathstate->outpass());

                    if ((!inputflags.limitquality ||
                         pathstate->quality() <= uqualitylimit)
                        // quality is not limited or increment has allowed
                        // quality.
                        && (ibest < 0
                            // best is not yet set.
                            || pathstate::lesspriority (
                                *pathstatelist_[ibest], *pathstate)))
                        // current is better than set.
                    {
                        writelog (lsdebug, ripplecalc)
                            << "ripplecalc: better:"
                            << " mindex=" << pathstate->index()
                            << " uquality=" << pathstate->quality()
                            << " rate="
                            << amountfromrate (pathstate->quality())
                            << " inpass()=" << pathstate->inpass()
                            << " saoutpass=" << pathstate->outpass();

                        assert (mactiveledger.isvalid ());
                        mactiveledger.swapwith (pathstate->ledgerentries());
                        // for the path, save ledger state.
                        mactiveledger.invalidate ();

                        ibest   = pathstate->index ();
                    }
                }
            }
        }

        if (shouldlog (lsdebug, ripplecalc))
        {
            writelog (lsdebug, ripplecalc)
                << "ripplecalc: summary:"
                << " pass: " << ++ipass
                << " dry: " << idry
                << " paths: " << pathstatelist_.size ();
            for (auto pathstate: pathstatelist_)
            {
                writelog (lsdebug, ripplecalc)
                    << "ripplecalc: "
                    << "summary: " << pathstate->index()
                    << " rate: "
                    << amountfromrate (pathstate->quality())
                    << " quality:" << pathstate->quality()
                    << " best: " << (ibest == pathstate->index ());
            }
        }

        if (ibest >= 0)
        {
            // apply best path.
            auto pathstate = pathstatelist_[ibest];

            writelog (lsdebug, ripplecalc)
                << "ripplecalc: best:"
                << " uquality="
                << amountfromrate (pathstate->quality())
                << " inpass()=" << pathstate->inpass()
                << " saoutpass=" << pathstate->outpass();

            // record best pass' offers that became unfunded for deletion on
            // success.

            unfundedoffersfrombestpaths.insert (
                pathstate->unfundedoffers().begin (),
                pathstate->unfundedoffers().end ());

            // record best pass' ledgerentryset to build off of and potentially
            // return.
            assert (pathstate->ledgerentries().isvalid ());
            mactiveledger.swapwith (pathstate->ledgerentries());
            pathstate->ledgerentries().invalidate ();

            actualamountin_ += pathstate->inpass();
            actualamountout_ += pathstate->outpass();

            if (pathstate->allliquidityconsumed() || multiquality)
            {
                ++idry;
                pathstate->setquality(0);
            }

            if (actualamountout_ == sadstamountreq_)
            {
                // done. delivered requested amount.

                resultcode   = tessuccess;
            }
            else if (actualamountout_ > sadstamountreq_)
            {
                writelog (lsfatal, ripplecalc)
                    << "ripplecalc: too much:"
                    << " actualamountout_:" << actualamountout_
                    << " sadstamountreq_:" << sadstamountreq_;

                return tefexception;  // temporary
                assert (false);
            }
            else if (actualamountin_ != samaxamountreq_ &&
                     idry != pathstatelist_.size ())
            {
                // have not met requested amount or max send, try to do
                // more. prepare for next pass.
                //
                // merge best pass' umreverse.
                mumsource_.insert (
                    pathstate->reverse().begin (), pathstate->reverse().end ());

            }
            else if (!inputflags.partialpaymentallowed)
            {
                // have sent maximum allowed. partial payment not allowed.

                resultcode   = tecpath_partial;
            }
            else
            {
                // have sent maximum allowed. partial payment allowed.  success.

                resultcode   = tessuccess;
            }
        }
        // not done and ran out of paths.
        else if (!inputflags.partialpaymentallowed)
        {
            // partial payment not allowed.
            resultcode = tecpath_partial;
        }
        // partial payment ok.
        else if (!actualamountout_)
        {
            // no payment at all.
            resultcode = tecpath_dry;
        }
        else
        {
            // we must restore the activeledger from lescheckpoint in the case
            // when ibest is -1 and just before the result is set to tessuccess.

            mactiveledger.swapwith (lescheckpoint);
            resultcode   = tessuccess;
        }
    }

    if (resultcode == tessuccess)
    {
        resultcode = deleteoffers(mactiveledger, unfundedoffersfrombestpaths);
        if (resultcode == tessuccess)
            resultcode = deleteoffers(mactiveledger, permanentlyunfundedoffers_);
    }

    // if isopenledger, then ledger is not final, can vote no.
    if (resultcode == telfailed_processing && !inputflags.isledgeropen)
        return tecfailed_processing;
    return resultcode;
}

} // path
} // ripple
