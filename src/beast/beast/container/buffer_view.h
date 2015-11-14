//------------------------------------------------------------------------------
/*
    this file is part of beast: https://github.com/vinniefalco/beast
    copyright 2013, vinnie falco <vinnie.falco@gmail.com>

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

#ifndef beast_container_buffer_view_h_included
#define beast_container_buffer_view_h_included

#include <beast/config.h>

#include <array>
#include <beast/cxx14/algorithm.h> // <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <string>
#include <vector>
#include <beast/cxx14/type_traits.h> // <type_traits>

namespace beast {

namespace detail {

template <class t, class u,
    bool = std::is_const <std::remove_reference_t <t>>::value>
struct apply_const
{
    typedef u type;
};

template <class t, class u>
struct apply_const <t, u, true>
{
    typedef const u type;
};

// is_contiguous is true if c is a contiguous container
template <class c>
struct is_contiguous
    : public std::false_type
{
};

template <class c>
struct is_contiguous <c const>
    : public is_contiguous <c>
{
};

template <class t, class alloc>
struct is_contiguous <std::vector <t, alloc>>
    : public std::true_type
{
};

template <class chart, class traits, class alloc>
struct is_contiguous <std::basic_string<
    chart, traits, alloc>>
    : public std::true_type
{
};

template <class t, std::size_t n>
struct is_contiguous <std::array<t, n>>
    : public std::true_type
{
};

// true if t is const or u is not const
template <class t, class u>
struct buffer_view_const_compatible : std::integral_constant <bool,
    std::is_const<t>::value || ! std::is_const<u>::value
>
{
};

// true if t and u are the same or differ only in const, or
// if t and u are equally sized integral types.
template <class t, class u>
struct buffer_view_ptr_compatible : std::integral_constant <bool,
    (std::is_same <std::remove_const <t>, std::remove_const <u>>::value) ||
        (std::is_integral <t>::value && std::is_integral <u>::value &&
            sizeof (u) == sizeof (t))
>
{
};

// determine if buffer_view <t, ..> is constructible from u*
template <class t, class u>
struct buffer_view_convertible : std::integral_constant <bool,
    buffer_view_const_compatible <t, u>::value &&
        buffer_view_ptr_compatible <t, u>::value
>
{
};

// true if c is a container that can be used to construct a buffer_view<t>
template <class t, class c>
struct buffer_view_container_compatible : std::integral_constant <bool,
    is_contiguous <c>::value && buffer_view_convertible <t,
        typename apply_const <c, typename c::value_type>::type>::value
>
{
};

} // detail

struct buffer_view_default_tag
{
};

//------------------------------------------------------------------------------

/** a view into a range of contiguous container elements.

    the size of the view is determined at the time of construction.
    this tries to emulate the interface of std::vector as closely as possible,
    with the constraint that the size of the container cannot be changed.

    @tparam t the underlying element type. if t is const, member functions
              which can modify elements are removed from the interface.

    @tparam tag a type used to prevent two views with the same t from being
                comparable or assignable.
*/
template <
    class t,
    class tag = buffer_view_default_tag
>
class buffer_view
{
private:
    t* m_base;
    std::size_t m_size;

    static_assert (std::is_same <t, std::remove_reference_t <t>>::value,
        "t may not be a reference type");

    static_assert (! std::is_same <t, void>::value,
        "t may not be void");

    static_assert (std::is_same <std::add_const_t <t>,
        std::remove_reference_t <t> const>::value,
            "expected std::add_const to produce t const");

    template <class iter>
    void
    assign (iter first, iter last) noexcept
    {
        typedef typename std::iterator_traits <iter>::value_type u;

        static_assert (detail::buffer_view_const_compatible <t, u>::value,
            "cannot convert from 'u const' to 't', "
                "conversion loses const qualifiers");

        static_assert (detail::buffer_view_ptr_compatible <t, u>::value,
            "cannot convert from 'u*' to 't*, "
                "types are incompatible");

        if (first == last)
        {
            m_base = nullptr;
            m_size = 0;
        }
        else
        {
        #if 0
            // fails on gcc
            m_base = reinterpret_cast <t*> (
                std::addressof (*first));
        #else
            m_base = reinterpret_cast <t*> (&*first);
        #endif
            m_size = std::distance (first, last);
        }
    }

public:
    typedef t value_type;
    typedef std::size_t size_type;
    typedef std::ptrdiff_t difference_type;
    typedef t& reference;
    typedef t const& const_reference;
    typedef t* pointer;
    typedef t const* const_pointer;
    typedef t* iterator;
    typedef t const* const_iterator;
    typedef std::reverse_iterator <iterator> reverse_iterator;
    typedef std::reverse_iterator <const_iterator> const_reverse_iterator;

    // default construct
    buffer_view () noexcept
        : m_base (nullptr)
        , m_size (0)
    {
    }

    // copy construct
    template <class u,
        class = std::enable_if_t <
            detail::buffer_view_convertible <t, u>::value>
    >
    buffer_view (buffer_view <u, tag> v) noexcept
    {
        assign (v.begin(), v.end());
    }

