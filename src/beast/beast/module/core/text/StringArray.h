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

#ifndef beast_stringarray_h_included
#define beast_stringarray_h_included

#include <beast/strings/string.h>
#include <beast/module/core/containers/array.h>
#include <beast/module/core/threads/criticalsection.h>

namespace beast {

//==============================================================================
/**
    a special array for holding a list of strings.

    @see string, stringpairarray
*/
class stringarray
{
public:
    //==============================================================================
    /** creates an empty string array */
    stringarray() noexcept;

    /** creates a copy of another string array */
    stringarray (const stringarray& other);

   #if beast_compiler_supports_move_semantics
    stringarray (stringarray&& other) noexcept;
   #endif

    /** creates an array containing a single string. */
    explicit stringarray (const string& firstvalue);

    /** creates an array from a raw array of strings.
        @param strings          an array of strings to add
        @param numberofstrings  how many items there are in the array
    */
    stringarray (const string* strings, int numberofstrings);

    /** creates a copy of an array of string literals.
        @param strings          an array of strings to add. null pointers in the array will be
                                treated as empty strings
        @param numberofstrings  how many items there are in the array
    */
    stringarray (const char* const* strings, int numberofstrings);

    /** creates a copy of a null-terminated array of string literals.

        each item from the array passed-in is added, until it encounters a null pointer,
        at which point it stops.
    */
    explicit stringarray (const char* const* strings);

    /** creates a copy of a null-terminated array of string literals.
        each item from the array passed-in is added, until it encounters a null pointer,
        at which point it stops.
    */
    explicit stringarray (const wchar_t* const* strings);

    /** creates a copy of an array of string literals.
        @param strings          an array of strings to add. null pointers in the array will be
                                treated as empty strings
        @param numberofstrings  how many items there are in the array
    */
    stringarray (const wchar_t* const* strings, int numberofstrings);

    /** destructor. */
    ~stringarray();

    /** copies the contents of another string array into this one */
    stringarray& operator= (const stringarray& other);

   #if beast_compiler_supports_move_semantics
    stringarray& operator= (stringarray&& other) noexcept;
   #endif

    /** swaps the contents of this and another stringarray. */
    void swapwith (stringarray& other) noexcept;

    //==============================================================================
    /** compares two arrays.
        comparisons are case-sensitive.
        @returns    true only if the other array contains exactly the same strings in the same order
    */
    bool operator== (const stringarray& other) const noexcept;

    /** compares two arrays.
        comparisons are case-sensitive.
        @returns    false if the other array contains exactly the same strings in the same order
    */
    bool operator!= (const stringarray& other) const noexcept;

    //==============================================================================
    /** returns the number of strings in the array */
    inline int size() const noexcept                                    { return strings.size(); };

    /** returns one of the strings from the array.

        if the index is out-of-range, an empty string is returned.

        obviously the reference returned shouldn't be stored for later use, as the
        string it refers to may disappear when the array changes.
    */
    const string& operator[] (int index) const noexcept;

    /** returns a reference to one of the strings in the array.
        this lets you modify a string in-place in the array, but you must be sure that
        the index is in-range.
    */
    string& getreference (int index) noexcept;

    /** returns a pointer to the first string in the array.
        this method is provided for compatibility with standard c++ iteration mechanisms.
    */
    inline string* begin() const noexcept
    {
        return strings.begin();
    }

    /** returns a pointer to the string which follows the last element in the array.
        this method is provided for compatibility with standard c++ iteration mechanisms.
    */
    inline string* end() const noexcept
    {
        return strings.end();
    }

    /** searches for a string in the array.

        the comparison will be case-insensitive if the ignorecase parameter is true.

        @returns    true if the string is found inside the array
    */
    bool contains (const string& stringtolookfor,
                   bool ignorecase = false) const;

