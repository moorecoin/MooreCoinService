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

#ifndef ripple_ripplelinecache_h
#define ripple_ripplelinecache_h

#include <ripple/app/paths/ripplestate.h>
#include <ripple/basics/hardened_hash.h>
#include <cstddef>
#include <memory>
#include <vector>

namespace ripple {

// used by pathfinder
class ripplelinecache
{
public:
    typedef std::vector <ripplestate::pointer> ripplestatevector;
    typedef std::shared_ptr <ripplelinecache> pointer;
    typedef pointer const& ref;

    explicit ripplelinecache (ledger::ref l);

    ledger::ref getledger () // vfalco todo const?
    {
        return mledger;
    }

    std::vector<ripplestate::pointer> const&
    getripplelines (account const& accountid);

private:
    typedef ripplemutex locktype;
    typedef std::lock_guard <locktype> scopedlocktype;
    locktype mlock;

    ripple::hardened_hash<> hasher_;
    ledger::pointer mledger;

    struct accountkey
    {
        account account_;
        std::size_t hash_value_;

        accountkey (account const& account, std::size_t hash)
            : account_ (account)
            , hash_value_ (hash)
        { }

        accountkey (accountkey const& other) = default;

        accountkey&
        operator=(accountkey const& other) = default;

        bool operator== (accountkey const& lhs) const
        {
            return hash_value_ == lhs.hash_value_ && account_ == lhs.account_;
        }

        std::size_t
        get_hash () const
        {
            return hash_value_;
        }

        struct hash
        {
            std::size_t
            operator () (accountkey const& key) const noexcept
            {
                return key.get_hash ();
            }
        };
    };

    hash_map <accountkey, ripplestatevector, accountkey::hash> mrlmap;
};

} // ripple

#endif
