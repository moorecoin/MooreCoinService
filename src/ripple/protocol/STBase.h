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

#ifndef ripple_protocol_stbase_h_included
#define ripple_protocol_stbase_h_included

#include <ripple/protocol/sfield.h>
#include <ripple/protocol/serializer.h>
#include <ostream>
#include <string>
#include <typeinfo>

namespace ripple {

// vfalco todo fix this restriction on copy assignment.
//
// caution: do not create a vector (or similar container) of any object derived
// from stbase. use boost ptr_* containers. the copy assignment operator
// of stbase has semantics that will cause contained types to change
// their names when an object is deleted because copy assignment is used to
// "slide down" the remaining types and this will not copy the field
// name. changing the copy assignment operator to copy the field name breaks the
// use of copy assignment just to copy values, which is used in the transaction
// engine code.

//------------------------------------------------------------------------------

/** a type which can be exported to a well known binary format.

    a stbase:
        - always a field
        - can always go inside an eligible enclosing stbase
            (such as starray)
        - has a field name

    like json, a serializedobject is a basket which has rules
    on what it can hold.

    @note "st" stands for "serialized type."
*/
class stbase
{
public:
    stbase();

    explicit
    stbase (sfield::ref n);

    virtual ~stbase() = default;

    stbase& operator= (const stbase& t);

    bool operator== (const stbase& t) const;
    bool operator!= (const stbase& t) const;

    template <class d>
    d&
    downcast()
    {
        d* ptr = dynamic_cast<d*> (this);
        if (ptr == nullptr)
            throw std::bad_cast();
        return *ptr;
    }

    template <class d>
    d const&
    downcast() const
    {
        d const * ptr = dynamic_cast<d const*> (this);
        if (ptr == nullptr)
            throw std::bad_cast();
        return *ptr;
    }

    virtual
    serializedtypeid
    getstype() const;

    virtual
    std::string
    getfulltext() const;

    virtual
    std::string
    gettext() const;

    virtual
    json::value
    getjson (int /*options*/) const;

    virtual
    void
    add (serializer& s) const;

    virtual
    bool
    isequivalent (stbase const& t) const;

    virtual
    bool
    isdefault() const;

    /** a stbase is a field.
        this sets the name.
    */
    void
    setfname (sfield::ref n);

    sfield::ref
    getfname() const;

    std::unique_ptr<stbase>
    clone() const;

    void
    addfieldid (serializer& s) const;

    static
    std::unique_ptr <stbase>
    deserialize (sfield::ref name);

protected:
    sfield::ptr fname;

private:
    // vfalco todo return std::unique_ptr <stbase>
    virtual
    stbase*
    duplicate() const
    {
        return new stbase (*fname);
    }
};

//------------------------------------------------------------------------------

stbase* new_clone (const stbase& s);

void delete_clone (const stbase* s);

std::ostream& operator<< (std::ostream& out, const stbase& t);

} // ripple

#endif
