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
#include <ripple/app/paths/credit.h>
#include <ripple/app/paths/pathstate.h>
#include <ripple/basics/log.h>
#include <ripple/json/to_string.h>
#include <ripple/protocol/indexes.h>
#include <boost/lexical_cast.hpp>

namespace ripple {

// optimize: when calculating path increment, note if increment consumes all
// liquidity. no need to revisit path in the future if all liquidity is used.
//

class ripplecalc; // for logging

void pathstate::clear()
{
    allliquidityconsumed_ = false;
    sainpass = sainreq.zeroed();
    saoutpass = saoutreq.zeroed();
    unfundedoffers_.clear ();
    umreverse.clear ();

    for (auto& node: nodes_)
        node.clear();
}

void pathstate::reset(stamount const& in, stamount const& out)
{
    clear();

    // update to current amount processed.
    sainact = in;
    saoutact = out;

    condlog (inreq() > zero && inact() >= inreq(),
             lswarning, ripplecalc)
        << "ripplecalc: done:"
        << " inact()=" << inact()
        << " inreq()=" << inreq();

    assert (inreq() < zero || inact() < inreq());
    // error if done.

    condlog (outact() >= outreq(), lswarning, ripplecalc)
        << "ripplecalc: already done:"
        << " saoutact=" << outact()
        << " saoutreq=" << outreq();

    assert (outact() < outreq());
    assert (nodes().size () >= 2);
}

// return true, iff lhs has less priority than rhs.
bool pathstate::lesspriority (pathstate const& lhs, pathstate const& rhs)
{
    // first rank is quality.
    if (lhs.uquality != rhs.uquality)
        return lhs.uquality > rhs.uquality;     // bigger is worse.

    // second rank is best quantity.
    if (lhs.saoutpass != rhs.saoutpass)
        return lhs.saoutpass < rhs.saoutpass;   // smaller is worse.

    // third rank is path index.
    return lhs.mindex > rhs.mindex;             // bigger is worse.
}

// make sure last path node delivers to account_: currency from.issue_.account.
//
// if the unadded next node as specified by arguments would not work as is, then
// add the necessary nodes so it would work.
// precondition: the pathstate must be non-empty.
//
// rules:
// - currencies must be converted via an offer.
// - a node names its output.

// - a ripple nodes output issuer must be the node's account or the next node's
//   account.
// - offers can only go directly to another offer if the currency and issuer are
//   an exact match.
// - real issuers must be specified for non-xrp.
ter pathstate::pushimpliednodes (
    account const& account,    // --> delivering to this account.
    currency const& currency,  // --> delivering this currency.
    account const& issuer)    // --> delivering this issuer.
{
    ter resultcode = tessuccess;

     writelog (lstrace, ripplecalc) << "pushimpliednodes>" <<
         " " << account <<
         " " << currency <<
         " " << issuer;

    if (nodes_.back ().issue_.currency != currency)
    {
        // currency is different, need to convert via an offer from an order
        // book.  xrpaccount() does double duty as signaling "this is an order
        // book".

        // corresponds to "implies an offer directory" in the diagram, currently
        // at http://goo.gl/uj3hab.

        auto type = isnative (currency) ? stpathelement::typecurrency
            : stpathelement::typecurrency | stpathelement::typeissuer;

        // the offer's output is what is now wanted.
        // xrpaccount() is a placeholder for offers.
        resultcode = pushnode (type, isxrp(currency)?xrpaccount():vbcaccount(), currency, issuer);
    }


    // for ripple, non-xrp && non-vbc, ensure the issuer is on at least one side of the
    // transaction.
    if (resultcode == tessuccess
		&& !isnative (currency)
        && nodes_.back ().account_ != issuer
        // previous is not issuing own ious.
        && account != issuer)
        // current is not receiving own ious.
    {
        // need to ripple through issuer's account.
        // case "implies an another node: (pushimpliednodes)" in the document.
        // intermediate account is the needed issuer.
        resultcode = pushnode (
            stpathelement::typeall, issuer, currency, issuer);
    }

    writelog (lstrace, ripplecalc)
        << "pushimpliednodes< : " << transtoken (resultcode);

    return resultcode;
}

// append a node, then create and insert before it any implied nodes.  order
// book nodes may go back to back.
//
// for each non-matching pair of issuedcurrency, there's an order book.
//
// <-- resultcode: tessuccess, tembad_path, terno_account, terno_auth,
//                 terno_line, tecpath_dry
ter pathstate::pushnode (
    const int itype,
    account const& account,    // if not specified, means an order book.
    currency const& currency,  // if not specified, default to previous.
    account const& issuer)     // if not specified, default to previous.
{
    path::node node;
    const bool pathisempty = nodes_.empty ();

    // todo(tom): if pathisempty, we probably don't need to do anything below.
    // indeed, we might just not even call pushnode in the first place!

    auto const& backnode = pathisempty ? path::node () : nodes_.back ();

    // true, iff node is a ripple account. false, iff node is an offer node.
    const bool hasaccount = (itype & stpathelement::typeaccount);

    // is currency specified for the output of the current node?
    const bool hascurrency = (itype & stpathelement::typecurrency);

    // issuer is specified for the output of the current node.
    const bool hasissuer = (itype & stpathelement::typeissuer);

    ter resultcode = tessuccess;

    writelog (lstrace, ripplecalc)
         << "pushnode> " << itype << ": "
         << (hasaccount ? to_string(account) : std::string("-")) << " "
         << (hascurrency ? to_string(currency) : std::string("-")) << "/"
         << (hasissuer ? to_string(issuer) : std::string("-")) << "/";

    node.uflags = itype;
    node.issue_.currency = hascurrency ?
            currency : backnode.issue_.currency;

    // todo(tom): we can probably just return immediately whenever we hit an
    // error in these next pages.

    if (itype & ~stpathelement::typeall)
    {
        // of course, this could never happen.
        writelog (lsdebug, ripplecalc) << "pushnode: bad bits.";
        resultcode = tembad_path;
    }
    else if (hasissuer && isnative (node.issue_))
    {
        writelog (lsdebug, ripplecalc) << "pushnode: issuer specified for native.";

        resultcode = tembad_path;
    }
    else if (hasissuer && !issuer)
    {
        writelog (lsdebug, ripplecalc) << "pushnode: specified bad issuer.";

        resultcode = tembad_path;
    }
    else if (!hasaccount && !hascurrency && !hasissuer)
    {
        // you can't default everything to the previous node as you would make
        // no progress.
        writelog (lsdebug, ripplecalc)
            << "pushnode: offer must specify at least currency or issuer.";
        resultcode = tembad_path;
    }
    else if (hasaccount)
    {
        // account link
        node.account_ = account;
        node.issue_.account = hasissuer ? issuer :
                (isxrp(node.issue_) ? xrpaccount() :
                 (isvbc(node.issue_) ? vbcaccount() : account));
        // zero value - for accounts.
        node.sarevredeem = stamount ({node.issue_.currency, account});
        node.sarevissue = node.sarevredeem;

        // for order books only - zero currency with the issuer id.
        node.sarevdeliver = stamount (node.issue_);
        node.safwddeliver = node.sarevdeliver;

        if (pathisempty)
        {
            // the first node is always correct as is.
        }
        else if (!account)
        {
            writelog (lsdebug, ripplecalc)
                << "pushnode: specified bad account.";
            resultcode = tembad_path;
        }
        else
        {
            // add required intermediate nodes to deliver to current account.
            writelog (lstrace, ripplecalc)
                << "pushnode: imply for account.";

            resultcode = pushimpliednodes (
                node.account_,
                node.issue_.currency,
                isxrp(node.issue_.currency) ? xrpaccount() :
                  (isvbc(node.issue_.currency) ? vbcaccount() : account));

            // note: backnode may no longer be the immediately previous node.
        }

        if (resultcode == tessuccess && !nodes_.empty ())
        {
            auto const& backnode = nodes_.back ();
            if (backnode.isaccount())
            {
                auto sleripplestate = lesentries.entrycache (
                    ltripple_state,
                    getripplestateindex (
                        backnode.account_,
                        node.account_,
                        backnode.issue_.currency));

                // a "ripplestate" means a balance betweeen two accounts for a
                // specific currency.
                if (!sleripplestate)
                {
                    writelog (lstrace, ripplecalc)
                            << "pushnode: no credit line between "
                            << backnode.account_ << " and " << node.account_
                            << " for " << node.issue_.currency << "." ;

                    writelog (lstrace, ripplecalc) << getjson ();

                    resultcode   = terno_line;
                }
                else
                {
                    writelog (lstrace, ripplecalc)
                            << "pushnode: credit line found between "
                            << backnode.account_ << " and " << node.account_
                            << " for " << node.issue_.currency << "." ;

                    auto slebck  = lesentries.entrycache (
                        ltaccount_root,
                        getaccountrootindex (backnode.account_));
                    // is the source account the highest numbered account id?
                    bool bhigh = backnode.account_ > node.account_;

                    if (!slebck)
                    {
                        writelog (lswarning, ripplecalc)
                            << "pushnode: delay: can't receive ious from "
                            << "non-existent issuer: " << backnode.account_;

                        resultcode   = terno_account;
                    }
                    else if ((slebck->getfieldu32 (sfflags) & lsfrequireauth) &&
                             !(sleripplestate->getfieldu32 (sfflags) &
                                  (bhigh ? lsfhighauth : lsflowauth)) &&
                             sleripplestate->getfieldamount(sfbalance) == zero)
                    {
                        writelog (lswarning, ripplecalc)
                                << "pushnode: delay: can't receive ious from "
                                << "issuer without auth.";

                        resultcode   = terno_auth;
                    }

                    if (resultcode == tessuccess)
                    {
                        stamount saowed = creditbalance (lesentries,
                            node.account_, backnode.account_,
                            node.issue_.currency);
                        stamount salimit;

                        if (saowed <= zero) {
                            salimit = creditlimit (lesentries,
                                node.account_,
                                backnode.account_,
                                node.issue_.currency);
                            if (-saowed >= salimit)
                            {
                                writelog (lswarning, ripplecalc) <<
                                    "pushnode: dry:" <<
                                    " saowed=" << saowed <<
                                    " salimit=" << salimit;

                                resultcode   = tecpath_dry;
                            }
                        }
                    }
                }
            }
        }

        if (resultcode == tessuccess)
            nodes_.push_back (node);
    }
    else
    {
        // offer link.
        //
        // offers bridge a change in currency and issuer, or just a change in
        // issuer.
        if (hasissuer)
            node.issue_.account = issuer;
        else if (isxrp (node.issue_.currency))
            node.issue_.account = xrpaccount();
        else if (isvbc (node.issue_.currency))
            node.issue_.account = vbcaccount();
        else if (isnative (backnode.issue_.account))
            node.issue_.account = backnode.account_;
        else
            node.issue_.account = backnode.issue_.account;

        node.saratemax = sazero;
        node.sarevdeliver = stamount (node.issue_);
        node.safwddeliver = node.sarevdeliver;

        if (!isconsistent (node.issue_))
        {
            writelog (lsdebug, ripplecalc)
                << "pushnode: currency is inconsistent with issuer.";

            resultcode = tembad_path;
        }
        else if (backnode.issue_ == node.issue_)
        {
            writelog (lsdebug, ripplecalc) <<
                "pushnode: bad path: offer to same currency and issuer";
            resultcode = tembad_path;
        }
        else {
            writelog (lstrace, ripplecalc) << "pushnode: imply for offer.";

            // insert intermediary issuer account if needed.
            resultcode   = pushimpliednodes (
                xrpaccount(), // rippling, but offers don't have an account.
                backnode.issue_.currency,
                backnode.issue_.account);
        }

        if (resultcode == tessuccess)
            nodes_.push_back (node);
    }

    writelog (lstrace, ripplecalc) << "pushnode< : " << transtoken (resultcode);
    return resultcode;
}

// set this object to be an expanded path from spsourcepath - take the implied
// nodes and makes them explicit.  it also sanitizes the path.
//
// there are only two types of nodes: account nodes and order books nodes.
//
// you can infer some nodes automatically.  if you're paying me bitstamp usd,
// then there must be an intermediate bitstamp node.
//
// if you have accounts a and b, and they're delivery currency issued by c, then
// there must be a node with account c in the middle.
//
// if you're paying usd and getting bitcoins, there has to be an order book in
// between.
//
// terstatus = tessuccess, tembad_path, terno_line, terno_account, terno_auth,
// or tembad_path_loop
ter pathstate::expandpath (
    const ledgerentryset& lessource,
    stpath const& spsourcepath,
    account const& ureceiverid,
    account const& usenderid)
{
    uquality = 1;            // mark path as active.

    currency const& umaxcurrencyid = sainreq.getcurrency ();
    account const& umaxissuerid = sainreq.getissuer ();

    currency const& currencyoutid = saoutreq.getcurrency ();
    account const& issueroutid = saoutreq.getissuer ();
    account const& usenderissuerid
        = isxrp(umaxcurrencyid) ? xrpaccount() :
            (isvbc(umaxcurrencyid) ? vbcaccount() : usenderid);
    // sender is always issuer for non-xrp && non-vbc.

    writelog (lstrace, ripplecalc)
        << "expandpath> " << spsourcepath.getjson (0);

    lesentries = lessource.duplicate ();

    terstatus = tessuccess;

    // vrp or vbc with issuer is malformed.
    if ((isxrp (umaxcurrencyid) && !isxrp (umaxissuerid))
        || (isxrp (currencyoutid) && !isxrp (issueroutid))
		|| (isvbc (umaxcurrencyid) && !isvbc (umaxissuerid))
		|| (isvbc (currencyoutid) && !isvbc (issueroutid)))
    {
        writelog (lsdebug, ripplecalc)
            << "expandpath> issuer with xrp";
        terstatus   = tembad_path;
    }

    // push sending node.
    // for non-xrp, issuer is always sending account.
    // - trying to expand, not-compact.
    // - every issuer will be traversed through.
    if (terstatus == tessuccess)
    {
        terstatus   = pushnode (
			!isnative(umaxcurrencyid)
            ? stpathelement::typeaccount | stpathelement::typecurrency |
              stpathelement::typeissuer
            : stpathelement::typeaccount | stpathelement::typecurrency,
            usenderid,
            umaxcurrencyid, // max specifies the currency.
            usenderissuerid);
    }

    writelog (lsdebug, ripplecalc)
        << "expandpath: pushed:"
        << " account=" << usenderid
        << " currency=" << umaxcurrencyid
        << " issuer=" << usenderissuerid;

    // issuer was not same as sender.
    if (tessuccess == terstatus && umaxissuerid != usenderissuerid)
    {
        // may have an implied account node.
        // - if it was xrp, then issuers would have matched.

        // figure out next node properties for implied node.
        const auto unxtcurrencyid  = spsourcepath.size ()
                ? currency(spsourcepath.front ().getcurrency ())
                // use next node.
                : currencyoutid;
                // use send.

        // todo(tom): complexify this next logic further in case someone
        // understands it.
		const auto nextaccountid   = spsourcepath.size ()
                ? account(spsourcepath. front ().getaccountid ())
                : !isxrp(currencyoutid)
                ? (isvbc(currencyoutid) ? vbcaccount() : (issueroutid == ureceiverid)
                ? account(ureceiverid)
                : account(issueroutid))                     // use implied node.
                : xrpaccount();

        writelog (lsdebug, ripplecalc)
            << "expandpath: implied check:"
            << " umaxissuerid=" << umaxissuerid
            << " usenderissuerid=" << usenderissuerid
            << " unxtcurrencyid=" << unxtcurrencyid
            << " nextaccountid=" << nextaccountid;

        // can't just use push implied, because it can't compensate for next
        // account.
        if (!unxtcurrencyid
            // next is xrp, offer next. must go through issuer.
            || umaxcurrencyid != unxtcurrencyid
            // next is different currency, offer next...
            || umaxissuerid != nextaccountid)
            // next is not implied issuer
        {
            writelog (lsdebug, ripplecalc)
                << "expandpath: sender implied:"
                << " account=" << umaxissuerid
                << " currency=" << umaxcurrencyid
                << " issuer=" << umaxissuerid;

            // add account implied by sendmax.
            terstatus = pushnode (
                !isnative(umaxcurrencyid)
                    ? stpathelement::typeaccount | stpathelement::typecurrency |
                      stpathelement::typeissuer
                    : stpathelement::typeaccount | stpathelement::typecurrency,
                umaxissuerid,
                umaxcurrencyid,
                umaxissuerid);
        }
    }

    for (auto & speelement: spsourcepath)
    {
        if (terstatus == tessuccess)
        {
            writelog (lstrace, ripplecalc) << "expandpath: element in path";
            terstatus = pushnode (
                speelement.getnodetype (), speelement.getaccountid (),
                speelement.getcurrency (), speelement.getissuerid ());
        }
    }

    if (terstatus == tessuccess
        && !isnative(currencyoutid)            // next is not xrp or vbc
        && issueroutid != ureceiverid)         // out issuer is not receiver
    {
        assert (!nodes_.empty ());

        auto const& backnode = nodes_.back ();

        if (backnode.issue_.currency != currencyoutid  // previous will be offer
            || backnode.account_ != issueroutid)       // need implied issuer
        {
            // add implied account.
            writelog (lsdebug, ripplecalc)
                << "expandpath: receiver implied:"
                << " account=" << issueroutid
                << " currency=" << currencyoutid
                << " issuer=" << issueroutid;

            terstatus   = pushnode (
                !isnative(currencyoutid)
                    ? stpathelement::typeaccount | stpathelement::typecurrency |
                      stpathelement::typeissuer
                    : stpathelement::typeaccount | stpathelement::typecurrency,
                issueroutid,
                currencyoutid,
                issueroutid);
        }
    }

    if (terstatus == tessuccess)
    {
        // create receiver node.
        // last node is always an account.

        terstatus   = pushnode (
			!isxrp(currencyoutid) && !isvbc(currencyoutid)
                ? stpathelement::typeaccount | stpathelement::typecurrency |
                   stpathelement::typeissuer
               : stpathelement::typeaccount | stpathelement::typecurrency,
            ureceiverid,                                    // receive to output
            currencyoutid,                                 // desired currency
            ureceiverid);
    }

    if (terstatus == tessuccess)
    {
        // look for first mention of source in nodes and detect loops.
        // note: the output is not allowed to be a source.
        unsigned int index = 0;
        for (auto& node: nodes_)
        {
            accountissue accountissue (node.account_, node.issue_);
            if (!umforward.insert ({accountissue, index++}).second)
            {
                // failed to insert. have a loop.
                writelog (lsdebug, ripplecalc) <<
                    "expandpath: loop detected: " << getjson ();

                terstatus = tembad_path_loop;
                break;
            }
        }
    }

    writelog (lsdebug, ripplecalc)
        << "expandpath:"
        << " in=" << umaxcurrencyid
        << "/" << umaxissuerid
        << " out=" << currencyoutid
        << "/" << issueroutid
        << ": " << getjson ();
    return terstatus;
}


/** check if an expanded path violates freeze rules */
void pathstate::checkfreeze()
{
    assert (nodes_.size() >= 2);

    // a path with no intermediaries -- pure issue/redeem
    // cannot be frozen.
    if (nodes_.size() == 2)
        return;

    sle::pointer sle;

    for (std::size_t i = 0; i < (nodes_.size() - 1); ++i)
    {
        // check each order book for a global freeze
        if (nodes_[i].uflags & stpathelement::typeissuer)
        {
            sle = lesentries.entrycache (ltaccount_root,
                getaccountrootindex (nodes_[i].issue_.account));

            if (sle && sle->isflag (lsfglobalfreeze))
            {
                terstatus = terno_line;
                return;
            }
        }

        // check each account change to make sure funds can leave
        if (nodes_[i].uflags & stpathelement::typeaccount)
        {
            currency const& currencyid = nodes_[i].issue_.currency;
            account const& inaccount = nodes_[i].account_;
            account const& outaccount = nodes_[i+1].account_;

            if (inaccount != outaccount)
            {
                sle = lesentries.entrycache (ltaccount_root,
                    getaccountrootindex (outaccount));

                if (sle && sle->isflag (lsfglobalfreeze))
                {
                    terstatus = terno_line;
                    return;
                }

                sle = lesentries.entrycache (ltripple_state,
                    getripplestateindex (inaccount,
                        outaccount, currencyid));

                if (sle && sle->isflag (
                    (outaccount > inaccount) ? lsfhighfreeze : lsflowfreeze))
                {
                    terstatus = terno_line;
                    return;
                }
            }
        }
    }
}

/** check if a sequence of three accounts violates the no ripple constrains
    [first] -> [second] -> [third]
    disallowed if 'second' set no ripple on [first]->[second] and
    [second]->[third]
*/
ter pathstate::checknoripple (
    account const& firstaccount,
    account const& secondaccount,
    // this is the account whose constraints we are checking
    account const& thirdaccount,
    currency const& currency)
{
    // fetch the ripple lines into and out of this node
    sle::pointer slein = lesentries.entrycache (ltripple_state,
        getripplestateindex (firstaccount, secondaccount, currency));
    sle::pointer sleout = lesentries.entrycache (ltripple_state,
        getripplestateindex (secondaccount, thirdaccount, currency));

    if (!slein || !sleout)
    {
        terstatus = terno_line;
    }
    else if (
        slein->getfieldu32 (sfflags) &
            ((secondaccount > firstaccount) ? lsfhighnoripple : lsflownoripple) &&
        sleout->getfieldu32 (sfflags) &
            ((secondaccount > thirdaccount) ? lsfhighnoripple : lsflownoripple))
    {
        writelog (lsinfo, ripplecalc)
            << "path violates noripple constraint between "
            << firstaccount << ", "
            << secondaccount << " and "
            << thirdaccount;

        terstatus = terno_ripple;
    }
    return terstatus;
}

// check a fully-expanded path to make sure it doesn't violate no-ripple
// settings.
ter pathstate::checknoripple (
    account const& udstaccountid,
    account const& usrcaccountid)
{
    // there must be at least one node for there to be two consecutive ripple
    // lines.
    if (nodes_.size() == 0)
       return terstatus;

    if (nodes_.size() == 1)
    {
        // there's just one link in the path
        // we only need to check source-node-dest
        if (nodes_[0].isaccount() &&
            (nodes_[0].account_ != usrcaccountid) &&
            (nodes_[0].account_ != udstaccountid))
        {
            if (sainreq.getcurrency() != saoutreq.getcurrency())
            {
                terstatus = terno_line;
            }
            else
            {
                terstatus = checknoripple (
                    usrcaccountid, nodes_[0].account_, udstaccountid,
                    nodes_[0].issue_.currency);
            }
        }
        return terstatus;
    }

    // check source <-> first <-> second
    if (nodes_[0].isaccount() &&
        nodes_[1].isaccount() &&
        (nodes_[0].account_ != usrcaccountid))
    {
        if ((nodes_[0].issue_.currency != nodes_[1].issue_.currency))
        {
            terstatus = terno_line;
            return terstatus;
        }
        else
        {
            terstatus = checknoripple (
                usrcaccountid, nodes_[0].account_, nodes_[1].account_,
                nodes_[0].issue_.currency);
            if (terstatus != tessuccess)
                return terstatus;
        }
    }

    // check second_from_last <-> last <-> destination
    size_t s = nodes_.size() - 2;
    if (nodes_[s].isaccount() &&
        nodes_[s + 1].isaccount() &&
        (udstaccountid != nodes_[s+1].account_))
    {
        if ((nodes_[s].issue_.currency != nodes_[s+1].issue_.currency))
        {
            terstatus = terno_line;
            return terstatus;
        }
        else
        {
            terstatus = checknoripple (
                nodes_[s].account_, nodes_[s+1].account_,
                udstaccountid, nodes_[s].issue_.currency);
            if (tessuccess != terstatus)
                return terstatus;
        }
    }

    // loop through all nodes that have a prior node and successor nodes
    // these are the nodes whose no ripple constraints could be violated
    for (int i = 1; i < nodes_.size() - 1; ++i)
    {
        if (nodes_[i - 1].isaccount() &&
            nodes_[i].isaccount() &&
            nodes_[i + 1].isaccount())
        { // two consecutive account-to-account links

            auto const& currencyid = nodes_[i].issue_.currency;
            if ((nodes_[i-1].issue_.currency != currencyid) ||
                (nodes_[i+1].issue_.currency != currencyid))
            {
                terstatus = tembad_path;
                return terstatus;
            }
            terstatus = checknoripple (
                nodes_[i-1].account_, nodes_[i].account_, nodes_[i+1].account_,
                currencyid);
            if (terstatus != tessuccess)
                return terstatus;
        }
    }

    return tessuccess;
}

// this is for debugging not end users. output names can be changed without
// warning.
json::value pathstate::getjson () const
{
    json::value jvpathstate (json::objectvalue);
    json::value jvnodes (json::arrayvalue);

    for (auto const &pnnode: nodes_)
        jvnodes.append (pnnode.getjson ());

    jvpathstate["status"]   = terstatus;
    jvpathstate["index"]    = mindex;
    jvpathstate["nodes"]    = jvnodes;

    if (sainreq)
        jvpathstate["in_req"]   = sainreq.getjson (0);

    if (sainact)
        jvpathstate["in_act"]   = sainact.getjson (0);

    if (sainpass)
        jvpathstate["in_pass"]  = sainpass.getjson (0);

    if (saoutreq)
        jvpathstate["out_req"]  = saoutreq.getjson (0);

    if (saoutact)
        jvpathstate["out_act"]  = saoutact.getjson (0);

    if (saoutpass)
        jvpathstate["out_pass"] = saoutpass.getjson (0);

    if (uquality)
        jvpathstate["uquality"] = boost::lexical_cast<std::string>(uquality);

    return jvpathstate;
}

} // ripple
