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
#include <ripple/protocol/staccount.h>

namespace ripple {

std::string staccount::gettext () const
{
    account u;
    rippleaddress a;

    if (!getvalueh160 (u))
        return stblob::gettext ();

    a.setaccountid (u);
    return a.humanaccountid ();
}

staccount*
staccount::construct (serializeriterator& u, sfield::ref name)
{
    return new staccount (name, u.getvl ());
}

staccount::staccount (sfield::ref n, account const& v) : stblob (n)
{
    peekvalue ().insert (peekvalue ().end (), v.begin (), v.end ());
}

bool staccount::isvalueh160 () const
{
    return peekvalue ().size () == (160 / 8);
}

rippleaddress staccount::getvaluenca () const
{
    rippleaddress a;
    account account;

    if (getvalueh160 (account))
        a.setaccountid (account);

    return a;
}

void staccount::setvaluenca (rippleaddress const& nca)
{
    setvalueh160 (nca.getaccountid ());
}

} // ripple
