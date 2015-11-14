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

#ifndef beast_array_h_included
#define beast_array_h_included

#include <beast/module/core/containers/arrayallocationbase.h>
#include <beast/module/core/containers/elementcomparator.h>
#include <beast/module/core/threads/criticalsection.h>
#include <beast/arithmetic.h>

namespace beast {

//==============================================================================
/**
    holds a resizable array of primitive or copy-by-value objects.

    examples of arrays are: array<int>, array<rectangle> or array<myclass*>

    the array class can be used to hold simple, non-polymorphic objects as well as primitive types - to
    do so, the class must fulfil these requirements:
    - it must have a copy constructor and assignment operator
    - it must be able to be relocated in memory by a memcpy without this causing any problems - so
      objects whose functionality relies on external pointers or references to themselves can be used.

    for holding lists of strings, you can use array\<string\>, but it's usually better to use the
    specialised class stringarray, which provides more useful functions.

    to make all the array's methods thread-safe, pass in "criticalsection" as the templated
    typeofcriticalsectiontouse parameter, instead of the default dummycriticalsection.

    @see sharedobjectarray, stringarray, criticalsection
*/
template <typename elementtype,
          typename typeofcriticalsectiontouse = dummycriticalsection,
          int minimumallocatedsize = 0>
class array
{
private:
    typedef elementtype parametertype;

public:
    //==============================================================================
    /** creates an empty array. */
    array() noexcept
       : numused (0)
    {
    }

    /** creates a copy of another array.
        @param other    the array to copy
    */
    array (const array<elementtype, typeofcriticalsectiontouse>& other)
    {
        const scopedlocktype lock (other.getlock());
        numused = other.numused;
        data.setallocatedsize (other.numused);

        for (int i = 0; i < numused; ++i)
            new (data.elements + i) elementtype (other.data.elements[i]);
    }

   #if beast_compiler_supports_move_semantics
    array (array<elementtype, typeofcriticalsectiontouse>&& other) noexcept
        : data (static_cast <arrayallocationbase<elementtype, typeofcriticalsectiontouse>&&> (other.data)),
          numused (other.numused)
    {
        other.numused = 0;
    }
   #endif

    /** initalises from a null-terminated c array of values.

        @param values   the array to copy from
    */
    template <typename typetocreatefrom>
    explicit array (const typetocreatefrom* values)
       : numused (0)
    {
        while (*values != typetocreatefrom())
            add (*values++);
    }

    /** initalises from a c array of values.

        @param values       the array to copy from
        @param numvalues    the number of values in the array
    */
    template <typename typetocreatefrom>
    array (const typetocreatefrom* values, int numvalues)
       : numused (numvalues)
    {
        data.setallocatedsize (numvalues);

        for (int i = 0; i < numvalues; ++i)
            new (data.elements + i) elementtype (values[i]);
    }

    /** destructor. */
    ~array()
    {
        deleteallelements();
    }

    /** copies another array.
        @param other    the array to copy
    */
    array& operator= (const array& other)
    {
        if (this != &other)
        {
            array<elementtype, typeofcriticalsectiontouse> othercopy (other);
            swapwith (othercopy);
        }

        return *this;
    }

   #if beast_compiler_supports_move_semantics
    array& operator= (array&& other) noexcept
    {
        const scopedlocktype lock (getlock());
        data = static_cast <arrayallocationbase<elementtype, typeofcriticalsectiontouse>&&> (other.data);
        numused = other.numused;
        other.numused = 0;
        return *this;
    }
   #endif

    //==============================================================================
    /** compares this array to another one.
        two arrays are considered equal if they both contain the same set of
        elements, in the same order.
        @param other    the other array to compare with
    */
    template <class otherarraytype>
    bool operator== (const otherarraytype& other) const
    {
        const scopedlocktype lock (getlock());
        const typename otherarraytype::scopedlocktype lock2 (other.getlock());

        if (numused != other.numused)
            return false;

        for (int i = numused; --i >= 0;)
            if (! (data.elements [i] == other.data.elements [i]))
                return false;

        return true;
    }

    /** compares this array to another one.
        two arrays are considered equal if they both contain the same set of
        elements, in the same order.
        @param other    the other array to compare with
    */
    template <class otherarraytype>
    bool operator!= (const otherarraytype& other) const
    {
        return ! operator== (other);
    }

