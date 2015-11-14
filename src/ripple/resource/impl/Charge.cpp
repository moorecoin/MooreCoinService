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
#include <ripple/resource/charge.h>
#include <sstream>

namespace ripple {
namespace resource {

charge::charge (value_type cost, std::string const& label)
    : m_cost (cost)
    , m_label (label)
{
}

std::string const& charge::label () const
{
    return m_label;
}

charge::value_type charge::cost() const
{
    return m_cost;
}

std::string charge::to_string () const
{
    std::stringstream ss;
    ss << m_label << " ($" << m_cost << ")";
    return ss.str();
}

std::ostream& operator<< (std::ostream& os, charge const& v)
{
    os << v.to_string();
    return os;
}

bool charge::operator== (charge const& c) const
{
    return c.m_cost == m_cost;
}

bool charge::operator!= (charge const& c) const
{
    return c.m_cost != m_cost;
}

}
}
