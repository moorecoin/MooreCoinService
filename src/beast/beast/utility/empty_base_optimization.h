//------------------------------------------------------------------------------
/*
    this file is part of beast: https://github.com/vinniefalco/beast
    copyright 2014, howard hinnant <howard.hinnant@gmail.com>,
        vinnie falco <vinnie.falco@gmail.com>

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

#ifndef beast_utility_empty_base_optimization_h_included
#define beast_utility_empty_base_optimization_h_included

#include <beast/utility/noexcept.h>
#include <type_traits>
#include <utility>

namespace beast {

namespace detail {

template <class t>
struct empty_base_optimization_decide
    : std::integral_constant <bool,
        std::is_empty <t>::value
#ifdef __clang__
        && !__is_final(t)
#endif
    >
{
};

}

//------------------------------------------------------------------------------

template <
    class t,
    int uniqueid = 0,
    bool shouldderivefrom =
        detail::empty_base_optimization_decide <t>::value
>
class empty_base_optimization : private t
{
public:
    empty_base_optimization() = default;

    empty_base_optimization(t const& t)
        : t (t)
    {}

    empty_base_optimization(t&& t)
        : t (std::move (t))
    {}

    t& member() noexcept
    {
        return *this;
    }

    t const& member() const noexcept
    {
        return *this;
    }
};

//------------------------------------------------------------------------------

template <
    class t,
    int uniqueid
>
class empty_base_optimization <t, uniqueid, false>
{
public:
    empty_base_optimization() = default;

    empty_base_optimization(t const& t)
        : m_t (t)
    {}

    empty_base_optimization(t&& t)
        : m_t (std::move (t))
    {}

    t& member() noexcept
    {
        return m_t;
    }

    t const& member() const noexcept
    {
        return m_t;
    }

private:
    t m_t;
};

}

#endif
