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

#ifndef ripple_protocol_issue_included
#define ripple_protocol_issue_included

#include <cassert>
#include <functional>
#include <type_traits>

#include <ripple/protocol/uinttypes.h>

namespace ripple {

/** a currency issued by an account.

    when byvalue is `false`, this only stores references, and the caller
    is responsible for managing object lifetime.

    @see currency, account, issue, issueref, book
*/
template <bool byvalue>
class issuetype
{
public:
    typedef typename
    std::conditional <byvalue, currency, currency const&>::type
    issuecurrency;

    typedef typename
    std::conditional <byvalue, account, account const&>::type
    issueaccount;

    issuecurrency currency;
    issueaccount account;

    issuetype ()
    {
    }

    issuetype (currency const& c, account const& a)
            : currency (c), account (a)
    {
    }

    template <bool otherbyvalue>
    issuetype (issuetype <otherbyvalue> const& other)
        : currency (other.currency)
        , account (other.account)
    {
    }

    /** assignment. */
    template <bool maybebyvalue = byvalue, bool otherbyvalue>
    std::enable_if_t <maybebyvalue, issuetype&>
    operator= (issuetype <otherbyvalue> const& other)
    {
        currency = other.currency;
        account = other.account;
        return *this;
    }
};

template <bool byvalue>
bool isconsistent(issuetype<byvalue> const& ac)
{
    return isxrp (ac.currency) == isxrp (ac.account)
        || isvbc(ac.currency) == isvbc(ac.account);
}

template <bool byvalue>
std::string to_string (issuetype<byvalue> const& ac)
{
    return to_string(ac.account) + "/" + to_string(ac.currency);
}

template <bool byvalue>
std::ostream& operator<< (
    std::ostream& os, issuetype<byvalue> const& x)
{
    os << to_string (x);
    return os;
}

template <bool byvalue, class hasher>
void hash_append (hasher& h, issuetype<byvalue> const& r)
{
    using beast::hash_append;
    hash_append (h, r.currency, r.account);
}

/** ordered comparison.
    the assets are ordered first by currency and then by account,
    if the currency is not xrp.
*/
template <bool lhsbyvalue, bool rhsbyvalue>
int compare (issuetype <lhsbyvalue> const& lhs,
    issuetype <rhsbyvalue> const& rhs)
{
    int diff = compare (lhs.currency, rhs.currency);
    if (diff != 0)
        return diff;
	if (isnative (lhs.currency))
        return 0;
    return compare (lhs.account, rhs.account);
}

/** equality comparison. */
/** @{ */
template <bool lhsbyvalue, bool rhsbyvalue>
bool operator== (issuetype <lhsbyvalue> const& lhs,
    issuetype <rhsbyvalue> const& rhs)
{
    return compare (lhs, rhs) == 0;
}

template <bool lhsbyvalue, bool rhsbyvalue>
bool operator!= (issuetype <lhsbyvalue> const& lhs,
    issuetype <rhsbyvalue> const& rhs)
{
    return ! (lhs == rhs);
}
/** @} */

/** strict weak ordering. */
/** @{ */
template <bool lhsbyvalue, bool rhsbyvalue>
bool operator< (issuetype <lhsbyvalue> const& lhs,
    issuetype <rhsbyvalue> const& rhs)
{
    return compare (lhs, rhs) < 0;
}

template <bool lhsbyvalue, bool rhsbyvalue>
bool operator> (issuetype <lhsbyvalue> const& lhs,
    issuetype <rhsbyvalue> const& rhs)
{
    return rhs < lhs;
}

template <bool lhsbyvalue, bool rhsbyvalue>
bool operator>= (issuetype <lhsbyvalue> const& lhs,
    issuetype <rhsbyvalue> const& rhs)
{
    return ! (lhs < rhs);
}

template <bool lhsbyvalue, bool rhsbyvalue>
bool operator<= (issuetype <lhsbyvalue> const& lhs,
    issuetype <rhsbyvalue> const& rhs)
{
    return ! (rhs < lhs);
}
/** @} */

//------------------------------------------------------------------------------

typedef issuetype <true> issue;
typedef issuetype <false> issueref;

//------------------------------------------------------------------------------

/** returns an asset specifier that represents xrp. */
inline issue const& xrpissue ()
{
    static issue issue {xrpcurrency(), xrpaccount()};
    return issue;
}

/** returns an asset specifier that represents vbc. */
inline issue const& vbcissue()
{
	static issue issue{ vbccurrency(), vbcaccount() };
	return issue;
}

/** returns an asset specifier that represents no account and currency. */
inline issue const& noissue ()
{
    static issue issue {nocurrency(), noaccount()};
    return issue;
}

}

#endif
