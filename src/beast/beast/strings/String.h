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

#ifndef beast_strings_string_h_included
#define beast_strings_string_h_included

#include <beast/config.h>
#include <beast/memory.h>

#include <beast/strings/characterfunctions.h>
#if beast_msvc
# pragma warning (push)
# pragma warning (disable: 4514 4996)
#endif
#include <beast/strings/charpointer_utf8.h>
#include <beast/strings/charpointer_utf16.h>
#include <beast/strings/charpointer_utf32.h>
#include <beast/strings/charpointer_ascii.h>
#if beast_msvc
# pragma warning (pop)
#endif
#include <beast/strings/stringcharpointertype.h>
#include <beast/strings/stringfromnumber.h>
#include <beast/strings/string.h>

#include <cstdint>
#include <ostream>

namespace beast {

#if beast_native_wchar_is_utf8
 typedef charpointer_utf8          charpointer_wchar_t;
#elif beast_native_wchar_is_utf16
 typedef charpointer_utf16         charpointer_wchar_t;
#else
 typedef charpointer_utf32         charpointer_wchar_t;
#endif

//==============================================================================
/**
    the beast string class!

    using a reference-counted internal representation, these strings are fast
    and efficient, and there are methods to do just about any operation you'll ever
    dream of.

    @see stringarray, stringpairarray
*/
class string
{
public:
    //==============================================================================
    /** creates an empty string.
        @see empty
    */
    string() noexcept;

    /** creates a copy of another string. */
    string (const string& other) noexcept;

   #if beast_compiler_supports_move_semantics
    string (string&& other) noexcept;
   #endif

    /** creates a string from a zero-terminated ascii text string.

        the string passed-in must not contain any characters with a value above 127, because
        these can't be converted to unicode without knowing the original encoding that was
        used to create the string. if you attempt to pass-in values above 127, you'll get an
        assertion.

        to create strings with extended characters from utf-8, you should explicitly call
        string (charpointer_utf8 ("my utf8 string..")). it's *highly* recommended that you
        use utf-8 with escape characters in your source code to represent extended characters,
        because there's no other way to represent unicode strings in a way that isn't dependent
        on the compiler, source code editor and platform.
    */
    string (const char* text);

    /** creates a string from a string of 8-bit ascii characters.

        the string passed-in must not contain any characters with a value above 127, because
        these can't be converted to unicode without knowing the original encoding that was
        used to create the string. if you attempt to pass-in values above 127, you'll get an
        assertion.

        to create strings with extended characters from utf-8, you should explicitly call
        string (charpointer_utf8 ("my utf8 string..")). it's *highly* recommended that you
        use utf-8 with escape characters in your source code to represent extended characters,
        because there's no other way to represent unicode strings in a way that isn't dependent
        on the compiler, source code editor and platform.

        this will use up the the first maxchars characters of the string (or less if the string
        is actually shorter).
    */
    string (const char* text, size_t maxchars);

    /** creates a string from a whcar_t character string.
        depending on the platform, this may be treated as either utf-32 or utf-16.
    */
    string (const wchar_t* text);

    /** creates a string from a whcar_t character string.
        depending on the platform, this may be treated as either utf-32 or utf-16.
    */
    string (const wchar_t* text, size_t maxchars);

    //==============================================================================
    /** creates a string from a utf-8 character string */
    string (const charpointer_utf8 text);

    /** creates a string from a utf-8 character string */
    string (const charpointer_utf8 text, size_t maxchars);

    /** creates a string from a utf-8 character string */
    string (const charpointer_utf8 start, const charpointer_utf8 end);

    //==============================================================================
    /** creates a string from a utf-16 character string */
    string (const charpointer_utf16 text);

    /** creates a string from a utf-16 character string */
    string (const charpointer_utf16 text, size_t maxchars);

    /** creates a string from a utf-16 character string */
    string (const charpointer_utf16 start, const charpointer_utf16 end);

    //==============================================================================
    /** creates a string from a utf-32 character string */
    string (const charpointer_utf32 text);

    /** creates a string from a utf-32 character string */
    string (const charpointer_utf32 text, size_t maxchars);

    /** creates a string from a utf-32 character string */
    string (const charpointer_utf32 start, const charpointer_utf32 end);

    //==============================================================================
    /** creates a string from an ascii character string */
    string (const charpointer_ascii text);

    /** creates a string from a utf-8 encoded std::string. */
    string (const std::string&);

    //==============================================================================
    /** creates a string from a single character. */
    static string chartostring (beast_wchar character);

    /** destructor. */
    ~string() noexcept;

    /** create a string from a specific number type (integer or floating point)
        if numberofdecimalplaces is specified and number is a floating point type,
        the resulting string will have that many decimal places. a value below 0
        means use exponent notation if necessary.
    */
    template <typename number>
    static string fromnumber (number number, int numberofdecimalplaces = -1);

    //==============================================================================
    /** this is an empty string that can be used whenever one is needed.

        it's better to use this than string() because it explains what's going on
        and is more efficient.
    */
    static const string empty;

    /** this is the character encoding type used internally to store the string. */
    typedef stringcharpointertype charpointertype;

    //==============================================================================
    /** generates a probably-unique 32-bit hashcode from this string. */
    int hashcode() const noexcept;

    /** generates a probably-unique 64-bit hashcode from this string. */
    std::int64_t hashcode64() const noexcept;

    /** returns a hash value suitable for use with std::hash. */
    std::size_t hash() const noexcept;

    /** returns the number of characters in the string. */
    int length() const noexcept;

    //==============================================================================
    // assignment and concatenation operators..

    /** replaces this string's contents with another string. */
    string& operator= (const string& other) noexcept;

   #if beast_compiler_supports_move_semantics
    string& operator= (string&& other) noexcept;
   #endif

