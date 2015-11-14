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

#ifndef ripple_ledger_transactionstatesf_h_included
#define ripple_ledger_transactionstatesf_h_included

#include <ripple/shamap/shamapsyncfilter.h>
#include <cstdint>

namespace ripple {

// this class is only needed on add functions
// sync filter for transactions tree during ledger sync
class transactionstatesf : public shamapsyncfilter
{
public:
    transactionstatesf();

    // note that the nodedata is overwritten by this call
    void gotnode (bool fromfilter,
                  shamapnodeid const& id,
                  uint256 const& nodehash,
                  blob& nodedata,
                  shamaptreenode::tntype);

    bool havenode (shamapnodeid const& id,
                   uint256 const& nodehash,
                   blob& nodedata);
};

} // ripple

#endif
