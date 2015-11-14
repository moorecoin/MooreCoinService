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
#include <ripple/protocol/stbase.h>
#include <boost/checked_delete.hpp>
#include <cassert>
#include <beast/cxx14/memory.h> // <memory>

namespace ripple {

stbase::stbase()
    : fname(&sfgeneric)
{
}

stbase::stbase (sfield::ref n)
    : fname(&n)
{
    assert(fname);
}

stbase&
stbase::operator= (const stbase& t)
{
    if ((t.fname != fname) && fname->isuseful() && t.fname->isuseful())
    {
        // vfalco we shouldn't be logging at this low a level
        /*
        writelog ((t.getstype () == sti_amount) ? lstrace : lswarning, stbase) // this is common for amounts
                << "caution: " << t.fname->getname () << " not replacing " << fname->getname ();
        */
    }
    if (!fname->isuseful())
        fname = t.fname;
    return *this;
}

bool
stbase::operator== (const stbase& t) const
{
    return (getstype () == t.getstype ()) && isequivalent (t);
}

bool
stbase::operator!= (const stbase& t) const
{
    return (getstype () != t.getstype ()) || !isequivalent (t);
}

serializedtypeid
stbase::getstype() const
{
    return sti_notpresent;
}

std::string
stbase::getfulltext() const
{
    std::string ret;

    if (getstype () != sti_notpresent)
    {
        if (fname->hasname ())
        {
            ret = fname->fieldname;
            ret += " = ";
        }

        ret += gettext ();
    }

    return ret;
}

std::string
stbase::gettext() const
{
    return std::string();
}

json::value
stbase::getjson (int /*options*/) const
{
    return gettext();
}

void
stbase::add (serializer& s) const
{
    // should never be called
    assert(false);
}

bool
stbase::isequivalent (const stbase& t) const
{
    assert (getstype () == sti_notpresent);
    if (t.getstype () == sti_notpresent)
        return true;
    // vfalco we shouldn't be logging at this low a level
    //writelog (lsdebug, stbase) << "notequiv " << getfulltext() << " not sti_notpresent";
    return false;
}

bool
stbase::isdefault() const
{
    return true;
}

void
stbase::setfname (sfield::ref n)
{
    fname = &n;
    assert (fname);
}

sfield::ref
stbase::getfname() const
{
    return *fname;
}

std::unique_ptr<stbase>
stbase::clone() const
{
    return std::unique_ptr<stbase> (duplicate());
}

void
stbase::addfieldid (serializer& s) const
{
    assert (fname->isbinary ());
    s.addfieldid (fname->fieldtype, fname->fieldvalue);
}

std::unique_ptr <stbase>
stbase::deserialize (sfield::ref name)
{
    return std::make_unique<stbase>(name);
}

//------------------------------------------------------------------------------

stbase*
new_clone (const stbase& s)
{
    stbase* const copy (s.clone ().release ());
    assert (typeid (*copy) == typeid (s));
    return copy;
}

void
delete_clone (const stbase* s)
{
    boost::checked_delete (s);
}

std::ostream&
operator<< (std::ostream& out, const stbase& t)
{
    return out << t.getfulltext ();
}

} // ripple