    /** appends another string at the end of this one. */
    string& operator+= (const string& stringtoappend);
    /** appends another string at the end of this one. */
    string& operator+= (const char* texttoappend);
    /** appends another string at the end of this one. */
    string& operator+= (const wchar_t* texttoappend);
    /** appends a decimal number at the end of this string. */
    string& operator+= (int numbertoappend);
    /** appends a character at the end of this string. */
    string& operator+= (char charactertoappend);
    /** appends a character at the end of this string. */
    string& operator+= (wchar_t charactertoappend);
   #if ! beast_native_wchar_is_utf32
    /** appends a character at the end of this string. */
    string& operator+= (beast_wchar charactertoappend);
   #endif

    /** appends a string to the end of this one.

        @param texttoappend     the string to add
        @param maxcharstotake   the maximum number of characters to take from the string passed in
    */
    void append (const string& texttoappend, size_t maxcharstotake);

    /** appends a string to the end of this one.

    @param startoftexttoappend the start of the string to add. this must not be a nullptr
    @param endoftexttoappend the end of the string to add. this must not be a nullptr
    */
    void appendcharpointer (const charpointertype startoftexttoappend,
                            const charpointertype endoftexttoappend);

    /** appends a string to the end of this one. */
    void appendcharpointer (const charpointertype texttoappend);

    /** appends a string to the end of this one.

        @param texttoappend     the string to add
        @param maxcharstotake   the maximum number of characters to take from the string passed in
    */
    template <class charpointer>
    void appendcharpointer (const charpointer texttoappend, size_t maxcharstotake)
    {
        if (texttoappend.getaddress() != nullptr)
        {
            size_t extrabytesneeded = 0;
            size_t numchars = 0;

            for (charpointer t (texttoappend); numchars < maxcharstotake && ! t.isempty();)
            {
                extrabytesneeded += charpointertype::getbytesrequiredfor (t.getandadvance());
                ++numchars;
            }

            if (numchars > 0)
            {
                const size_t byteoffsetofnull = getbyteoffsetofend();

                preallocatebytes (byteoffsetofnull + extrabytesneeded);
                charpointertype (addbytestopointer (text.getaddress(), (int) byteoffsetofnull)).writewithcharlimit (texttoappend, (int) (numchars + 1));
            }
        }
    }

    /** appends a string to the end of this one. */
    template <class charpointer>
    void appendcharpointer (const charpointer texttoappend)
    {
        if (texttoappend.getaddress() != nullptr)
        {
            size_t extrabytesneeded = 0;

            for (charpointer t (texttoappend); ! t.isempty();)
                extrabytesneeded += charpointertype::getbytesrequiredfor (t.getandadvance());

            if (extrabytesneeded > 0)
            {
                const size_t byteoffsetofnull = getbyteoffsetofend();

                preallocatebytes (byteoffsetofnull + extrabytesneeded);
                charpointertype (addbytestopointer (text.getaddress(), (int) byteoffsetofnull)).writeall (texttoappend);
            }
        }
    }

    //==============================================================================
    // comparison methods..

    /** returns true if the string contains no characters.
        note that there's also an isnotempty() method to help write readable code.
        @see containsnonwhitespacechars()
    */
    inline bool isempty() const noexcept                    { return text[0] == 0; }

    /** returns true if the string contains at least one character.
        note that there's also an isempty() method to help write readable code.
        @see containsnonwhitespacechars()
    */
    inline bool isnotempty() const noexcept                 { return text[0] != 0; }

    /** case-insensitive comparison with another string. */
    bool equalsignorecase (const string& other) const noexcept;

    /** case-insensitive comparison with another string. */
    bool equalsignorecase (const wchar_t* other) const noexcept;

    /** case-insensitive comparison with another string. */
    bool equalsignorecase (const char* other) const noexcept;

    /** case-sensitive comparison with another string.
        @returns     0 if the two strings are identical; negative if this string comes before
                     the other one alphabetically, or positive if it comes after it.
    */
    int compare (const string& other) const noexcept;

    /** case-sensitive comparison with another string.
        @returns     0 if the two strings are identical; negative if this string comes before
                     the other one alphabetically, or positive if it comes after it.
    */
    int compare (const char* other) const noexcept;

    /** case-sensitive comparison with another string.
        @returns     0 if the two strings are identical; negative if this string comes before
                     the other one alphabetically, or positive if it comes after it.
    */
    int compare (const wchar_t* other) const noexcept;

    /** case-insensitive comparison with another string.
        @returns     0 if the two strings are identical; negative if this string comes before
                     the other one alphabetically, or positive if it comes after it.
    */
    int compareignorecase (const string& other) const noexcept;

    /** lexicographic comparison with another string.

        the comparison used here is case-insensitive and ignores leading non-alphanumeric
        characters, making it good for sorting human-readable strings.

        @returns     0 if the two strings are identical; negative if this string comes before
                     the other one alphabetically, or positive if it comes after it.
    */
    int comparelexicographically (const string& other) const noexcept;

    /** tests whether the string begins with another string.
        if the parameter is an empty string, this will always return true.
        uses a case-sensitive comparison.
    */
    bool startswith (const string& text) const noexcept;

    /** tests whether the string begins with a particular character.
        if the character is 0, this will always return false.
        uses a case-sensitive comparison.
    */
    bool startswithchar (beast_wchar character) const noexcept;

    /** tests whether the string begins with another string.
        if the parameter is an empty string, this will always return true.
        uses a case-insensitive comparison.
    */
    bool startswithignorecase (const string& text) const noexcept;

    /** tests whether the string ends with another string.
        if the parameter is an empty string, this will always return true.
        uses a case-sensitive comparison.
    */
    bool endswith (const string& text) const noexcept;

    /** tests whether the string ends with a particular character.
        if the character is 0, this will always return false.
        uses a case-sensitive comparison.
    */
    bool endswithchar (beast_wchar character) const noexcept;

