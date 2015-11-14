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

#ifndef beast_elementcomparator_h_included
#define beast_elementcomparator_h_included

#include <algorithm>

namespace beast {

#ifndef doxygen

/** this is an internal helper class which converts a beast elementcomparator style
    class (using a "compareelements" method) into a class that's compatible with
    std::sort (i.e. using an operator() to compare the elements)
*/
template <typename elementcomparator>
struct sortfunctionconverter
{
    sortfunctionconverter (elementcomparator& e) : comparator (e) {}

    template <typename type>
    bool operator() (type a, type b)  { return comparator.compareelements (a, b) < 0; }

private:
    elementcomparator& comparator;
    sortfunctionconverter& operator= (const sortfunctionconverter&);
};

#endif

//==============================================================================
/**
    sorts a range of elements in an array.

    the comparator object that is passed-in must define a public method with the following
    signature:
    @code
    int compareelements (elementtype first, elementtype second);
    @endcode

    ..and this method must return:
      - a value of < 0 if the first comes before the second
      - a value of 0 if the two objects are equivalent
      - a value of > 0 if the second comes before the first

    to improve performance, the compareelements() method can be declared as static or const.

    @param comparator       an object which defines a compareelements() method
    @param array            the array to sort
    @param firstelement     the index of the first element of the range to be sorted
    @param lastelement      the index of the last element in the range that needs
                            sorting (this is inclusive)
    @param retainorderofequivalentitems     if true, the order of items that the
                            comparator deems the same will be maintained - this will be
                            a slower algorithm than if they are allowed to be moved around.

    @see sortarrayretainingorder
*/
template <class elementtype, class elementcomparator>
static void sortarray (elementcomparator& comparator,
                       elementtype* const array,
                       int firstelement,
                       int lastelement,
                       const bool retainorderofequivalentitems)
{
    sortfunctionconverter<elementcomparator> converter (comparator);

    if (retainorderofequivalentitems)
        std::stable_sort (array + firstelement, array + lastelement + 1, converter);
    else
        std::sort        (array + firstelement, array + lastelement + 1, converter);
}


//==============================================================================
/**
    searches a sorted array of elements, looking for the index at which a specified value
    should be inserted for it to be in the correct order.

    the comparator object that is passed-in must define a public method with the following
    signature:
    @code
    int compareelements (elementtype first, elementtype second);
    @endcode

    ..and this method must return:
      - a value of < 0 if the first comes before the second
      - a value of 0 if the two objects are equivalent
      - a value of > 0 if the second comes before the first

    to improve performance, the compareelements() method can be declared as static or const.

    @param comparator       an object which defines a compareelements() method
    @param array            the array to search
    @param newelement       the value that is going to be inserted
    @param firstelement     the index of the first element to search
    @param lastelement      the index of the last element in the range (this is non-inclusive)
*/
template <class elementtype, class elementcomparator>
static int findinsertindexinsortedarray (elementcomparator& comparator,
                                         elementtype* const array,
                                         const elementtype newelement,
                                         int firstelement,
                                         int lastelement)
{
    bassert (firstelement <= lastelement);

    (void) comparator;  // if you pass in an object with a static compareelements() method, this
                        // avoids getting warning messages about the parameter being unused

    while (firstelement < lastelement)
    {
        if (comparator.compareelements (newelement, array [firstelement]) == 0)
        {
            ++firstelement;
            break;
        }
        else
        {
            const int halfway = (firstelement + lastelement) >> 1;

            if (halfway == firstelement)
            {
                if (comparator.compareelements (newelement, array [halfway]) >= 0)
                    ++firstelement;

                break;
            }
            else if (comparator.compareelements (newelement, array [halfway]) >= 0)
            {
                firstelement = halfway;
            }
            else
            {
                lastelement = halfway;
            }
        }
    }

    return firstelement;
}

//==============================================================================
/**
    a simple elementcomparator class that can be used to sort an array of
    objects that support the '<' operator.

    this will work for primitive types and objects that implement operator<().

    example: @code
    array <int> myarray;
    defaultelementcomparator<int> sorter;
    myarray.sort (sorter);
    @endcode

    @see elementcomparator
*/
template <class elementtype>
class defaultelementcomparator
{
private:
    typedef elementtype parametertype;

public:
    static int compareelements (parametertype first, parametertype second)
    {
        return (first < second) ? -1 : ((second < first) ? 1 : 0);
    }
};

} // beast

#endif

