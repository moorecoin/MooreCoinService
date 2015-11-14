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

#ifndef ripple_validators_connectionimp_h_included
#define ripple_validators_connectionimp_h_included

#include <ripple/basics/hardened_hash.h>
#include <ripple/protocol/rippleaddress.h>
#include <ripple/validators/connection.h>
#include <ripple/validators/impl/logic.h>
#include <beast/utility/wrappedsink.h>
#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>
#include <boost/optional.hpp>
#include <mutex>
#include <sstream>
#include <utility>
#include <vector>

#include <map>

namespace ripple {
namespace validators {

class connectionimp
    : public connection
{
private:
    // metadata on a validation source
    struct source
    {
        // new sources are just at the threshold
        double score = 0.8;

        // returns `true` if the score is high
        // enough to count as available
        bool
        available() const
        {
            return score >= 0.8;
        }

        // returns `true` if the score is so low we
        // have no expectation of seeing the validator again
        bool
        gone() const
        {
            return score <= 0.2;
        }

        // returns `true` if became unavailable
        bool
        onhit()
        {
            bool const prev = available();
            score = 0.90 * score + 0.10;
            if (! prev && available())
                return true;
            return false;
        }

        // returns `true` if became available
        bool
        onmiss()
        {
            bool const prev = available();
            score = 0.90 * score;
            if (prev && ! available())
                return true;
            return false;
        }
    };

    using item = std::pair<ledgerhash, rippleaddress>;

    logic& logic_;
    beast::wrappedsink sink_;
    beast::journal journal_;
    std::mutex mutex_;
    boost::optional<ledgerhash> ledger_;
    boost::container::flat_set<item> items_;
    boost::container::flat_map<rippleaddress, source> sources_;
    boost::container::flat_set<rippleaddress> good_;

    static
    std::string
    makeprefix (int id)
    {
        std::stringstream ss;
        ss << "[" << std::setfill('0') << std::setw(3) << id << "] ";
        return ss.str();
    }

public:
    template <class clock>
    connectionimp (int id, logic& logic, clock& clock)
        : logic_ (logic)
        , sink_ (logic.journal(), makeprefix(id))
        , journal_ (sink_)
    {
        logic_.add(*this);
    }

    ~connectionimp()
    {
        logic_.remove(*this);
    }

    void
    onvalidation (stvalidation const& v) override
    {
        auto const key = v.getsignerpublic();
        auto const ledger = v.getledgerhash();

        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (! items_.emplace(ledger, key).second)
                return;
            if (journal_.debug) journal_.debug <<
                "onvalidation: " << ledger;
#if 0
            auto const result = sources_.emplace(
                std::piecewise_construct, std::make_tuple(key),
                    std::make_tuple());
#else
            // work-around for boost::container
            auto const result = sources_.emplace(key, source{});
#endif
            if (result.second)
                good_.insert(key);
            // register a hit for slightly late validations 
            if (ledger_ && ledger == ledger_)
                if (result.first->second.onhit())
                    good_.insert(key);
        }

        // this can call onledger, do it last
        logic_.onvalidation(v);
    }

    // called when a supermajority of
    // validations are received for the next ledger.
    void
    onledger (ledgerhash const& ledger)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (journal_.debug) journal_.debug <<
            "onledger: " << ledger;
        assert(ledger != ledger_);
        ledger_ = ledger;
        auto item = items_.lower_bound(
            std::make_pair(ledger, rippleaddress()));
        auto source = sources_.begin();
        while (item != items_.end() &&
            source != sources_.end() &&
                item->first == ledger)
        {
            if (item->second < source->first)
            {
                ++item;
            }
            else if (item->second == source->first)
            {
                if (source->second.onhit())
                    good_.insert(source->first);
                ++item;
                ++source;
            }
            else
            {
                if (source->second.onmiss())
                    good_.erase(source->first);
                ++source;
            }
        }
        while (source != sources_.end())
        {
            if (source->second.onmiss())
                good_.erase(source->first);
            ++source;
        }
        // vfalco what if there are validations
        // for the ledger after this one in the map?
        items_.clear();
    }
};

}
}

#endif
