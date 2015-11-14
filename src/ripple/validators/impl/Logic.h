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

#ifndef ripple_validators_logic_h_included
#define ripple_validators_logic_h_included

#include <ripple/protocol/protocol.h>
#include <ripple/basics/hardened_hash.h>
#include <ripple/basics/seconds_clock.h>
#include <ripple/protocol/rippleledgerhash.h>
#include <ripple/validators/impl/store.h>
#include <ripple/validators/impl/tuning.h>
#include <beast/chrono/manual_clock.h>
#include <beast/container/aged_container_utility.h>
#include <beast/container/aged_unordered_map.h>
#include <beast/container/aged_unordered_set.h>
#include <beast/smart_ptr/sharedptr.h>
#include <beast/utility/journal.h>
#include <boost/container/flat_set.hpp>
#include <memory>
#include <mutex>

namespace ripple {
namespace validators {

class connectionimp;

class logic
{
public:
    using clock_type = beast::abstract_clock<std::chrono::steady_clock>;

private:
    struct ledgermeta
    {
        std::uint32_t seq_no = 0;
        std::unordered_set<rippleaddress,
            hardened_hash<>> keys;
    };

    class policy
    {
    public:
        /** returns `true` if we should accept this as the last validated. */
        bool
        acceptledgermeta (std::pair<ledgerhash const, ledgermeta> const& value)
        {
            return value.second.keys.size() >= 3; // quorum
        }
    };

    std::mutex mutex_;
    //store& store_;
    beast::journal journal_;

    policy policy_;
    beast::aged_unordered_map <ledgerhash, ledgermeta,
        std::chrono::steady_clock, hardened_hash<>> ledgers_;
    std::pair<ledgerhash, ledgermeta> latest_; // last fully validated
    boost::container::flat_set<connectionimp*> connections_;
    
    //boost::container::flat_set<

public:
    explicit
    logic (store& store, beast::journal journal);

    beast::journal const&
    journal() const
    {
        return journal_;
    }

    void stop();

    void load();

    void
    add (connectionimp& c);

    void
    remove (connectionimp& c);

    bool
    isstale (stvalidation const& v);

    void
    ontimer();

    void
    onvalidation (stvalidation const& v);

    void
    onledgerclosed (ledgerindex index,
        ledgerhash const& hash, ledgerhash const& parent);
};

}
}

#endif
