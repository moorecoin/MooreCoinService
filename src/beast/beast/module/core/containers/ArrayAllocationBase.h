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

#ifndef beast_arrayallocationbase_h_included
#define beast_arrayallocationbase_h_included

#include <beast/heapblock.h>

namespace beast {

//==============================================================================
/**
    implements some basic array storage allocation functions.

    this class isn't really for public use - it's used by the other
    array classes, but might come in handy for some purposes.

    it inherits from a critical section class to allow the arrays to use
    the "empty base class optimisation" pattern to reduce their footprint.

    @see array, sharedobjectarray
*/
template <class elementtype, class typeofcriticalsectiontouse>
class arrayallocationbase
    : public typeofcriticalsectiontouse
{
public:
    //==============================================================================
    /** creates an empty array. */
    arrayallocationbase() noexcept
        : numallocated (0)
    {
    }

    /** destructor. */
    ~arrayallocationbase() noexcept
    {
    }

   #if beast_compiler_supports_move_semantics
    arrayallocationbase (arrayallocationbase<elementtype, typeofcriticalsectiontouse>&& other) noexcept
        : elements (static_cast <heapblock <elementtype>&&> (other.elements)),
          numallocated (other.numallocated)
    {
    }

    arrayallocationbase& operator= (arrayallocationbase<elementtype, typeofcriticalsectiontouse>&& other) noexcept
    {
        elements = static_cast <heapblock <elementtype>&&> (other.elements);
        numallocated = other.numallocated;
        return *this;
    }
   #endif

    //==============================================================================
    /** changes the amount of storage allocated.

        this will retain any data currently held in the array, and either add or
        remove extra space at the end.

        @param numelements  the number of elements that are needed
    */
    void setallocatedsize (const int numelements)
    {
        if (numallocated != numelements)
        {
            if (numelements > 0)
                elements.reallocate ((size_t) numelements);
            else
                elements.free_up();

            numallocated = numelements;
        }
    }

    /** increases the amount of storage allocated if it is less than a given amount.

        this will retain any data currently held in the array, but will add
        extra space at the end to make sure there it's at least as big as the size
        passed in. if it's already bigger, no action is taken.

        @param minnumelements  the minimum number of elements that are needed
    */
    void ensureallocatedsize (const int minnumelements)
    {
        if (minnumelements > numallocated)
            setallocatedsize ((minnumelements + minnumelements / 2 + 8) & ~7);

        bassert (numallocated <= 0 || elements != nullptr);
    }

    /** minimises the amount of storage allocated so that it's no more than
        the given number of elements.
    */
    void shrinktonomorethan (const int maxnumelements)
    {
        if (maxnumelements < numallocated)
            setallocatedsize (maxnumelements);
    }

    /** swap the contents of two objects. */
    void swapwith (arrayallocationbase <elementtype, typeofcriticalsectiontouse>& other) noexcept
    {
        elements.swapwith (other.elements);
        std::swap (numallocated, other.numallocated);
    }

    //==============================================================================
    heapblock <elementtype> elements;
    int numallocated;
};

} // beast

#endif   // beast_arrayallocationbase_h_included
