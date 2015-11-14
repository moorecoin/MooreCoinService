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

string string::fromcfstring (cfstringref cfstring)
{
    if (cfstring == 0)
        return string::empty;

    cfrange range = { 0, cfstringgetlength (cfstring) };
    heapblock <unichar> u ((size_t) range.length + 1);
    cfstringgetcharacters (cfstring, range, u);
    u[range.length] = 0;

    return string (charpointer_utf16 ((const charpointer_utf16::chartype*) u.getdata()));
}

cfstringref string::tocfstring() const
{
    charpointer_utf16 utf16 (toutf16());
    return cfstringcreatewithcharacters (kcfallocatordefault, (const unichar*) utf16.getaddress(), (cfindex) utf16.length());
}

string string::converttoprecomposedunicode() const
{
   #if beast_ios
    beast_autoreleasepool
    {
        return nsstringtobeast ([beaststringtons (*this) precomposedstringwithcanonicalmapping]);
    }
   #else
    unicodemapping map;

    map.unicodeencoding = createtextencoding (ktextencodingunicodedefault,
                                              kunicodenosubset,
                                              ktextencodingdefaultformat);

    map.otherencoding = createtextencoding (ktextencodingunicodedefault,
                                            kunicodecanonicalcompvariant,
                                            ktextencodingdefaultformat);

    map.mappingversion = kunicodeuselatestmapping;

    unicodetotextinfo conversioninfo = 0;
    string result;

    if (createunicodetotextinfo (&map, &conversioninfo) == noerr)
    {
        const size_t bytesneeded = charpointer_utf16::getbytesrequiredfor (getcharpointer());

        heapblock <char> tempout;
        tempout.calloc (bytesneeded + 4);

        bytecount bytesread = 0;
        bytecount outputbuffersize = 0;

        if (convertfromunicodetotext (conversioninfo,
                                      bytesneeded, (constunichararrayptr) toutf16().getaddress(),
                                      kunicodedefaultdirectionmask,
                                      0, 0, 0, 0,
                                      bytesneeded, &bytesread,
                                      &outputbuffersize, tempout) == noerr)
        {
            result = string (charpointer_utf16 ((charpointer_utf16::chartype*) tempout.getdata()));
        }

        disposeunicodetotextinfo (&conversioninfo);
    }

    return result;
   #endif
}

} // beast
