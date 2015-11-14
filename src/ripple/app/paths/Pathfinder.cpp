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
#include <ripple/app/main/application.h>
#include <ripple/app/paths/tuning.h>
#include <ripple/app/paths/pathfinder.h>
#include <ripple/app/paths/ripplecalc.h>
#include <ripple/app/paths/ripplelinecache.h>
#include <ripple/app/ledger/orderbookdb.h>
#include <ripple/basics/log.h>
#include <ripple/json/to_string.h>
#include <ripple/core/jobqueue.h>
#include <tuple>

/*

core pathfinding engine

the pathfinding request is identified by category, xrp to xrp, xrp to
non-xrp, non-xrp to xrp, same currency non-xrp to non-xrp, cross-currency
non-xrp to non-xrp.  for each category, there is a table of paths that the
pathfinder searches for.  complete paths are collected.

each complete path is then rated and sorted. paths with no or trivial
liquidity are dropped.  otherwise, paths are sorted based on quality,
liquidity, and path length.

path slots are filled in quality (ratio of out to in) order, with the
exception that the last path must have enough liquidity to complete the
payment (assuming no liquidity overlap).  in addition, if no selected path
is capable of providing enough liquidity to complete the payment by itself,
an extra "covering" path is returned.

the selected paths are then tested to determine if they can complete the
payment and, if so, at what cost.  if they fail and a covering path was
found, the test is repeated with the covering path.  if this succeeds, the
final paths and the estimated cost are returned.

the engine permits the search depth to be selected and the paths table
includes the depth at which each path type is found.  a search depth of zero
causes no searching to be done.  extra paths can also be injected, and this
should be used to preserve previously-found paths across invokations for the
same path request (particularly if the search depth may change).

*/

namespace ripple {

namespace {

// we sort possible paths by:
//    cost of path
//    length of path
//    width of path

// compare two pathranks.  a better pathrank is lower, so the best are sorted to
// the beginning.
bool comparepathrank (
    pathfinder::pathrank const& a, pathfinder::pathrank const& b)
{
    // 1) higher quality (lower cost) is better
    if (a.quality != b.quality)
        return a.quality < b.quality;

    // 2) more liquidity (higher volume) is better
    if (a.liquidity != b.liquidity)
        return a.liquidity > b.liquidity;

    // 3) shorter paths are better
    if (a.length != b.length)
        return a.length < b.length;

    // 4) tie breaker
    return a.index > b.index;
}

struct accountcandidate
{
    int priority;
    account account;