    //==============================================================================
    /** removes all elements from the array.
        this will remove all the elements, and free any storage that the array is
        using. to clear the array without freeing the storage, use the clearquick()
        method instead.

        @see clearquick
    */
    void clear()
    {
        const scopedlocktype lock (getlock());
        deleteallelements();
        data.setallocatedsize (0);
        numused = 0;
    }

    /** removes all elements from the array without freeing the array's allocated storage.

        @see clear
    */
    void clearquick()
    {
        const scopedlocktype lock (getlock());
        deleteallelements();
        numused = 0;
    }

    //==============================================================================
    /** returns the current number of elements in the array.
    */
    inline int size() const noexcept
    {
        return numused;
    }

    /** returns one of the elements in the array.
        if the index passed in is beyond the range of valid elements, this
        will return a default value.

        if you're certain that the index will always be a valid element, you
        can call getunchecked() instead, which is faster.

        @param index    the index of the element being requested (0 is the first element in the array)
        @see getunchecked, getfirst, getlast
    */
    elementtype operator[] (const int index) const
    {
        const scopedlocktype lock (getlock());

        if (ispositiveandbelow (index, numused))
        {
            bassert (data.elements != nullptr);
            return data.elements [index];
        }

        return elementtype();
    }

    /** returns one of the elements in the array, without checking the index passed in.

        unlike the operator[] method, this will try to return an element without
        checking that the index is within the bounds of the array, so should only
        be used when you're confident that it will always be a valid index.

        @param index    the index of the element being requested (0 is the first element in the array)
        @see operator[], getfirst, getlast
    */
    inline elementtype getunchecked (const int index) const
    {
        const scopedlocktype lock (getlock());
        bassert (ispositiveandbelow (index, numused) && data.elements != nullptr);
        return data.elements [index];
    }

    /** returns a direct reference to one of the elements in the array, without checking the index passed in.

        this is like getunchecked, but returns a direct reference to the element, so that
        you can alter it directly. obviously this can be dangerous, so only use it when
        absolutely necessary.

        @param index    the index of the element being requested (0 is the first element in the array)
        @see operator[], getfirst, getlast
    */
    inline elementtype& getreference (const int index) const noexcept
    {
        const scopedlocktype lock (getlock());
        bassert (ispositiveandbelow (index, numused) && data.elements != nullptr);
        return data.elements [index];
    }

    /** returns the first element in the array, or a default value if the array is empty.

        @see operator[], getunchecked, getlast
    */
    inline elementtype getfirst() const
    {
        const scopedlocktype lock (getlock());
        return (numused > 0) ? data.elements [0]
                             : elementtype();
    }

    /** returns the last element in the array, or a default value if the array is empty.

        @see operator[], getunchecked, getfirst
    */
    inline elementtype getlast() const
    {
        const scopedlocktype lock (getlock());
        return (numused > 0) ? data.elements [numused - 1]
                             : elementtype();
    }

    /** returns a pointer to the actual array data.
        this pointer will only be valid until the next time a non-const method
        is called on the array.
    */
    inline elementtype* getrawdatapointer() noexcept
    {
        return data.elements;
    }

    //==============================================================================
    /** returns a pointer to the first element in the array.
        this method is provided for compatibility with standard c++ iteration mechanisms.
    */
    inline elementtype* begin() const noexcept
    {
        return data.elements;
    }

    /** returns a pointer to the element which follows the last element in the array.
        this method is provided for compatibility with standard c++ iteration mechanisms.
    */
    inline elementtype* end() const noexcept
    {
        return data.elements + numused;
    }

    //==============================================================================
    /** finds the index of the first element which matches the value passed in.

        this will search the array for the given object, and return the index
        of its first occurrence. if the object isn't found, the method will return -1.

        @param elementtolookfor   the value or object to look for
        @returns                  the index of the object, or -1 if it's not found
    */
    int indexof (parametertype elementtolookfor) const
    {
        const scopedlocktype lock (getlock());
        const elementtype* e = data.elements.getdata();
        const elementtype* const end_ = e + numused;

        for (; e != end_; ++e)
            if (elementtolookfor == *e)
                return static_cast <int> (e - data.elements.getdata());

        return -1;
    }

