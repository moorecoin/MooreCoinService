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

memoryblock::memoryblock() noexcept
    : size (0)
{
}

memoryblock::memoryblock (const size_t initialsize, const bool initialisetozero)
{
    if (initialsize > 0)
    {
        size = initialsize;
        data.allocate (initialsize, initialisetozero);
    }
    else
    {
        size = 0;
    }
}

memoryblock::memoryblock (const memoryblock& other)
    : size (other.size)
{
    if (size > 0)
    {
        bassert (other.data != nullptr);
        data.malloc (size);
        memcpy (data, other.data, size);
    }
}

memoryblock::memoryblock (const void* const datatoinitialisefrom, const size_t sizeinbytes)
    : size (sizeinbytes)
{
    bassert (((std::ptrdiff_t) sizeinbytes) >= 0);

    if (size > 0)
    {
        bassert (datatoinitialisefrom != nullptr); // non-zero size, but a zero pointer passed-in?

        data.malloc (size);

        if (datatoinitialisefrom != nullptr)
            memcpy (data, datatoinitialisefrom, size);
    }
}

memoryblock::~memoryblock() noexcept
{
}

memoryblock& memoryblock::operator= (const memoryblock& other)
{
    if (this != &other)
    {
        setsize (other.size, false);
        memcpy (data, other.data, size);
    }

    return *this;
}

#if beast_compiler_supports_move_semantics
memoryblock::memoryblock (memoryblock&& other) noexcept
    : data (static_cast <heapblock <char>&&> (other.data)),
      size (other.size)
{
}

memoryblock& memoryblock::operator= (memoryblock&& other) noexcept
{
    data = static_cast <heapblock <char>&&> (other.data);
    size = other.size;
    return *this;
}
#endif


//==============================================================================
bool memoryblock::operator== (const memoryblock& other) const noexcept
{
    return matches (other.data, other.size);
}

bool memoryblock::operator!= (const memoryblock& other) const noexcept
{
    return ! operator== (other);
}

bool memoryblock::matches (const void* datatocompare, size_t datasize) const noexcept
{
    return size == datasize
            && memcmp (data, datatocompare, size) == 0;
}

//==============================================================================
// this will resize the block to this size
void memoryblock::setsize (const size_t newsize, const bool initialisetozero)
{
    if (size != newsize)
    {
        if (newsize <= 0)
        {
            data.free_up();
            size = 0;
        }
        else
        {
            if (data != nullptr)
            {
                data.reallocate (newsize);

                if (initialisetozero && (newsize > size))
                    zeromem (data + size, newsize - size);
            }
            else
            {
                data.allocate (newsize, initialisetozero);
            }

            size = newsize;
        }
    }
}

void memoryblock::ensuresize (const size_t minimumsize, const bool initialisetozero)
{
    if (size < minimumsize)
        setsize (minimumsize, initialisetozero);
}

void memoryblock::swapwith (memoryblock& other) noexcept
{
    std::swap (size, other.size);
    data.swapwith (other.data);
}

//==============================================================================
void memoryblock::fillwith (const std::uint8_t value) noexcept
{
    memset (data, (int) value, size);
}

void memoryblock::append (const void* const srcdata, const size_t numbytes)
{
    if (numbytes > 0)
    {
        bassert (srcdata != nullptr); // this must not be null!
        const size_t oldsize = size;
        setsize (size + numbytes);
        memcpy (data + oldsize, srcdata, numbytes);
    }
}

void memoryblock::replacewith (const void* const srcdata, const size_t numbytes)
{
    if (numbytes > 0)
    {
        bassert (srcdata != nullptr); // this must not be null!
        setsize (numbytes);
        memcpy (data, srcdata, numbytes);
    }
}

void memoryblock::insert (const void* const srcdata, const size_t numbytes, size_t insertposition)
{
    if (numbytes > 0)
    {
        bassert (srcdata != nullptr); // this must not be null!
        insertposition = std::min (size, insertposition);
        const size_t trailingdatasize = size - insertposition;
        setsize (size + numbytes, false);

        if (trailingdatasize > 0)
            memmove (data + insertposition + numbytes,
                     data + insertposition,
                     trailingdatasize);

        memcpy (data + insertposition, srcdata, numbytes);
    }
}

void memoryblock::removesection (const size_t startbyte, const size_t numbytestoremove)
{
    if (startbyte + numbytestoremove >= size)
    {
        setsize (startbyte);
    }
    else if (numbytestoremove > 0)
    {
        memmove (data + startbyte,
                 data + startbyte + numbytestoremove,
                 size - (startbyte + numbytestoremove));

        setsize (size - numbytestoremove);
    }
}

void memoryblock::copyfrom (const void* const src, int offset, size_t num) noexcept
{
    const char* d = static_cast<const char*> (src);

    if (offset < 0)
    {
        d -= offset;
        num += (size_t) -offset;
        offset = 0;
    }

    if ((size_t) offset + num > size)
        num = size - (size_t) offset;

    if (num > 0)
        memcpy (data + offset, d, num);
}

