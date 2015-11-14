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

fileinputsource::fileinputsource (const file& f, bool usefiletimeinhash)
    : file (f), usefiletimeinhashgeneration (usefiletimeinhash)
{
}

fileinputsource::~fileinputsource()
{
}

inputstream* fileinputsource::createinputstream()
{
    return file.createinputstream();
}

inputstream* fileinputsource::createinputstreamfor (const string& relateditempath)
{
    return file.getsiblingfile (relateditempath).createinputstream();
}

std::int64_t fileinputsource::hashcode() const
{
    std::int64_t h = file.hashcode();

    if (usefiletimeinhashgeneration)
        h ^= file.getlastmodificationtime().tomilliseconds();

    return h;
}

} // beast
