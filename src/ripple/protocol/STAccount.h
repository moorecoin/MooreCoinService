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

#ifndef ripple_protocol_staccount_h_included
#define ripple_protocol_staccount_h_included

#include <ripple/protocol/rippleaddress.h>
#include <ripple/protocol/stblob.h>
#include <string>

namespace ripple {

class staccount : public stblob
{
public:
    staccount (blob const& v) : stblob (v)
    {
        ;
    }
    staccount (sfield::ref n, blob const& v) : stblob (n, v)
    {
        ;
    }
    staccount (sfield::ref n, account const& v);
    staccount (sfield::ref n) : stblob (n)
    {
        ;
    }
    staccount ()
    {
        ;
    }
    static std::unique_ptr<stbase> deserialize (serializeriterator& sit, sfield::ref name)
    {
        return std::unique_ptr<stbase> (construct (sit, name));
    }

    serializedtypeid getstype () const
    {
        return sti_account;
    }
    std::string gettext () const;

    rippleaddress getvaluenca () const;
    void setvaluenca (rippleaddress const& nca);

    template <typename tag>
    void setvalueh160 (base_uint<160, tag> const& v)
    {
        peekvalue ().clear ();
        peekvalue ().insert (peekvalue ().end (), v.begin (), v.end ());
        assert (peekvalue ().size () == (160 / 8));
    }

    template <typename tag>
    bool getvalueh160 (base_uint<160, tag>& v) const
    {
        auto success = isvalueh160 ();
        if (success)
            memcpy (v.begin (), & (peekvalue ().front ()), (160 / 8));
        return success;
    }

    bool isvalueh160 () const;

private:
    virtual staccount* duplicate () const
    {
        return new staccount (*this);
    }
    static staccount* construct (serializeriterator&, sfield::ref);
};

} // ripple

#endif
