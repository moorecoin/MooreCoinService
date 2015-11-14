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

#include <beastconfig.h>
#include <ripple/crypto/base58data.h>
#include <algorithm>

namespace ripple {

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

cbase58data::cbase58data ()
    : nversion (1)
{
}

cbase58data::~cbase58data ()
{
    // ensures that any potentially sensitive data is cleared from memory
    std::fill (vchdata.begin(), vchdata.end(), 0);
}

bool cbase58data::setstring (
    std::string const& str,
    unsigned char version,
    base58::alphabet const& alphabet)
{
    blob vchtemp;
    base58::decodewithcheck (str.c_str (), vchtemp, alphabet);

    if (vchtemp.empty () || vchtemp[0] != version)
    {
        vchdata.clear ();
        nversion = 1;
        return false;
    }

    nversion = vchtemp[0];

    vchdata.assign (vchtemp.begin () + 1, vchtemp.end ());

    // ensures that any potentially sensitive data is cleared from memory
    std::fill (vchtemp.begin(), vchtemp.end(), 0);

    return true;
}

std::string cbase58data::tostring () const
{
    blob vch (1, nversion);

    vch.insert (vch.end (), vchdata.begin (), vchdata.end ());

    return base58::encodewithcheck (vch);
}

} // ripple
