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
#include <ripple/basics/log.h>
#include <ripple/basics/stringutilities.h>
#include <ripple/protocol/jsonfields.h>
#include <ripple/protocol/stvector256.h>
#include <ripple/protocol/stamount.h>

namespace ripple {

const stamount sazero (noissue(), 0u);
const stamount saone (noissue(), 1u);

//
// stvector256
//

// return a new object from a serializeriterator.
stvector256* stvector256::construct (serializeriterator& u, sfield::ref name)
{
    blob data = u.getvl ();
    blob ::iterator begin = data.begin ();

    std::unique_ptr<stvector256> vec (new stvector256 (name));

    int count = data.size () / (256 / 8);
    vec->mvalue.reserve (count);

    unsigned int    ustart  = 0;

    for (unsigned int i = 0; i != count; i++)
    {
        unsigned int    uend    = ustart + (256 / 8);

        // this next line could be optimized to construct a default uint256 in the vector and then copy into it
        vec->mvalue.push_back (uint256 (blob (begin + ustart, begin + uend)));
        ustart  = uend;
    }

    return vec.release ();
}

void stvector256::add (serializer& s) const
{
    assert (fname->isbinary ());
    assert (fname->fieldtype == sti_vector256);
    s.addvl (mvalue.empty () ? nullptr : mvalue[0].begin (), mvalue.size () * (256 / 8));
}

bool stvector256::isequivalent (const stbase& t) const
{
    const stvector256* v = dynamic_cast<const stvector256*> (&t);
    return v && (mvalue == v->mvalue);
}

json::value stvector256::getjson (int) const
{
    json::value ret (json::arrayvalue);

    for (auto const& ventry : mvalue)
        ret.append (to_string (ventry));

    return ret;
}

} // ripple
