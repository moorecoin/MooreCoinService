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

#ifndef ripple_protocol_basics
#define ripple_protocol_basics

#include <ripple/basics/unorderedcontainers.h>
#include <ripple/basics/base_uint.h>

namespace ripple {
namespace detail {

class accounttag {};
class currencytag {};
class directorytag {};
class nodeidtag {};

} // detail

/** directory is an index into the directory of offer books.
    the last 64 bits of this are the quality. */
typedef base_uint<256, detail::directorytag> directory;

/** account is a hash representing a specific account. */
typedef base_uint<160, detail::accounttag> account;

/** currency is a hash representing a specific currency. */
typedef base_uint<160, detail::currencytag> currency;

/** nodeid is a 160-bit hash representing one node. */
typedef base_uint<160, detail::nodeidtag> nodeid;

typedef hash_set<currency> currencyset;
typedef hash_set<nodeid> nodeidset;

/** a special account that's used as the "issuer" for xrp. */
account const& xrpaccount();

/** a special account that's used as the "issuer" for vbc. */
account const& vbcaccount();

/** xrp currency. */
currency const& xrpcurrency();

/** vbc currency. */
currency const& vbccurrency();

/** a placeholder for empty accounts. */
account const& noaccount();

/** a placeholder for empty currencies. */
currency const& nocurrency();

/** we deliberately disallow the currency that looks like "xrp" because too
    many people were using it instead of the correct xrp currency. */


currency const& badcurrency();

/** a placeholder for asset currency. */
currency const& assetcurrency();

inline bool isxrp(currency const& c)
{
    return c == zero;
}

inline bool isxrp(account const& c)
{
    return c == zero;
}

inline bool isvbc(currency const& c)
{
	return c == vbccurrency();
}

inline bool isvbc(account const& c)
{
	return c == vbcaccount();
}

inline bool isnative(currency const& c)
{
    return isxrp(c) || isvbc(c);
}

inline bool isnative(account const& c)
{
    return isxrp(c) || isvbc(c);
}

/** returns a human-readable form of the account. */
std::string to_string(account const&);

/** returns "", "xrp", or three letter iso code. */
std::string to_string(currency const& c);

/** tries to convert a string to a currency, returns true on success. */
bool to_currency(currency&, std::string const&);

/** tries to convert a string to a currency, returns nocurrency() on failure. */
currency to_currency(std::string const&);

/** tries to convert a string to an account representing an issuer, returns true
 * on success. */
bool to_issuer(account&, std::string const&);

inline std::ostream& operator<< (std::ostream& os, account const& x)
{
    os << to_string (x);
    return os;
}

inline std::ostream& operator<< (std::ostream& os, currency const& x)
{
    os << to_string (x);
    return os;
}

} // ripple

namespace std {

template <>
struct hash <ripple::account> : ripple::account::hasher
{
};

template <>
struct hash <ripple::currency> : ripple::currency::hasher
{
};

template <>
struct hash <ripple::nodeid> : ripple::nodeid::hasher
{
};

template <>
struct hash <ripple::directory> : ripple::directory::hasher
{
};

} // std

#endif