    // construct from container
    template <class c,
        class = std::enable_if_t <
            detail::buffer_view_container_compatible <t, c>::value
        >
    >
    buffer_view (c& c) noexcept
    {
        assign (c.begin(), c.end());
    }

    // construct from pointer range
    template <class u,
        class = std::enable_if_t <
            detail::buffer_view_convertible <t, u>::value>
    >
    buffer_view (u* first, u* last) noexcept
    {
        assign (first, last);
    }

    // construct from base and size
    template <class u,
        class = std::enable_if_t <
            detail::buffer_view_convertible <t, u>::value>
    >
    buffer_view (u* u, std::size_t n) noexcept
        : m_base (u)
        , m_size (n)
    {
    }

    // assign from container
    template <class c,
        class = std::enable_if_t <
            detail::buffer_view_container_compatible <t, c>::value
        >
    >
    buffer_view&
    operator= (c& c) noexcept
    {
        assign (c.begin(), c.end());
        return *this;
    }

    //
    // element access
    //

    reference
    at (size_type pos)
    {
        if (! (pos < size()))
            throw std::out_of_range ("bad array index");
        return m_base [pos];
    }

    const_reference
    at (size_type pos) const
    {
        if (! (pos < size()))
            throw std::out_of_range ("bad array index");
        return m_base [pos];
    }

    reference
    operator[] (size_type pos) noexcept
    {
        return m_base [pos];
    }

    const_reference
    operator[] (size_type pos) const noexcept
    {
        return m_base [pos];
    }

    reference
    back() noexcept
    {
        return m_base [m_size - 1];
    }

    const_reference
    back() const noexcept
    {
        return m_base [m_size - 1];
    }

    reference
    front() noexcept
    {
        return *m_base;
    }

    const_reference
    front() const noexcept
    {
        return *m_base;
    }

    pointer
    data() noexcept
    {
        return m_base;
    }

    const_pointer
    data() const noexcept
    {
        return m_base;
    }

    //
    // iterators
    //

    iterator
    begin() noexcept
    {
        return m_base;
    }

    const_iterator
    begin() const noexcept
    {
        return m_base;
    }

    const_iterator
    cbegin() const noexcept
    {
        return m_base;
    }

    iterator
    end() noexcept
    {
        return m_base + m_size;
    }

    const_iterator
    end() const noexcept
    {
        return m_base + m_size;
    }

    const_iterator
    cend() const noexcept
    {
        return m_base + m_size;
    }

    reverse_iterator
    rbegin() noexcept
    {
        return reverse_iterator (end());
    }

    const_reverse_iterator
    rbegin() const noexcept
    {
        return const_reverse_iterator (cend());
    }

    const_reverse_iterator
    crbegin() const noexcept
    {
        return const_reverse_iterator (cend());
    }

    reverse_iterator
    rend() noexcept
    {
        return reverse_iterator (begin());
    }

    const_reverse_iterator
    rend() const noexcept
    {
        return const_reverse_iterator (cbegin());
    }

    const_reverse_iterator
    crend() const noexcept
    {
        return const_reverse_iterator (cbegin());
    }

    //
    // capacity
    //

    bool
    empty() const noexcept
    {
        return m_size == 0;
    }

    size_type
    size() const noexcept
    {
        return m_size;
    }

    size_type
    max_size() const noexcept
    {
        return size();
    }

    size_type
    capacity() const noexcept
    {
        return size();
    }

    //
    // modifiers
    //

    template <class u, class k>
    friend void swap (buffer_view <u, k>& lhs,
        buffer_view <u, k>& rhs) noexcept;
};

//------------------------------------------------------------------------------

template <class t, class tag>
inline
bool
operator== (buffer_view <t, tag> lhs, buffer_view <t, tag> rhs)
{
    return std::equal (
        lhs.cbegin(), lhs.cend(), rhs.cbegin(), rhs.cend());
}

template <class t, class tag>
inline
bool
operator!= (buffer_view <t, tag> lhs, buffer_view <t, tag> rhs)
{
    return ! (lhs == rhs);
}

template <class t, class tag>
inline
bool
operator< (buffer_view <t, tag> lhs, buffer_view <t, tag> rhs)
{
    return std::lexicographical_compare (
        lhs.cbegin(), lhs.cend(), rhs.cbegin(), rhs.cend());
}

template <class t, class tag>
inline
bool
operator>= (buffer_view <t, tag> lhs, buffer_view <t, tag> rhs)
{
    return ! (lhs < rhs);
}

template <class t, class tag>
inline
bool
operator> (buffer_view <t, tag> lhs, buffer_view <t, tag> rhs)
{
    return rhs < lhs;
}

template <class t, class tag>
inline
bool
operator<= (buffer_view <t, tag> lhs, buffer_view <t, tag> rhs)
{
    return ! (rhs < lhs);
}

template <class t, class tag>
inline
void
swap (buffer_view <t, tag>& lhs, buffer_view <t, tag>& rhs) noexcept
{
    std::swap (lhs.m_base, rhs.m_base);
    std::swap (lhs.m_size, rhs.m_size);
}

//------------------------------------------------------------------------------

template <
    class t,
    class tag = buffer_view_default_tag
>
using const_buffer_view = buffer_view <
    std::add_const_t <t>, tag>;

}

#endif