    /** tests whether the string ends with another string.
        if the parameter is an empty string, this will always return true.
        uses a case-insensitive comparison.
    */
    bool endswithignorecase (const string& text) const noexcept;

    /** tests whether the string contains another substring.
        if the parameter is an empty string, this will always return true.
        uses a case-sensitive comparison.
    */
    bool contains (const string& text) const noexcept;

    /** tests whether the string contains a particular character.
        uses a case-sensitive comparison.
    */
    bool containschar (beast_wchar character) const noexcept;

    /** tests whether the string contains another substring.
        uses a case-insensitive comparison.
    */
    bool containsignorecase (const string& text) const noexcept;

    /** tests whether the string contains another substring as a distinct word.

        @returns    true if the string contains this word, surrounded by
                    non-alphanumeric characters
        @see indexofwholeword, containswholewordignorecase
    */
    bool containswholeword (const string& wordtolookfor) const noexcept;

    /** tests whether the string contains another substring as a distinct word.

        @returns    true if the string contains this word, surrounded by
                    non-alphanumeric characters
        @see indexofwholewordignorecase, containswholeword
    */
    bool containswholewordignorecase (const string& wordtolookfor) const noexcept;

    /** finds an instance of another substring if it exists as a distinct word.

        @returns    if the string contains this word, surrounded by non-alphanumeric characters,
                    then this will return the index of the start of the substring. if it isn't
                    found, then it will return -1
        @see indexofwholewordignorecase, containswholeword
    */
    int indexofwholeword (const string& wordtolookfor) const noexcept;

    /** finds an instance of another substring if it exists as a distinct word.

        @returns    if the string contains this word, surrounded by non-alphanumeric characters,
                    then this will return the index of the start of the substring. if it isn't
                    found, then it will return -1
        @see indexofwholeword, containswholewordignorecase
    */
    int indexofwholewordignorecase (const string& wordtolookfor) const noexcept;

    /** looks for any of a set of characters in the string.
        uses a case-sensitive comparison.

        @returns    true if the string contains any of the characters from
                    the string that is passed in.
    */
    bool containsanyof (const string& charactersitmightcontain) const noexcept;

    /** looks for a set of characters in the string.
        uses a case-sensitive comparison.

        @returns    returns false if any of the characters in this string do not occur in
                    the parameter string. if this string is empty, the return value will
                    always be true.
    */
    bool containsonly (const string& charactersitmightcontain) const noexcept;

    /** returns true if this string contains any non-whitespace characters.

        this will return false if the string contains only whitespace characters, or
        if it's empty.

        it is equivalent to calling "mystring.trim().isnotempty()".
    */
    bool containsnonwhitespacechars() const noexcept;

    /** returns true if the string matches this simple wildcard expression.

        so for example string ("abcdef").matcheswildcard ("*def", true) would return true.

        this isn't a full-blown regex though! the only wildcard characters supported
        are "*" and "?". it's mainly intended for filename pattern matching.
    */
    bool matcheswildcard (const string& wildcard, bool ignorecase) const noexcept;

    //==============================================================================
    // substring location methods..

    /** searches for a character inside this string.
        uses a case-sensitive comparison.
        @returns    the index of the first occurrence of the character in this
                    string, or -1 if it's not found.
    */
    int indexofchar (beast_wchar charactertolookfor) const noexcept;

    /** searches for a character inside this string.
        uses a case-sensitive comparison.
        @param startindex           the index from which the search should proceed
        @param charactertolookfor   the character to look for
        @returns            the index of the first occurrence of the character in this
                            string, or -1 if it's not found.
    */
    int indexofchar (int startindex, beast_wchar charactertolookfor) const noexcept;

    /** returns the index of the first character that matches one of the characters
        passed-in to this method.

        this scans the string, beginning from the startindex supplied, and if it finds
        a character that appears in the string characterstolookfor, it returns its index.

        if none of these characters are found, it returns -1.

        if ignorecase is true, the comparison will be case-insensitive.

        @see indexofchar, lastindexofanyof
    */
    int indexofanyof (const string& characterstolookfor,
                      int startindex = 0,
                      bool ignorecase = false) const noexcept;

    /** searches for a substring within this string.
        uses a case-sensitive comparison.
        @returns    the index of the first occurrence of this substring, or -1 if it's not found.
                    if texttolookfor is an empty string, this will always return 0.
    */
    int indexof (const string& texttolookfor) const noexcept;

    /** searches for a substring within this string.
        uses a case-sensitive comparison.
        @param startindex       the index from which the search should proceed
        @param texttolookfor    the string to search for
        @returns                the index of the first occurrence of this substring, or -1 if it's not found.
                                if texttolookfor is an empty string, this will always return -1.
    */
    int indexof (int startindex, const string& texttolookfor) const noexcept;

    /** searches for a substring within this string.
        uses a case-insensitive comparison.
        @returns    the index of the first occurrence of this substring, or -1 if it's not found.
                    if texttolookfor is an empty string, this will always return 0.
    */
    int indexofignorecase (const string& texttolookfor) const noexcept;

    /** searches for a substring within this string.
        uses a case-insensitive comparison.
        @param startindex       the index from which the search should proceed
        @param texttolookfor    the string to search for
        @returns                the index of the first occurrence of this substring, or -1 if it's not found.
                                if texttolookfor is an empty string, this will always return -1.
    */
    int indexofignorecase (int startindex, const string& texttolookfor) const noexcept;

    /** searches for a character inside this string (working backwards from the end of the string).
        uses a case-sensitive comparison.
        @returns    the index of the last occurrence of the character in this string, or -1 if it's not found.
    */
    int lastindexofchar (beast_wchar character) const noexcept;

    /** searches for a substring inside this string (working backwards from the end of the string).
        uses a case-sensitive comparison.
        @returns    the index of the start of the last occurrence of the substring within this string,
                    or -1 if it's not found. if texttolookfor is an empty string, this will always return -1.
    */
    int lastindexof (const string& texttolookfor) const noexcept;

