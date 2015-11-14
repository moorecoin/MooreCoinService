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
#include <ripple/protocol/sotemplate.h>

namespace ripple {

sotemplate::sotemplate ()
{
}

void sotemplate::push_back (soelement const& r)
{
    // ensure there is the enough space in the index mapping
    // table for all possible fields.
    //
    if (mindex.empty ())
    {
        // unmapped indices will be set to -1
        //
        mindex.resize (sfield::getnumfields () + 1, -1);
    }

    // make sure the field's index is in range
    //
    assert (r.e_field.getnum () < mindex.size ());

    // make sure that this field hasn't already been assigned
    //
    assert (getindex (r.e_field) == -1);

    // add the field to the index mapping table
    //
    mindex [r.e_field.getnum ()] = mtypes.size ();

    // append the new element.
    //
    mtypes.push_back (value_type (new soelement (r)));
}

int sotemplate::getindex (sfield::ref f) const
{
    // the mapping table should be large enough for any possible field
    //
    assert (f.getnum () < mindex.size ());

    return mindex[f.getnum ()];
}

} // ripple
