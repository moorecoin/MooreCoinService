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

#ifndef ripple_shamapsyncfilter_h
#define ripple_shamapsyncfilter_h

#include <ripple/shamap/shamaptreenode.h>

/** callback for filtering shamap during sync. */
namespace ripple {

class shamapsyncfilter
{
public:
    virtual ~shamapsyncfilter () { }

    // note that the nodedata is overwritten by this call
    virtual void gotnode (bool fromfilter,
                          shamapnodeid const& id,
                          uint256 const& nodehash,
                          blob& nodedata,
                          shamaptreenode::tntype type) = 0;

    virtual bool havenode (shamapnodeid const& id,
                           uint256 const& nodehash,
                           blob& nodedata) = 0;
};

}

#endif
