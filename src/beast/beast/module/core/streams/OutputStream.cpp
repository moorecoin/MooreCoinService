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

#if beast_debug

struct danglingstreamchecker
{
    danglingstreamchecker() {}

    ~danglingstreamchecker()
    {
        /*
            it's always a bad idea to leak any object, but if you're leaking output
            streams, then there's a good chance that you're failing to flush a file
            to disk properly, which could result in corrupted data and other similar
            nastiness..
        */
        bassert (activestreams.size() == 0);
    }

    array<void*, criticalsection> activestreams;
};

static danglingstreamchecker danglingstreamchecker;
#endif

//==============================================================================
outputstream::outputstream()
    : newlinestring (newline::getdefault())
{
   #if beast_debug
    danglingstreamchecker.activestreams.add (this);
   #endif
}

outputstream::~outputstream()
{
   #if beast_debug
    danglingstreamchecker.activestreams.removefirstmatchingvalue (this);
   #endif
}

//==============================================================================
bool outputstream::writebool (const bool b)
{
    return writebyte (b ? (char) 1
                        : (char) 0);
}

bool outputstream::writebyte (char byte)
{
    return write (&byte, 1);
}

bool outputstream::writerepeatedbyte (std::uint8_t byte, size_t numtimestorepeat)
{
    for (size_t i = 0; i < numtimestorepeat; ++i)
        if (! writebyte ((char) byte))
            return false;

    return true;
}

bool outputstream::writeshort (short value)
{
    const unsigned short v = byteorder::swapifbigendian ((unsigned short) value);
    return write (&v, 2);
}

bool outputstream::writeshortbigendian (short value)
{
    const unsigned short v = byteorder::swapiflittleendian ((unsigned short) value);
    return write (&v, 2);
}

bool outputstream::writeint32 (std::int32_t value)
{
    static_assert (sizeof (std::int32_t) == 4,
        "the size of an integer must be exactly 4 bytes.");

    const unsigned int v = byteorder::swapifbigendian ((std::uint32_t) value);
    return write (&v, 4);
}

bool outputstream::writeint (int value)
{
    static_assert (sizeof (int) == 4,
        "the size of an integer must be exactly 4 bytes.");

    const unsigned int v = byteorder::swapifbigendian ((unsigned int) value);
    return write (&v, 4);
}

bool outputstream::writeint32bigendian (int value)
{
    static_assert (sizeof (std::int32_t) == 4,
        "the size of an integer must be exactly 4 bytes.");
    const std::uint32_t v = byteorder::swapiflittleendian ((std::uint32_t) value);
    return write (&v, 4);
}

bool outputstream::writeintbigendian (int value)
{
    static_assert (sizeof (int) == 4,
        "the size of an integer must be exactly 4 bytes.");
    const unsigned int v = byteorder::swapiflittleendian ((unsigned int) value);
    return write (&v, 4);
}

bool outputstream::writecompressedint (int value)
{
    unsigned int un = (value < 0) ? (unsigned int) -value
                                  : (unsigned int) value;

    std::uint8_t data[5];
    int num = 0;

    while (un > 0)
    {
        data[++num] = (std::uint8_t) un;
        un >>= 8;
    }

    data[0] = (std::uint8_t) num;

    if (value < 0)
        data[0] |= 0x80;

    return write (data, (size_t) num + 1);
}

bool outputstream::writeint64 (std::int64_t value)
{
    const std::uint64_t v = byteorder::swapifbigendian ((std::uint64_t) value);
    return write (&v, 8);
}

bool outputstream::writeint64bigendian (std::int64_t value)
{
    const std::uint64_t v = byteorder::swapiflittleendian ((std::uint64_t) value);
    return write (&v, 8);
}

bool outputstream::writefloat (float value)
{
    union { int asint; float asfloat; } n;
    n.asfloat = value;
    return writeint (n.asint);
}

bool outputstream::writefloatbigendian (float value)
{
    union { int asint; float asfloat; } n;
    n.asfloat = value;
    return writeintbigendian (n.asint);
}

bool outputstream::writedouble (double value)
{
    union { std::int64_t asint; double asdouble; } n;
    n.asdouble = value;
    return writeint64 (n.asint);
}

bool outputstream::writedoublebigendian (double value)
{
    union { std::int64_t asint; double asdouble; } n;
    n.asdouble = value;
    return writeint64bigendian (n.asint);
}

bool outputstream::writestring (const string& text)
{
    // (this avoids using toutf8() to prevent the memory bloat that it would leave behind
    // if lots of large, persistent strings were to be written to streams).
    const size_t numbytes = text.getnumbytesasutf8() + 1;
    heapblock<char> temp (numbytes);
    text.copytoutf8 (temp, numbytes);
    return write (temp, numbytes);
}

bool outputstream::writetext (const string& text, const bool asutf16,
                              const bool writeutf16byteordermark)
{
    if (asutf16)
    {
        if (writeutf16byteordermark)
            write ("\x0ff\x0fe", 2);

        string::charpointertype src (text.getcharpointer());
        bool lastcharwasreturn = false;

        for (;;)
        {
            const beast_wchar c = src.getandadvance();

            if (c == 0)
                break;

            if (c == '\n' && ! lastcharwasreturn)
                writeshort ((short) '\r');

            lastcharwasreturn = (c == l'\r');

            if (! writeshort ((short) c))
                return false;
        }
    }
    else
    {
        const char* src = text.toutf8();
        const char* t = src;

        for (;;)
        {
            if (*t == '\n')
            {
                if (t > src)
                    if (! write (src, (size_t) (t - src)))
                        return false;

                if (! write ("\r\n", 2))
                    return false;

                src = t + 1;
            }
            else if (*t == '\r')
            {
                if (t[1] == '\n')
                    ++t;
            }
            else if (*t == 0)
            {
                if (t > src)
                    if (! write (src, (size_t) (t - src)))
                        return false;

                break;
            }

            ++t;
        }
    }

    return true;
}

