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

#ifndef ripple_canonicaltxset_h
#define ripple_canonicaltxset_h

#include <ripple/protocol/rippleledgerhash.h>
#include <ripple/protocol/sttx.h>

namespace ripple {

/** holds transactions which were deferred to the next pass of consensus.

    "canonical" refers to the order in which transactions are applied.

    - puts transactions from the same account in sequence order

*/
// vfalco todo rename to sortedtxset
class canonicaltxset
{
public:
    class key
    {
    public:
        key (uint256 const& account, std::uint32_t seq, uint256 const& id)
            : maccount (account)
            , mtxid (id)
            , mseq (seq)
        {
        }

        bool operator<  (key const& rhs) const;
        bool operator>  (key const& rhs) const;
        bool operator<= (key const& rhs) const;
        bool operator>= (key const& rhs) const;

        bool operator== (key const& rhs) const
        {
            return mtxid == rhs.mtxid;
        }
        bool operator!= (key const& rhs) const
        {
            return mtxid != rhs.mtxid;
        }

        uint256 const& gettxid () const
        {
            return mtxid;
        }

    private:
        uint256 maccount;
        uint256 mtxid;
        std::uint32_t mseq;
    };

    typedef std::map <key, sttx::pointer>::iterator iterator;
    typedef std::map <key, sttx::pointer>::const_iterator const_iterator;

public:
    explicit canonicaltxset (ledgerhash const& lastclosedledgerhash)
        : msethash (lastclosedledgerhash)
    {
    }

    void push_back (sttx::ref txn);

    // vfalco todo remove this function
    void reset (ledgerhash const& newlastclosedledgerhash)
    {
        msethash = newlastclosedledgerhash;

        mmap.clear ();
    }

    iterator erase (iterator const& it);

    iterator begin ()
    {
        return mmap.begin ();
    }
    iterator end ()
    {
        return mmap.end ();
    }
    const_iterator begin ()  const
    {
        return mmap.begin ();
    }
    const_iterator end () const
    {
        return mmap.end ();
    }
    size_t size () const
    {
        return mmap.size ();
    }
    bool empty () const
    {
        return mmap.empty ();
    }

private:
    // used to salt the accounts so people can't mine for low account numbers
    uint256 msethash;

    std::map <key, sttx::pointer> mmap;
};

} // ripple

#endif
