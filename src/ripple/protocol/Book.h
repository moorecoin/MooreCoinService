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

#ifndef ripple_protocol_book_h_included
#define ripple_protocol_book_h_included

#include <ripple/protocol/issue.h>
#include <boost/utility/base_from_member.hpp>

namespace ripple {

/** specifies an order book.
    the order book is a pair of issues called in and out.
    @see issue.
*/
template <bool byvalue>
class booktype
{
public:
    typedef issuetype <byvalue> issue;

    issue in;
    issue out;

    booktype ()
    {
    }

    booktype (issue const& in_, issue const& out_)
        : in (in_)
        , out (out_)
    {
    }

    template <bool otherbyvalue>
    booktype (booktype <otherbyvalue> const& other)
        : in (other.in)
        , out (other.out)
    {
    }

    /** assignment.
        this is only valid when byvalue == `true`
    */
    template <bool otherbyvalue>
    booktype& operator= (booktype <otherbyvalue> const& other)
    {
        in = other.in;
        out = other.out;
        return *this;
    }
};

template <bool byvalue>
bool isconsistent(booktype<byvalue> const& book)
{
    return isconsistent(book.in) && isconsistent (book.out)
            && book.in != book.out;
}

template <bool byvalue>
std::string to_string (booktype<byvalue> const& book)
{
    return to_string(book.in) + "->" + to_string(book.out);
}

template <bool byvalue>
std::ostream& operator<<(std::ostream& os, booktype<byvalue> const& x)
{
    os << to_string (x);
    return os;
}

template <bool byvalue, class hasher>
void hash_append (hasher& h, booktype<byvalue> const& b)
{
    using beast::hash_append;
    hash_append (h, b.in, b.out);
}

template <bool byvalue>
booktype<byvalue> reversed (booktype<byvalue> const& book)
{
    return booktype<byvalue> (book.out, book.in);
}

/** ordered comparison. */
template <bool lhsbyvalue, bool rhsbyvalue>
int compare (booktype <lhsbyvalue> const& lhs,
    booktype <rhsbyvalue> const& rhs)
{
    int const diff (compare (lhs.in, rhs.in));
    if (diff != 0)
        return diff;
    return compare (lhs.out, rhs.out);
}

/** equality comparison. */
/** @{ */
template <bool lhsbyvalue, bool rhsbyvalue>
bool operator== (booktype <lhsbyvalue> const& lhs,
    booktype <rhsbyvalue> const& rhs)
{
    return (lhs.in == rhs.in) &&
           (lhs.out == rhs.out);
}

template <bool lhsbyvalue, bool rhsbyvalue>
bool operator!= (booktype <lhsbyvalue> const& lhs,
    booktype <rhsbyvalue> const& rhs)
{
    return (lhs.in != rhs.in) ||
           (lhs.out != rhs.out);
}
/** @} */

/** strict weak ordering. */
/** @{ */
template <bool lhsbyvalue, bool rhsbyvalue>
bool operator< (booktype <lhsbyvalue> const& lhs,
    booktype <rhsbyvalue> const& rhs)
{
    int const diff (compare (lhs.in, rhs.in));
    if (diff != 0)
        return diff < 0;
    return lhs.out < rhs.out;
}

template <bool lhsbyvalue, bool rhsbyvalue>
bool operator> (booktype <lhsbyvalue> const& lhs,
    booktype <rhsbyvalue> const& rhs)
{
    return rhs < lhs;
}

template <bool lhsbyvalue, bool rhsbyvalue>
bool operator>= (booktype <lhsbyvalue> const& lhs,
    booktype <rhsbyvalue> const& rhs)
{
    return ! (lhs < rhs);
}

template <bool lhsbyvalue, bool rhsbyvalue>
bool operator<= (booktype <lhsbyvalue> const& lhs,
    booktype <rhsbyvalue> const& rhs)
{
    return ! (rhs < lhs);
}
/** @} */

//------------------------------------------------------------------------------

typedef booktype <true> book;
typedef booktype <false> bookref;

}

//------------------------------------------------------------------------------

namespace std {

template <bool byvalue>
struct hash <ripple::issuetype <byvalue>>
    : private boost::base_from_member <std::hash <ripple::currency>, 0>
    , private boost::base_from_member <std::hash <ripple::account>, 1>
{
private:
    typedef boost::base_from_member <
        std::hash <ripple::currency>, 0> currency_hash_type;
    typedef boost::base_from_member <
        std::hash <ripple::account>, 1> issuer_hash_type;

public:
    typedef std::size_t value_type;
    typedef ripple::issuetype <byvalue> argument_type;

    value_type operator() (argument_type const& value) const
    {
        value_type result (currency_hash_type::member (value.currency));
        if (!isnative (value.currency))
            boost::hash_combine (result,
                issuer_hash_type::member (value.account));
        return result;
    }
};

//------------------------------------------------------------------------------

template <bool byvalue>
struct hash <ripple::booktype <byvalue>>
{
private:
    typedef std::hash <ripple::issuetype <byvalue>> hasher;

    hasher m_hasher;

public:
    typedef std::size_t value_type;
    typedef ripple::booktype <byvalue> argument_type;

    value_type operator() (argument_type const& value) const
    {
        value_type result (m_hasher (value.in));
        boost::hash_combine (result, m_hasher (value.out));
        return result;
    }
};

}

//------------------------------------------------------------------------------

namespace boost {

template <bool byvalue>
struct hash <ripple::issuetype <byvalue>>
    : std::hash <ripple::issuetype <byvalue>>
{
    typedef std::hash <ripple::issuetype <byvalue>> base;
    // vfalco note broken in vs2012
    //using base::base; // inherit ctors
};

template <bool byvalue>
struct hash <ripple::booktype <byvalue>>
    : std::hash <ripple::booktype <byvalue>>
{
    typedef std::hash <ripple::booktype <byvalue>> base;
    // vfalco note broken in vs2012
    //using base::base; // inherit ctors
};

}

#endif
