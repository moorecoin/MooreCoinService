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

#ifndef ripple_core_quality_h_included
#define ripple_core_quality_h_included

#include <ripple/app/book/amount.h>
#include <ripple/app/book/amounts.h>

#include <cstdint>
#include <ostream>

namespace ripple {
namespace core {

// ripple specific constant used for parsing qualities and other things
#define quality_one         1000000000  // 10e9

/** represents the logical ratio of output currency to input currency.
    internally this is stored using a custom floating point representation,
    as the inverse of the ratio, so that quality will be descending in
    a sequence of actual values that represent qualities.
*/
class quality
{
public:
    // type of the internal representation. higher qualities
    // have lower unsigned integer representations.
    typedef std::uint64_t value_type;

private:
    value_type m_value;

public:
    quality() = default;

    /** create a quality from the integer encoding of an amount */
    explicit
    quality (std::uint64_t value);

    /** create a quality from the ratio of two amounts. */
    explicit
    quality (amounts const& amount);

    /** advances to the next higher quality level. */
    /** @{ */
    quality&
    operator++();

    quality
    operator++ (int);
    /** @} */

    /** advances to the next lower quality level. */
    /** @{ */
    quality&
    operator--();

    quality
    operator-- (int);
    /** @} */

    /** returns the quality as amount. */
    amount
    rate () const
    {
        return amountfromquality (m_value);
    }

    /** returns the scaled amount with in capped.
        math is avoided if the result is exact. the output is clamped
        to prevent money creation.
    */
    amounts
    ceil_in (amounts const& amount, amount const& limit) const;

    /** returns the scaled amount with out capped.
        math is avoided if the result is exact. the input is clamped
        to prevent money creation.
    */
    amounts
    ceil_out (amounts const& amount, amount const& limit) const;

    /** returns `true` if lhs is lower quality than `rhs`.
        lower quality means the taker receives a worse deal.
        higher quality is better for the taker.
    */
    friend
    bool
    operator< (quality const& lhs, quality const& rhs) noexcept
    {
        return lhs.m_value > rhs.m_value;
    }

    friend
    bool
    operator== (quality const& lhs, quality const& rhs) noexcept
    {
        return lhs.m_value == rhs.m_value;
    }

    friend
    bool
    operator!= (quality const& lhs, quality const& rhs) noexcept
    {
        return ! (lhs == rhs);
    }

    friend
    std::ostream&
    operator<< (std::ostream& os, quality const& quality)
    {
        os << quality.m_value;
        return os;
    }
};

/** calculate the quality of a two-hop path given the two hops.
    @param lhs  the first leg of the path: input to intermediate.
    @param rhs  the second leg of the path: intermediate to output.
*/
quality
composed_quality (quality const& lhs, quality const& rhs);

}
}

#endif
