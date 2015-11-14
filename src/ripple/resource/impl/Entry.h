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

#ifndef ripple_resource_entry_h_included
#define ripple_resource_entry_h_included

#include <ripple/basics/decayingsample.h>
#include <ripple/resource/impl/key.h>
#include <ripple/resource/impl/tuning.h>
#include <beast/chrono/abstract_clock.h>
#include <beast/intrusive/list.h>

namespace ripple {
namespace resource {

typedef beast::abstract_clock <std::chrono::steady_clock> clock_type;

// an entry in the table
// vfalco deprecated using boost::intrusive list
struct entry 
    : public beast::list <entry>::node
{
    entry () = delete;

    /**
       @param now construction time of entry.
    */
    explicit entry(clock_type::time_point const now)
        : refcount (0)
        , local_balance (now)
        , remote_balance (0)
        , lastwarningtime (0)
        , whenexpires (0)
    {
    }

    std::string to_string() const
    {
        switch (key->kind)
        {
        case kindinbound:   return key->address.to_string();
        case kindoutbound:  return key->address.to_string();
        case kindadmin:     return std::string ("\"") + key->name + "\"";
        default:
            bassertfalse;
        }

        return "(undefined)";
    }

    // returns `true` if this connection is privileged
    bool admin () const
    {
        return key->kind == kindadmin;
    }

    // balance including remote contributions
    int balance (clock_type::time_point const now)
    {
        return local_balance.value (now) + remote_balance;
    }

    // add a charge and return normalized balance
    // including contributions from imports.
    int add (int charge, clock_type::time_point const now)
    {
        return local_balance.add (charge, now) + remote_balance;
    }

    // back pointer to the map key (bit of a hack here)
    key const* key;

    // number of consumer references
    int refcount;

    // exponentially decaying balance of resource consumption
    decayingsample <decaywindowseconds, clock_type> local_balance;

    // normalized balance contribution from imports
    int remote_balance;

    // time of the last warning
    clock_type::rep lastwarningtime;

    // for inactive entries, time after which this entry will be erased
    clock_type::rep whenexpires;
};

std::ostream& operator<< (std::ostream& os, entry const& v)
{
    os << v.to_string();
    return os;
}

}
}

#endif
