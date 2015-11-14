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

std::int64_t beast_filesetposition (void* handle, std::int64_t pos);

//==============================================================================
fileinputstream::fileinputstream (const file& f)
    : file (f),
      filehandle (nullptr),
      currentposition (0),
      status (result::ok()),
      needtoseek (true)
{
    openhandle();
}

fileinputstream::~fileinputstream()
{
    closehandle();
}

//==============================================================================
std::int64_t fileinputstream::gettotallength()
{
    return file.getsize();
}

int fileinputstream::read (void* buffer, int bytestoread)
{
    bassert (openedok());
    bassert (buffer != nullptr && bytestoread >= 0);

    if (needtoseek)
    {
        if (beast_filesetposition (filehandle, currentposition) < 0)
            return 0;

        needtoseek = false;
    }

    const size_t num = readinternal (buffer, (size_t) bytestoread);
    currentposition += num;

    return (int) num;
}

bool fileinputstream::isexhausted()
{
    return currentposition >= gettotallength();
}

std::int64_t fileinputstream::getposition()
{
    return currentposition;
}

bool fileinputstream::setposition (std::int64_t pos)
{
    bassert (openedok());

    if (pos != currentposition)
    {
        pos = blimit ((std::int64_t) 0, gettotallength(), pos);

        needtoseek |= (currentposition != pos);
        currentposition = pos;
    }

    return true;
}

} // beast
