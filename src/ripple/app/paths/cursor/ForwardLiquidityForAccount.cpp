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
#include <ripple/app/book/quality.h>
#include <ripple/app/paths/cursor/rippleliquidity.h>
#include <ripple/basics/log.h>
#include <ripple/protocol/indexes.h>

namespace ripple {
namespace path {

// the reverse pass has been narrowing by credit available and inflating by fees
// as it worked backwards.  now, for the current account node, take the actual
// amount from previous and adjust forward balances.
//
// perform balance adjustments between previous and current node.
// - the previous node: specifies what to push through to current.
// - all of previous output is consumed.
//
// then, compute current node's output for next node.
// - current node: specify what to push through to next.
// - output to next node is computed as input minus quality or transfer fee.
// - if next node is an offer and output is non-xrp then we are the issuer and
//   do not need to push funds.
// - if next node is an offer and output is xrp then we need to deliver funds to
//   limbo.
ter pathcursor::forwardliquidityforaccount () const
{
    ter resultcode   = tessuccess;
    auto const lastnodeindex       = pathstate_.nodes().size () - 1;

    std::uint64_t uratemax = 0;

    account const& previousaccountid =
            previousnode().isaccount() ? previousnode().account_ :
            node().account_;
    // offers are always issue.
    account const& nextaccountid =
            nextnode().isaccount() ? nextnode().account_ : node().account_;

    std::uint32_t uqualityin = nodeindex_
        ? quality_in (ledger(),
            node().account_,
            previousaccountid,
            node().issue_.currency)
        : quality_one;
    std::uint32_t  uqualityout = (nodeindex_ == lastnodeindex)
        ? quality_out (ledger(),
            node().account_,
            nextaccountid,
            node().issue_.currency)
        : quality_one;

    // when looking backward (prv) for req we care about what we just
    // calculated: use fwd.
    // when looking forward (cur) for req we care about what was desired: use
    // rev.

    // for nextnode().isaccount()
    auto saprvredeemact = previousnode().safwdredeem.zeroed();
    auto saprvissueact = previousnode().safwdissue.zeroed();

    // for !previousnode().isaccount()
    auto saprvdeliveract = previousnode().safwddeliver.zeroed ();

    writelog (lstrace, ripplecalc)
        << "forwardliquidityforaccount> "
        << "nodeindex_=" << nodeindex_ << "/" << lastnodeindex
        << " previousnode.safwdredeem:" << previousnode().safwdredeem
        << " saprvissuereq:" << previousnode().safwdissue
        << " previousnode.safwddeliver:" << previousnode().safwddeliver
        << " node.sarevredeem:" << node().sarevredeem
        << " node.sarevissue:" << node().sarevissue
        << " node.sarevdeliver:" << node().sarevdeliver;

    // ripple through account.

    if (previousnode().isaccount() && nextnode().isaccount())
    {
        // next is an account, must be rippling.

        if (!nodeindex_)
        {
            // ^ --> account --> account

            // for the first node, calculate amount to ripple based on what is
            // available.
            node().safwdredeem = node().sarevredeem;

            if (pathstate_.inreq() >= zero)
            {
                // limit by send max.
                node().safwdredeem = std::min (
                    node().safwdredeem, pathstate_.inreq() - pathstate_.inact());
            }

            pathstate_.setinpass (node().safwdredeem);

            node().safwdissue = node().safwdredeem == node().sarevredeem
                // fully redeemed.
                ? node().sarevissue : stamount (node().sarevissue);

            if (node().safwdissue && pathstate_.inreq() >= zero)
            {
                // limit by send max.
                node().safwdissue = std::min (
                    node().safwdissue,
                    pathstate_.inreq() - pathstate_.inact() - node().safwdredeem);
            }

            pathstate_.setinpass (pathstate_.inpass() + node().safwdissue);

            writelog (lstrace, ripplecalc)
                << "forwardliquidityforaccount: ^ --> "
                << "account --> account :"
                << " sainreq=" << pathstate_.inreq()
                << " sainact=" << pathstate_.inact()
                << " node.safwdredeem:" << node().safwdredeem
                << " node.sarevissue:" << node().sarevissue
                << " node.safwdissue:" << node().safwdissue
                << " pathstate_.sainpass:" << pathstate_.inpass();
        }
        else if (nodeindex_ == lastnodeindex)
        {
            // account --> account --> $
            writelog (lstrace, ripplecalc)
                << "forwardliquidityforaccount: account --> "
                << "account --> $ :"
                << " previousaccountid="
                << to_string (previousaccountid)
                << " node.account_="
                << to_string (node().account_)
                << " previousnode.safwdredeem:" << previousnode().safwdredeem
                << " previousnode.safwdissue:" << previousnode().safwdissue;

            // last node. accept all funds. calculate amount actually to credit.

            auto& sacurreceive = pathstate_.outpass();
            stamount saissuecrd = uqualityin >= quality_one
                    ? previousnode().safwdissue  // no fee.
                    : mulround (
                          previousnode().safwdissue,
                          stamount (noissue(), uqualityin, -9),
                          true); // amount to credit.

            // amount to credit. credit for less than received as a surcharge.
            pathstate_.setoutpass (previousnode().safwdredeem + saissuecrd);

            if (sacurreceive)
            {
                // actually receive.
                resultcode = ledger().ripplecredit (
                    previousaccountid,
                    node().account_,
                    previousnode().safwdredeem + previousnode().safwdissue,
                    false);
            }
            else
            {
                // after applying quality, total payment was microscopic.
                resultcode   = tecpath_dry;
            }
        }
        else
        {
            // account --> account --> account
            writelog (lstrace, ripplecalc)
                << "forwardliquidityforaccount: account --> "
                << "account --> account";

            node().safwdredeem.clear (node().sarevredeem);
            node().safwdissue.clear (node().sarevissue);

            // previous redeem part 1: redeem -> redeem
            if (previousnode().safwdredeem && node().sarevredeem)
                // previous wants to redeem.
            {
                // rate : 1.0 : quality out
                rippleliquidity (
                    ripplecalc_,
                    quality_one,
                    uqualityout,
                    previousnode().safwdredeem,
                    node().sarevredeem,
                    saprvredeemact,
                    node().safwdredeem,
                    uratemax);
            }

            // previous issue part 1: issue -> redeem
            if (previousnode().safwdissue != saprvissueact
                // previous wants to issue.
                && node().sarevredeem != node().safwdredeem)
                // current has more to redeem to next.
            {
                // rate: quality in : quality out
                rippleliquidity (
                    ripplecalc_,
                    uqualityin,
                    uqualityout,
                    previousnode().safwdissue,
                    node().sarevredeem,
                    saprvissueact,
                    node().safwdredeem,
                    uratemax);
            }

            // previous redeem part 2: redeem -> issue.
            if (previousnode().safwdredeem != saprvredeemact
                // previous still wants to redeem.
                && node().sarevredeem == node().safwdredeem
                // current redeeming is done can issue.
                && node().sarevissue)
                // current wants to issue.
            {
                // rate : 1.0 : transfer_rate
                rippleliquidity (
                    ripplecalc_,
                    quality_one,
                    rippletransferrate (ledger(), node().account_),
                    previousnode().safwdredeem,
                    node().sarevissue,
                    saprvredeemact,
                    node().safwdissue,
                    uratemax);
            }

            // previous issue part 2 : issue -> issue
            if (previousnode().safwdissue != saprvissueact
                // previous wants to issue.
                && node().sarevredeem == node().safwdredeem
                // current redeeming is done can issue.
                && node().sarevissue)
                // current wants to issue.
            {
                // rate: quality in : 1.0
                rippleliquidity (
                    ripplecalc_,
                    uqualityin,
                    quality_one,
                    previousnode().safwdissue,
                    node().sarevissue,
                    saprvissueact,
                    node().safwdissue,
                    uratemax);
            }

            stamount saprovide = node().safwdredeem + node().safwdissue;
            
            if (saprovide)
            {
                stamount satotalsend = previousnode().safwdredeem + previousnode().safwdissue;
                // adjust prv --> cur balance : take all inbound
                resultcode = ledger().ripplecredit (
                                       previousaccountid,
                                       node().account_,
                                       satotalsend,
                                       false);
                
                stamount safee = satotalsend - saprovide;
                writelog (lstrace, ripplecalc)
                    << "\n--------------------"
                    << "\npreviousnode():" << previousnode().account_
                    << "\n\tpreviousnode().safwdredeem:" << previousnode().safwdredeem
                    << "\n\tpreviousnode().safwdissue:" << previousnode().safwdissue
                    << "\nnode():" << node().account_
                    << "\n\tnode().safwdredeem:" << node().safwdredeem
                    << "\n\tnode().safwdissue:" << node().safwdissue
                    << "\nsatotalsend:" << satotalsend
                    << "\nsaprovide:" << saprovide
                    << "\nsafee:"<< safee
                    << "\n--------------------";

                if (safee > zero)
                {
                    // share fee with sender referee
                    stamount sasharerate = stamount(safee.issue(), 25, -2);
                    stamount sasharefee = multiply(safee, sasharerate);
                    account sender = node(0).account_;
                    account issuer = node().account_;
                    resultcode = ledger().sharefeewithreferee(sender, issuer, sasharefee);
                }
            }
            else
            {
                resultcode = tecpath_dry;
            }
        }
    }
    else if (previousnode().isaccount() && !nextnode().isaccount())
    {
        // current account is issuer to next offer.
        // determine deliver to offer amount.
        // don't adjust outbound balances- keep funds with issuer as limbo.
        // if issuer hold's an offer owners inbound ious, there is no fee and
        // redeem/issue will transparently happen.

        if (nodeindex_)
        {
            // non-xrp, current node is the issuer.
            writelog (lstrace, ripplecalc)
                << "forwardliquidityforaccount: account --> "
                << "account --> offer";

            node().safwddeliver.clear (node().sarevdeliver);

            // redeem -> issue/deliver.
            // previous wants to redeem.
            // current is issuing to an offer so leave funds in account as
            // "limbo".
            if (previousnode().safwdredeem)
                // previous wants to redeem.
            {
                // rate : 1.0 : transfer_rate
                // xxx is having the transfer rate here correct?
                rippleliquidity (
                    ripplecalc_,
                    quality_one,
                    rippletransferrate (ledger(), node().account_),
                    previousnode().safwdredeem,
                    node().sarevdeliver,
                    saprvredeemact,
                    node().safwddeliver,
                    uratemax);
            }

            // issue -> issue/deliver
            if (previousnode().safwdredeem == saprvredeemact
                // previous done redeeming: previous has no ious.
                && previousnode().safwdissue)
                // previous wants to issue. to next must be ok.
            {
                // rate: quality in : 1.0
                rippleliquidity (
                    ripplecalc_,
                    uqualityin,
                    quality_one,
                    previousnode().safwdissue,
                    node().sarevdeliver,
                    saprvissueact,
                    node().safwddeliver,
                    uratemax);
            }

            // adjust prv --> cur balance : take all inbound
            resultcode   = node().safwddeliver
                ? ledger().ripplecredit (
                    previousaccountid, node().account_,
                    previousnode().safwdredeem + previousnode().safwdissue,
                    false)
                : tecpath_dry;  // didn't actually deliver anything.
        }
        else
        {
            // delivering amount requested from downstream.
            node().safwddeliver = node().sarevdeliver;

            // if limited, then limit by send max and available.
            if (pathstate_.inreq() >= zero)
            {
                // limit by send max.
                node().safwddeliver = std::min (
                    node().safwddeliver, pathstate_.inreq() - pathstate_.inact());

                // limit xrp by available. no limit for non-xrp as issuer.
                if (isxrp (node().issue_))
                    node().safwddeliver = std::min (
                        node().safwddeliver,
                        ledger().accountholds (
                            node().account_,
                            xrpcurrency(),
                            xrpaccount(),
                            fhignore_freeze)); // xrp can't be frozen

				// limit vbc by available. no limit for non-vbc as issuer.
				if (isvbc(node().issue_))
					node().safwddeliver = std::min(
					node().safwddeliver,
					ledger().accountholds(
					node().account_,
					vbccurrency(),
					vbcaccount(),
					fhignore_freeze)); // vbc can't be frozen

            }

            // record amount sent for pass.
            pathstate_.setinpass (node().safwddeliver);

            if (!node().safwddeliver)
            {
                resultcode   = tecpath_dry;
            }
            else if (!isnative(node().issue_))
            {
                // non-xrp & non-vbc, current node is the issuer.
                // we could be delivering to multiple accounts, so we don't know
                // which ripple balance will be adjusted.  assume just issuing.

                writelog (lstrace, ripplecalc)
                    << "forwardliquidityforaccount: ^ --> "
                    << "account -- !xrp&!vbc --> offer";

                // as the issuer, would only issue.
                // don't need to actually deliver. as from delivering leave in
                // the issuer as limbo.
            }
            else
            {
                writelog (lstrace, ripplecalc)
                    << "forwardliquidityforaccount: ^ --> "
                    << "account -- xrp --> offer";

                // deliver xrp to limbo.
                resultcode = ledger().accountsend (
                      node().account_, isxrp(node().issue_)?xrpaccount():vbcaccount(), node().safwddeliver);
            }
        }
    }
    else if (!previousnode().isaccount() && nextnode().isaccount())
    {
        if (nodeindex_ == lastnodeindex)
        {
            // offer --> account --> $
            writelog (lstrace, ripplecalc)
                << "forwardliquidityforaccount: offer --> "
                << "account --> $ : "
                << previousnode().safwddeliver;

            // amount to credit.
            pathstate_.setoutpass (previousnode().safwddeliver);

            // no income balance adjustments necessary.  the paying side inside
            // the offer paid to this account.
        }
        else
        {
            // offer --> account --> account
            writelog (lstrace, ripplecalc)
                << "forwardliquidityforaccount: offer --> "
                << "account --> account";

            node().safwdredeem.clear (node().sarevredeem);
            node().safwdissue.clear (node().sarevissue);

            // deliver -> redeem
            if (previousnode().safwddeliver && node().sarevredeem)
                // previous wants to deliver and can current redeem.
            {
                // rate : 1.0 : quality out
                rippleliquidity (
                    ripplecalc_,
                    quality_one,
                    uqualityout,
                    previousnode().safwddeliver,
                    node().sarevredeem,
                    saprvdeliveract,
                    node().safwdredeem,
                    uratemax);
            }

            // deliver -> issue
            // wants to redeem and current would and can issue.
            if (previousnode().safwddeliver != saprvdeliveract
                // previous still wants to deliver.
                && node().sarevredeem == node().safwdredeem
                // current has more to redeem to next.
                && node().sarevissue)
                // current wants issue.
            {
                // rate : 1.0 : transfer_rate
                rippleliquidity (
                    ripplecalc_,
                    quality_one,
                    rippletransferrate (ledger(), node().account_),
                    previousnode().safwddeliver,
                    node().sarevissue,
                    saprvdeliveract,
                    node().safwdissue,
                    uratemax);
            }

            // no income balance adjustments necessary.  the paying side inside
            // the offer paid and the next link will receive.
            stamount saprovide = node().safwdredeem + node().safwdissue;

            if (!saprovide)
                resultcode = tecpath_dry;
        }
    }
    else
    {
        // offer --> account --> offer
        // deliver/redeem -> deliver/issue.
        writelog (lstrace, ripplecalc)
            << "forwardliquidityforaccount: offer --> account --> offer";

        node().safwddeliver.clear (node().sarevdeliver);

        if (previousnode().safwddeliver
            // previous wants to deliver
            && node().sarevissue)
            // current wants issue.
        {
            // rate : 1.0 : transfer_rate
            rippleliquidity (
                ripplecalc_,
                quality_one,
                rippletransferrate (ledger(), node().account_),
                previousnode().safwddeliver,
                node().sarevdeliver,
                saprvdeliveract,
                node().safwddeliver,
                uratemax);
        }

        // no income balance adjustments necessary.  the paying side inside the
        // offer paid and the next link will receive.
        if (!node().safwddeliver)
            resultcode   = tecpath_dry;
    }

    return resultcode;
}

} // path
} // ripple
