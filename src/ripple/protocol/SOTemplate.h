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

#ifndef ripple_protocol_sotemplate_h_included
#define ripple_protocol_sotemplate_h_included

#include <ripple/protocol/sfield.h>
#include <memory>

namespace ripple {

/** flags for elements in a sotemplate. */
// vfalco note these don't look like bit-flags...
enum soe_flags
{
    soe_invalid  = -1,
    soe_required = 0,   // required
    soe_optional = 1,   // optional, may be present with default value
    soe_default  = 2,   // optional, if present, must not have default value
};

//------------------------------------------------------------------------------

/** an element in a sotemplate. */
class soelement
{
public:
    sfield::ref       e_field;
    soe_flags const   flags;

    soelement (sfield::ref fieldname, soe_flags flags)
        : e_field (fieldname)
        , flags (flags)
    {
    }
};

//------------------------------------------------------------------------------

/** defines the fields and their attributes within a stobject.
    each subclass of serializedobject will provide its own template
    describing the available fields and their metadata attributes.
*/
class sotemplate
{
public:
    typedef std::unique_ptr <soelement const> value_type;
    typedef std::vector <value_type> list_type;

    /** create an empty template.
        after creating the template, call @ref push_back with the
        desired fields.
        @see push_back
    */
    sotemplate ();

    // vfalco note why do we even bother with the 'private' keyword if
    //             this function is present?
    //
    list_type const& peek () const
    {
        return mtypes;
    }

    /** add an element to the template. */
    void push_back (soelement const& r);

    /** retrieve the position of a named field. */
    int getindex (sfield::ref) const;

private:
    list_type mtypes;

    std::vector <int> mindex;       // field num -> index
};

} // ripple

#endif
