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

#ifndef beast_stringpairarray_h_included
#define beast_stringpairarray_h_included

#include <beast/module/core/text/stringarray.h>

namespace beast {

//==============================================================================
/**
    a container for holding a set of strings which are keyed by another string.

    @see stringarray
*/
class stringpairarray
{
public:
    //==============================================================================
    /** creates an empty array */
    stringpairarray (bool ignorecasewhencomparingkeys = true);

    /** creates a copy of another array */
    stringpairarray (const stringpairarray& other);

    /** destructor. */
    ~stringpairarray();

    /** copies the contents of another string array into this one */
    stringpairarray& operator= (const stringpairarray& other);

    /** swap the contents of this array with another. */
    void swapwith (stringpairarray& other);

    //==============================================================================
    /** compares two arrays.
        comparisons are case-sensitive.
        @returns    true only if the other array contains exactly the same strings with the same keys
    */
    bool operator== (const stringpairarray& other) const;

    /** compares two arrays.
        comparisons are case-sensitive.
        @returns    false if the other array contains exactly the same strings with the same keys
    */
    bool operator!= (const stringpairarray& other) const;

    //==============================================================================
    /** finds the value corresponding to a key string.

        if no such key is found, this will just return an empty string. to check whether
        a given key actually exists (because it might actually be paired with an empty string), use
        the getallkeys() method to obtain a list.

        obviously the reference returned shouldn't be stored for later use, as the
        string it refers to may disappear when the array changes.

        @see getvalue
    */
    const string& operator[] (const string& key) const;

    /** finds the value corresponding to a key string.

        if no such key is found, this will just return the value provided as a default.

        @see operator[]
    */
    string getvalue (const string& key, const string& defaultreturnvalue) const;


    /** returns a list of all keys in the array. */
    const stringarray& getallkeys() const noexcept          { return keys; }

    /** returns a list of all values in the array. */
    const stringarray& getallvalues() const noexcept        { return values; }

    /** returns the number of strings in the array */
    inline int size() const noexcept                        { return keys.size(); };


    //==============================================================================
    /** adds or amends a key/value pair.

        if a value already exists with this key, its value will be overwritten,
        otherwise the key/value pair will be added to the array.
    */
    void set (const string& key, const string& value);

    /** adds the items from another array to this one.

        this is equivalent to using set() to add each of the pairs from the other array.
    */
    void addarray (const stringpairarray& other);

    //==============================================================================
    /** removes all elements from the array. */
    void clear();

    /** removes a string from the array based on its key.

        if the key isn't found, nothing will happen.
    */
    void remove (const string& key);

    /** removes a string from the array based on its index.

        if the index is out-of-range, no action will be taken.
    */
    void remove (int index);

    //==============================================================================
    /** indicates whether to use a case-insensitive search when looking up a key string.
    */
    void setignorescase (bool shouldignorecase);

    //==============================================================================
    /** returns a descriptive string containing the items.
        this is handy for dumping the contents of an array.
    */
    string getdescription() const;

    //==============================================================================
    /** reduces the amount of storage being used by the array.

        arrays typically allocate slightly more storage than they need, and after
        removing elements, they may have quite a lot of unused space allocated.
        this method will reduce the amount of allocated storage to a minimum.
    */
    void minimisestorageoverheads();


private:
    //==============================================================================
    stringarray keys, values;
    bool ignorecase;
};

} // beast

#endif   // beast_stringpairarray_h_included
