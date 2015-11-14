//------------------------------------------------------------------------------
/*
    this file is part of beast: https://github.com/vinniefalco/beast
    copyright 2014, nikolaos d. bougalis <nikb@bougalis.net>

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

#ifndef beast_utility_tagged_integer_h_included
#define beast_utility_tagged_integer_h_included

#include <beast/hash/hash_append.h>

#include <beast/utility/noexcept.h>

#include <functional>
#include <iostream>
#include <type_traits>
#include <utility>

namespace beast {

/** a type-safe wrap around standard unsigned integral types

    the tag is used to implement type safety, catching mismatched types at
    compile time. multiple instantiations wrapping the same underlying integral
    type are distinct types (distinguished by tag) and will not interoperate.

    a tagged_integer supports all the comparison operators that are available
    for the underlying integral type. it only supports a subset of arithmetic
    operators, restricting mutation to only safe and meaningful types.
*/
template <class int, class tag>
class tagged_integer
{
private:
    friend struct is_contiguously_hashable<tagged_integer <int, tag>>;

    static_assert (std::is_unsigned <int>::value,
        "the specified int type must be unsigned");

    int m_value;

public:
    typedef int value_type;
    typedef tag tag_type;

    tagged_integer() = default;

    template <
        class otherint,
        class = typename std::enable_if <
            std::is_integral <otherint>::value &&
            sizeof (otherint) <= sizeof (int)
        >::type
    >
    explicit
    /* constexpr */
    tagged_integer (otherint value) noexcept
        : m_value (value)
    {
    }

    // arithmetic operators
    tagged_integer&
    operator++ () noexcept
    {
        ++m_value;
        return *this;
    }

    tagged_integer
    operator++ (int) noexcept
    {
        tagged_integer orig (*this);
        ++(*this);
        return orig;
    }

    tagged_integer&
    operator-- () noexcept
    {
        --m_value;
        return *this;
    }

    tagged_integer
    operator-- (int) noexcept
    {
        tagged_integer orig (*this);
        --(*this);
        return orig;
    }

    template <class otherint>
    typename std::enable_if <
        std::is_integral <otherint>::value &&
            sizeof (otherint) <= sizeof (int),
    tagged_integer <int, tag>>::type&
    operator+= (otherint rhs) noexcept
    {
        m_value += rhs;
        return *this;
    }

    template <class otherint>
    typename std::enable_if <
        std::is_integral <otherint>::value &&
            sizeof (otherint) <= sizeof (int),
    tagged_integer <int, tag>>::type&
    operator-= (otherint rhs) noexcept
    {
        m_value -= rhs;
        return *this;
    }

    template <class otherint>
    friend
    typename std::enable_if <
        std::is_integral <otherint>::value &&
            sizeof (otherint) <= sizeof (int),
    tagged_integer <int, tag>>::type
    operator+ (tagged_integer const& lhs,
               otherint rhs) noexcept
    {
        return tagged_integer (lhs.m_value + rhs);
    }

    template <class otherint>
    friend
    typename std::enable_if <
        std::is_integral <otherint>::value &&
            sizeof (otherint) <= sizeof (int),
    tagged_integer <int, tag>>::type
    operator+ (otherint lhs,
               tagged_integer const& rhs) noexcept
    {
        return tagged_integer (lhs + rhs.m_value);
    }

    template <class otherint>
    friend
    typename std::enable_if <
        std::is_integral <otherint>::value &&
            sizeof (otherint) <= sizeof (int),
    tagged_integer <int, tag>>::type
    operator- (tagged_integer const& lhs,
               otherint rhs) noexcept
    {
        return tagged_integer (lhs.m_value - rhs);
    }

    friend
    int
    operator- (tagged_integer const& lhs,
               tagged_integer const& rhs) noexcept
    {
        return lhs.m_value - rhs.m_value;
    }

    // comparison operators
    friend
    bool
    operator== (tagged_integer const& lhs,
                tagged_integer const& rhs) noexcept
    {
        return lhs.m_value == rhs.m_value;
    }

    friend
    bool
    operator!= (tagged_integer const& lhs,
                tagged_integer const& rhs) noexcept
    {
        return lhs.m_value != rhs.m_value;
    }

    friend
    bool
    operator< (tagged_integer const& lhs,
               tagged_integer const& rhs) noexcept
    {
        return lhs.m_value < rhs.m_value;
    }

    friend
    bool
    operator<= (tagged_integer const& lhs,
                tagged_integer const& rhs) noexcept
    {
        return lhs.m_value <= rhs.m_value;
    }

    friend
    bool
    operator> (tagged_integer const& lhs,
               tagged_integer const& rhs) noexcept
    {
        return lhs.m_value > rhs.m_value;
    }

    friend
    bool
    operator>= (tagged_integer const& lhs,
                tagged_integer const& rhs) noexcept
    {
        return lhs.m_value >= rhs.m_value;
    }

    friend
    std::ostream&
    operator<< (std::ostream& s, tagged_integer const& t)
    {
        s << t.m_value;
        return s;
    }

    friend
    std::istream&
    operator>> (std::istream& s, tagged_integer& t)
    {
        s >> t.m_value;
        return s;
    }
};

template <class int, class tag>
struct is_contiguously_hashable<tagged_integer<int, tag>>
    : public is_contiguously_hashable<int>
{
};

}

#endif