    /** returns true if the array contains at least one occurrence of an object.

        @param elementtolookfor     the value or object to look for
        @returns                    true if the item is found
    */
    bool contains (parametertype elementtolookfor) const
    {
        const scopedlocktype lock (getlock());
        const elementtype* e = data.elements.getdata();
        const elementtype* const end_ = e + numused;

        for (; e != end_; ++e)
            if (elementtolookfor == *e)
                return true;

        return false;
    }

    //==============================================================================
    /** appends a new element at the end of the array.

        @param newelement       the new object to add to the array
        @return the index of the new item, or -1 if it already existed.
        @see set, insert, addifnotalreadythere, addsorted, addusingdefaultsort, addarray
    */
    int add (parametertype newelement)
    {
        const scopedlocktype lock (getlock());
        data.ensureallocatedsize (numused + 1);
        new (data.elements + numused++) elementtype (newelement);
        return numused;
    }

    /** inserts a new element into the array at a given position.

        if the index is less than 0 or greater than the size of the array, the
        element will be added to the end of the array.
        otherwise, it will be inserted into the array, moving all the later elements
        along to make room.

        @param indextoinsertat    the index at which the new element should be
                                  inserted (pass in -1 to add it to the end)
        @param newelement         the new object to add to the array
        @see add, addsorted, addusingdefaultsort, set
    */
    void insert (int indextoinsertat, parametertype newelement)
    {
        const scopedlocktype lock (getlock());
        data.ensureallocatedsize (numused + 1);
        bassert (data.elements != nullptr);

        if (ispositiveandbelow (indextoinsertat, numused))
        {
            elementtype* const insertpos = data.elements + indextoinsertat;
            const int numbertomove = numused - indextoinsertat;

            if (numbertomove > 0)
                memmove (insertpos + 1, insertpos, ((size_t) numbertomove) * sizeof (elementtype));

            new (insertpos) elementtype (newelement);
            ++numused;
        }
        else
        {
            new (data.elements + numused++) elementtype (newelement);
        }
    }

    /** inserts multiple copies of an element into the array at a given position.

        if the index is less than 0 or greater than the size of the array, the
        element will be added to the end of the array.
        otherwise, it will be inserted into the array, moving all the later elements
        along to make room.

        @param indextoinsertat    the index at which the new element should be inserted
        @param newelement         the new object to add to the array
        @param numberoftimestoinsertit  how many copies of the value to insert
        @see insert, add, addsorted, set
    */
    void insertmultiple (int indextoinsertat, parametertype newelement,
                         int numberoftimestoinsertit)
    {
        if (numberoftimestoinsertit > 0)
        {
            const scopedlocktype lock (getlock());
            data.ensureallocatedsize (numused + numberoftimestoinsertit);
            elementtype* insertpos;

            if (ispositiveandbelow (indextoinsertat, numused))
            {
                insertpos = data.elements + indextoinsertat;
                const int numbertomove = numused - indextoinsertat;
                memmove (insertpos + numberoftimestoinsertit, insertpos, ((size_t) numbertomove) * sizeof (elementtype));
            }
            else
            {
                insertpos = data.elements + numused;
            }

            numused += numberoftimestoinsertit;

            while (--numberoftimestoinsertit >= 0)
                new (insertpos++) elementtype (newelement);
        }
    }

    /** inserts an array of values into this array at a given position.

        if the index is less than 0 or greater than the size of the array, the
        new elements will be added to the end of the array.
        otherwise, they will be inserted into the array, moving all the later elements
        along to make room.

        @param indextoinsertat      the index at which the first new element should be inserted
        @param newelements          the new values to add to the array
        @param numberofelements     how many items are in the array
        @see insert, add, addsorted, set
    */
    void insertarray (int indextoinsertat,
                      const elementtype* newelements,
                      int numberofelements)
    {
        if (numberofelements > 0)
        {
            const scopedlocktype lock (getlock());
            data.ensureallocatedsize (numused + numberofelements);
            elementtype* insertpos = data.elements;

            if (ispositiveandbelow (indextoinsertat, numused))
            {
                insertpos += indextoinsertat;
                const int numbertomove = numused - indextoinsertat;
                memmove (insertpos + numberofelements, insertpos, numbertomove * sizeof (elementtype));
            }
            else
            {
                insertpos += numused;
            }

            numused += numberofelements;

            while (--numberofelements >= 0)
                new (insertpos++) elementtype (*newelements++);
        }
    }

