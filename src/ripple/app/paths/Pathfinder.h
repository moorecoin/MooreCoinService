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

#ifndef ripple_pathfinder_h
#define ripple_pathfinder_h

#include <ripple/app/book/types.h>
#include <ripple/app/paths/ripplelinecache.h>
#include <ripple/core/loadevent.h>
#include <ripple/protocol/stamount.h>
#include <ripple/protocol/stpathset.h>

namespace ripple {

/** calculates payment paths.

    the @ref ripplecalc determines the quality of the found paths.

    @see ripplecalc
*/
class pathfinder
{
public:
    /** construct a pathfinder with an issuer.*/
    pathfinder (
        ripplelinecache::ref cache,
        account const& srcaccount,
        account const& dstaccount,
        currency const& usrccurrency,
        account const& usrcissuer,
        stamount const& dstamount);

    /** construct a pathfinder without an issuer.*/
    pathfinder (
        ripplelinecache::ref cache,
        account const& srcaccount,
        account const& dstaccount,
        currency const& usrccurrency,
        stamount const& dstamount);

    ~pathfinder();

    static void initpathtable ();

    bool findpaths (int searchlevel);

    /** compute the rankings of the paths. */
    void computepathranks (int maxpaths);

    /* get the best paths, up to maxpaths in number, from mcompletepaths.

       on return, if fullliquiditypath is not empty, then it contains the best
       additional single path which can consume all the liquidity.
    */
    stpathset getbestpaths (
        int maxpaths,
        stpath& fullliquiditypath,
        stpathset& extrapaths,
        account const& srcissuer);

    enum nodetype
    {
        nt_source,     // the source account: with an issuer account, if needed.
        nt_accounts,   // accounts that connect from this source/currency.
        nt_books,      // order books that connect to this currency.
        nt_xrp_book,   // the order book from this currency to xrp.
        nt_dest_book,  // the order book to the destination currency/issuer.
        nt_destination // the destination account only.
    };

    // the pathtype is a list of the nodetypes for a path.
    using pathtype = std::vector <nodetype>;

    // paymenttype represents the types of the source and destination currencies
    // in a path request.
    enum paymenttype
    {
        pt_xrp_to_xrp,
        pt_xrp_to_nonxrp,
        pt_nonxrp_to_xrp,
        pt_nonxrp_to_same,   // destination currency is the same as source.
        pt_nonxrp_to_nonxrp  // destination currency is not the same as source.
        ,
		pt_vbc_to_vbc,
		pt_vbc_to_nonvbc,
		pt_nonvbc_to_vbc,
		pt_nonvbc_to_same,
		pt_nonvbc_to_nonvbc,
    };

    struct pathrank
    {
        std::uint64_t quality;
        std::uint64_t length;
        stamount liquidity;
        int index;
    };

private:
    /*
      call graph of pathfinder methods.

      findpaths:
          addpathsfortype:
              addlinks:
                  addlink:
                      getpathsout
                      issuematchesorigin
                      isnorippleout:
                          isnoripple

      computepathranks:
          ripplecalculate
          getpathliquidity:
              ripplecalculate

      getbestpaths
     */


    // add all paths of one type to mcompletepaths.
    stpathset& addpathsfortype (pathtype const& type);

    bool issuematchesorigin (issue const&);

    int getpathsout (
        currency const& currency,
        account const& account,
        bool isdestcurrency,
        account const& dest);

    void addlink (
        stpath const& currentpath,
        stpathset& incompletepaths,
        int addflags);

    // call addlink() for each path in currentpaths.
    void addlinks (
        stpathset const& currentpaths,
        stpathset& incompletepaths,
        int addflags);

    // compute the liquidity for a path.  return tessuccess if it has has enough
    // liquidity to be worth keeping, otherwise an error.
    ter getpathliquidity (
        stpath const& path,            // in:  the path to check.
        stamount const& mindstamount,  // in:  the minimum output this path must
                                       //      deliver to be worth keeping.
        stamount& amountout,           // out: the actual liquidity on the path.
        uint64_t& qualityout) const;   // out: the returned initial quality

    // does this path end on an account-to-account link whose last account has
    // set the "no ripple" flag on the link?
    bool isnorippleout (stpath const& currentpath);

    // is the "no ripple" flag set from one account to another?
    bool isnoripple (
        account const& fromaccount,
        account const& toaccount,
        currency const& currency);

    void rankpaths (
        int maxpaths,
        stpathset const& paths,
        std::vector <pathrank>& rankedpaths);

    account msrcaccount;
    account mdstaccount;
    stamount mdstamount;
    currency msrccurrency;
    boost::optional<account> msrcissuer;
    stamount msrcamount;
    /** the amount remaining from msrcaccount after the default liquidity has
        been removed. */
    stamount mremainingamount;

    ledger::pointer mledger;
    loadevent::pointer m_loadevent;
    ripplelinecache::pointer mrlcache;

    stpathelement msource;
    stpathset mcompletepaths;
    std::vector<pathrank> mpathranks;
    std::map<pathtype, stpathset> mpaths;

    hash_map<issue, int> mpathsoutcountmap;

    // add ripple paths
    static std::uint32_t const afadd_accounts = 0x001;

    // add order books
    static std::uint32_t const afadd_books = 0x002;

    // add order book to xrp only
    static std::uint32_t const afob_xrp = 0x010;

    // must link to destination currency
    static std::uint32_t const afob_last = 0x040;

    // destination account only
    static std::uint32_t const afac_last = 0x080;
};

} // ripple

#endif
