//------------------------------------------------------------------------------
/*
    this file is part of beast: https://github.com/vinniefalco/beast
    copyright 2013, vinnie falco <vinnie.falco@gmail.com>

    based on work with these copyrights:
        copyright carl philipp reh 2009 - 2013.
        copyright philipp middendorf 2009 - 2013.
        distributed under the boost software license, version 1.0.
        (see accompanying file license_1_0.txt or copy at
        http://www.boost.org/license_1_0.txt)

    original code taken from
        https://github.com/freundlich/fcppt

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

#ifndef beast_cyclic_iterator_h_included
#define beast_cyclic_iterator_h_included

#include <iterator>
#include <boost/iterator/iterator_facade.hpp>

namespace beast {

//
// cyclic_iterator_fwd.hpp
//

template<
        typename containeriterator
>
class cyclic_iterator;

//
// cyclic_iterator_category.hpp
//

namespace detail
{

template<
        typename sourcecategory
>
struct cyclic_iterator_category;

template<>
struct cyclic_iterator_category<
        std::forward_iterator_tag
>
{
        typedef std::forward_iterator_tag type;
};

template<>
struct cyclic_iterator_category<
        std::bidirectional_iterator_tag
>
{
        typedef std::bidirectional_iterator_tag type;
};

template<>
struct cyclic_iterator_category<
        std::random_access_iterator_tag
>
{
        typedef std::bidirectional_iterator_tag type;
};

}

//
// cyclic_iterator_base.hpp
//

namespace detail
{

template<
        typename containeriterator
>
struct cyclic_iterator_base
{
        typedef boost::iterator_facade<
                cyclic_iterator<
                        containeriterator
                >,
                typename std::iterator_traits<
                        containeriterator
                >::value_type,
                typename detail::cyclic_iterator_category<
                        typename std::iterator_traits<
                                containeriterator
                        >::iterator_category
                >::type,
                typename std::iterator_traits<
                        containeriterator
                >::reference
        > type;
};

}

//
// cyclic_iterator_decl.hpp 
//

/**
\brief an iterator adaptor that cycles through a range

\ingroup fcpptmain

\tparam containeriterator the underlying iterator which must be at least a
forward iterator

a cyclic iterator can be useful in cases where you want <code>end()</code> to
become <code>begin()</code> again. for example, imagine a cycling through a
list of items which means if you skip over the last, you will return to the
first one.

this class can only increment or decrement its underlying iterator, random
access is not supported. the iterator category will be at most bidirectional.
it inherits all capabilities from <code>boost::iterator_facade</code> which
means that it will have the usual iterator operations with their semantics.

here is a short example demonstrating its use.

\snippet cyclic_iterator.cpp cyclic_iterator
*/
template<
    typename containeriterator
>
class cyclic_iterator
:
    public detail::cyclic_iterator_base<
        containeriterator
    >::type
{
public:
    /**
    \brief the base type which is a <code>boost::iterator_facade</code>
    */
    typedef typename detail::cyclic_iterator_base<
        containeriterator
    >::type base_type;

    /**
    \brief the underlying iterator type
    */
    typedef containeriterator container_iterator_type;

    /**
    \brief the value type adapted from \a containeriterator
    */
    typedef typename base_type::value_type value_type;

    /**
    \brief the reference type adapted from \a containeriterator
    */
    typedef typename base_type::reference reference;

    /**
    \brief the pointer type adapted from \a containeriterator
    */
    typedef typename base_type::pointer pointer;

    /**
    \brief the difference type adapted from \a containeriterator
    */
    typedef typename base_type::difference_type difference_type;

    /**
    \brief the iterator category, either forward or bidirectional
    */
    typedef typename base_type::iterator_category iterator_category;

    /**
    \brief creates a singular iterator
    */
    cyclic_iterator();

    /**
    \brief copy constructs from another cyclic iterator

    copy constructs from another cyclic iterator \a other. this only works
    if the underlying iterators are convertible.

    \param other the iterator to copy construct from
    */
    template<
        typename otheriterator
    >
    explicit
    cyclic_iterator(
        cyclic_iterator<otheriterator> const &other
    );

    /**
    \brief constructs a new cyclic iterator

    constructs a new cyclic iterator, starting at \a it, inside
    a range from \a begin to \a end.

    \param pos the start of the iterator
    \param begin the beginning of the range
    \param end the end of the range

    \warning the behaviour is undefined if \a pos isn't between \a begin
    and \a end. also, the behaviour is undefined, if \a begin and \a end
    don't form a valid range.
    */
    cyclic_iterator(
        container_iterator_type const &pos,
        container_iterator_type const &begin,
        container_iterator_type const &end
    );

    /**
    \brief assigns from another cyclic iterator

    assigns from another cyclic iterator \a other. this only works if the
    underlying iterators are convertible.

    \param other the iterator to assign from

    \return <code>*this</code>
    */
    template<
        typename otheriterator
    >
    cyclic_iterator<containeriterator> &
    operator=(
        cyclic_iterator<otheriterator> const &other
    );

    /**
    \brief returns the beginning of the range
    */
    container_iterator_type
    begin() const;

    /**
    \brief returns the end of the range
    */
    container_iterator_type
    end() const;

    /**
    \brief returns the underlying iterator
    */
    container_iterator_type
    get() const;
private:
    friend class boost::iterator_core_access;

    void
    increment();

    void
    decrement();

    bool
    equal(
        cyclic_iterator const &
    ) const;

    reference
    dereference() const;

    difference_type
    distance_to(
        cyclic_iterator const &
    ) const;
private:
    container_iterator_type
        it_,
        begin_,
        end_;
};