    /** appends a new element at the end of the array as long as the array doesn't
        already contain it.

        if the array already contains an element that matches the one passed in, nothing
        will be done.

        @param newelement   the new object to add to the array
        @return the index of the new item, or -1 if it already existed.
    */
    int addifnotalreadythere (parametertype newelement)
    {
        const scopedlocktype lock (getlock());

        if (! contains (newelement))
            return add (newelement);

        return -1;
    }

    /** replaces an element with a new value.

        if the index is less than zero, this method does nothing.
        if the index is beyond the end of the array, the item is added to the end of the array.

        @param indextochange    the index whose value you want to change
        @param newvalue         the new value to set for this index.
        @see add, insert
    */
    void set (const int indextochange, parametertype newvalue)
    {
        bassert (indextochange >= 0);
        const scopedlocktype lock (getlock());

        if (ispositiveandbelow (indextochange, numused))
        {
            data.elements [indextochange] = newvalue;
        }
        else if (indextochange >= 0)
        {
            data.ensureallocatedsize (numused + 1);
            new (data.elements + numused++) elementtype (newvalue);
        }
    }

    /** replaces an element with a new value without doing any bounds-checking.

        this just sets a value directly in the array's internal storage, so you'd
        better make sure it's in range!

        @param indextochange    the index whose value you want to change
        @param newvalue         the new value to set for this index.
        @see set, getunchecked
    */
    void setunchecked (const int indextochange, parametertype newvalue)
    {
        const scopedlocktype lock (getlock());
        bassert (ispositiveandbelow (indextochange, numused));
        data.elements [indextochange] = newvalue;
    }

    /** adds elements from an array to the end of this array.

        @param elementstoadd        the array of elements to add
        @param numelementstoadd     how many elements are in this other array
        @see add
    */
    void addarray (const elementtype* elementstoadd, int numelementstoadd)
    {
        const scopedlocktype lock (getlock());

        if (numelementstoadd > 0)
        {
            data.ensureallocatedsize (numused + numelementstoadd);

            while (--numelementstoadd >= 0)
            {
                new (data.elements + numused) elementtype (*elementstoadd++);
                ++numused;
            }
        }
    }

    /** this swaps the contents of this array with those of another array.

        if you need to exchange two arrays, this is vastly quicker than using copy-by-value
        because it just swaps their internal pointers.
    */
    template <class otherarraytype>
    void swapwith (otherarraytype& otherarray) noexcept
    {
        const scopedlocktype lock1 (getlock());
        const typename otherarraytype::scopedlocktype lock2 (otherarray.getlock());

        data.swapwith (otherarray.data);
        std::swap (numused, otherarray.numused);
    }

    /** adds elements from another array to the end of this array.

        @param arraytoaddfrom       the array from which to copy the elements
        @param startindex           the first element of the other array to start copying from
        @param numelementstoadd     how many elements to add from the other array. if this
                                    value is negative or greater than the number of available elements,
                                    all available elements will be copied.
        @see add
    */
    template <class otherarraytype>
    void addarray (const otherarraytype& arraytoaddfrom,
                   int startindex = 0,
                   int numelementstoadd = -1)
    {
        const typename otherarraytype::scopedlocktype lock1 (arraytoaddfrom.getlock());

        {
            const scopedlocktype lock2 (getlock());

            if (startindex < 0)
            {
                bassertfalse;
                startindex = 0;
            }

            if (numelementstoadd < 0 || startindex + numelementstoadd > arraytoaddfrom.size())
                numelementstoadd = arraytoaddfrom.size() - startindex;

            while (--numelementstoadd >= 0)
                add (arraytoaddfrom.getunchecked (startindex++));
        }
    }