    /** searches for a string in the array.

        the comparison will be case-insensitive if the ignorecase parameter is true.

        @param stringtolookfor  the string to try to find
        @param ignorecase       whether the comparison should be case-insensitive
        @param startindex       the first index to start searching from
        @returns                the index of the first occurrence of the string in this array,
                                or -1 if it isn't found.
    */
    int indexof (const string& stringtolookfor,
                 bool ignorecase = false,
                 int startindex = 0) const;

    //==============================================================================
    /** appends a string at the end of the array. */
    void add (const string& stringtoadd);

    /** inserts a string into the array.

        this will insert a string into the array at the given index, moving
        up the other elements to make room for it.
        if the index is less than zero or greater than the size of the array,
        the new string will be added to the end of the array.
    */
    void insert (int index, const string& stringtoadd);

    /** adds a string to the array as long as it's not already in there.

        the search can optionally be case-insensitive.
    */
    void addifnotalreadythere (const string& stringtoadd, bool ignorecase = false);

    /** replaces one of the strings in the array with another one.

        if the index is higher than the array's size, the new string will be
        added to the end of the array; if it's less than zero nothing happens.
    */
    void set (int index, const string& newstring);

    /** appends some strings from another array to the end of this one.

        @param other                the array to add
        @param startindex           the first element of the other array to add
        @param numelementstoadd     the maximum number of elements to add (if this is
                                    less than zero, they are all added)
    */
    void addarray (const stringarray& other,
                   int startindex = 0,
                   int numelementstoadd = -1);

    /** breaks up a string into tokens and adds them to this array.

        this will tokenise the given string using whitespace characters as the
        token delimiters, and will add these tokens to the end of the array.
        @returns    the number of tokens added
        @see fromtokens
    */
    int addtokens (const string& stringtotokenise,
                   bool preservequotedstrings);

    /** breaks up a string into tokens and adds them to this array.

        this will tokenise the given string (using the string passed in to define the
        token delimiters), and will add these tokens to the end of the array.

        @param stringtotokenise     the string to tokenise
        @param breakcharacters      a string of characters, any of which will be considered
                                    to be a token delimiter.
        @param quotecharacters      if this string isn't empty, it defines a set of characters
                                    which are treated as quotes. any text occurring
                                    between quotes is not broken up into tokens.
        @returns    the number of tokens added
        @see fromtokens
    */
    int addtokens (const string& stringtotokenise,
                   const string& breakcharacters,
                   const string& quotecharacters);

    /** breaks up a string into lines and adds them to this array.

        this breaks a string down into lines separated by \\n or \\r\\n, and adds each line
        to the array. line-break characters are omitted from the strings that are added to
        the array.
    */
    int addlines (const string& stringtobreakup);

    /** returns an array containing the tokens in a given string.

        this will tokenise the given string using whitespace characters as the
        token delimiters, and return these tokens as an array.
        @see addtokens
    */
    static stringarray fromtokens (const string& stringtotokenise,
                                   bool preservequotedstrings);

    /** returns an array containing the tokens in a given string.

        this will tokenise the given string using whitespace characters as the
        token delimiters, and return these tokens as an array.

        @param stringtotokenise     the string to tokenise
        @param breakcharacters      a string of characters, any of which will be considered
                                    to be a token delimiter.
        @param quotecharacters      if this string isn't empty, it defines a set of characters
                                    which are treated as quotes. any text occurring
                                    between quotes is not broken up into tokens.
        @see addtokens
    */
    static stringarray fromtokens (const string& stringtotokenise,
                                   const string& breakcharacters,
                                   const string& quotecharacters);

    /** returns an array containing the lines in a given string.

        this breaks a string down into lines separated by \\n or \\r\\n, and returns an
        array containing these lines. line-break characters are omitted from the strings that
        are added to the array.
    */
    static stringarray fromlines (const string& stringtobreakup);

    //==============================================================================
    /** removes all elements from the array. */
    void clear();

    /** removes a string from the array.

        if the index is out-of-range, no action will be taken.
    */
    void remove (int index);

