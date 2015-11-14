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

#ifndef ripple_protocol_serializedtypes_h_included
#define ripple_protocol_serializedtypes_h_included

#include <ripple/protocol/stbitstring.h>
#include <ripple/protocol/stinteger.h>
#include <ripple/protocol/stbase.h>
#include <ripple/protocol/rippleaddress.h>

namespace ripple {

class stvector256 : public stbase
{
public:
    stvector256 () = default;
    explicit stvector256 (sfield::ref n)
        : stbase (n)
    { }
    explicit stvector256 (std::vector<uint256> const& vector)
        : mvalue (vector)
    { }

    serializedtypeid getstype () const
    {
        return sti_vector256;
    }
    void add (serializer& s) const;

    static
    std::unique_ptr<stbase>
    deserialize (serializeriterator& sit, sfield::ref name)
    {
        return std::unique_ptr<stbase> (construct (sit, name));
    }

    const std::vector<uint256>&
    peekvalue () const
    {
        return mvalue;
    }

    std::vector<uint256>&
    peekvalue ()
    {
        return mvalue;
    }

    virtual bool isequivalent (const stbase& t) const;
    virtual bool isdefault () const
    {
        return mvalue.empty ();
    }

    std::vector<uint256>::size_type
    size () const
    {
        return mvalue.size ();
    }
    bool empty () const
    {
        return mvalue.empty ();
    }

    std::vector<uint256>::const_reference
    operator[] (std::vector<uint256>::size_type n) const
    {
        return mvalue[n];
    }

    void setvalue (const stvector256& v)
    {
        mvalue = v.mvalue;
    }

    void push_back (uint256 const& v)
    {
        mvalue.push_back (v);
    }

    void sort ()
    {
        std::sort (mvalue.begin (), mvalue.end ());
    }

    json::value getjson (int) const;

    std::vector<uint256>::const_iterator
    begin() const
    {
        return mvalue.begin ();
    }
    std::vector<uint256>::const_iterator
    end() const
    {
        return mvalue.end ();
    }

private:
    std::vector<uint256>    mvalue;

    stvector256* duplicate () const
    {
        return new stvector256 (*this);
    }
    static stvector256* construct (serializeriterator&, sfield::ref);
};

} // ripple

#endif
