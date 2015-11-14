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

stringarray::stringarray() noexcept
{
}

stringarray::stringarray (const stringarray& other)
    : strings (other.strings)
{
}

#if beast_compiler_supports_move_semantics
stringarray::stringarray (stringarray&& other) noexcept
    : strings (static_cast <array <string>&&> (other.strings))
{
}
#endif

stringarray::stringarray (const string& firstvalue)
{
    strings.add (firstvalue);
}

namespace stringarrayhelpers
{
    template <typename chartype>
    void addarray (array<string>& dest, const chartype* const* strings)
    {
        if (strings != nullptr)
            while (*strings != nullptr)
                dest.add (*strings++);
    }

    template <typename type>
    void addarray (array<string>& dest, const type* const strings, const int numberofstrings)
    {
        for (int i = 0; i < numberofstrings; ++i)
            dest.add (strings [i]);
    }
}

stringarray::stringarray (const string* initialstrings, int numberofstrings)
{
    stringarrayhelpers::addarray (strings, initialstrings, numberofstrings);
}

stringarray::stringarray (const char* const* const initialstrings)
{
    stringarrayhelpers::addarray (strings, initialstrings);
}

stringarray::stringarray (const char* const* const initialstrings, const int numberofstrings)
{
    stringarrayhelpers::addarray (strings, initialstrings, numberofstrings);
}

stringarray::stringarray (const wchar_t* const* const initialstrings)
{
    stringarrayhelpers::addarray (strings, initialstrings);
}

stringarray::stringarray (const wchar_t* const* const initialstrings, const int numberofstrings)
{
    stringarrayhelpers::addarray (strings, initialstrings, numberofstrings);
}

stringarray& stringarray::operator= (const stringarray& other)
{
    strings = other.strings;
    return *this;
}

#if beast_compiler_supports_move_semantics
stringarray& stringarray::operator= (stringarray&& other) noexcept
{
    strings = static_cast <array<string>&&> (other.strings);
    return *this;
}
#endif

stringarray::~stringarray()
{
}

bool stringarray::operator== (const stringarray& other) const noexcept
{
    if (other.size() != size())
        return false;

    for (int i = size(); --i >= 0;)
        if (other.strings.getreference(i) != strings.getreference(i))
            return false;

    return true;
}

bool stringarray::operator!= (const stringarray& other) const noexcept
{
    return ! operator== (other);
}

void stringarray::swapwith (stringarray& other) noexcept
{
    strings.swapwith (other.strings);
}

void stringarray::clear()
{
    strings.clear();
}

const string& stringarray::operator[] (const int index) const noexcept
{
    if (ispositiveandbelow (index, strings.size()))
        return strings.getreference (index);

    return string::empty;
}

string& stringarray::getreference (const int index) noexcept
{
    bassert (ispositiveandbelow (index, strings.size()));
    return strings.getreference (index);
}

void stringarray::add (const string& newstring)
{
    strings.add (newstring);
}

void stringarray::insert (const int index, const string& newstring)
{
    strings.insert (index, newstring);
}

void stringarray::addifnotalreadythere (const string& newstring, const bool ignorecase)
{
    if (! contains (newstring, ignorecase))
        add (newstring);
}

void stringarray::addarray (const stringarray& otherarray, int startindex, int numelementstoadd)
{
    if (startindex < 0)
    {
        bassertfalse;
        startindex = 0;
    }

    if (numelementstoadd < 0 || startindex + numelementstoadd > otherarray.size())
        numelementstoadd = otherarray.size() - startindex;

    while (--numelementstoadd >= 0)
        strings.add (otherarray.strings.getreference (startindex++));
}

void stringarray::set (const int index, const string& newstring)
{
    strings.set (index, newstring);
}

bool stringarray::contains (const string& stringtolookfor, const bool ignorecase) const
{
    if (ignorecase)
    {
        for (int i = size(); --i >= 0;)
            if (strings.getreference(i).equalsignorecase (stringtolookfor))
                return true;
    }
    else
    {
        for (int i = size(); --i >= 0;)
            if (stringtolookfor == strings.getreference(i))
                return true;
    }

    return false;
}

int stringarray::indexof (const string& stringtolookfor, const bool ignorecase, int i) const
{
    if (i < 0)
        i = 0;

    const int numelements = size();

    if (ignorecase)
    {
        while (i < numelements)
        {
            if (strings.getreference(i).equalsignorecase (stringtolookfor))
                return i;

            ++i;
        }
    }
    else
    {
        while (i < numelements)
        {
            if (stringtolookfor == strings.getreference (i))
                return i;

            ++i;
        }
    }

    return -1;
}

//==============================================================================
void stringarray::remove (const int index)
{
    strings.remove (index);
}

void stringarray::removestring (const string& stringtoremove,
                                const bool ignorecase)
{
    if (ignorecase)
    {
        for (int i = size(); --i >= 0;)
            if (strings.getreference(i).equalsignorecase (stringtoremove))
                strings.remove (i);
    }
    else
    {
        for (int i = size(); --i >= 0;)
            if (stringtoremove == strings.getreference (i))
                strings.remove (i);
    }
}

void stringarray::removerange (int startindex, int numbertoremove)
{
    strings.removerange (startindex, numbertoremove);
}

//==============================================================================
void stringarray::removeemptystrings (const bool removewhitespacestrings)
{
    if (removewhitespacestrings)
    {
        for (int i = size(); --i >= 0;)
            if (! strings.getreference(i).containsnonwhitespacechars())
                strings.remove (i);
    }
    else
    {
        for (int i = size(); --i >= 0;)
            if (strings.getreference(i).isempty())
                strings.remove (i);
    }
}

