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
#include <ripple/app/misc/canonicaltxset.h>

namespace ripple {

bool canonicaltxset::key::operator< (key const& rhs) const
{
    if (maccount < rhs.maccount) return true;

    if (maccount > rhs.maccount) return false;

    if (mseq < rhs.mseq) return true;

    if (mseq > rhs.mseq) return false;

    return mtxid < rhs.mtxid;
}

bool canonicaltxset::key::operator> (key const& rhs) const
{
    if (maccount > rhs.maccount) return true;

    if (maccount < rhs.maccount) return false;

    if (mseq > rhs.mseq) return true;

    if (mseq < rhs.mseq) return false;

    return mtxid > rhs.mtxid;
}

bool canonicaltxset::key::operator<= (key const& rhs) const
{
    if (maccount < rhs.maccount) return true;

    if (maccount > rhs.maccount) return false;

    if (mseq < rhs.mseq) return true;

    if (mseq > rhs.mseq) return false;

    return mtxid <= rhs.mtxid;
}

bool canonicaltxset::key::operator>= (key const& rhs)const
{
    if (maccount > rhs.maccount) return true;

    if (maccount < rhs.maccount) return false;

    if (mseq > rhs.mseq) return true;

    if (mseq < rhs.mseq) return false;

    return mtxid >= rhs.mtxid;
}

void canonicaltxset::push_back (sttx::ref txn)
{
    uint256 effectiveaccount = msethash;

    effectiveaccount ^= to256 (txn->getsourceaccount ().getaccountid ());

    mmap.insert (std::make_pair (
                     key (effectiveaccount, txn->getsequence (), txn->gettransactionid ()),
                     txn));
}

canonicaltxset::iterator canonicaltxset::erase (iterator const& it)
{
    iterator tmp = it;
    ++tmp;
    mmap.erase (it);
    return tmp;
}

} // ripple
