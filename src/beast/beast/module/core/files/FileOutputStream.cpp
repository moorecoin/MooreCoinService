//------------------------------------------------------------------------------
/*
    this file is part of beast: https://github.com/vinniefalco/beast
    copyright 2013, vinnie falco <vinnie.falco@gmail.com>

    portions of this file are from juce.
    copyright (c) 2013 - raw material software ltd.
    please visit http://www.juce.com

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

#include <algorithm>

namespace beast
{

std::int64_t beast_filesetposition (void* handle, std::int64_t pos);

//==============================================================================
fileoutputstream::fileoutputstream (const file& f, const size_t buffersizetouse)
    : file (f),
      filehandle (nullptr),
      status (result::ok()),
      currentposition (0),
      buffersize (buffersizetouse),
      bytesinbuffer (0),
      buffer (std::max (buffersizetouse, (size_t) 16))
{
    openhandle();
}

fileoutputstream::~fileoutputstream()
{
    flushbuffer();
    flushinternal();
    closehandle();
}

std::int64_t fileoutputstream::getposition()
{
    return currentposition;
}

bool fileoutputstream::setposition (std::int64_t newposition)
{
    if (newposition != currentposition)
    {
        flushbuffer();
        currentposition = beast_filesetposition (filehandle, newposition);
    }

    return newposition == currentposition;
}

bool fileoutputstream::flushbuffer()
{
    bool ok = true;

    if (bytesinbuffer > 0)
    {
        ok = (writeinternal (buffer, bytesinbuffer) == (std::ptrdiff_t) bytesinbuffer);
        bytesinbuffer = 0;
    }

    return ok;
}

void fileoutputstream::flush()
{
    flushbuffer();
    flushinternal();
}

bool fileoutputstream::write (const void* const src, const size_t numbytes)
{
    bassert (src != nullptr && ((std::ptrdiff_t) numbytes) >= 0);

    if (bytesinbuffer + numbytes < buffersize)
    {
        memcpy (buffer + bytesinbuffer, src, numbytes);
        bytesinbuffer += numbytes;
        currentposition += numbytes;
    }
    else
    {
        if (! flushbuffer())
            return false;

        if (numbytes < buffersize)
        {
            memcpy (buffer + bytesinbuffer, src, numbytes);
            bytesinbuffer += numbytes;
            currentposition += numbytes;
        }
        else
        {
            const std::ptrdiff_t byteswritten = writeinternal (src, numbytes);

            if (byteswritten < 0)
                return false;

            currentposition += byteswritten;
            return byteswritten == (std::ptrdiff_t) numbytes;
        }
    }

    return true;
}

bool fileoutputstream::writerepeatedbyte (std::uint8_t byte, size_t numbytes)
{
    bassert (((std::ptrdiff_t) numbytes) >= 0);

    if (bytesinbuffer + numbytes < buffersize)
    {
        memset (buffer + bytesinbuffer, byte, numbytes);
        bytesinbuffer += numbytes;
        currentposition += numbytes;
        return true;
    }

    return outputstream::writerepeatedbyte (byte, numbytes);
}

} // beast
