//------------------------------------------------------------------------------
/*
    this file is part of rippled: https://github.com/ripple/rippled
    copyright (c) 2014 ripple labs inc.

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

#ifndef ripple_core_amounts_h_included
#define ripple_core_amounts_h_included

#include <ripple/app/book/amount.h>

#include <beast/utility/noexcept.h>

namespace ripple {
namespace core {

struct amounts
{
    amounts() = default;

    amounts (amount const& in_, amount const& out_)
        : in (in_)
        , out (out_)
    {
    }

    /** returns `true` if either quantity is not positive. */
    bool
    empty() const noexcept
    {
        return in <= zero || out <= zero;
    }

    amount in;
    amount out;
};

inline
bool
operator== (amounts const& lhs, amounts const& rhs) noexcept
{
    return lhs.in == rhs.in && lhs.out == rhs.out;
}

inline
bool
operator!= (amounts const& lhs, amounts const& rhs) noexcept
{
    return ! (lhs == rhs);
}

}
}

#endif
