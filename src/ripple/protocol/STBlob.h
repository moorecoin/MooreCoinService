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

#ifndef ripple_protocol_stblob_h_included
#define ripple_protocol_stblob_h_included

#include <ripple/protocol/stbase.h>
#include <memory>

namespace ripple {

// variable length byte string
class stblob : public stbase
{
public:
    stblob (blob const& v) : value (v)
    {
        ;
    }
    stblob (sfield::ref n, blob const& v) : stbase (n), value (v)
    {
        ;
    }
    stblob (sfield::ref n) : stbase (n)
    {
        ;
    }
    stblob (serializeriterator&, sfield::ref name = sfgeneric);
    stblob ()
    {
        ;
    }
    static std::unique_ptr<stbase> deserialize (serializeriterator& sit, sfield::ref name)
    {
        return std::unique_ptr<stbase> (construct (sit, name));
    }

    virtual serializedtypeid getstype () const
    {
        return sti_vl;
    }
    virtual std::string gettext () const;
    void add (serializer& s) const
    {
        assert (fname->isbinary ());
        assert ((fname->fieldtype == sti_vl) ||
            (fname->fieldtype == sti_account));
        s.addvl (value);
    }

    blob const& peekvalue () const
    {
        return value;
    }
    blob& peekvalue ()
    {
        return value;
    }
    blob getvalue () const
    {
        return value;
    }
    void setvalue (blob const& v)
    {
        value = v;
    }

    operator blob () const
    {
        return value;
    }
    virtual bool isequivalent (const stbase& t) const;
    virtual bool isdefault () const
    {
        return value.empty ();
    }

private:
    blob value;

    virtual stblob* duplicate () const
    {
        return new stblob (*this);
    }
    static stblob* construct (serializeriterator&, sfield::ref);
};

} // ripple

#endif