    static const int highpriority = 10000;
};

bool compareaccountcandidate (
    std::uint32_t seq,
    accountcandidate const& first, accountcandidate const& second)
{
    if (first.priority < second.priority)
        return false;

    if (first.account > second.account)
        return true;

    return (first.priority ^ seq) < (second.priority ^ seq);
}

typedef std::vector<accountcandidate> accountcandidates;

struct costedpath
{
    int searchlevel;
    pathfinder::pathtype type;
};

typedef std::vector<costedpath> costedpathlist;

typedef std::map<pathfinder::paymenttype, costedpathlist> pathtable;

struct pathcost {
    int cost;
    char const* path;
};
typedef std::vector<pathcost> pathcostlist;

static pathtable mpathtable;

std::string pathtypetostring (pathfinder::pathtype const& type)
{
    std::string ret;

    for (auto const& node : type)
    {
        switch (node)
        {
            case pathfinder::nt_source:
                ret.append("s");
                break;
            case pathfinder::nt_accounts:
                ret.append("a");
                break;
            case pathfinder::nt_books:
                ret.append("b");
                break;
            case pathfinder::nt_xrp_book:
                ret.append("x");
                break;
            case pathfinder::nt_dest_book:
                ret.append("f");
                break;
            case pathfinder::nt_destination:
                ret.append("d");
                break;
        }
    }

    return ret;
}

}  // namespace

pathfinder::pathfinder (
    ripplelinecache::ref cache,
    account const& usrcaccount,
    account const& udstaccount,
    currency const& usrccurrency,
    account const& usrcissuer,
    stamount const& sadstamount)
    :   msrcaccount (usrcaccount),
        mdstaccount (udstaccount),
        mdstamount (sadstamount),
        msrccurrency (usrccurrency),
        msrcissuer (usrcissuer),
        msrcamount ({usrccurrency, usrcissuer}, 1u, 0, true),
        mledger (cache->getledger ()),
        mrlcache (cache)
{
    assert (isxrp(usrccurrency) == isxrp(usrcissuer));
    assert (isvbc(usrccurrency) == isvbc(usrcissuer));
}

pathfinder::pathfinder (
    ripplelinecache::ref cache,
    account const& usrcaccount,
    account const& udstaccount,
    currency const& usrccurrency,
    stamount const& sadstamount)
    :   msrcaccount (usrcaccount),
        mdstaccount (udstaccount),
        mdstamount (sadstamount),
        msrccurrency (usrccurrency),
        msrcamount (
        {
            usrccurrency,
            isxrp (usrccurrency) ? xrpaccount () : (isvbc(usrccurrency) ? vbcaccount() : usrcaccount)
        }, 1u, 0, true),
        mledger (cache->getledger ()),
        mrlcache (cache)
{
}

pathfinder::~pathfinder()
{
}

bool pathfinder::findpaths (int searchlevel)
{
    if (mdstamount == zero)
    {
        // no need to send zero money.
        writelog (lsdebug, pathfinder) << "destination amount was zero.";
        mledger.reset ();
        return false;

        // todo(tom): why do we reset the ledger just in this case and the one
        // below - why don't we do it each time we return false?
    }

    if (msrcaccount == mdstaccount &&
        msrccurrency == mdstamount.getcurrency ())
    {
        // no need to send to same account with same currency.
        writelog (lsdebug, pathfinder) << "tried to send to same issuer";
        mledger.reset ();
        return false;
    }

    m_loadevent = getapp ().getjobqueue ().getloadevent (
        jtpath_find, "findpath");
    auto currencyisxrp = isxrp (msrccurrency);
    auto currencyisvbc = isvbc (msrccurrency);

    bool useissueraccount
            = msrcissuer && !currencyisxrp && !isxrp (*msrcissuer) && !currencyisvbc && !isvbc (*msrcissuer);
    auto& account = useissueraccount ? *msrcissuer : msrcaccount;
    auto issuer = currencyisxrp ? account() : ( currencyisvbc ? vbcaccount() : account );
    msource = stpathelement (account, msrccurrency, issuer);
    auto issuerstring = msrcissuer
            ? to_string (*msrcissuer) : std::string ("none");
    writelog (lstrace, pathfinder)
            << "findpaths>"
            << " msrcaccount=" << msrcaccount
            << " mdstaccount=" << mdstaccount
            << " mdstamount=" << mdstamount.getfulltext ()
            << " msrccurrency=" << msrccurrency
            << " msrcissuer=" << issuerstring;

    if (!mledger)
    {
        writelog (lsdebug, pathfinder) << "findpaths< no ledger";
        return false;
    }

    bool bsrcxrp = isxrp (msrccurrency);
    bool bdstxrp = isxrp (mdstamount.getcurrency());

    if (!mledger->getslei (getaccountrootindex (msrcaccount)))
    {
        // we can't even start without a source account.
        writelog (lsdebug, pathfinder) << "invalid source account";
        return false;
    }

    if (!mledger->getslei (getaccountrootindex (mdstaccount)))
    {
        // can't find the destination account - we must be funding a new
        // account.
        if (!bdstxrp)
        {
            writelog (lsdebug, pathfinder)
                    << "new account not being funded in xrp ";
            return false;
        }

        auto reserve = mledger->getreserve (0);
        if (mdstamount < reserve)
        {
            writelog (lsdebug, pathfinder)
                    << "new account not getting enough funding: "
                    << mdstamount << " < " << reserve;
            return false;
        }
    }

    // now compute the payment type from the types of the source and destination
    // currencies.
    paymenttype paymenttype;
    if (bsrcxrp && bdstxrp)
    {
        // xrp -> xrp
        writelog (lsdebug, pathfinder) << "xrp to xrp payment";
        paymenttype = pt_xrp_to_xrp;
    }
    else if (bsrcxrp)
    {
        // xrp -> non-xrp
        writelog (lsdebug, pathfinder) << "xrp to non-xrp payment";
        paymenttype = pt_xrp_to_nonxrp;
    }
    else if (bdstxrp)
    {
        // non-xrp -> xrp
        writelog (lsdebug, pathfinder) << "non-xrp to xrp payment";
        paymenttype = pt_nonxrp_to_xrp;
    }
    else if (msrccurrency == mdstamount.getcurrency ())
    {
        // non-xrp -> non-xrp - same currency
        writelog (lsdebug, pathfinder) << "non-xrp to non-xrp - same currency";
        paymenttype = pt_nonxrp_to_same;
    }
    else
    {
        // non-xrp to non-xrp - different currency
        writelog (lsdebug, pathfinder) << "non-xrp to non-xrp - cross currency";
        paymenttype = pt_nonxrp_to_nonxrp;
    }

    // now iterate over all paths for that paymenttype.
    for (auto const& costedpath : mpathtable[paymenttype])
    {
        // only use paths with at most the current search level.
        if (costedpath.searchlevel <= searchlevel)
        {
            addpathsfortype (costedpath.type);

            // todo(tom): we might be missing other good paths with this
            // arbitrary cut off.
            if (mcompletepaths.size () > pathfinder_max_complete_paths)
                break;
        }
    }

    writelog (lsdebug, pathfinder)
            << mcompletepaths.size () << " complete paths found";

    // even if we find no paths, default paths may work, and we don't check them
    // currently.
    return true;
}

ter pathfinder::getpathliquidity (
    stpath const& path,            // in:  the path to check.
    stamount const& mindstamount,  // in:  the minimum output this path must
                                   //      deliver to be worth keeping.
    stamount& amountout,           // out: the actual liquidity along the path.
    uint64_t& qualityout) const    // out: the returned initial quality
{
    stpathset pathset;
    pathset.push_back (path);

    path::ripplecalc::input rcinput;
    rcinput.defaultpathsallowed = false;

    ledgerentryset lessandbox (mledger, tapnone);

    try
    {
        // compute a path that provides at least the minimum liquidity.
        auto rc = path::ripplecalc::ripplecalculate (
            lessandbox,
            msrcamount,
            mindstamount,
            mdstaccount,
            msrcaccount,
            pathset,
            &rcinput);

        // if we can't get even the minimum liquidity requested, we're done.
        if (rc.result () != tessuccess)
            return rc.result ();

        qualityout = getrate (rc.actualamountout, rc.actualamountin);
        amountout = rc.actualamountout;

        // now try to compute the remaining liquidity.
        rcinput.partialpaymentallowed = true;
        rc = path::ripplecalc::ripplecalculate (
            lessandbox,
            msrcamount,
            mdstamount - amountout,
            mdstaccount,
            msrcaccount,
            pathset,
            &rcinput);

        // if we found further liquidity, add it into the result.
        if (rc.result () == tessuccess)
            amountout += rc.actualamountout;

        return tessuccess;
    }
    catch (std::exception const& e)
    {
        writelog (lsinfo, pathfinder) <<
            "checkpath: exception (" << e.what() << ") " <<
            path.getjson (0);
        return tefexception;
    }
}

namespace {

// return the smallest amount of useful liquidity for a given amount, and the
// total number of paths we have to evaluate.
stamount smallestusefulamount (stamount const& amount, int maxpaths)
{
    return divide (amount, stamount (maxpaths + 2), amount);
}

} // namespace

void pathfinder::computepathranks (int maxpaths)
{
    mremainingamount = mdstamount;

    // must subtract liquidity in default path from remaining amount.
    try
    {
        ledgerentryset lessandbox (mledger, tapnone);

        path::ripplecalc::input rcinput;
        rcinput.partialpaymentallowed = true;
        auto rc = path::ripplecalc::ripplecalculate (
            lessandbox,
            msrcamount,
            mdstamount,
            mdstaccount,
            msrcaccount,
            stpathset(),
            &rcinput);

        if (rc.result () == tessuccess)
        {
            writelog (lsdebug, pathfinder)
                    << "default path contributes: " << rc.actualamountin;
            mremainingamount -= rc.actualamountout;
        }
        else
        {
            writelog (lsdebug, pathfinder)
                << "default path fails: " << transtoken (rc.result ());
        }
    }
    catch (...)
    {
        writelog (lsdebug, pathfinder) << "default path causes exception";
    }

    rankpaths (maxpaths, mcompletepaths, mpathranks);
}

static bool isdefaultpath (stpath const& path)
{
    // todo(tom): default paths can consist of more than just an account:
    // https://forum.ripple.com/viewtopic.php?f=2&t=8206&start=10#p57713
    //
    // joelkatz writes:
    // so the test for whether a path is a default path is incorrect. i'm not
    // sure it's worth the complexity of fixing though. if we are going to fix
    // it, i'd suggest doing it this way:
    //
    // 1) compute the default path, probably by using 'expandpath' to expand an
    // empty path.  2) chop off the source and destination nodes.
    //
    // 3) in the pathfinding loop, if the source issuer is not the sender,
    // reject all paths that don't begin with the issuer's account node or match
    // the path we built at step 2.
    return path.size() == 1;
}

static stpath removeissuer (stpath const& path)
{
    // this path starts with the issuer, which is already implied
    // so remove the head node
    stpath ret;

    for (auto it = path.begin() + 1; it != path.end(); ++it)
        ret.push_back (*it);

    return ret;
}

// for each useful path in the input path set,
// create a ranking entry in the output vector of path ranks
void pathfinder::rankpaths (
    int maxpaths,
    stpathset const& paths,
    std::vector <pathrank>& rankedpaths)
{

    rankedpaths.clear ();
    rankedpaths.reserve (paths.size());

    // ignore paths that move only very small amounts.
    auto samindstamount = smallestusefulamount (mdstamount, maxpaths);

    for (int i = 0; i < paths.size (); ++i)
    {
        auto const& currentpath = paths[i];
        stamount liquidity;
        uint64_t uquality;

        if (currentpath.empty ())
            continue;

        auto const resultcode = getpathliquidity (
            currentpath, samindstamount, liquidity, uquality);

        if (resultcode != tessuccess)
        {
            writelog (lsdebug, pathfinder) <<
                "findpaths: dropping : " << transtoken (resultcode) <<
                ": " << currentpath.getjson (0);
        }
        else
        {
            writelog (lsdebug, pathfinder) <<
                "findpaths: quality: " << uquality <<
                ": " << currentpath.getjson (0);

            rankedpaths.push_back ({uquality, currentpath.size (), liquidity, i});
        }
    }
    std::sort (rankedpaths.begin (), rankedpaths.end (), comparepathrank);
}

stpathset pathfinder::getbestpaths (
    int maxpaths,
    stpath& fullliquiditypath,
    stpathset& extrapaths,
    account const& srcissuer)
{
    writelog (lsdebug, pathfinder) << "findpaths: " <<
        mcompletepaths.size() << " paths and " <<
        extrapaths.size () << " extras";

    assert (fullliquiditypath.empty ());
    const bool issuerissender = isxrp (msrccurrency) || isvbc (msrccurrency) || (srcissuer == msrcaccount);

    if (issuerissender &&
            (mcompletepaths.size () <= maxpaths) &&
            (extrapaths.size() == 0))
        return mcompletepaths;

    std::vector <pathrank> extrapathranks;
    rankpaths (maxpaths, extrapaths, extrapathranks);

    stpathset bestpaths;

    // the best pathranks are now at the start.  pull off enough of them to
    // fill bestpaths, then look through the rest for the best individual
    // path that can satisfy the entire liquidity - if one exists.
    stamount remaining = mremainingamount;

    auto pathsiterator = mpathranks.begin();
    auto extrapathsiterator = extrapathranks.begin();

    while (pathsiterator != mpathranks.end() ||
        extrapathsiterator != extrapathranks.end())
    {
        bool usepath = false;
        bool useextrapath = false;

        if (pathsiterator == mpathranks.end())
	    useextrapath = true;
        else if (extrapathsiterator == extrapathranks.end())
            usepath = true;
        else if (extrapathsiterator->quality < pathsiterator->quality)
            useextrapath = true;
        else if (extrapathsiterator->quality > pathsiterator->quality)
            usepath = true;
        else if (extrapathsiterator->liquidity > pathsiterator->liquidity)
            useextrapath = true;
        else if (extrapathsiterator->liquidity < pathsiterator->liquidity)
            usepath = true;
        else
        {
            // risk is high they have identical liquidity
            useextrapath = true;
            usepath = true;
        }

        auto& pathrank = usepath ? *pathsiterator : *extrapathsiterator;

        auto& path = usepath ? mcompletepaths[pathrank.index] :
            extrapaths[pathrank.index];

        if (useextrapath)
            ++extrapathsiterator;

        if (usepath)
            ++pathsiterator;

        auto ipathsleft = maxpaths - bestpaths.size ();
        if (!(ipathsleft > 0 || fullliquiditypath.empty ()))
            break;

        if (path.empty ())
        {
            assert (false);
            continue;
        }

        bool startswithissuer = false;

        if (! issuerissender && usepath)
        {
            // need to make sure path matches issuer constraints

            if (path.front ().getaccountid() != srcissuer)
                continue;
            if (isdefaultpath (path))
            {
                continue;
            }
            startswithissuer = true;
        }

        if (ipathsleft > 1 ||
            (ipathsleft > 0 && pathrank.liquidity >= remaining))
            // last path must fill
        {
            --ipathsleft;
            remaining -= pathrank.liquidity;
            bestpaths.push_back (startswithissuer ? removeissuer (path) : path);
        }
        else if (ipathsleft == 0 &&
                 pathrank.liquidity >= mdstamount &&
                 fullliquiditypath.empty ())
        {
            // we found an extra path that can move the whole amount.
            fullliquiditypath = (startswithissuer ? removeissuer (path) : path);
            writelog (lsdebug, pathfinder) <<
                "found extra full path: " << fullliquiditypath.getjson (0);
        }
        else
        {
            writelog (lsdebug, pathfinder) <<
                "skipping a non-filling path: " << path.getjson (0);
        }
    }

    if (remaining > zero)
    {
        assert (fullliquiditypath.empty ());
        writelog (lsinfo, pathfinder) <<
            "paths could not send " << remaining << " of " << mdstamount;
    }
    else
    {
        writelog (lsdebug, pathfinder) <<
            "findpaths: results: " << bestpaths.getjson (0);
    }
    return bestpaths;
}

bool pathfinder::issuematchesorigin (issue const& issue)
{
    bool matchingcurrency = (issue.currency == msrccurrency);
    bool matchingaccount =
            isnative (issue.currency) ||
            (msrcissuer && issue.account == msrcissuer) ||
            issue.account == msrcaccount;

    return matchingcurrency && matchingaccount;
}

int pathfinder::getpathsout (
    currency const& currency,
    account const& account,
    bool isdstcurrency,
    account const& dstaccount)
{
    issue const issue (currency, account);

    auto it = mpathsoutcountmap.emplace (issue, 0);

    // if it was already present, return the stored number of paths
    if (!it.second)
        return it.first->second;

    auto sleaccount = mledger->getslei (getaccountrootindex (account));

    if (!sleaccount)
        return 0;

    int aflags = sleaccount->getfieldu32 (sfflags);
    bool const bauthrequired = (aflags & lsfrequireauth) != 0;
    bool const bfrozen = ((aflags & lsfglobalfreeze) != 0)
        && mledger->enforcefreeze ();

    int count = 0;

    if (!bfrozen)
    {
        count = getapp ().getorderbookdb ().getbooksize (issue);

        for (auto const& item : mrlcache->getripplelines (account))
        {
            ripplestate* rspentry = (ripplestate*) item.get ();

            if (currency != rspentry->getlimit ().getcurrency ())
            {
            }
            else if (rspentry->getbalance () <= zero &&
                     (!rspentry->getlimitpeer ()
                      || -rspentry->getbalance () >= rspentry->getlimitpeer ()
                      ||  (bauthrequired && !rspentry->getauth ())))
            {
            }
            else if (isdstcurrency &&
                     dstaccount == rspentry->getaccountidpeer ())
            {
                count += 10000; // count a path to the destination extra
            }
            else if (rspentry->getnoripplepeer ())
            {
                // this probably isn't a useful path out
            }
            else if (rspentry->getfreezepeer () && mledger->enforcefreeze ())
            {
                // not a useful path out
            }
            else
            {
                ++count;
            }
        }
    }
    it.first->second = count;
    return count;
}

void pathfinder::addlinks (
    stpathset const& currentpaths,  // the paths to build from
    stpathset& incompletepaths,     // the set of partial paths we add to
    int addflags)
{
    writelog (lsdebug, pathfinder)
        << "addlink< on " << currentpaths.size ()
        << " source(s), flags=" << addflags;
    for (auto const& path: currentpaths)
        addlink (path, incompletepaths, addflags);
}

stpathset& pathfinder::addpathsfortype (pathtype const& pathtype)
{
    // see if the set of paths for this type already exists.
    auto it = mpaths.find (pathtype);
    if (it != mpaths.end ())
        return it->second;

    // otherwise, if the type has no nodes, return the empty path.
    if (pathtype.empty ())
        return mpaths[pathtype];

    // otherwise, get the paths for the parent pathtype by calling
    // addpathsfortype recursively.
    pathtype parentpathtype = pathtype;
    parentpathtype.pop_back ();

    stpathset const& parentpaths = addpathsfortype (parentpathtype);
    stpathset& pathsout = mpaths[pathtype];

    writelog (lsdebug, pathfinder)
        << "getpaths< adding onto '"
        << pathtypetostring (parentpathtype) << "' to get '"
        << pathtypetostring (pathtype) << "'";

    int initialsize = mcompletepaths.size ();

    // add the last nodetype to the lists.
    auto nodetype = pathtype.back ();
    switch (nodetype)
    {
    case nt_source:
        // source must always be at the start, so pathsout has to be empty.
        assert (pathsout.empty ());
        pathsout.push_back (stpath ());
        break;

    case nt_accounts:
        addlinks (parentpaths, pathsout, afadd_accounts);
        break;

    case nt_books:
        addlinks (parentpaths, pathsout, afadd_books);
        break;

    case nt_xrp_book:
        addlinks (parentpaths, pathsout, afadd_books | afob_xrp);
        break;

    case nt_dest_book:
        addlinks (parentpaths, pathsout, afadd_books | afob_last);
        break;

    case nt_destination:
        // fixme: what if a different issuer was specified on the
        // destination amount?
        // todo(tom): what does this even mean?  should it be a jira?
        addlinks (parentpaths, pathsout, afadd_accounts | afac_last);
        break;
    }

    condlog (mcompletepaths.size () != initialsize, lsdebug, pathfinder)
        << (mcompletepaths.size () - initialsize)
        << " complete paths added";
    writelog (lsdebug, pathfinder)
        << "getpaths> " << pathsout.size () << " partial paths found";
    return pathsout;
}

bool pathfinder::isnoripple (
    account const& fromaccount,
    account const& toaccount,
    currency const& currency)
{
    auto sleripple = mledger->getslei (
        getripplestateindex (toaccount, fromaccount, currency));

    auto const flag ((toaccount > fromaccount)
                     ? lsfhighnoripple : lsflownoripple);

    return sleripple && (sleripple->getfieldu32 (sfflags) & flag);
}

// does this path end on an account-to-account link whose last account has
// set "no ripple" on the link?
bool pathfinder::isnorippleout (stpath const& currentpath)
{
    // must have at least one link.
    if (currentpath.empty ())
        return false;

    // last link must be an account.
    stpathelement const& endelement = currentpath.back ();
    if (!(endelement.getnodetype () & stpathelement::typeaccount))
        return false;

    // if there's only one item in the path, return true if that item specifies
    // no ripple on the output. a path with no ripple on its output can't be
    // followed by a link with no ripple on its input.
    auto const& fromaccount = (currentpath.size () == 1)
        ? msrcaccount
        : (currentpath.end () - 2)->getaccountid ();
    auto const& toaccount = endelement.getaccountid ();
    return isnoripple (fromaccount, toaccount, endelement.getcurrency ());
}

void adduniquepath (stpathset& pathset, stpath const& path)
{
    // todo(tom): building an stpathset this way is quadratic in the size
    // of the stpathset!
    for (auto const& p : pathset)
    {
        if (p == path)
            return;
    }
    pathset.push_back (path);
}

void pathfinder::addlink (
    const stpath& currentpath,      // the path to build from
    stpathset& incompletepaths,     // the set of partial paths we add to
    int addflags)
{
    auto const& pathend = currentpath.empty() ? msource : currentpath.back ();
    auto const& uendcurrency = pathend.getcurrency ();
    auto const& uendissuer = pathend.getissuerid ();
    auto const& uendaccount = pathend.getaccountid ();
    bool const bonxrp = uendcurrency.iszero ();
	bool const bonvbc = isvbc(uendcurrency);

    writelog (lstrace, pathfinder) << "addlink< flags="
                                   << addflags << " onxrp=" << bonxrp;
    writelog (lstrace, pathfinder) << currentpath.getjson (0);

    if (addflags & afadd_accounts)
    {
        // add accounts
        if (bonxrp || bonvbc)
        {
            if (mdstamount.isnative () && !currentpath.empty ())
            { // non-default path to xrp destination
                writelog (lstrace, pathfinder)
                    << "complete path found ax: " << currentpath.getjson(0);
                adduniquepath (mcompletepaths, currentpath);
            }
        }
        else if (!(addflags & afac_last) && assetcurrency () == uendcurrency)
        {
            // ignore asset
        }
        else
        {
            // search for accounts to add
            auto sleend = mledger->getslei(getaccountrootindex (uendaccount));

            if (sleend)
            {
                bool const brequireauth (
                    sleend->getfieldu32 (sfflags) & lsfrequireauth);
                bool const bisendcurrency (
                    uendcurrency == mdstamount.getcurrency ());
                bool const bisnorippleout (
                    isnorippleout (currentpath));
                bool const bdestonly (
                    addflags & afac_last);

                auto& ripplelines (mrlcache->getripplelines (uendaccount));

                accountcandidates candidates;
                candidates.reserve (ripplelines.size ());

                for (auto const& item : ripplelines)
                {
                    auto* rs = dynamic_cast<ripplestate const *> (item.get ());
                    if (!rs)
                    {
                        writelog (lserror, pathfinder)
                                << "couldn't decipher ripplestate";
                        continue;
                    }
                    auto const& acct = rs->getaccountidpeer ();
                    bool const btodestination = acct == mdstaccount;

                    if (bdestonly && !btodestination)
                    {
                        continue;
                    }

                    if ((uendcurrency == rs->getlimit ().getcurrency ()) &&
                        !currentpath.hasseen (acct, uendcurrency, acct))
                    {
                        // path is for correct currency and has not been seen
                        if (rs->getbalance () <= zero
                            && (!rs->getlimitpeer ()
                                || -rs->getbalance () >= rs->getlimitpeer ()
                                || (brequireauth && !rs->getauth ())))
                        {
                            // path has no credit
                        }
                        else if (bisnorippleout && rs->getnoripple ())
                        {
                            // can't leave on this path
                        }
                        else if (btodestination)
                        {
                            // destination is always worth trying
                            if (uendcurrency == mdstamount.getcurrency ())
                            {
                                // this is a complete path
                                if (!currentpath.empty ())
                                {
                                    writelog (lstrace, pathfinder)
                                            << "complete path found ae: "
                                            << currentpath.getjson (0);
                                    adduniquepath
                                            (mcompletepaths, currentpath);
                                }
                            }
                            else if (!bdestonly)
                            {
                                // this is a high-priority candidate
                                candidates.push_back (
                                    {accountcandidate::highpriority, acct});
                            }
                        }
                        else if (acct == msrcaccount)
                        {
                            // going back to the source is bad
                        }
                        else
                        {
                            // save this candidate
                            int out = getpathsout (
                                uendcurrency,
                                acct,
                                bisendcurrency,
                                mdstaccount);
                            if (out)
                                candidates.push_back ({out, acct});
                        }
                    }
                }

                if (!candidates.empty())
                {
                    std::sort (candidates.begin (), candidates.end (),
                        std::bind(compareaccountcandidate,
                                  mledger->getledgerseq (),
                                  std::placeholders::_1,
                                  std::placeholders::_2));

                    int count = candidates.size ();
                    // allow more paths from source
                    if ((count > 10) && (uendaccount != msrcaccount))
                        count = 10;
                    else if (count > 50)
                        count = 50;

                    auto it = candidates.begin();
                    while (count-- != 0)
                    {
                        // add accounts to incompletepaths
                        stpathelement pathelement (
                            stpathelement::typeaccount,
                            it->account,
                            uendcurrency,
                            it->account);
                        incompletepaths.assembleadd (currentpath, pathelement);
                        ++it;
                    }
                }

            }
            else
            {
                writelog (lswarning, pathfinder)
                    << "path ends on non-existent issuer";
            }
        }
    }
    if (addflags & afadd_books)
    {
        // add order books
        if (addflags & afob_xrp)
        {
            // to xrp only
            if (!bonxrp && getapp ().getorderbookdb ().isbooktoxrp (
                    {uendcurrency, uendissuer}))
            {
                stpathelement pathelement(
                    stpathelement::typecurrency,
                    xrpaccount (),
                    xrpcurrency (),
                    xrpaccount ());
                incompletepaths.assembleadd (currentpath, pathelement);
            }
        }
        else
        {
            bool bdestonly = (addflags & afob_last) != 0;
            auto books = getapp ().getorderbookdb ().getbooksbytakerpays(
                {uendcurrency, uendissuer});
            writelog (lstrace, pathfinder)
                << books.size () << " books found from this currency/issuer";

            for (auto const& book : books)
            {
                if ((bdestonly || assetcurrency () != book->getcurrencyout ()) &&
                    !currentpath.hasseen (
                        xrpaccount (),
                        book->getcurrencyout (),
                        book->getissuerout ()) &&
                    !issuematchesorigin (book->book ().out) &&
                    (!bdestonly ||
                     (book->getcurrencyout () == mdstamount.getcurrency ())))
                {
                    stpath newpath (currentpath);
                    
                    bool bvbc = isvbc(book->getcurrencyout());
                    
                    if (book->getcurrencyout().iszero() || bvbc)
                    { // to xrp

                        // add the order book itself
                        newpath.emplace_back (
                            stpathelement::typecurrency,
                            bvbc?vbcaccount ():xrpaccount (),
                            bvbc?vbccurrency ():xrpcurrency (),
                            bvbc?vbcaccount ():xrpaccount ());

                        if (bvbc ?
                            isvbc (mdstamount.getcurrency ()):
                            mdstamount.getcurrency ().iszero ())
                        {
                            // destination is xrp, add account and path is
                            // complete
                            writelog (lstrace, pathfinder)
                                << "complete path found bx: "
                                << currentpath.getjson(0);
                            adduniquepath (mcompletepaths, newpath);
                        }
                        else
                            incompletepaths.push_back (newpath);
                    }
                    else if (!currentpath.hasseen(
                        book->getissuerout (),
                        book->getcurrencyout (),
                        book->getissuerout ()))
                    { // don't want the book if we've already seen the issuer
                        // add the order book itself
                        newpath.emplace_back (
                            stpathelement::typecurrency |
                            stpathelement::typeissuer,
                            xrpaccount (),
                            book->getcurrencyout (),
                            book->getissuerout ());

                        if (book->getissuerout () == mdstaccount &&
                            book->getcurrencyout () == mdstamount.getcurrency())
                        {
                            // with the destination account, this path is
                            // complete
                            writelog (lstrace, pathfinder)
                                << "complete path found ba: "
                                << currentpath.getjson(0);
                            adduniquepath (mcompletepaths, newpath);
                        }
                        else
                        {
                            // add issuer's account, path still incomplete
                            incompletepaths.assembleadd(newpath,
                                stpathelement (stpathelement::typeaccount,
                                               book->getissuerout (),
                                               book->getcurrencyout (),
                                               book->getissuerout ()));
                        }
                    }

                }
            }
        }
    }
}

namespace {

pathfinder::pathtype makepath (char const *string)
{
    pathfinder::pathtype ret;

    while (true)
    {
        switch (*string++)
        {
            case 's': // source
                ret.push_back (pathfinder::nt_source);
                break;

            case 'a': // accounts
                ret.push_back (pathfinder::nt_accounts);
                break;

            case 'b': // books
                ret.push_back (pathfinder::nt_books);
                break;

            case 'x': // xrp book
                ret.push_back (pathfinder::nt_xrp_book);
                break;

            case 'f': // book to final currency
                ret.push_back (pathfinder::nt_dest_book);
                break;

            case 'd':
                // destination (with account, if required and not already
                // present).
                ret.push_back (pathfinder::nt_destination);
                break;

            case 0:
                return ret;
        }
    }
}

void fillpaths (pathfinder::paymenttype type, pathcostlist const& costs)
{
    auto& list = mpathtable[type];
    assert (list.empty());
    for (auto& cost: costs)
        list.push_back ({cost.cost, makepath (cost.path)});
}

} // namespace


// costs:
// 0 = minimum to make some payments possible
// 1 = include trivial paths to make common cases work
// 4 = normal fast search level
// 7 = normal slow search level
// 10 = most agressive

void pathfinder::initpathtable ()
{
    // caution: do not include rules that build default paths

    mpathtable.clear();
    fillpaths (pt_xrp_to_xrp, {});

    fillpaths(
        pt_xrp_to_nonxrp, {
            {1, "sfd"},   // source -> book -> gateway
            {3, "sfad"},  // source -> book -> account -> destination
            {5, "sfaad"}, // source -> book -> account -> account -> destination
            {6, "sbfd"},  // source -> book -> book -> destination
            {8, "sbafd"}, // source -> book -> account -> book -> destination
            {9, "sbfad"}, // source -> book -> book -> account -> destination
            {10, "sbafad"}
        });

    fillpaths(
        pt_nonxrp_to_xrp, {
            {1, "sxd"},       // gateway buys xrp
            {2, "saxd"},      // source -> gateway -> book(xrp) -> dest
            {6, "saaxd"},
            {7, "sbxd"},
            {8, "sabxd"},
            {9, "sabaxd"}
        });

    // non-xrp to non-xrp (same currency)
    fillpaths(
        pt_nonxrp_to_same,  {
            {1, "sad"},     // source -> gateway -> destination
            {1, "sfd"},     // source -> book -> destination
            {4, "safd"},    // source -> gateway -> book -> destination
            {4, "sfad"},
            {5, "saad"},
            {5, "sbfd"},
            {6, "sxfad"},
            {6, "safad"},
            {6, "saxfd"},   // source -> gateway -> book to xrp -> book ->
                            // destination
            {6, "saxfad"},
            {6, "sabfd"},   // source -> gateway -> book -> book -> destination
            {7, "saaad"},
        });

    // non-xrp to non-xrp (different currency)
    fillpaths(
        pt_nonxrp_to_nonxrp, {
            {1, "sfad"},
            {1, "safd"},
            {3, "safad"},
            {4, "sxfd"},
            {5, "saxfd"},
            {5, "sxfad"},
            {5, "sbfd"},
            {6, "saxfad"},
            {6, "sabfd"},
            {7, "saafd"},
            {8, "saafad"},
            {9, "safaad"},
        });
}

} // ripple