//
// cyclic_iterator_impl.hpp
//

template<
        typename containeriterator
>
cyclic_iterator<
        containeriterator
>::cyclic_iterator()
:
        it_(),
        begin_(),
        end_()
{
}

template<
        typename containeriterator
>
template<
        typename otheriterator
>
cyclic_iterator<
        containeriterator
>::cyclic_iterator(
        cyclic_iterator<
                otheriterator
        > const &_other
)
:
        it_(
                _other.it_
        ),
        begin_(
                _other.begin_
        ),
        end_(
                _other.end_
        )
{
}

template<
        typename containeriterator
>
cyclic_iterator<
        containeriterator
>::cyclic_iterator(
        container_iterator_type const &_it,
        container_iterator_type const &_begin,
        container_iterator_type const &_end
)
:
        it_(
                _it
        ),
        begin_(
                _begin
        ),
        end_(
                _end
        )
{
}

template<
        typename containeriterator
>
template<
        typename otheriterator
>
cyclic_iterator<
        containeriterator
> &
cyclic_iterator<
        containeriterator
>::operator=(
        cyclic_iterator<
                otheriterator
        > const &_other
)
{
        it_ = _other.it_;

        begin_ = _other.begin_;

        end_ = _other.end_;

        return *this;
}

template<
        typename containeriterator
>
typename cyclic_iterator<
        containeriterator
>::container_iterator_type
cyclic_iterator<
        containeriterator
>::begin() const
{
        return begin_;
}

template<
        typename containeriterator
>
typename cyclic_iterator<
        containeriterator
>::container_iterator_type
cyclic_iterator<
        containeriterator
>::end() const
{
        return end_;
}

template<
        typename containeriterator
>
typename cyclic_iterator<
        containeriterator
>::container_iterator_type
cyclic_iterator<
        containeriterator
>::get() const
{
        return it_;
}

template<
        typename containeriterator
>
void
cyclic_iterator<
        containeriterator
>::increment()
{
        if(
                begin_ != end_
                && ++it_ == end_
        )
                it_ = begin_;
}

template<
        typename containeriterator
>
void
cyclic_iterator<
        containeriterator
>::decrement()
{
        if(
                begin_ == end_
        )
                return;

        if(
                it_ == begin_
        )
                it_ =
                        std::prev(
                                end_
                        );
        else
                --it_;
}

template<
        typename containeriterator
>
bool
cyclic_iterator<
        containeriterator
>::equal(
        cyclic_iterator const &_other
) const
{
        return it_ == _other.it;
}

template<
        typename containeriterator
>
typename cyclic_iterator<
        containeriterator
>::reference
cyclic_iterator<
        containeriterator
>::dereference() const
{
        return *it_;
}

template<
        typename containeriterator
>
typename cyclic_iterator<
        containeriterator
>::difference_type
cyclic_iterator<
        containeriterator
>::distance_to(
        cyclic_iterator const &_other
) const
{
        return _other.it_ - it_;
}

// convenience function for template argument deduction
template <typename containeriterator>
cyclic_iterator <containeriterator> make_cyclic (
    containeriterator const& pos,
    containeriterator const& begin,
    containeriterator const& end);
}

#endif
