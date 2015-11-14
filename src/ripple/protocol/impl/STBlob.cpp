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
#include <ripple/protocol/stblob.h>
#include <ripple/basics/stringutilities.h>

namespace ripple {

stblob::stblob (serializeriterator& st, sfield::ref name)
    : stbase (name)
{
    value = st.getvl ();
}

std::string stblob::gettext () const
{
    return strhex (value);
}

stblob* stblob::construct (serializeriterator& u, sfield::ref name)
{
    return new stblob (name, u.getvl ());
}

bool stblob::isequivalent (const stbase& t) const
{
    const stblob* v = dynamic_cast<const stblob*> (&t);
    return v && (value == v->value);
}

} // ripple
