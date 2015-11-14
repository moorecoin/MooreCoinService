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

#ifndef ripple_resource_charge_h_included
#define ripple_resource_charge_h_included

#include <ios>
#include <string>

namespace ripple {
namespace resource {

/** a consumption charge. */
class charge
{
public:
    /** the type used to hold a consumption charge. */
    typedef int value_type;

    // a default constructed charge has no way to get a label.  delete
    charge () = delete;

    /** create a charge with the specified cost and name. */
    charge (value_type cost, std::string const& label = std::string());

    /** return the human readable label associated with the charge. */
    std::string const& label() const;

    /** return the cost of the charge in resource::manager units. */
    value_type cost () const;

    /** converts this charge into a human readable string. */
    std::string to_string () const;

    bool operator== (charge const&) const;
    bool operator!= (charge const&) const;

private:
    value_type m_cost;
    std::string m_label;
};

std::ostream& operator<< (std::ostream& os, charge const& v);

}
}

#endif