void stringarray::trim()
{
    for (int i = size(); --i >= 0;)
    {
        string& s = strings.getreference(i);
        s = s.trim();
    }
}

//==============================================================================
struct internalstringarraycomparator_casesensitive
{
    static int compareelements (string& first, string& second)      { return first.compare (second); }
};

struct internalstringarraycomparator_caseinsensitive
{
    static int compareelements (string& first, string& second)      { return first.compareignorecase (second); }
};

void stringarray::sort (const bool ignorecase)
{
    if (ignorecase)
    {
        internalstringarraycomparator_caseinsensitive comp;
        strings.sort (comp);
    }
    else
    {
        internalstringarraycomparator_casesensitive comp;
        strings.sort (comp);
    }
}

void stringarray::move (const int currentindex, int newindex) noexcept
{
    strings.move (currentindex, newindex);
}


//==============================================================================
string stringarray::joinintostring (const string& separator, int start, int numbertojoin) const
{
    const int last = (numbertojoin < 0) ? size()
                                        : std::min (size(), start + numbertojoin);

    if (start < 0)
        start = 0;

    if (start >= last)
        return string::empty;

    if (start == last - 1)
        return strings.getreference (start);

    const size_t separatorbytes = separator.getcharpointer().sizeinbytes() - sizeof (string::charpointertype::chartype);
    size_t bytesneeded = separatorbytes * (size_t) (last - start - 1);

    for (int i = start; i < last; ++i)
        bytesneeded += strings.getreference(i).getcharpointer().sizeinbytes() - sizeof (string::charpointertype::chartype);

    string result;
    result.preallocatebytes (bytesneeded);

    string::charpointertype dest (result.getcharpointer());

    while (start < last)
    {
        const string& s = strings.getreference (start);

        if (! s.isempty())
            dest.writeall (s.getcharpointer());

        if (++start < last && separatorbytes > 0)
            dest.writeall (separator.getcharpointer());
    }

    dest.writenull();

    return result;
}

int stringarray::addtokens (const string& text, const bool preservequotedstrings)
{
    return addtokens (text, " \n\r\t", preservequotedstrings ? "\"" : "");
}

int stringarray::addtokens (const string& text, const string& breakcharacters, const string& quotecharacters)
{
    int num = 0;
    string::charpointertype t (text.getcharpointer());

    if (! t.isempty())
    {
        for (;;)
        {
            string::charpointertype tokenend (characterfunctions::findendoftoken (t,
                                                                                  breakcharacters.getcharpointer(),
                                                                                  quotecharacters.getcharpointer()));
            strings.add (string (t, tokenend));
            ++num;

            if (tokenend.isempty())
                break;

            t = ++tokenend;
        }
    }

    return num;
}

int stringarray::addlines (const string& sourcetext)
{
    int numlines = 0;
    string::charpointertype text (sourcetext.getcharpointer());
    bool finished = text.isempty();

    while (! finished)
    {
        for (string::charpointertype startofline (text);;)
        {
            const string::charpointertype endofline (text);

            switch (text.getandadvance())
            {
                case 0:     finished = true; break;
                case '\n':  break;
                case '\r':  if (*text == '\n') ++text; break;
                default:    continue;
            }

            strings.add (string (startofline, endofline));
            ++numlines;
            break;
        }
    }

    return numlines;
}

stringarray stringarray::fromtokens (const string& stringtotokenise,
                                     bool preservequotedstrings)
{
    stringarray s;
    s.addtokens (stringtotokenise, preservequotedstrings);
    return s;
}

stringarray stringarray::fromtokens (const string& stringtotokenise,
                                     const string& breakcharacters,
                                     const string& quotecharacters)
{
    stringarray s;
    s.addtokens (stringtotokenise, breakcharacters, quotecharacters);
    return s;
}

stringarray stringarray::fromlines (const string& stringtobreakup)
{
    stringarray s;
    s.addlines (stringtobreakup);
    return s;
}

//==============================================================================
void stringarray::removeduplicates (const bool ignorecase)
{
    for (int i = 0; i < size() - 1; ++i)
    {
        const string s (strings.getreference(i));

        int nextindex = i + 1;

        for (;;)
        {
            nextindex = indexof (s, ignorecase, nextindex);

            if (nextindex < 0)
                break;

            strings.remove (nextindex);
        }
    }
}

void stringarray::appendnumberstoduplicates (const bool ignorecase,
                                             const bool appendnumbertofirstinstance,
                                             charpointer_utf8 prenumberstring,
                                             charpointer_utf8 postnumberstring)
{
    charpointer_utf8 defaultpre (" ("), defaultpost (")");

    if (prenumberstring.getaddress() == nullptr)
        prenumberstring = defaultpre;

    if (postnumberstring.getaddress() == nullptr)
        postnumberstring = defaultpost;

    for (int i = 0; i < size() - 1; ++i)
    {
        string& s = strings.getreference(i);

        int nextindex = indexof (s, ignorecase, i + 1);

        if (nextindex >= 0)
        {
            const string original (s);

            int number = 0;

            if (appendnumbertofirstinstance)
                s = original + string (prenumberstring) + string (++number) + string (postnumberstring);
            else
                ++number;

            while (nextindex >= 0)
            {
                set (nextindex, (*this)[nextindex] + string (prenumberstring) + string (++number) + string (postnumberstring));
                nextindex = indexof (original, ignorecase, nextindex + 1);
            }
        }
    }
}

void stringarray::ensurestorageallocated (int minnumelements)
{
    strings.ensurestorageallocated (minnumelements);
}

void stringarray::minimisestorageoverheads()
{
    strings.minimisestorageoverheads();
}

} // beast
