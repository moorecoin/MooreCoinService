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
#include <ripple/overlay/message.h>
#include <cstdint>

namespace ripple {

message::message (::google::protobuf::message const& message, int type)
{
    unsigned const messagebytes = message.bytesize ();

    assert (messagebytes != 0);

    mbuffer.resize (kheaderbytes + messagebytes);

    encodeheader (messagebytes, type);

    if (messagebytes != 0)
    {
        message.serializetoarray (&mbuffer [message::kheaderbytes], messagebytes);
    }
}

bool message::operator== (message const& other) const
{
    return mbuffer == other.mbuffer;
}

unsigned message::getlength (std::vector <uint8_t> const& buf)
{
    unsigned result;

    if (buf.size () >= message::kheaderbytes)
    {
        result = buf [0];
        result <<= 8;
        result |= buf [1];
        result <<= 8;
        result |= buf [2];
        result <<= 8;
        result |= buf [3];
    }
    else
    {
        result = 0;
    }

    return result;
}

int message::gettype (std::vector<uint8_t> const& buf)
{
    if (buf.size () < message::kheaderbytes)
        return 0;

    int ret = buf[4];
    ret <<= 8;
    ret |= buf[5];
    return ret;
}

void message::encodeheader (unsigned size, int type)
{
    assert (mbuffer.size () >= message::kheaderbytes);
    mbuffer[0] = static_cast<std::uint8_t> ((size >> 24) & 0xff);
    mbuffer[1] = static_cast<std::uint8_t> ((size >> 16) & 0xff);
    mbuffer[2] = static_cast<std::uint8_t> ((size >> 8) & 0xff);
    mbuffer[3] = static_cast<std::uint8_t> (size & 0xff);
    mbuffer[4] = static_cast<std::uint8_t> ((type >> 8) & 0xff);
    mbuffer[5] = static_cast<std::uint8_t> (type & 0xff);
}

}
