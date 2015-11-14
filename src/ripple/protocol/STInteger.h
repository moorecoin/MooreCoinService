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

#ifndef ripple_protocol_stinteger_h_included
#define ripple_protocol_stinteger_h_included

#include <ripple/protocol/stbase.h>

namespace ripple {

template <typename integer>
class stinteger : public stbase
{
public:
    explicit stinteger (integer v) : value_ (v)
    {
    }

    stinteger (sfield::ref n, integer v = 0) : stbase (n), value_ (v)
    {
    }

    static std::unique_ptr<stbase> deserialize (
        serializeriterator& sit, sfield::ref name)
    {
        return std::unique_ptr<stbase> (construct (sit, name));
    }

    serializedtypeid getstype () const
    {
        return sti_uint8;
    }

    json::value getjson (int) const;
    std::string gettext () const;

    void add (serializer& s) const
    {
        assert (fname->isbinary ());
        assert (fname->fieldtype == getstype ());
        s.addinteger (value_);
    }

    integer getvalue () const
    {
        return value_;
    }
    void setvalue (integer v)
    {
        value_ = v;
    }

    operator integer () const
    {
        return value_;
    }
    virtual bool isdefault () const
    {
        return value_ == 0;
    }

    bool isequivalent (const stbase& t) const
    {
        const stinteger* v = dynamic_cast<const stinteger*> (&t);
        return v && (value_ == v->value_);
    }

private:
    integer value_;

    stinteger* duplicate () const
    {
        return new stinteger (*this);
    }
    static stinteger* construct (serializeriterator&, sfield::ref f);
};

using stuint8 = stinteger<unsigned char>;
using stuint16 = stinteger<std::uint16_t>;
using stuint32 = stinteger<std::uint32_t>;
using stuint64 = stinteger<std::uint64_t>;

} // ripple

#endif
