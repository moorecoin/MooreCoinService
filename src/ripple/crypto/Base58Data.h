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

// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2011 the bitcoin developers
// distributed under the mit/x11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.
//
// why base-58 instead of standard base-64 encoding?
// - don't want 0oil characters that look the same in some fonts and
//      could be used to create visually identical looking account numbers.
// - a string with non-alphanumeric characters is not as easily accepted as an account number.
// - e-mail usually won't line-break if there's no punctuation to break at.
// - doubleclicking selects the whole number as one word if it's all alphanumeric.
//
#ifndef ripple_crypto_base58data_h
#define ripple_crypto_base58data_h

#include <ripple/crypto/base58.h>
#include <ripple/basics/base_uint.h>

namespace ripple {

class cbase58data
{
protected:
    // nikb todo: combine nversion into vchdata so that cbase58data becomes
    //            unnecessary and is replaced by a couple of helper functions
    //            that operate on a blob.

    unsigned char nversion;
    blob vchdata;

    cbase58data ();
    ~cbase58data ();
    cbase58data (cbase58data const&) = default;
    cbase58data& operator= (cbase58data const&) = default;
#ifndef _msc_ver
    cbase58data (cbase58data&&) = default;
    cbase58data& operator= (cbase58data&&) = default;
#endif

    void setdata (int version, blob const& vchdatain)
    {
        nversion = version;
        vchdata = vchdatain;
    }

    template <size_t bits, class tag>
    void setdata (int version, base_uint<bits, tag> const& from)
    {
        nversion = version;
        vchdata.resize (from.size ());
        std::copy (from.begin(), from.end(), vchdata.begin());
    }

public:
    bool setstring (std::string const& str, unsigned char version,
        base58::alphabet const& alphabet);

    std::string tostring () const;

    int compare (const cbase58data& b58) const
    {
        if (nversion < b58.nversion)
            return -1;

        if (nversion > b58.nversion)
            return  1;

        if (vchdata < b58.vchdata)
            return -1;

        if (vchdata > b58.vchdata)
            return  1;

        return 0;
    }

    template <class hasher>
    friend
    void
    hash_append(hasher& hasher, cbase58data const& value)
    {
        beast::hash_append(hasher, value.vchdata);
    }
};

inline bool
operator== (cbase58data const& lhs, cbase58data const& rhs)
{
    return lhs.compare (rhs) == 0;
}

inline bool
operator!= (cbase58data const& lhs, cbase58data const& rhs)
{
    return lhs.compare (rhs) != 0;
}

inline bool
operator< (cbase58data const& lhs, cbase58data const& rhs)
{
    return lhs.compare (rhs) <  0;
}

inline bool
operator<= (cbase58data const& lhs, cbase58data const& rhs)
{
    return lhs.compare (rhs) <= 0;
}

inline bool
operator> (cbase58data const& lhs, cbase58data const& rhs)
{
    return lhs.compare (rhs) > 0;
}

inline bool
operator>= (cbase58data const& lhs, cbase58data const& rhs)
{
    return lhs.compare (rhs) >= 0;
}

} // ripple

#endif