void memoryblock::copyto (void* const dst, int offset, size_t num) const noexcept
{
    char* d = static_cast<char*> (dst);

    if (offset < 0)
    {
        zeromem (d, (size_t) -offset);
        d -= offset;
        num -= (size_t) -offset;
        offset = 0;
    }

    if ((size_t) offset + num > size)
    {
        const size_t newnum = size - (size_t) offset;
        zeromem (d + newnum, num - newnum);
        num = newnum;
    }

    if (num > 0)
        memcpy (d, data + offset, num);
}

string memoryblock::tostring() const
{
    return string (charpointer_utf8 (data), size);
}

//==============================================================================
int memoryblock::getbitrange (const size_t bitrangestart, size_t numbits) const noexcept
{
    int res = 0;

    size_t byte = bitrangestart >> 3;
    size_t offsetinbyte = bitrangestart & 7;
    size_t bitssofar = 0;

    while (numbits > 0 && (size_t) byte < size)
    {
        const size_t bitsthistime = std::min (numbits, 8 - offsetinbyte);
        const int mask = (0xff >> (8 - bitsthistime)) << offsetinbyte;

        res |= (((data[byte] & mask) >> offsetinbyte) << bitssofar);

        bitssofar += bitsthistime;
        numbits -= bitsthistime;
        ++byte;
        offsetinbyte = 0;
    }

    return res;
}

void memoryblock::setbitrange (const size_t bitrangestart, size_t numbits, int bitstoset) noexcept
{
    size_t byte = bitrangestart >> 3;
    size_t offsetinbyte = bitrangestart & 7;
    std::uint32_t mask = ~((((std::uint32_t) 0xffffffff) << (32 - numbits)) >> (32 - numbits));

    while (numbits > 0 && (size_t) byte < size)
    {
        const size_t bitsthistime = std::min (numbits, 8 - offsetinbyte);

        const std::uint32_t tempmask = (mask << offsetinbyte) | ~((((std::uint32_t) 0xffffffff) >> offsetinbyte) << offsetinbyte);
        const std::uint32_t tempbits = (std::uint32_t) bitstoset << offsetinbyte;

        data[byte] = (char) (((std::uint32_t) data[byte] & tempmask) | tempbits);

        ++byte;
        numbits -= bitsthistime;
        bitstoset >>= bitsthistime;
        mask >>= bitsthistime;
        offsetinbyte = 0;
    }
}

//==============================================================================
void memoryblock::loadfromhexstring (const string& hex)
{
    ensuresize ((size_t) hex.length() >> 1);
    char* dest = data;
    string::charpointertype t (hex.getcharpointer());

    for (;;)
    {
        int byte = 0;

        for (int loop = 2; --loop >= 0;)
        {
            byte <<= 4;

            for (;;)
            {
                const beast_wchar c = t.getandadvance();

                if (c >= '0' && c <= '9') { byte |= c - '0'; break; }
                if (c >= 'a' && c <= 'z') { byte |= c - ('a' - 10); break; }
                if (c >= 'a' && c <= 'z') { byte |= c - ('a' - 10); break; }

                if (c == 0)
                {
                    setsize (static_cast <size_t> (dest - data));
                    return;
                }
            }
        }

        *dest++ = (char) byte;
    }
}

//==============================================================================

static char const* const base64encodingtable = ".abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz0123456789+";

string memoryblock::tobase64encoding() const
{
    const size_t numchars = ((size << 3) + 5) / 6;

    string deststring ((unsigned int) size); // store the length, followed by a '.', and then the data.
    const int initiallen = deststring.length();
    deststring.preallocatebytes (sizeof (string::charpointertype::chartype) * (size_t) initiallen + 2 + numchars);

    string::charpointertype d (deststring.getcharpointer());
    d += initiallen;
    d.write ('.');

    for (size_t i = 0; i < numchars; ++i)
        d.write ((beast_wchar) (std::uint8_t) base64encodingtable [getbitrange (i * 6, 6)]);

    d.writenull();
    return deststring;
}

bool memoryblock::frombase64encoding (const string& s)
{
    const int startpos = s.indexofchar ('.') + 1;

    if (startpos <= 0)
        return false;

    const int numbytesneeded = s.substring (0, startpos - 1).getintvalue();

    setsize ((size_t) numbytesneeded, true);

    const int numchars = s.length() - startpos;

    string::charpointertype srcchars (s.getcharpointer());
    srcchars += startpos;
    int pos = 0;

    for (int i = 0; i < numchars; ++i)
    {
        const char c = (char) srcchars.getandadvance();

        for (int j = 0; j < 64; ++j)
        {
            if (base64encodingtable[j] == c)
            {
                setbitrange ((size_t) pos, 6, j);
                pos += 6;
                break;
            }
        }
    }

    return true;
}

} // beast