    /** this will enlarge or shrink the array to the given number of elements, by adding
        or removing items from its end.

        if the array is smaller than the given target size, empty elements will be appended
        until its size is as specified. if its size is larger than the target, items will be
        removed from its end to shorten it.
    */
    void resize (const int targetnumitems)
    {
        bassert (targetnumitems >= 0);

        const int numtoadd = targetnumitems - numused;
        if (numtoadd > 0)
            insertmultiple (numused, elementtype(), numtoadd);
        else if (numtoadd < 0)
            removerange (targetnumitems, -numtoadd);
    }

    /** inserts a new element into the array, assuming that the array is sorted.

        this will use a comparator to find the position at which the new element
        should go. if the array isn't sorted, the behaviour of this
        method will be unpredictable.

        @param comparator   the comparator to use to compare the elements - see the sort()
                            method for details about the form this object should take
        @param newelement   the new element to insert to the array
        @returns the index at which the new item was added
        @see addusingdefaultsort, add, sort
    */
    template <class elementcomparator>
    int addsorted (elementcomparator& comparator, parametertype newelement)
    {
        const scopedlocktype lock (getlock());
        const int index = findinsertindexinsortedarray (comparator, data.elements.getdata(), newelement, 0, numused);
        insert (index, newelement);
        return index;
    }

    /** inserts a new element into the array, assuming that the array is sorted.

        this will use the defaultelementcomparator class for sorting, so your elementtype
        must be suitable for use with that class. if the array isn't sorted, the behaviour of this
        method will be unpredictable.

        @param newelement   the new element to insert to the array
        @see addsorted, sort
    */
    void addusingdefaultsort (parametertype newelement)
    {
        defaultelementcomparator <elementtype> comparator;
        addsorted (comparator, newelement);
    }

    /** finds the index of an element in the array, assuming that the array is sorted.

        this will use a comparator to do a binary-chop to find the index of the given
        element, if it exists. if the array isn't sorted, the behaviour of this
        method will be unpredictable.

        @param comparator           the comparator to use to compare the elements - see the sort()
                                    method for details about the form this object should take
        @param elementtolookfor     the element to search for
        @returns                    the index of the element, or -1 if it's not found
        @see addsorted, sort
    */
    template <typename elementcomparator, typename targetvaluetype>
    int indexofsorted (elementcomparator& comparator, targetvaluetype elementtolookfor) const
    {
        (void) comparator;  // if you pass in an object with a static compareelements() method, this
                            // avoids getting warning messages about the parameter being unused

        const scopedlocktype lock (getlock());

        for (int s = 0, e = numused;;)
        {
            if (s >= e)
                return -1;

            if (comparator.compareelements (elementtolookfor, data.elements [s]) == 0)
                return s;

            const int halfway = (s + e) / 2;
            if (halfway == s)
                return -1;

            if (comparator.compareelements (elementtolookfor, data.elements [halfway]) >= 0)
                s = halfway;
            else
                e = halfway;
        }
    }

    //==============================================================================
    /** removes an element from the array.

        this will remove the element at a given index, and move back
        all the subsequent elements to close the gap.
        if the index passed in is out-of-range, nothing will happen.

        @param indextoremove    the index of the element to remove
        @returns                the element that has been removed
        @see removevalue, removerange
    */
    elementtype remove (const int indextoremove)
    {
        const scopedlocktype lock (getlock());

        if (ispositiveandbelow (indextoremove, numused))
        {
            bassert (data.elements != nullptr); 
            elementtype removed (data.elements[indextoremove]);
            removeinternal (indextoremove);
            return removed;
        }

        return elementtype();
    }

    /** removes an item from the array.

        this will remove the first occurrence of the given element from the array.
        if the item isn't found, no action is taken.

        @param valuetoremove   the object to try to remove
        @see remove, removerange
    */
    void removefirstmatchingvalue (parametertype valuetoremove)
    {
        const scopedlocktype lock (getlock());
        elementtype* const e = data.elements;

        for (int i = 0; i < numused; ++i)
        {
            if (valuetoremove == e[i])
            {
                removeinternal (i);
                break;
            }
        }
    }

    /** removes an item from the array.

        this will remove the first occurrence of the given element from the array.
        if the item isn't found, no action is taken.

        @param valuetoremove   the object to try to remove
        @see remove, removerange
    */
    void removeallinstancesof (parametertype valuetoremove)
    {
        const scopedlocktype lock (getlock());

        for (int i = numused; --i >= 0;)
            if (valuetoremove == data.elements[i])
                removeinternal (i);
    }

