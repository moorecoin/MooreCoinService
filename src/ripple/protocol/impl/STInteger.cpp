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
#include <ripple/protocol/ledgerformats.h>
#include <ripple/protocol/stinteger.h>
#include <ripple/protocol/txformats.h>
#include <ripple/protocol/ter.h>
#include <beast/module/core/text/lexicalcast.h>

namespace ripple {

template <>
serializedtypeid stuint8::getstype () const
{
    return sti_uint8;
}

template <>
stuint8* stuint8::construct (serializeriterator& u, sfield::ref name)
{
    return new stuint8 (name, u.get8 ());
}

template <>
std::string stuint8::gettext () const
{
    if (getfname () == sftransactionresult)
    {
        std::string token, human;

        if (transresultinfo (static_cast<ter> (value_), token, human))
            return human;
    }

    return beast::lexicalcastthrow <std::string> (value_);
}

template <>
json::value stuint8::getjson (int) const
{
    if (getfname () == sftransactionresult)
    {
        std::string token, human;

        if (transresultinfo (static_cast<ter> (value_), token, human))
            return token;
        else
            writelog (lswarning, stbase)
                << "unknown result code in metadata: " << value_;
    }

    return value_;
}

//------------------------------------------------------------------------------

template <>
serializedtypeid stuint16::getstype () const
{
    return sti_uint16;
}

template <>
stuint16* stuint16::construct (serializeriterator& u, sfield::ref name)
{
    return new stuint16 (name, u.get16 ());
}

template <>
std::string stuint16::gettext () const
{
    if (getfname () == sfledgerentrytype)
    {
        auto item = ledgerformats::getinstance ().findbytype (
            static_cast <ledgerentrytype> (value_));

        if (item != nullptr)
            return item->getname ();
    }

    if (getfname () == sftransactiontype)
    {
        txformats::item const* const item =
            txformats::getinstance().findbytype (static_cast <txtype> (value_));

        if (item != nullptr)
            return item->getname ();
    }

    return beast::lexicalcastthrow <std::string> (value_);
}

template <>
json::value stuint16::getjson (int) const
{
    if (getfname () == sfledgerentrytype)
    {
        ledgerformats::item const* const item =
            ledgerformats::getinstance ().findbytype (static_cast <ledgerentrytype> (value_));

        if (item != nullptr)
            return item->getname ();
    }

    if (getfname () == sftransactiontype)
    {
        txformats::item const* const item =
            txformats::getinstance().findbytype (static_cast <txtype> (value_));

        if (item != nullptr)
            return item->getname ();
    }

    return value_;
}

//------------------------------------------------------------------------------

template <>
serializedtypeid stuint32::getstype () const
{
    return sti_uint32;
}

template <>
stuint32* stuint32::construct (serializeriterator& u, sfield::ref name)
{
    return new stuint32 (name, u.get32 ());
}

template <>
std::string stuint32::gettext () const
{
    return beast::lexicalcastthrow <std::string> (value_);
}

template <>
json::value stuint32::getjson (int) const
{
    return value_;
}

template <>
serializedtypeid stuint64::getstype () const
{
    return sti_uint64;
}

template <>
stuint64* stuint64::construct (serializeriterator& u, sfield::ref name)
{
    return new stuint64 (name, u.get64 ());
}

template <>
std::string stuint64::gettext () const
{
    return beast::lexicalcastthrow <std::string> (value_);
}

template <>
json::value stuint64::getjson (int) const
{
    return strhex (value_);
}

} // ripple