    /** finds a string in the array and removes it.

        this will remove the first occurrence of the given string from the array. the
        comparison may be case-insensitive depending on the ignorecase parameter.
    */
    void removestring (const string& stringtoremove,
                       bool ignorecase = false);

    /** removes a range of elements from the array.

        this will remove a set of elements, starting from the given index,
        and move subsequent elements down to close the gap.

        if the range extends beyond the bounds of the array, it will
        be safely clipped to the size of the array.

        @param startindex       the index of the first element to remove
        @param numbertoremove   how many elements should be removed
    */
    void removerange (int startindex, int numbertoremove);

    /** removes any duplicated elements from the array.

        if any string appears in the array more than once, only the first occurrence of
        it will be retained.

        @param ignorecase   whether to use a case-insensitive comparison
    */
    void removeduplicates (bool ignorecase);

    /** removes empty strings from the array.

        @param removewhitespacestrings  if true, strings that only contain whitespace
                                        characters will also be removed
    */
    void removeemptystrings (bool removewhitespacestrings = true);

    /** moves one of the strings to a different position.

        this will move the string to a specified index, shuffling along
        any intervening elements as required.

        so for example, if you have the array { 0, 1, 2, 3, 4, 5 } then calling
        move (2, 4) would result in { 0, 1, 3, 4, 2, 5 }.

        @param currentindex     the index of the value to be moved. if this isn't a
                                valid index, then nothing will be done
        @param newindex         the index at which you'd like this value to end up. if this
                                is less than zero, the value will be moved to the end
                                of the array
    */
    void move (int currentindex, int newindex) noexcept;

    /** deletes any whitespace characters from the starts and ends of all the strings. */
    void trim();

    /** adds numbers to the strings in the array, to make each string unique.

        this will add numbers to the ends of groups of similar strings.
        e.g. if there are two "moose" strings, they will become "moose (1)" and "moose (2)"

        @param ignorecasewhencomparing      whether the comparison used is case-insensitive
        @param appendnumbertofirstinstance  whether the first of a group of similar strings
                                            also has a number appended to it.
        @param prenumberstring              when adding a number, this string is added before the number.
                                            if you pass 0, a default string will be used, which adds
                                            brackets around the number.
        @param postnumberstring             this string is appended after any numbers that are added.
                                            if you pass 0, a default string will be used, which adds
                                            brackets around the number.
    */
    void appendnumberstoduplicates (bool ignorecasewhencomparing,
                                    bool appendnumbertofirstinstance,
                                    charpointer_utf8 prenumberstring = charpointer_utf8 (nullptr),
                                    charpointer_utf8 postnumberstring = charpointer_utf8 (nullptr));

    //==============================================================================
    /** joins the strings in the array together into one string.

        this will join a range of elements from the array into a string, separating
        them with a given string.

        e.g. joinintostring (",") will turn an array of "a" "b" and "c" into "a,b,c".

        @param separatorstring      the string to insert between all the strings
        @param startindex           the first element to join
        @param numberofelements     how many elements to join together. if this is less
                                    than zero, all available elements will be used.
    */
    string joinintostring (const string& separatorstring,
                           int startindex = 0,
                           int numberofelements = -1) const;

    //==============================================================================
    /** sorts the array into alphabetical order.

        @param ignorecase       if true, the comparisons used will be case-sensitive.
    */
    void sort (bool ignorecase);

    //==============================================================================
    /** increases the array's internal storage to hold a minimum number of elements.

        calling this before adding a large known number of elements means that
        the array won't have to keep dynamically resizing itself as the elements
        are added, and it'll therefore be more efficient.
    */
    void ensurestorageallocated (int minnumelements);

    //==============================================================================
    /** reduces the amount of storage being used by the array.

        arrays typically allocate slightly more storage than they need, and after
        removing elements, they may have quite a lot of unused space allocated.
        this method will reduce the amount of allocated storage to a minimum.
    */
    void minimisestorageoverheads();


private:
    array <string> strings;
};

} // beast

#endif   // beast_stringarray_h_included
