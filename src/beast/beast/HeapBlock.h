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

#ifndef beast_heapblock_h_included
#define beast_heapblock_h_included

#include <cstddef>
#include <cstdlib>
#include <stdexcept>

#include <beast/memory.h>

// if the msvc debug heap headers were included, disable
// the macros during the juce include since they conflict.
#ifdef _crtdbg_map_alloc
#pragma push_macro("calloc")
#pragma push_macro("free")
#pragma push_macro("malloc")
#pragma push_macro("realloc")
#pragma push_macro("_recalloc")
#pragma push_macro("_aligned_free")
#pragma push_macro("_aligned_malloc")
#pragma push_macro("_aligned_offset_malloc")
#pragma push_macro("_aligned_realloc")
#pragma push_macro("_aligned_recalloc")
#pragma push_macro("_aligned_offset_realloc")
#pragma push_macro("_aligned_offset_recalloc")
#pragma push_macro("_aligned_msize")
#undef calloc
#undef free
#undef malloc
#undef realloc
#undef _recalloc
#undef _aligned_free
#undef _aligned_malloc
#undef _aligned_offset_malloc
#undef _aligned_realloc
#undef _aligned_recalloc
#undef _aligned_offset_realloc
#undef _aligned_offset_recalloc
#undef _aligned_msize
#endif

namespace beast {

#ifndef doxygen
namespace heapblockhelper
{
    template <bool shouldthrow>
    struct throwonfail          { static void check (void*) {} };

    template<>
    struct throwonfail <true>   { static void check (void* data) { if (data == nullptr) throw std::bad_alloc(); } };
}
#endif

//==============================================================================
/**
    very simple container class to hold a pointer to some data on the heap.

    when you need to allocate some heap storage for something, always try to use
    this class instead of allocating the memory directly using malloc/free.

    a heapblock<char> object can be treated in pretty much exactly the same way
    as an char*, but as long as you allocate it on the stack or as a class member,
    it's almost impossible for it to leak memory.

    it also makes your code much more concise and readable than doing the same thing
    using direct allocations,

    e.g. instead of this:
    @code
        int* temp = (int*) malloc (1024 * sizeof (int));
        memcpy (temp, xyz, 1024 * sizeof (int));
        free (temp);
        temp = (int*) calloc (2048 * sizeof (int));
        temp[0] = 1234;
        memcpy (foobar, temp, 2048 * sizeof (int));
        free (temp);
    @endcode

    ..you could just write this:
    @code
        heapblock <int> temp (1024);
        memcpy (temp, xyz, 1024 * sizeof (int));
        temp.calloc (2048);
        temp[0] = 1234;
        memcpy (foobar, temp, 2048 * sizeof (int));
    @endcode

    the class is extremely lightweight, containing only a pointer to the
    data, and exposes malloc/realloc/calloc/free methods that do the same jobs
    as their less object-oriented counterparts. despite adding safety, you probably
    won't sacrifice any performance by using this in place of normal pointers.

    the throwonfailure template parameter can be set to true if you'd like the class
    to throw a std::bad_alloc exception when an allocation fails. if this is false,
    then a failed allocation will just leave the heapblock with a null pointer (assuming
    that the system's malloc() function doesn't throw).

    @see array, memoryblock
*/
template <class elementtype, bool throwonfailure = false>
class heapblock
{
public:
    //==============================================================================
    /** creates a heapblock which is initially just a null pointer.

        after creation, you can resize the array using the malloc(), calloc(),
        or realloc() methods.
    */
    heapblock() noexcept
        : data (nullptr)
    {
    }

    /** creates a heapblock containing a number of elements.

        the contents of the block are undefined, as it will have been created by a
        malloc call.

        if you want an array of zero values, you can use the calloc() method or the
        other constructor that takes an initialisationstate parameter.
    */
    explicit heapblock (const size_t numelements)
        : data (static_cast <elementtype*> (std::malloc (numelements * sizeof (elementtype))))
    {
        throwonallocationfailure();
    }

    /** creates a heapblock containing a number of elements.

        the initialisetozero parameter determines whether the new memory should be cleared,
        or left uninitialised.
    */
    heapblock (const size_t numelements, const bool initialisetozero)
        : data (static_cast <elementtype*> (initialisetozero
                                               ? std::calloc (numelements, sizeof (elementtype))
                                               : std::malloc (numelements * sizeof (elementtype))))
    {
        throwonallocationfailure();
    }


    heapblock(heapblock const&) = delete;
    heapblock& operator= (heapblock const&) = delete;

    /** destructor.
        this will free the data, if any has been allocated.
    */
    ~heapblock()
    {
        std::free (data);
    }

   #if beast_compiler_supports_move_semantics
    heapblock (heapblock&& other) noexcept
        : data (other.data)
    {
        other.data = nullptr;
    }

    heapblock& operator= (heapblock&& other) noexcept
    {
        std::swap (data, other.data);
        return *this;
    }
   #endif

    //==============================================================================
    /** returns a raw pointer to the allocated data.
        this may be a null pointer if the data hasn't yet been allocated, or if it has been
        freed by calling the free() method.
    */
    inline operator elementtype*() const noexcept                           { return data; }

    /** returns a raw pointer to the allocated data.
        this may be a null pointer if the data hasn't yet been allocated, or if it has been
        freed by calling the free() method.
    */
    inline elementtype* getdata() const noexcept                            { return data; }

