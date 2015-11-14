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

#ifndef beast_container_aged_container_iterator_h_included
#define beast_container_aged_container_iterator_h_included

#include <iterator>
#include <type_traits>

namespace beast {

template <bool, bool, class, class, class, class, class>
class aged_ordered_container;

namespace detail {

// idea for base template argument to prevent having to repeat
// the base class declaration comes from newbiz on ##c++/freenode
//
// if iterator is scary then this iterator will be as well.
template <
    bool is_const,
    class iterator,
    class base =
        std::iterator <
            typename std::iterator_traits <iterator>::iterator_category,
            typename std::conditional <is_const,
                typename iterator::value_type::stashed::value_type const,
                typename iterator::value_type::stashed::value_type>::type,
            typename std::iterator_traits <iterator>::difference_type>
>
class aged_container_iterator
    : public base
{
public:
    typedef typename iterator::value_type::stashed::time_point time_point;

    // could be '= default', but visual studio 2013 chokes on it [aug 2014]
    aged_container_iterator ()
    {
    }

    // copy constructor
    aged_container_iterator (
        aged_container_iterator<is_const, iterator, base>
            const& other) = default;

    // disable constructing a const_iterator from a non-const_iterator.
    // converting between reverse and non-reverse iterators should be explicit.
    template <bool other_is_const, class otheriterator, class otherbase,
        class = typename std::enable_if <
            (other_is_const == false || is_const == true) &&
                std::is_same<iterator, otheriterator>::value == false>::type>
    explicit aged_container_iterator (aged_container_iterator <
        other_is_const, otheriterator, otherbase> const& other)
        : m_iter (other.m_iter)
    {
    }

    // disable constructing a const_iterator from a non-const_iterator.
    template <bool other_is_const, class otherbase,
        class = typename std::enable_if <
            other_is_const == false || is_const == true>::type>
    aged_container_iterator (aged_container_iterator <
        other_is_const, iterator, otherbase> const& other)
        : m_iter (other.m_iter)
    {
    }

    // disable assigning a const_iterator to a non-const iterator
    template <bool other_is_const, class otheriterator, class otherbase>
    auto
    operator= (aged_container_iterator <
        other_is_const, otheriterator, otherbase> const& other) ->
            typename std::enable_if <
                other_is_const == false || is_const == true,
                    aged_container_iterator&>::type
    {
        m_iter = other.m_iter;
        return *this;
    }

    template <bool other_is_const, class otheriterator, class otherbase>
    bool operator== (aged_container_iterator <
        other_is_const, otheriterator, otherbase> const& other) const
    {
        return m_iter == other.m_iter;
    }

    template <bool other_is_const, class otheriterator, class otherbase>
    bool operator!= (aged_container_iterator <
        other_is_const, otheriterator, otherbase> const& other) const
    {
        return m_iter != other.m_iter;
    }

    aged_container_iterator& operator++ ()
    {
        ++m_iter;
        return *this;
    }

    aged_container_iterator operator++ (int)
    {
        aged_container_iterator const prev (*this);
        ++m_iter;
        return prev;
    }

    aged_container_iterator& operator-- ()
    {
        --m_iter;
        return *this;
    }

    aged_container_iterator operator-- (int)
    {
        aged_container_iterator const prev (*this);
        --m_iter;
        return prev;
    }

    typename base::reference operator* () const
    {
        return m_iter->value;
    }

    typename base::pointer operator-> () const
    {
        return &m_iter->value;
    }

    time_point const& when () const
    {
        return m_iter->when;
    }

private:
    template <bool, bool, class, class, class, class, class>
    friend class aged_ordered_container;

    template <bool, bool, class, class, class, class, class, class>
    friend class aged_unordered_container;

    template <bool, class, class>
    friend class aged_container_iterator;

    template <class otheriterator>
    aged_container_iterator (otheriterator const& iter)
        : m_iter (iter)
    {
    }

    iterator const& iterator() const
    {
        return m_iter;
    }

    iterator m_iter;
};

}

}

#endif