int outputstream::writefrominputstream (inputstream& source, std::int64_t numbytestowrite)
{
    if (numbytestowrite < 0)
        numbytestowrite = std::numeric_limits<std::int64_t>::max();

    int numwritten = 0;

    while (numbytestowrite > 0)
    {
        char buffer [8192];
        const int num = source.read (buffer, (int) std::min (numbytestowrite, (std::int64_t) sizeof (buffer)));

        if (num <= 0)
            break;

        write (buffer, (size_t) num);

        numbytestowrite -= num;
        numwritten += num;
    }

    return numwritten;
}

//==============================================================================
void outputstream::setnewlinestring (const string& newlinestring_)
{
    newlinestring = newlinestring_;
}

//==============================================================================
outputstream& operator<< (outputstream& stream, const int number)
{
    return stream << string (number);
}

outputstream& operator<< (outputstream& stream, const std::int64_t number)
{
    return stream << string (number);
}

outputstream& operator<< (outputstream& stream, const double number)
{
    return stream << string (number);
}

outputstream& operator<< (outputstream& stream, const char character)
{
    stream.writebyte (character);
    return stream;
}

outputstream& operator<< (outputstream& stream, const char* const text)
{
    stream.write (text, strlen (text));
    return stream;
}

outputstream& operator<< (outputstream& stream, const memoryblock& data)
{
    if (data.getsize() > 0)
        stream.write (data.getdata(), data.getsize());

    return stream;
}

outputstream& operator<< (outputstream& stream, const file& filetoread)
{
    fileinputstream in (filetoread);

    if (in.openedok())
        return stream << in;

    return stream;
}

outputstream& operator<< (outputstream& stream, inputstream& streamtoread)
{
    stream.writefrominputstream (streamtoread, -1);
    return stream;
}

outputstream& operator<< (outputstream& stream, const newline&)
{
    return stream << stream.getnewlinestring();
}

//------------------------------------------------------------------------------

// unfortunately, putting these in the header causes duplicate
// definition linker errors, even with the inline keyword!

template <>
bool outputstream::writetype <char> (char v) { return writebyte (v); }

template <>
bool outputstream::writetype <short> (short v) { return writeshort (v); }

template <>
bool outputstream::writetype <std::int32_t> (std::int32_t v) { return writeint32 (v); }

template <>
bool outputstream::writetype <std::int64_t> (std::int64_t v) { return writeint64 (v); }

template <>
bool outputstream::writetype <unsigned char> (unsigned char v) { return writebyte (static_cast <char> (v)); }

template <>
bool outputstream::writetype <unsigned short> (unsigned short v) { return writeshort (static_cast <short> (v)); }

template <>
bool outputstream::writetype <std::uint32_t> (std::uint32_t v) { return writeint32 (static_cast <std::int32_t> (v)); }

template <>
bool outputstream::writetype <std::uint64_t> (std::uint64_t v) { return writeint64 (static_cast <std::int64_t> (v)); }

template <>
bool outputstream::writetype <float> (float v) { return writefloat (v); }

template <>
bool outputstream::writetype <double> (double v) { return writedouble (v); }

//------------------------------------------------------------------------------

template <>
bool outputstream::writetypebigendian <char> (char v) { return writebyte (v); }

template <>
bool outputstream::writetypebigendian <short> (short v) { return writeshortbigendian (v); }

template <>
bool outputstream::writetypebigendian <std::int32_t> (std::int32_t v) { return writeint32bigendian (v); }

template <>
bool outputstream::writetypebigendian <std::int64_t> (std::int64_t v) { return writeint64bigendian (v); }

template <>
bool outputstream::writetypebigendian <unsigned char> (unsigned char v) { return writebyte (static_cast <char> (v)); }

template <>
bool outputstream::writetypebigendian <unsigned short> (unsigned short v) { return writeshortbigendian (static_cast <short> (v)); }

template <>
bool outputstream::writetypebigendian <std::uint32_t> (std::uint32_t v) { return writeint32bigendian (static_cast <std::int32_t> (v)); }

template <>
bool outputstream::writetypebigendian <std::uint64_t> (std::uint64_t v) { return writeint64bigendian (static_cast <std::int64_t> (v)); }

template <>
bool outputstream::writetypebigendian <float> (float v) { return writefloatbigendian (v); }

template <>
bool outputstream::writetypebigendian <double> (double v) { return writedoublebigendian (v); }

outputstream& operator<< (outputstream& stream, const string& text)
{
    const size_t numbytes = text.getnumbytesasutf8();

   #if (beast_string_utf_type == 8)
    stream.write (text.getcharpointer().getaddress(), numbytes);
   #else
    // (this avoids using toutf8() to prevent the memory bloat that it would leave behind
    // if lots of large, persistent strings were to be written to streams).
    heapblock<char> temp (numbytes + 1);
    charpointer_utf8 (temp).writeall (text.getcharpointer());
    stream.write (temp, numbytes);
   #endif

    return stream;
}

} // beast