    /** returns a void pointer to the allocated data.
        this may be a null pointer if the data hasn't yet been allocated, or if it has been
        freed by calling the free() method.
    */
    inline operator void*() const noexcept                                  { return static_cast <void*> (data); }

    /** returns a void pointer to the allocated data.
        this may be a null pointer if the data hasn't yet been allocated, or if it has been
        freed by calling the free() method.
    */
    inline operator const void*() const noexcept                            { return static_cast <const void*> (data); }

    /** lets you use indirect calls to the first element in the array.
        obviously this will cause problems if the array hasn't been initialised, because it'll
        be referencing a null pointer.
    */
    inline elementtype* operator->() const  noexcept                        { return data; }

    /** returns a reference to one of the data elements.
        obviously there's no bounds-checking here, as this object is just a dumb pointer and
        has no idea of the size it currently has allocated.
    */
    template <typename indextype>
    inline elementtype& operator[] (indextype index) const noexcept         { return data [index]; }

    /** returns a pointer to a data element at an offset from the start of the array.
        this is the same as doing pointer arithmetic on the raw pointer itself.
    */
    template <typename indextype>
    inline elementtype* operator+ (indextype index) const noexcept          { return data + index; }

    //==============================================================================
    /** compares the pointer with another pointer.
        this can be handy for checking whether this is a null pointer.
    */
    inline bool operator== (const elementtype* const otherpointer) const noexcept   { return otherpointer == data; }

    /** compares the pointer with another pointer.
        this can be handy for checking whether this is a null pointer.
    */
    inline bool operator!= (const elementtype* const otherpointer) const noexcept   { return otherpointer != data; }

    //==============================================================================
    /** allocates a specified amount of memory.

        this uses the normal malloc to allocate an amount of memory for this object.
        any previously allocated memory will be freed by this method.

        the number of bytes allocated will be (newnumelements * elementsize). normally
        you wouldn't need to specify the second parameter, but it can be handy if you need
        to allocate a size in bytes rather than in terms of the number of elements.

        the data that is allocated will be freed when this object is deleted, or when you
        call free() or any of the allocation methods.
    */
    void malloc (const size_t newnumelements, const size_t elementsize = sizeof (elementtype))
    {
        std::free (data);
        data = static_cast <elementtype*> (std::malloc (newnumelements * elementsize));
        throwonallocationfailure();
    }

    /** allocates a specified amount of memory and clears it.
        this does the same job as the malloc() method, but clears the memory that it allocates.
    */
    void calloc (const size_t newnumelements, const size_t elementsize = sizeof (elementtype))
    {
        std::free (data);
        data = static_cast <elementtype*> (std::calloc (newnumelements, elementsize));
        throwonallocationfailure();
    }

    /** allocates a specified amount of memory and optionally clears it.
        this does the same job as either malloc() or calloc(), depending on the
        initialisetozero parameter.
    */
    void allocate (const size_t newnumelements, bool initialisetozero)
    {
        std::free (data);
        data = static_cast <elementtype*> (initialisetozero
                                             ? std::calloc (newnumelements, sizeof (elementtype))
                                             : std::malloc (newnumelements * sizeof (elementtype)));
        throwonallocationfailure();
    }

    /** re-allocates a specified amount of memory.

        the semantics of this method are the same as malloc() and calloc(), but it
        uses realloc() to keep as much of the existing data as possible.
    */
    void reallocate (const size_t newnumelements, const size_t elementsize = sizeof (elementtype))
    {
        data = static_cast <elementtype*> (data == nullptr ? std::malloc (newnumelements * elementsize)
                                                           : std::realloc (data, newnumelements * elementsize));
        throwonallocationfailure();
    }

    /** frees any currently-allocated data.
        this will free the data and reset this object to be a null pointer.
    */
    void free_up()
    {
        std::free (data);
        data = nullptr;
    }

    /** swaps this object's data with the data of another heapblock.
        the two objects simply exchange their data pointers.
    */
    template <bool otherblockthrows>
    void swapwith (heapblock <elementtype, otherblockthrows>& other) noexcept
    {
        std::swap (data, other.data);
    }

    /** this fills the block with zeros, up to the number of elements specified.
        since the block has no way of knowing its own size, you must make sure that the number of
        elements you specify doesn't exceed the allocated size.
    */
    void clear (size_t numelements) noexcept
    {
        zeromem (data, sizeof (elementtype) * numelements);
    }

    /** this typedef can be used to get the type of the heapblock's elements. */
    typedef elementtype type;

private:
    //==============================================================================
    elementtype* data;

    void throwonallocationfailure() const
    {
        heapblockhelper::throwonfail<throwonfailure>::check (data);
    }
};

}

#ifdef _crtdbg_map_alloc
#pragma pop_macro("_aligned_msize")
#pragma pop_macro("_aligned_offset_recalloc")
#pragma pop_macro("_aligned_offset_realloc")
#pragma pop_macro("_aligned_recalloc")
#pragma pop_macro("_aligned_realloc")
#pragma pop_macro("_aligned_offset_malloc")
#pragma pop_macro("_aligned_malloc")
#pragma pop_macro("_aligned_free")
#pragma pop_macro("_recalloc")
#pragma pop_macro("realloc")
#pragma pop_macro("malloc")
#pragma pop_macro("free")
#pragma pop_macro("calloc")
#endif

#endif