    /** removes a range of elements from the array.

        this will remove a set of elements, starting from the given index,
        and move subsequent elements down to close the gap.

        if the range extends beyond the bounds of the array, it will
        be safely clipped to the size of the array.

        @param startindex       the index of the first element to remove
        @param numbertoremove   how many elements should be removed
        @see remove, removefirstmatchingvalue, removeallinstancesof
    */
    void removerange (int startindex, int numbertoremove)
    {
        const scopedlocktype lock (getlock());
        const int endindex = blimit (0, numused, startindex + numbertoremove);
        startindex = blimit (0, numused, startindex);

        if (endindex > startindex)
        {
            elementtype* const e = data.elements + startindex;

            numbertoremove = endindex - startindex;
            for (int i = 0; i < numbertoremove; ++i)
                e[i].~elementtype();

            const int numtoshift = numused - endindex;
            if (numtoshift > 0)
                memmove (e, e + numbertoremove, ((size_t) numtoshift) * sizeof (elementtype));

            numused -= numbertoremove;
            minimisestorageafterremoval();
        }
    }

    /** removes the last n elements from the array.

        @param howmanytoremove   how many elements to remove from the end of the array
        @see remove, removefirstmatchingvalue, removeallinstancesof, removerange
    */
    void removelast (int howmanytoremove = 1)
    {
        const scopedlocktype lock (getlock());

        if (howmanytoremove > numused)
            howmanytoremove = numused;

        for (int i = 1; i <= howmanytoremove; ++i)
            data.elements [numused - i].~elementtype();

        numused -= howmanytoremove;
        minimisestorageafterremoval();
    }

    /** removes any elements which are also in another array.

        @param otherarray   the other array in which to look for elements to remove
        @see removevaluesnotin, remove, removefirstmatchingvalue, removeallinstancesof, removerange
    */
    template <class otherarraytype>
    void removevaluesin (const otherarraytype& otherarray)
    {
        const typename otherarraytype::scopedlocktype lock1 (otherarray.getlock());
        const scopedlocktype lock2 (getlock());

        if (this == &otherarray)
        {
            clear();
        }
        else
        {
            if (otherarray.size() > 0)
            {
                for (int i = numused; --i >= 0;)
                    if (otherarray.contains (data.elements [i]))
                        removeinternal (i);
            }
        }
    }

    /** removes any elements which are not found in another array.

        only elements which occur in this other array will be retained.

        @param otherarray    the array in which to look for elements not to remove
        @see removevaluesin, remove, removefirstmatchingvalue, removeallinstancesof, removerange
    */
    template <class otherarraytype>
    void removevaluesnotin (const otherarraytype& otherarray)
    {
        const typename otherarraytype::scopedlocktype lock1 (otherarray.getlock());
        const scopedlocktype lock2 (getlock());

        if (this != &otherarray)
        {
            if (otherarray.size() <= 0)
            {
                clear();
            }
            else
            {
                for (int i = numused; --i >= 0;)
                    if (! otherarray.contains (data.elements [i]))
                        removeinternal (i);
            }
        }
    }

    /** swaps over two elements in the array.

        this swaps over the elements found at the two indexes passed in.
        if either index is out-of-range, this method will do nothing.

        @param index1   index of one of the elements to swap
        @param index2   index of the other element to swap
    */
    void swap (const int index1,
               const int index2)
    {
        const scopedlocktype lock (getlock());

        if (ispositiveandbelow (index1, numused)
             && ispositiveandbelow (index2, numused))
        {
            std::swap (data.elements [index1],
                       data.elements [index2]);
        }
    }

