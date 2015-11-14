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

#ifndef rippled_ripple_app_paths_findpaths_h
#define rippled_ripple_app_paths_findpaths_h

#include <ripple/app/paths/ripplelinecache.h>

namespace ripple {

class findpaths
{
public:
    findpaths (
        ripplelinecache::ref cache,
        account const& srcaccount,
        account const& dstaccount,
        stamount const& dstamount,
        /** searchlevel is the maximum search level allowed in an output path.
         */
        int searchlevel,
        /** maxpaths is the maximum number of paths that can be returned in
            pathsout. */
        unsigned int const maxpaths);
    ~findpaths();

    bool findpathsforissue (
        issue const& issue,

        /** on input, pathsinout contains any paths you want to ensure are
            included if still good.

            on output, pathsinout will have any additional paths found. only
            non-default paths without source or destination will be added. */
        stpathset& pathsinout,

        /** on input, fullliquiditypath must be an empty stpath.

            on output, if fullliquiditypath is non-empty, it contains one extra
            path that can move the entire liquidity requested. */
        stpath& fullliquiditypath);

private:
    class impl;
    std::unique_ptr<impl> impl_;
};

bool findpathsforoneissuer (
    ripplelinecache::ref cache,
    account const& srcaccount,
    account const& dstaccount,
    issue const& srcissue,
    stamount const& dstamount,

    /** searchlevel is the maximum search level allowed in an output path. */
    int searchlevel,

    /** maxpaths is the maximum number of paths that can be returned in
        pathsout. */
    unsigned int const maxpaths,

    /** on input, pathsinout contains any paths you want to ensure are included if
        still good.

        on output, pathsinout will have any additional paths found. only
        non-default paths without source or destination will be added. */
    stpathset& pathsinout,

    /** on input, fullliquiditypath must be an empty stpath.

        on output, if fullliquiditypath is non-empty, it contains one extra path
        that can move the entire liquidity requested. */
    stpath& fullliquiditypath);

void initializepathfinding ();

} // ripple

#endif