    /** searches for a substring inside this string (working backwards from the end of the string).
        uses a case-insensitive comparison.
        @returns    the index of the start of the last occurrence of the substring within this string, or -1
                    if it's not found. if texttolookfor is an empty string, this will always return -1.
    */
    int lastindexofignorecase (const string& texttolookfor) const noexcept;

    /** returns the index of the last character in this string that matches one of the
        characters passed-in to this method.

        this scans the string backwards, starting from its end, and if it finds
        a character that appears in the string characterstolookfor, it returns its index.

        if none of these characters are found, it returns -1.

        if ignorecase is true, the comparison will be case-insensitive.

        @see lastindexof, indexofanyof
    */
    int lastindexofanyof (const string& characterstolookfor,
                          bool ignorecase = false) const noexcept;


    //==============================================================================
    // substring extraction and manipulation methods..

    /** returns the character at this index in the string.
        in a release build, no checks are made to see if the index is within a valid range, so be
        careful! in a debug build, the index is checked and an assertion fires if it's out-of-range.

        also beware that depending on the encoding format that the string is using internally, this
        method may execute in either o(1) or o(n) time, so be careful when using it in your algorithms.
        if you're scanning through a string to inspect its characters, you should never use this operator
        for random access, it's far more efficient to call getcharpointer() to return a pointer, and
        then to use that to iterate the string.
        @see getcharpointer
    */
    beast_wchar operator[] (int index) const noexcept;

    /** returns the final character of the string.
        if the string is empty this will return 0.
    */
    beast_wchar getlastcharacter() const noexcept;

    //==============================================================================
    /** returns a subsection of the string.

        if the range specified is beyond the limits of the string, as much as
        possible is returned.

        @param startindex   the index of the start of the substring needed
        @param endindex     all characters from startindex up to (but not including)
                            this index are returned
        @see fromfirstoccurrenceof, droplastcharacters, getlastcharacters, uptofirstoccurrenceof
    */
    string substring (int startindex, int endindex) const;

    /** returns a section of the string, starting from a given position.

        @param startindex   the first character to include. if this is beyond the end
                            of the string, an empty string is returned. if it is zero or
                            less, the whole string is returned.
        @returns            the substring from startindex up to the end of the string
        @see droplastcharacters, getlastcharacters, fromfirstoccurrenceof, uptofirstoccurrenceof, fromlastoccurrenceof
    */
    string substring (int startindex) const;

    /** returns a version of this string with a number of characters removed
        from the end.

        @param numbertodrop     the number of characters to drop from the end of the
                                string. if this is greater than the length of the string,
                                an empty string will be returned. if zero or less, the
                                original string will be returned.
        @see substring, fromfirstoccurrenceof, uptofirstoccurrenceof, fromlastoccurrenceof, getlastcharacter
    */
    string droplastcharacters (int numbertodrop) const;

    /** returns a number of characters from the end of the string.

        this returns the last numcharacters characters from the end of the string. if the
        string is shorter than numcharacters, the whole string is returned.

        @see substring, droplastcharacters, getlastcharacter
    */
    string getlastcharacters (int numcharacters) const;

    //==============================================================================
    /** returns a section of the string starting from a given substring.

        this will search for the first occurrence of the given substring, and
        return the section of the string starting from the point where this is
        found (optionally not including the substring itself).

        e.g. for the string "123456", fromfirstoccurrenceof ("34", true) would return "3456", and
                                      fromfirstoccurrenceof ("34", false) would return "56".

        if the substring isn't found, the method will return an empty string.

        if ignorecase is true, the comparison will be case-insensitive.

        @see uptofirstoccurrenceof, fromlastoccurrenceof
    */
    string fromfirstoccurrenceof (const string& substringtostartfrom,
                                  bool includesubstringinresult,
                                  bool ignorecase) const;

    /** returns a section of the string starting from the last occurrence of a given substring.

        similar to fromfirstoccurrenceof(), but using the last occurrence of the substring, and
        unlike fromfirstoccurrenceof(), if the substring isn't found, this method will
        return the whole of the original string.

        @see fromfirstoccurrenceof, uptolastoccurrenceof
    */
    string fromlastoccurrenceof (const string& substringtofind,
                                 bool includesubstringinresult,
                                 bool ignorecase) const;

    /** returns the start of this string, up to the first occurrence of a substring.

        this will search for the first occurrence of a given substring, and then
        return a copy of the string, up to the position of this substring,
        optionally including or excluding the substring itself in the result.

        e.g. for the string "123456", upto ("34", false) would return "12", and
                                      upto ("34", true) would return "1234".

        if the substring isn't found, this will return the whole of the original string.

        @see uptolastoccurrenceof, fromfirstoccurrenceof
    */
    string uptofirstoccurrenceof (const string& substringtoendwith,
                                  bool includesubstringinresult,
                                  bool ignorecase) const;

    /** returns the start of this string, up to the last occurrence of a substring.

        similar to uptofirstoccurrenceof(), but this finds the last occurrence rather than the first.
        if the substring isn't found, this will return the whole of the original string.

        @see uptofirstoccurrenceof, fromfirstoccurrenceof
    */
    string uptolastoccurrenceof (const string& substringtofind,
                                 bool includesubstringinresult,
                                 bool ignorecase) const;

    //==============================================================================
    /** returns a copy of this string with any whitespace characters removed from the start and end. */
    string trim() const;

    /** returns a copy of this string with any whitespace characters removed from the start. */
    string trimstart() const;

    /** returns a copy of this string with any whitespace characters removed from the end. */
    string trimend() const;

    /** returns a copy of this string, having removed a specified set of characters from its start.
        characters are removed from the start of the string until it finds one that is not in the
        specified set, and then it stops.
        @param characterstotrim     the set of characters to remove.
        @see trim, trimstart, trimcharactersatend
    */
    string trimcharactersatstart (const string& characterstotrim) const;

