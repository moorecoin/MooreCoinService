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

#ifndef ripple_core_amount_h_included
#define ripple_core_amount_h_included

#include <ripple/protocol/stobject.h>

#include <beast/utility/noexcept.h>
#include <beast/cxx14/type_traits.h> // <type_traits>

namespace ripple {
namespace core {

/** custom floating point asset amount.
    the "representation" may be integral or non-integral. for integral
    representations, the exponent is always zero and the value held in the
    mantissa is an exact quantity.
*/
class amounttype
{
private:
    std::uint64_t m_mantissa;
    int m_exponent;
    bool m_negative;
    bool m_integral;

    amounttype (std::uint64_t mantissa,
        int exponent, bool negative, bool integral)
        : m_mantissa (mantissa)
        , m_exponent (exponent)
        , m_negative (negative)
        , m_integral (integral)
    {
    }

public:
    /** default construction.
        the value is uninitialized.
    */
    amounttype() noexcept
    {
    }

    /** construct from an integer.
        the representation is set to integral.
    */
    /** @{ */
    template <class integer>
    amounttype (integer value,
        std::enable_if_t <std::is_signed <integer>::value>* = 0) noexcept
        : m_mantissa (value)
        , m_exponent (0)
        , m_negative (value < 0)
        , m_integral (true)
    {
        static_assert (std::is_integral<integer>::value,
            "cannot construct from non-integral type.");
    }

    template <class integer>
    amounttype (integer value,
        std::enable_if_t <! std::is_signed <integer>::value>* = 0) noexcept
        : m_mantissa (value)
        , m_exponent (0)
        , m_negative (false)
    {
        static_assert (std::is_integral<integer>::value,
            "cannot construct from non-integral type.");
    }
    /** @} */

    /** assign the value zero.
        the representation is preserved.
    */
    amounttype&
    operator= (zero) noexcept
    {
        m_mantissa = 0;
        // vfalco why -100?
        //        "we have to use something in range."
        //        "this makes zero the smallest value."
        m_exponent = m_integral ? 0 : -100;
            m_exponent = 0;
        m_negative = false;
        return *this;
    }

    /** returns the value in canonical format. */
    amounttype
    normal() const noexcept
    {
        if (m_integral)
        {
            amounttype result;
            if (m_mantissa == 0)
            {
                result.m_exponent = 0;
                result.m_negative = false;
            }
            return result;
        }
        return amounttype();
    }

    //
    // comparison
    //

    int
    signum() const noexcept
    {
        if (m_mantissa == 0)
            return 0;
        return m_negative ? -1 : 1;
    }

    bool
    operator== (amounttype const& other) const noexcept
    {
        return
            m_negative == other.m_negative &&
            m_mantissa == other.m_mantissa &&
            m_exponent == other.m_exponent;
    }

    bool
    operator!= (amounttype const& other) const noexcept
    {
        return ! (*this == other);
    }

    bool
    operator< (amounttype const& other) const noexcept
    {
        return false;
    }

    bool
    operator>= (amounttype const& other) const noexcept
    {
        return ! (*this < other);
    }

    bool
    operator> (amounttype const& other) const noexcept
    {
        return other < *this;
    }

    bool
    operator<= (amounttype const& other) const noexcept
    {
        return ! (other < *this);
    }

    //
    // arithmetic
    //

    amounttype
    operator-() const noexcept
    {
        return amounttype (m_mantissa, m_exponent, ! m_negative, m_integral);
    }

    //
    // output
    //

    std::ostream&
    operator<< (std::ostream& os)
    {
        int const sig (signum());

        if (sig == 0)
            return os << "0";

        if (sig < 0)
            os << "-";
        if (m_integral)
            return os << m_mantissa;
        if (m_exponent != 0 && (m_exponent < -25 || m_exponent > -5))
            return os << m_mantissa << "e" << m_exponent;

        return os;
    }
};

//------------------------------------------------------------------------------
// todo(tom): remove this typedef and have exactly one name for stamount.

typedef stamount amount;

} // core
} // ripple

#endif