    /** moves one of the values to a different position.

        this will move the value to a specified index, shuffling along
        any intervening elements as required.

        so for example, if you have the array { 0, 1, 2, 3, 4, 5 } then calling
        move (2, 4) would result in { 0, 1, 3, 4, 2, 5 }.

        @param currentindex     the index of the value to be moved. if this isn't a
                                valid index, then nothing will be done
        @param newindex         the index at which you'd like this value to end up. if this
                                is less than zero, the value will be moved to the end
                                of the array
    */
    void move (const int currentindex, int newindex) noexcept
    {
        if (currentindex != newindex)
        {
            const scopedlocktype lock (getlock());

            if (ispositiveandbelow (currentindex, numused))
            {
                if (! ispositiveandbelow (newindex, numused))
                    newindex = numused - 1;

                char tempcopy [sizeof (elementtype)];
                memcpy (tempcopy, data.elements + currentindex, sizeof (elementtype));

                if (newindex > currentindex)
                {
                    memmove (data.elements + currentindex,
                             data.elements + currentindex + 1,
                             sizeof (elementtype) * (size_t) (newindex - currentindex));
                }
                else
                {
                    memmove (data.elements + newindex + 1,
                             data.elements + newindex,
                             sizeof (elementtype) * (size_t) (currentindex - newindex));
                }

                memcpy (data.elements + newindex, tempcopy, sizeof (elementtype));
            }
        }
    }

    //==============================================================================
    /** reduces the amount of storage being used by the array.

        arrays typically allocate slightly more storage than they need, and after
        removing elements, they may have quite a lot of unused space allocated.
        this method will reduce the amount of allocated storage to a minimum.
    */
    void minimisestorageoverheads()
    {
        const scopedlocktype lock (getlock());
        data.shrinktonomorethan (numused);
    }

    /** increases the array's internal storage to hold a minimum number of elements.

        calling this before adding a large known number of elements means that
        the array won't have to keep dynamically resizing itself as the elements
        are added, and it'll therefore be more efficient.
    */
    void ensurestorageallocated (const int minnumelements)
    {
        const scopedlocktype lock (getlock());
        data.ensureallocatedsize (minnumelements);
    }

    //==============================================================================
    /** sorts the elements in the array.

        this will use a comparator object to sort the elements into order. the object
        passed must have a method of the form:
        @code
        int compareelements (elementtype first, elementtype second);
        @endcode

        ..and this method must return:
          - a value of < 0 if the first comes before the second
          - a value of 0 if the two objects are equivalent
          - a value of > 0 if the second comes before the first

        to improve performance, the compareelements() method can be declared as static or const.

        @param comparator   the comparator to use for comparing elements.
        @param retainorderofequivalentitems     if this is true, then items
                            which the comparator says are equivalent will be
                            kept in the order in which they currently appear
                            in the array. this is slower to perform, but may
                            be important in some cases. if it's false, a faster
                            algorithm is used, but equivalent elements may be
                            rearranged.

        @see addsorted, indexofsorted, sortarray
    */
    template <class elementcomparator>
    void sort (elementcomparator& comparator,
               const bool retainorderofequivalentitems = false) const
    {
        const scopedlocktype lock (getlock());
        (void) comparator;  // if you pass in an object with a static compareelements() method, this
                            // avoids getting warning messages about the parameter being unused
        sortarray (comparator, data.elements.getdata(), 0, size() - 1, retainorderofequivalentitems);
    }

    //==============================================================================
    /** returns the criticalsection that locks this array.
        to lock, you can call getlock().enter() and getlock().exit(), or preferably use
        an object of scopedlocktype as an raii lock for it.
    */
    inline const typeofcriticalsectiontouse& getlock() const noexcept      { return data; }

    /** returns the type of scoped lock to use for locking this array */
    typedef typename typeofcriticalsectiontouse::scopedlocktype scopedlocktype;


private:
    //==============================================================================
    arrayallocationbase <elementtype, typeofcriticalsectiontouse> data;
    int numused;

    void removeinternal (const int indextoremove)
    {
        --numused;
        elementtype* const e = data.elements + indextoremove;
        e->~elementtype();
        const int numbertoshift = numused - indextoremove;

        if (numbertoshift > 0)
            memmove (e, e + 1, ((size_t) numbertoshift) * sizeof (elementtype));

        minimisestorageafterremoval();
    }

    inline void deleteallelements() noexcept
    {
        for (int i = 0; i < numused; ++i)
            data.elements[i].~elementtype();
    }

    void minimisestorageafterremoval()
    {
        if (data.numallocated > std::max (minimumallocatedsize, numused * 2))
            data.shrinktonomorethan (std::max (numused, std::max (minimumallocatedsize, 64 / (int) sizeof (elementtype))));
    }
};

} // beast

#endif   // beast_array_h_included