    /** returns a copy of this string, having removed a specified set of characters from its end.
        characters are removed from the end of the string until it finds one that is not in the
        specified set, and then it stops.
        @param characterstotrim     the set of characters to remove.
        @see trim, trimend, trimcharactersatstart
    */
    string trimcharactersatend (const string& characterstotrim) const;

    //==============================================================================
    /** returns an upper-case version of this string. */
    string touppercase() const;

    /** returns an lower-case version of this string. */
    string tolowercase() const;

    //==============================================================================
    /** replaces a sub-section of the string with another string.

        this will return a copy of this string, with a set of characters
        from startindex to startindex + numcharstoreplace removed, and with
        a new string inserted in their place.

        note that this is a const method, and won't alter the string itself.

        @param startindex               the first character to remove. if this is beyond the bounds of the string,
                                        it will be constrained to a valid range.
        @param numcharacterstoreplace   the number of characters to remove. if zero or less, no
                                        characters will be taken out.
        @param stringtoinsert           the new string to insert at startindex after the characters have been
                                        removed.
    */
    string replacesection (int startindex,
                           int numcharacterstoreplace,
                           const string& stringtoinsert) const;

    /** replaces all occurrences of a substring with another string.

        returns a copy of this string, with any occurrences of stringtoreplace
        swapped for stringtoinsertinstead.

        note that this is a const method, and won't alter the string itself.
    */
    string replace (const string& stringtoreplace,
                    const string& stringtoinsertinstead,
                    bool ignorecase = false) const;

    /** returns a string with all occurrences of a character replaced with a different one. */
    string replacecharacter (beast_wchar charactertoreplace,
                             beast_wchar charactertoinsertinstead) const;

    /** replaces a set of characters with another set.

        returns a string in which each character from characterstoreplace has been replaced
        by the character at the equivalent position in newcharacters (so the two strings
        passed in must be the same length).

        e.g. replacecharacters ("abc", "def") replaces 'a' with 'd', 'b' with 'e', etc.

        note that this is a const method, and won't affect the string itself.
    */
    string replacecharacters (const string& characterstoreplace,
                              const string& characterstoinsertinstead) const;

    /** returns a version of this string that only retains a fixed set of characters.

        this will return a copy of this string, omitting any characters which are not
        found in the string passed-in.

        e.g. for "1122334455", retaincharacters ("432") would return "223344"

        note that this is a const method, and won't alter the string itself.
    */
    string retaincharacters (const string& characterstoretain) const;

    /** returns a version of this string with a set of characters removed.

        this will return a copy of this string, omitting any characters which are
        found in the string passed-in.

        e.g. for "1122334455", removecharacters ("432") would return "1155"

        note that this is a const method, and won't alter the string itself.
    */
    string removecharacters (const string& characterstoremove) const;

    /** returns a section from the start of the string that only contains a certain set of characters.

        this returns the leftmost section of the string, up to (and not including) the
        first character that doesn't appear in the string passed in.
    */
    string initialsectioncontainingonly (const string& permittedcharacters) const;

    /** returns a section from the start of the string that only contains a certain set of characters.

        this returns the leftmost section of the string, up to (and not including) the
        first character that occurs in the string passed in. (if none of the specified
        characters are found in the string, the return value will just be the original string).
    */
    string initialsectionnotcontaining (const string& characterstostopat) const;

    //==============================================================================
    /** checks whether the string might be in quotation marks.

        @returns    true if the string begins with a quote character (either a double or single quote).
                    it is also true if there is whitespace before the quote, but it doesn't check the end of the string.
        @see unquoted, quoted
    */
    bool isquotedstring() const;

    /** removes quotation marks from around the string, (if there are any).

        returns a copy of this string with any quotes removed from its ends. quotes that aren't
        at the ends of the string are not affected. if there aren't any quotes, the original string
        is returned.

        note that this is a const method, and won't alter the string itself.

        @see isquotedstring, quoted
    */
    string unquoted() const;

    /** adds quotation marks around a string.

        this will return a copy of the string with a quote at the start and end, (but won't
        add the quote if there's already one there, so it's safe to call this on strings that
        may already have quotes around them).

        note that this is a const method, and won't alter the string itself.

        @param quotecharacter   the character to add at the start and end
        @see isquotedstring, unquoted
    */
    string quoted (beast_wchar quotecharacter = '"') const;


    //==============================================================================
    /** creates a string which is a version of a string repeated and joined together.

        @param stringtorepeat         the string to repeat
        @param numberoftimestorepeat  how many times to repeat it
    */
    static string repeatedstring (const string& stringtorepeat,
                                  int numberoftimestorepeat);

    /** returns a copy of this string with the specified character repeatedly added to its
        beginning until the total length is at least the minimum length specified.
    */
    string paddedleft (beast_wchar padcharacter, int minimumlength) const;

    /** returns a copy of this string with the specified character repeatedly added to its
        end until the total length is at least the minimum length specified.
    */
    string paddedright (beast_wchar padcharacter, int minimumlength) const;

    /** creates a string from data in an unknown format.

        this looks at some binary data and tries to guess whether it's unicode
        or 8-bit characters, then returns a string that represents it correctly.

        should be able to handle unicode endianness correctly, by looking at
        the first two bytes.
    */
    static string createstringfromdata (const void* data, int size);

    /** creates a string from a printf-style parameter list.

        i don't like this method. i don't use it myself, and i recommend avoiding it and
        using the operator<< methods or pretty much anything else instead. it's only provided
        here because of the popular unrest that was stirred-up when i tried to remove it...

        if you're really determined to use it, at least make sure that you never, ever,
        pass any string objects to it as parameters. and bear in mind that internally, depending
        on the platform, it may be using wchar_t or char character types, so that even string
        literals can't be safely used as parameters if you're writing portable code.
    */
    static string formatted (const string formatstring, ... );

