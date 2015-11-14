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

namespace beast
{

std::int64_t inputstream::getnumbytesremaining()
{
    std::int64_t len = gettotallength();

    if (len >= 0)
        len -= getposition();

    return len;
}

char inputstream::readbyte()
{
    char temp = 0;
    read (&temp, 1);
    return temp;
}

bool inputstream::readbool()
{
    return readbyte() != 0;
}

short inputstream::readshort()
{
    char temp[2];

    if (read (temp, 2) == 2)
        return (short) byteorder::littleendianshort (temp);

    return 0;
}

short inputstream::readshortbigendian()
{
    char temp[2];

    if (read (temp, 2) == 2)
        return (short) byteorder::bigendianshort (temp);

    return 0;
}

int inputstream::readint()
{
    static_assert (sizeof (int) == 4,
        "the size of an integer must be exactly 4 bytes");

    char temp[4];

    if (read (temp, 4) == 4)
        return (int) byteorder::littleendianint (temp);

    return 0;
}

std::int32_t inputstream::readint32()
{
    char temp[4];

    if (read (temp, 4) == 4)
        return (std::int32_t) byteorder::littleendianint (temp);

    return 0;
}

int inputstream::readintbigendian()
{
    char temp[4];

    if (read (temp, 4) == 4)
        return (int) byteorder::bigendianint (temp);

    return 0;
}

std::int32_t inputstream::readint32bigendian()
{
    char temp[4];

    if (read (temp, 4) == 4)
        return (std::int32_t) byteorder::bigendianint (temp);

    return 0;
}

int inputstream::readcompressedint()
{
    const std::uint8_t sizebyte = (std::uint8_t) readbyte();
    if (sizebyte == 0)
        return 0;

    const int numbytes = (sizebyte & 0x7f);
    if (numbytes > 4)
    {
        bassertfalse;    // trying to read corrupt data - this method must only be used
                       // to read data that was written by outputstream::writecompressedint()
        return 0;
    }

    char bytes[4] = { 0, 0, 0, 0 };
    if (read (bytes, numbytes) != numbytes)
        return 0;

    const int num = (int) byteorder::littleendianint (bytes);
    return (sizebyte >> 7) ? -num : num;
}

std::int64_t inputstream::readint64()
{
    union { std::uint8_t asbytes[8]; std::uint64_t asint64; } n;

    if (read (n.asbytes, 8) == 8)
        return (std::int64_t) byteorder::swapifbigendian (n.asint64);

    return 0;
}

std::int64_t inputstream::readint64bigendian()
{
    union { std::uint8_t asbytes[8]; std::uint64_t asint64; } n;

    if (read (n.asbytes, 8) == 8)
        return (std::int64_t) byteorder::swapiflittleendian (n.asint64);

    return 0;
}

float inputstream::readfloat()
{
    // the union below relies on these types being the same size...
    static_assert (sizeof (std::int32_t) == sizeof (float),
        "the size of a float must be equal to the size of an std::int32_t");

    union { std::int32_t asint; float asfloat; } n;
    n.asint = (std::int32_t) readint();
    return n.asfloat;
}

float inputstream::readfloatbigendian()
{
    union { std::int32_t asint; float asfloat; } n;
    n.asint = (std::int32_t) readintbigendian();
    return n.asfloat;
}

double inputstream::readdouble()
{
    union { std::int64_t asint; double asdouble; } n;
    n.asint = readint64();
    return n.asdouble;
}

double inputstream::readdoublebigendian()
{
    union { std::int64_t asint; double asdouble; } n;
    n.asint = readint64bigendian();
    return n.asdouble;
}

string inputstream::readstring()
{
    memoryblock buffer (256);
    char* data = static_cast<char*> (buffer.getdata());
    size_t i = 0;

    while ((data[i] = readbyte()) != 0)
    {
        if (++i >= buffer.getsize())
        {
            buffer.setsize (buffer.getsize() + 512);
            data = static_cast<char*> (buffer.getdata());
        }
    }

    return string (charpointer_utf8 (data),
                   charpointer_utf8 (data + i));
}

string inputstream::readnextline()
{
    memoryblock buffer (256);
    char* data = static_cast<char*> (buffer.getdata());
    size_t i = 0;

    while ((data[i] = readbyte()) != 0)
    {
        if (data[i] == '\n')
            break;

        if (data[i] == '\r')
        {
            const std::int64_t lastpos = getposition();

            if (readbyte() != '\n')
                setposition (lastpos);

            break;
        }

        if (++i >= buffer.getsize())
        {
            buffer.setsize (buffer.getsize() + 512);
            data = static_cast<char*> (buffer.getdata());
        }
    }

    return string::fromutf8 (data, (int) i);
}

int inputstream::readintomemoryblock (memoryblock& block, std::ptrdiff_t numbytes)
{
    memoryoutputstream mo (block, true);
    return mo.writefrominputstream (*this, numbytes);
}

string inputstream::readentirestreamasstring()
{
    memoryoutputstream mo;
    mo << *this;
    return mo.tostring();
}

//==============================================================================
void inputstream::skipnextbytes (std::int64_t numbytestoskip)
{
    if (numbytestoskip > 0)
    {
        const int skipbuffersize = (int) std::min (numbytestoskip, (std::int64_t) 16384);
        heapblock<char> temp ((size_t) skipbuffersize);

        while (numbytestoskip > 0 && ! isexhausted())
            numbytestoskip -= read (temp, (int) std::min (numbytestoskip, (std::int64_t) skipbuffersize));
    }
}

//------------------------------------------------------------------------------

// unfortunately, putting these in the header causes duplicate
// definition linker errors, even with the inline keyword!

template <>
char inputstream::readtype <char> () { return readbyte (); }

template <>
short inputstream::readtype <short> () { return readshort (); }

template <>
std::int32_t inputstream::readtype <std::int32_t> () { return readint32 (); }

template <>
std::int64_t inputstream::readtype <std::int64_t> () { return readint64 (); }

template <>
unsigned char inputstream::readtype <unsigned char> () { return static_cast <unsigned char> (readbyte ()); }

template <>
unsigned short inputstream::readtype <unsigned short> () { return static_cast <unsigned short> (readshort ()); }

template <>
std::uint32_t inputstream::readtype <std::uint32_t> () { return static_cast <std::uint32_t> (readint32 ()); }

template <>
std::uint64_t inputstream::readtype <std::uint64_t> () { return static_cast <std::uint64_t> (readint64 ()); }

template <>
float inputstream::readtype <float> () { return readfloat (); }

template <>
double inputstream::readtype <double> () { return readdouble (); }

//------------------------------------------------------------------------------

template <>
char inputstream::readtypebigendian <char> () { return readbyte (); }

template <>
short inputstream::readtypebigendian <short> () { return readshortbigendian (); }

template <>
std::int32_t inputstream::readtypebigendian <std::int32_t> () { return readint32bigendian (); }

template <>
std::int64_t inputstream::readtypebigendian <std::int64_t> () { return readint64bigendian (); }

template <>
unsigned char inputstream::readtypebigendian <unsigned char> () { return static_cast <unsigned char> (readbyte ()); }

template <>
unsigned short inputstream::readtypebigendian <unsigned short> () { return static_cast <unsigned short> (readshortbigendian ()); }

template <>
std::uint32_t inputstream::readtypebigendian <std::uint32_t> () { return static_cast <std::uint32_t> (readint32bigendian ()); }

template <>
std::uint64_t inputstream::readtypebigendian <std::uint64_t> () { return static_cast <std::uint64_t> (readint64bigendian ()); }

template <>
float inputstream::readtypebigendian <float> () { return readfloatbigendian (); }

template <>
double inputstream::readtypebigendian <double> () { return readdoublebigendian (); }

} // beast
