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
#include <ripple/app/paths/findpaths.h>
#include <ripple/app/paths/pathfinder.h>

namespace ripple {

class findpaths::impl {
public:
    impl (
        ripplelinecache::ref cache,
        account const& srcaccount,
        account const& dstaccount,
        stamount const& dstamount,
        int searchlevel,
        unsigned int maxpaths)
            : cache_ (cache),
              srcaccount_ (srcaccount),
              dstaccount_ (dstaccount),
              dstamount_ (dstamount),
              searchlevel_ (searchlevel),
              maxpaths_ (maxpaths)
    {
    }


    bool findpathsforissue (
        issue const& issue,
        stpathset& pathsinout,
        stpath& fullliquiditypath)
    {
        if (auto& pathfinder = getpathfinder (issue.currency))
        {
            pathsinout = pathfinder->getbestpaths (
                maxpaths_,  fullliquiditypath, pathsinout, issue.account);
            return true;
        }
        assert (false);
        return false;
    }

private:
    hash_map<currency, std::unique_ptr<pathfinder>> currencymap_;

    ripplelinecache::ref cache_;
    account const srcaccount_;
    account const dstaccount_;
    stamount const dstamount_;
    int const searchlevel_;
    unsigned int const maxpaths_;

    std::unique_ptr<pathfinder> const& getpathfinder (currency const& currency)
    {
        auto i = currencymap_.find (currency);
        if (i != currencymap_.end ())
            return i->second;
        auto pathfinder = std::make_unique<pathfinder> (
            cache_, srcaccount_, dstaccount_, currency, dstamount_);
        if (pathfinder->findpaths (searchlevel_))
            pathfinder->computepathranks (maxpaths_);
        else
            pathfinder.reset ();  // it's a bad request - clear it.
        return currencymap_[currency] = std::move (pathfinder);

        // todo(tom): why doesn't this faster way compile?
        // return currencymap_.insert (i, std::move (pathfinder)).second;
    }
};

findpaths::findpaths (
    ripplelinecache::ref cache,
    account const& srcaccount,
    account const& dstaccount,
    stamount const& dstamount,
    int level,
    unsigned int maxpaths)
        : impl_ (std::make_unique<impl> (
              cache, srcaccount, dstaccount, dstamount, level, maxpaths))
{
}

findpaths::~findpaths() = default;

bool findpaths::findpathsforissue (
    issue const& issue,
    stpathset& pathsinout,
    stpath& fullliquiditypath)
{
    return impl_->findpathsforissue (issue, pathsinout, fullliquiditypath);
}

bool findpathsforoneissuer (
    ripplelinecache::ref cache,
    account const& srcaccount,
    account const& dstaccount,
    issue const& srcissue,
    stamount const& dstamount,
    int searchlevel,
    unsigned int const maxpaths,
    stpathset& pathsinout,
    stpath& fullliquiditypath)
{
    pathfinder pf (
        cache,
        srcaccount,
        dstaccount,
        srcissue.currency,
        srcissue.account,
        dstamount);

    if (!pf.findpaths (searchlevel))
        return false;

    pf.computepathranks (maxpaths);
    pathsinout = pf.getbestpaths(maxpaths, fullliquiditypath, pathsinout, srcissue.account);
    return true;
}

void initializepathfinding ()
{
    pathfinder::initpathtable ();
}

} // ripple