    //==============================================================================
    // numeric conversions..

    /** creates a string containing this signed 32-bit integer as a decimal number.
        @see getintvalue, getfloatvalue, getdoublevalue, tohexstring
    */
    explicit string (int decimalinteger);

    /** creates a string containing this unsigned 32-bit integer as a decimal number.
        @see getintvalue, getfloatvalue, getdoublevalue, tohexstring
    */
    explicit string (unsigned int decimalinteger);

    /** creates a string containing this signed 16-bit integer as a decimal number.
        @see getintvalue, getfloatvalue, getdoublevalue, tohexstring
    */
    explicit string (short decimalinteger);

    /** creates a string containing this unsigned 16-bit integer as a decimal number.
        @see getintvalue, getfloatvalue, getdoublevalue, tohexstring
    */
    explicit string (unsigned short decimalinteger);

    explicit string (long largeintegervalue);

    explicit string (unsigned long largeintegervalue);

    explicit string (long long largeintegervalue);

    explicit string (unsigned long long largeintegervalue);

    /** creates a string representing this floating-point number.
        @param floatvalue               the value to convert to a string
        @see getdoublevalue, getintvalue
    */
    explicit string (float floatvalue);

    /** creates a string representing this floating-point number.
        @param doublevalue              the value to convert to a string
        @see getfloatvalue, getintvalue
    */
    explicit string (double doublevalue);

    /** creates a string representing this floating-point number.
        @param floatvalue               the value to convert to a string
        @param numberofdecimalplaces    if this is > 0, it will format the number using that many
                                        decimal places, and will not use exponent notation. if 0 or
                                        less, it will use exponent notation if necessary.
        @see getdoublevalue, getintvalue
    */
    string (float floatvalue, int numberofdecimalplaces);

    /** creates a string representing this floating-point number.
        @param doublevalue              the value to convert to a string
        @param numberofdecimalplaces    if this is > 0, it will format the number using that many
                                        decimal places, and will not use exponent notation. if 0 or
                                        less, it will use exponent notation if necessary.
        @see getfloatvalue, getintvalue
    */
    string (double doublevalue, int numberofdecimalplaces);

    /** reads the value of the string as a decimal number (up to 32 bits in size).

        @returns the value of the string as a 32 bit signed base-10 integer.
        @see gettrailingintvalue, gethexvalue32, gethexvalue64
    */
    int getintvalue() const noexcept;

    /** reads the value of the string as a decimal number (up to 64 bits in size).

        @returns the value of the string as a 64 bit signed base-10 integer.
    */
    std::int64_t getlargeintvalue() const noexcept;

    /** parses a decimal number from the end of the string.

        this will look for a value at the end of the string.
        e.g. for "321 xyz654" it will return 654; for "2 3 4" it'll return 4.

        negative numbers are not handled, so "xyz-5" returns 5.

        @see getintvalue
    */
    int gettrailingintvalue() const noexcept;

    /** parses this string as a floating point number.

        @returns    the value of the string as a 32-bit floating point value.
        @see getdoublevalue
    */
    float getfloatvalue() const noexcept;

    /** parses this string as a floating point number.

        @returns    the value of the string as a 64-bit floating point value.
        @see getfloatvalue
    */
    double getdoublevalue() const noexcept;

    /** parses the string as a hexadecimal number.

        non-hexadecimal characters in the string are ignored.

        if the string contains too many characters, then the lowest significant
        digits are returned, e.g. "ffff12345678" would produce 0x12345678.

        @returns    a 32-bit number which is the value of the string in hex.
    */
    int gethexvalue32() const noexcept;

    /** parses the string as a hexadecimal number.

        non-hexadecimal characters in the string are ignored.

        if the string contains too many characters, then the lowest significant
        digits are returned, e.g. "ffff1234567812345678" would produce 0x1234567812345678.

        @returns    a 64-bit number which is the value of the string in hex.
    */
    std::int64_t gethexvalue64() const noexcept;

    /** creates a string representing this 32-bit value in hexadecimal. */
    static string tohexstring (int number);

    /** creates a string representing this 64-bit value in hexadecimal. */
    static string tohexstring (std::int64_t number);

    /** creates a string representing this 16-bit value in hexadecimal. */
    static string tohexstring (short number);

    /** creates a string containing a hex dump of a block of binary data.

        @param data         the binary data to use as input
        @param size         how many bytes of data to use
        @param groupsize    how many bytes are grouped together before inserting a
                            space into the output. e.g. group size 0 has no spaces,
                            group size 1 looks like: "be a1 c2 ff", group size 2 looks
                            like "bea1 c2ff".
    */
    static string tohexstring (const void* data, int size, int groupsize = 1);

    //==============================================================================
    /** returns the character pointer currently being used to store this string.

        because it returns a reference to the string's internal data, the pointer
        that is returned must not be stored anywhere, as it can be deleted whenever the
        string changes.
    */
    inline charpointertype getcharpointer() const noexcept      { return text; }

    /** returns a pointer to a utf-8 version of this string.

        because it returns a reference to the string's internal data, the pointer
        that is returned must not be stored anywhere, as it can be deleted whenever the
        string changes.

        to find out how many bytes you need to store this string as utf-8, you can call
        charpointer_utf8::getbytesrequiredfor (mystring.getcharpointer())

        @see torawutf8, getcharpointer, toutf16, toutf32
    */
    charpointer_utf8 toutf8() const;

    /** returns a pointer to a utf-8 version of this string.

        because it returns a reference to the string's internal data, the pointer
        that is returned must not be stored anywhere, as it can be deleted whenever the
        string changes.

        to find out how many bytes you need to store this string as utf-8, you can call
        charpointer_utf8::getbytesrequiredfor (mystring.getcharpointer())

        @see getcharpointer, toutf8, toutf16, toutf32
    */
    const char* torawutf8() const;

