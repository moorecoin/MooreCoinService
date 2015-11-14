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

#ifndef ripple_protocol_stbits_h_included
#define ripple_protocol_stbits_h_included

#include <ripple/protocol/stbase.h>

namespace ripple {

template <std::size_t bits>
class stbitstring : public stbase
{
public:
    typedef base_uint<bits> bitstring;

    stbitstring ()                                    {}
    stbitstring (sfield::ref n) : stbase (n)  {}
    stbitstring (const bitstring& v) : bitstring_ (v) {}

    stbitstring (sfield::ref n, const bitstring& v)
            : stbase (n), bitstring_ (v)
    {
    }

    stbitstring (sfield::ref n, const char* v) : stbase (n)
    {
        bitstring_.sethex (v);
    }

    stbitstring (sfield::ref n, std::string const& v) : stbase (n)
    {
        bitstring_.sethex (v);
    }

    static std::unique_ptr<stbase> deserialize (
        serializeriterator& sit, sfield::ref name)
    {
        return std::unique_ptr<stbase> (construct (sit, name));
    }

    serializedtypeid getstype () const;

    std::string gettext () const
    {
        return to_string (bitstring_);
    }

    bool isequivalent (const stbase& t) const
    {
        const stbitstring* v = dynamic_cast<const stbitstring*> (&t);
        return v && (bitstring_ == v->bitstring_);
    }

    void add (serializer& s) const
    {
        assert (fname->isbinary ());
        assert (fname->fieldtype == getstype());
        s.addbitstring<bits> (bitstring_);
    }

    const bitstring& getvalue () const
    {
        return bitstring_;
    }

    template <typename tag>
    void setvalue (base_uint<bits, tag> const& v)
    {
        bitstring_.copyfrom(v);
    }

    operator bitstring () const
    {
        return bitstring_;
    }

    virtual bool isdefault () const
    {
        return bitstring_ == zero;
    }

private:
    bitstring bitstring_;

    stbitstring* duplicate () const
    {
        return new stbitstring (*this);
    }

    static stbitstring* construct (serializeriterator& u, sfield::ref name)
    {
        return new stbitstring (name, u.getbitstring<bits> ());
    }
};

template <>
inline serializedtypeid stbitstring<128>::getstype () const
{
    return sti_hash128;
}

template <>
inline serializedtypeid stbitstring<160>::getstype () const
{
    return sti_hash160;
}

template <>
inline serializedtypeid stbitstring<256>::getstype () const
{
    return sti_hash256;
}

using sthash128 = stbitstring<128>;
using sthash160 = stbitstring<160>;
using sthash256 = stbitstring<256>;

} // ripple

#endif
