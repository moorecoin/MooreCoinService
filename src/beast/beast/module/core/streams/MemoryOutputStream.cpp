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

memoryoutputstream::memoryoutputstream (const size_t initialsize)
  : blocktouse (&internalblock), externaldata (nullptr),
    position (0), size (0), availablesize (0)
{
    internalblock.setsize (initialsize, false);
}

memoryoutputstream::memoryoutputstream (memoryblock& memoryblocktowriteto,
                                        const bool appendtoexistingblockcontent)
  : blocktouse (&memoryblocktowriteto), externaldata (nullptr),
    position (0), size (0), availablesize (0)
{
    if (appendtoexistingblockcontent)
        position = size = memoryblocktowriteto.getsize();
}

memoryoutputstream::memoryoutputstream (void* destbuffer, size_t destbuffersize)
  : blocktouse (nullptr), externaldata (destbuffer),
    position (0), size (0), availablesize (destbuffersize)
{
    bassert (externaldata != nullptr); // this must be a valid pointer.
}

memoryoutputstream::~memoryoutputstream()
{
    trimexternalblocksize();
}

void memoryoutputstream::flush()
{
    trimexternalblocksize();
}

void memoryoutputstream::trimexternalblocksize()
{
    if (blocktouse != &internalblock && blocktouse != nullptr)
        blocktouse->setsize (size, false);
}

void memoryoutputstream::preallocate (const size_t bytestopreallocate)
{
    if (blocktouse != nullptr)
        blocktouse->ensuresize (bytestopreallocate + 1);
}

void memoryoutputstream::reset() noexcept
{
    position = 0;
    size = 0;
}

char* memoryoutputstream::preparetowrite (size_t numbytes)
{
    bassert ((std::ptrdiff_t) numbytes >= 0);
    size_t storageneeded = position + numbytes;

    char* data;

    if (blocktouse != nullptr)
    {
        if (storageneeded >= blocktouse->getsize())
            blocktouse->ensuresize ((storageneeded + std::min (storageneeded / 2, (size_t) (1024 * 1024)) + 32) & ~31u);

        data = static_cast <char*> (blocktouse->getdata());
    }
    else
    {
        if (storageneeded > availablesize)
            return nullptr;

        data = static_cast <char*> (externaldata);
    }

    char* const writepointer = data + position;
    position += numbytes;
    size = std::max (size, position);
    return writepointer;
}

bool memoryoutputstream::write (const void* const buffer, size_t howmany)
{
    bassert (buffer != nullptr);

    if (howmany == 0)
        return true;

    if (char* dest = preparetowrite (howmany))
    {
        memcpy (dest, buffer, howmany);
        return true;
    }

    return false;
}

bool memoryoutputstream::writerepeatedbyte (std::uint8_t byte, size_t howmany)
{
    if (howmany == 0)
        return true;

    if (char* dest = preparetowrite (howmany))
    {
        memset (dest, byte, howmany);
        return true;
    }

    return false;
}

bool memoryoutputstream::appendutf8char (beast_wchar c)
{
    if (char* dest = preparetowrite (charpointer_utf8::getbytesrequiredfor (c)))
    {
        charpointer_utf8 (dest).write (c);
        return true;
    }

    return false;
}

memoryblock memoryoutputstream::getmemoryblock() const
{
    return memoryblock (getdata(), getdatasize());
}

const void* memoryoutputstream::getdata() const noexcept
{
    if (blocktouse == nullptr)
        return externaldata;

    if (blocktouse->getsize() > size)
        static_cast <char*> (blocktouse->getdata()) [size] = 0;

    return blocktouse->getdata();
}

bool memoryoutputstream::setposition (std::int64_t newposition)
{
    if (newposition <= (std::int64_t) size)
    {
        // ok to seek backwards
        position = blimit ((size_t) 0, size, (size_t) newposition);
        return true;
    }

    // can't move beyond the end of the stream..
    return false;
}

int memoryoutputstream::writefrominputstream (inputstream& source, std::int64_t maxnumbytestowrite)
{
    // before writing from an input, see if we can preallocate to make it more efficient..
    std::int64_t availabledata = source.gettotallength() - source.getposition();

    if (availabledata > 0)
    {
        if (maxnumbytestowrite > availabledata)
            maxnumbytestowrite = availabledata;

        if (blocktouse != nullptr)
            preallocate (blocktouse->getsize() + (size_t) maxnumbytestowrite);
    }

    return outputstream::writefrominputstream (source, maxnumbytestowrite);
}

string memoryoutputstream::toutf8() const
{
    const char* const d = static_cast <const char*> (getdata());
    return string (charpointer_utf8 (d), charpointer_utf8 (d + getdatasize()));
}

string memoryoutputstream::tostring() const
{
    return string::createstringfromdata (getdata(), (int) getdatasize());
}

outputstream& operator<< (outputstream& stream, const memoryoutputstream& streamtoread)
{
    const size_t datasize = streamtoread.getdatasize();

    if (datasize > 0)
        stream.write (streamtoread.getdata(), datasize);

    return stream;
}

} // beast