    /** returns a pointer to a utf-16 version of this string.

        because it returns a reference to the string's internal data, the pointer
        that is returned must not be stored anywhere, as it can be deleted whenever the
        string changes.

        to find out how many bytes you need to store this string as utf-16, you can call
        charpointer_utf16::getbytesrequiredfor (mystring.getcharpointer())

        @see getcharpointer, toutf8, toutf32
    */
    charpointer_utf16 toutf16() const;

    /** returns a pointer to a utf-32 version of this string.

        because it returns a reference to the string's internal data, the pointer
        that is returned must not be stored anywhere, as it can be deleted whenever the
        string changes.

        @see getcharpointer, toutf8, toutf16
    */
    charpointer_utf32 toutf32() const;

    /** returns a pointer to a wchar_t version of this string.

        because it returns a reference to the string's internal data, the pointer
        that is returned must not be stored anywhere, as it can be deleted whenever the
        string changes.

        bear in mind that the wchar_t type is different on different platforms, so on
        windows, this will be equivalent to calling toutf16(), on unix it'll be the same
        as calling toutf32(), etc.

        @see getcharpointer, toutf8, toutf16, toutf32
    */
    const wchar_t* towidecharpointer() const;

    /** */
    std::string tostdstring() const;

    //==============================================================================
    /** creates a string from a utf-8 encoded buffer.
        if the size is < 0, it'll keep reading until it hits a zero.
    */
    static string fromutf8 (const char* utf8buffer, int buffersizebytes = -1);

    /** returns the number of bytes required to represent this string as utf8.
        the number returned does not include the trailing zero.
        @see toutf8, copytoutf8
    */
    size_t getnumbytesasutf8() const noexcept;

    //==============================================================================
    /** copies the string to a buffer as utf-8 characters.

        returns the number of bytes copied to the buffer, including the terminating null
        character.

        to find out how many bytes you need to store this string as utf-8, you can call
        charpointer_utf8::getbytesrequiredfor (mystring.getcharpointer())

        @param destbuffer       the place to copy it to; if this is a null pointer, the method just
                                returns the number of bytes required (including the terminating null character).
        @param maxbuffersizebytes  the size of the destination buffer, in bytes. if the string won't fit, it'll
                                put in as many as it can while still allowing for a terminating null char at the
                                end, and will return the number of bytes that were actually used.
        @see charpointer_utf8::writewithdestbytelimit
    */
    size_t copytoutf8 (charpointer_utf8::chartype* destbuffer, size_t maxbuffersizebytes) const noexcept;

    /** copies the string to a buffer as utf-16 characters.

        returns the number of bytes copied to the buffer, including the terminating null
        character.

        to find out how many bytes you need to store this string as utf-16, you can call
        charpointer_utf16::getbytesrequiredfor (mystring.getcharpointer())

        @param destbuffer       the place to copy it to; if this is a null pointer, the method just
                                returns the number of bytes required (including the terminating null character).
        @param maxbuffersizebytes  the size of the destination buffer, in bytes. if the string won't fit, it'll
                                put in as many as it can while still allowing for a terminating null char at the
                                end, and will return the number of bytes that were actually used.
        @see charpointer_utf16::writewithdestbytelimit
    */
    size_t copytoutf16 (charpointer_utf16::chartype* destbuffer, size_t maxbuffersizebytes) const noexcept;

    /** copies the string to a buffer as utf-32 characters.

        returns the number of bytes copied to the buffer, including the terminating null
        character.

        to find out how many bytes you need to store this string as utf-32, you can call
        charpointer_utf32::getbytesrequiredfor (mystring.getcharpointer())

        @param destbuffer       the place to copy it to; if this is a null pointer, the method just
                                returns the number of bytes required (including the terminating null character).
        @param maxbuffersizebytes  the size of the destination buffer, in bytes. if the string won't fit, it'll
                                put in as many as it can while still allowing for a terminating null char at the
                                end, and will return the number of bytes that were actually used.
        @see charpointer_utf32::writewithdestbytelimit
    */
    size_t copytoutf32 (charpointer_utf32::chartype* destbuffer, size_t maxbuffersizebytes) const noexcept;

    //==============================================================================
    /** increases the string's internally allocated storage.

        although the string's contents won't be affected by this call, it will
        increase the amount of memory allocated internally for the string to grow into.

        if you're about to make a large number of calls to methods such
        as += or <<, it's more efficient to preallocate enough extra space
        beforehand, so that these methods won't have to keep resizing the string
        to append the extra characters.

        @param numbytesneeded   the number of bytes to allocate storage for. if this
                                value is less than the currently allocated size, it will
                                have no effect.
    */
    void preallocatebytes (size_t numbytesneeded);

    /** swaps the contents of this string with another one.
        this is a very fast operation, as no allocation or copying needs to be done.
    */
    void swapwith (string& other) noexcept;

    //==============================================================================
   #if beast_mac || beast_ios || doxygen
    /** mac only - creates a string from an osx cfstring. */
    static string fromcfstring (cfstringref cfstring);

    /** mac only - converts this string to a cfstring.
        remember that you must use cfrelease() to free the returned string when you're
        finished with it.
    */
    cfstringref tocfstring() const;

    /** mac only - returns a copy of this string in which any decomposed unicode characters have
        been converted to their precomposed equivalents. */
    string converttoprecomposedunicode() const;
   #endif

    template <class hasher>
    friend
    void
    hash_append (hasher& h, string const& s)
    {
        h.append(s.text.getaddress(), s.text.sizeinbytes());
    }

private:
    //==============================================================================
    struct fromnumber { };
    string (charpointertype text_, fromnumber) : text (text_) { }

    charpointertype text;

    //==============================================================================
    struct preallocationbytes
    {
        explicit preallocationbytes (size_t);
        size_t numbytes;
    };

