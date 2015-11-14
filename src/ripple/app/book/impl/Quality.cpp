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
#include <ripple/app/book/quality.h>
#include <cassert>
#include <limits>

namespace ripple {
namespace core {

quality::quality (std::uint64_t value)
    : m_value (value)
{
}

quality::quality (amounts const& amount)
    : m_value (getrate (amount.out, amount.in))
{
}

quality&
quality::operator++()
{
    assert (m_value > 0);
    --m_value;
    return *this;
}

quality
quality::operator++ (int)
{
    quality prev (*this);
    ++*this;
    return prev;
}

quality&
quality::operator--()
{
    assert (m_value < std::numeric_limits<value_type>::max());
    ++m_value;
    return *this;
}

quality
quality::operator-- (int)
{
    quality prev (*this);
    --*this;
    return prev;
}

amounts
quality::ceil_in (amounts const& amount, amount const& limit) const
{
    if (amount.in > limit)
    {
        amounts result (limit, divround (
            limit, rate(), amount.out, true));
        // clamp out
        if (result.out > amount.out)
            result.out = amount.out;
        assert (result.in == limit);
        return result;
    }
    assert (amount.in <= limit);
    return amount;
}

amounts
quality::ceil_out (amounts const& amount, amount const& limit) const
{
    if (amount.out > limit)
    {
        amounts result (mulround (
            limit, rate(), amount.in, true), limit);
        // clamp in
        if (result.in > amount.in)
            result.in = amount.in;
        assert (result.out == limit);
        return result;
    }
    assert (amount.out <= limit);
    return amount;
}

quality
composed_quality (quality const& lhs, quality const& rhs)
{
    amount const lhs_rate (lhs.rate ());
    assert (lhs_rate != zero);

    amount const rhs_rate (rhs.rate ());
    assert (rhs_rate != zero);

    amount const rate (mulround (lhs_rate, rhs_rate, true));

    std::uint64_t const stored_exponent (rate.exponent () + 100);
    std::uint64_t const stored_mantissa (rate.mantissa());

    assert ((stored_exponent > 0) && (stored_exponent <= 255));

    return quality ((stored_exponent << (64 - 8)) | stored_mantissa);
}

}
}