    explicit string (const preallocationbytes&); // this constructor preallocates a certain amount of memory
    void appendfixedlength (const char* text, int numextrachars);
    size_t getbyteoffsetofend() const noexcept;
    beast_deprecated (string (const string&, size_t));

    // this private cast operator should prevent strings being accidentally cast
    // to bools (this is possible because the compiler can add an implicit cast
    // via a const char*)
    operator bool() const noexcept  { return false; }
};

//------------------------------------------------------------------------------

template <typename number>
inline string string::fromnumber (number number, int)
{
    return string (numbertostringconverters::createfrominteger
        <number> (number), fromnumber ());
}

template <>
inline string string::fromnumber <float> (float number, int numberofdecimalplaces)
{
    if (numberofdecimalplaces == 0)
        number = std::floor (number);
    return string (numbertostringconverters::createfromdouble (
        number, numberofdecimalplaces));
}

template <>
inline string string::fromnumber <double> (double number, int numberofdecimalplaces)
{
    if (numberofdecimalplaces == 0)
        number = std::floor (number);

    return string (numbertostringconverters::createfromdouble (
        number, numberofdecimalplaces));
}

//------------------------------------------------------------------------------

/** concatenates two strings. */
string operator+ (const char* string1,     const string& string2);
/** concatenates two strings. */
string operator+ (const wchar_t* string1,  const string& string2);
/** concatenates two strings. */
string operator+ (char string1,            const string& string2);
/** concatenates two strings. */
string operator+ (wchar_t string1,         const string& string2);
#if ! beast_native_wchar_is_utf32
/** concatenates two strings. */
string operator+ (beast_wchar string1,      const string& string2);
#endif

/** concatenates two strings. */
string operator+ (string string1, const string& string2);
/** concatenates two strings. */
string operator+ (string string1, const char* string2);
/** concatenates two strings. */
string operator+ (string string1, const wchar_t* string2);
/** concatenates two strings. */
string operator+ (string string1, char charactertoappend);
/** concatenates two strings. */
string operator+ (string string1, wchar_t charactertoappend);
#if ! beast_native_wchar_is_utf32
/** concatenates two strings. */
string operator+ (string string1, beast_wchar charactertoappend);
#endif

//==============================================================================
/** appends a character at the end of a string. */
string& operator<< (string& string1, char charactertoappend);
/** appends a character at the end of a string. */
string& operator<< (string& string1, wchar_t charactertoappend);
#if ! beast_native_wchar_is_utf32
/** appends a character at the end of a string. */
string& operator<< (string& string1, beast_wchar charactertoappend);
#endif

/** appends a string to the end of the first one. */
string& operator<< (string& string1, const char* string2);
/** appends a string to the end of the first one. */
string& operator<< (string& string1, const wchar_t* string2);
/** appends a string to the end of the first one. */
string& operator<< (string& string1, const string& string2);

/** appends a decimal number at the end of a string. */
string& operator<< (string& string1, short number);
/** appends a decimal number at the end of a string. */
string& operator<< (string& string1, int number);
/** appends a decimal number at the end of a string. */
string& operator<< (string& string1, long number);
/** appends a decimal number at the end of a string. */
string& operator<< (string& string1, std::int64_t number);
/** appends a decimal number at the end of a string. */
string& operator<< (string& string1, float number);
/** appends a decimal number at the end of a string. */
string& operator<< (string& string1, double number);

//==============================================================================
/** case-sensitive comparison of two strings. */
bool operator== (const string& string1, const string& string2) noexcept;
/** case-sensitive comparison of two strings. */
bool operator== (const string& string1, const char* string2) noexcept;
/** case-sensitive comparison of two strings. */
bool operator== (const string& string1, const wchar_t* string2) noexcept;
/** case-sensitive comparison of two strings. */
bool operator== (const string& string1, const charpointer_utf8 string2) noexcept;
/** case-sensitive comparison of two strings. */
bool operator== (const string& string1, const charpointer_utf16 string2) noexcept;
/** case-sensitive comparison of two strings. */
bool operator== (const string& string1, const charpointer_utf32 string2) noexcept;
/** case-sensitive comparison of two strings. */
bool operator!= (const string& string1, const string& string2) noexcept;
/** case-sensitive comparison of two strings. */
bool operator!= (const string& string1, const char* string2) noexcept;
/** case-sensitive comparison of two strings. */
bool operator!= (const string& string1, const wchar_t* string2) noexcept;
/** case-sensitive comparison of two strings. */
bool operator!= (const string& string1, const charpointer_utf8 string2) noexcept;
/** case-sensitive comparison of two strings. */
bool operator!= (const string& string1, const charpointer_utf16 string2) noexcept;
/** case-sensitive comparison of two strings. */
bool operator!= (const string& string1, const charpointer_utf32 string2) noexcept;
/** case-sensitive comparison of two strings. */
bool operator>  (const string& string1, const string& string2) noexcept;
/** case-sensitive comparison of two strings. */
bool operator<  (const string& string1, const string& string2) noexcept;
/** case-sensitive comparison of two strings. */
bool operator>= (const string& string1, const string& string2) noexcept;
/** case-sensitive comparison of two strings. */
bool operator<= (const string& string1, const string& string2) noexcept;

//==============================================================================
/** this operator allows you to write a beast string directly to std output streams.
    this is handy for writing strings to std::cout, std::cerr, etc.
*/
template <class traits>
std::basic_ostream <char, traits>& operator<< (std::basic_ostream <char, traits>& stream, const string& stringtowrite)
{
    return stream << stringtowrite.torawutf8();
}

/** this operator allows you to write a beast string directly to std output streams.
    this is handy for writing strings to std::wcout, std::wcerr, etc.
*/
template <class traits>
std::basic_ostream <wchar_t, traits>& operator<< (std::basic_ostream <wchar_t, traits>& stream, const string& stringtowrite)
{
    return stream << stringtowrite.towidecharpointer();
}

}

#endif

