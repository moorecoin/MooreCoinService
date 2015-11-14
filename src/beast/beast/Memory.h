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

#ifndef beast_memory_h_included
#define beast_memory_h_included

#include <cstring>
    
#include <beast/config.h>

namespace beast {

//==============================================================================
/** fills a block of memory with zeros. */
inline void zeromem (void* memory, size_t numbytes) noexcept
    { memset (memory, 0, numbytes); }

/** overwrites a structure or object with zeros. */
template <typename type>
void zerostruct (type& structure) noexcept
    { memset (&structure, 0, sizeof (structure)); }

/** delete an object pointer, and sets the pointer to null.

    remember that it's not good c++ practice to use delete directly - always try to use a std::unique_ptr
    or other automatic lifetime-management system rather than resorting to deleting raw pointers!
*/
template <typename type>
void deleteandzero (type& pointer)
    { delete pointer; pointer = nullptr; }

/** a handy function which adds a number of bytes to any type of pointer and returns the result.
    this can be useful to avoid casting pointers to a char* and back when you want to move them by
    a specific number of bytes,
*/
template <typename type, typename integertype>
type* addbytestopointer (type* pointer, integertype bytes) noexcept
    { return (type*) (((char*) pointer) + bytes); }

/** a handy function which returns the difference between any two pointers, in bytes.
    the address of the second pointer is subtracted from the first, and the difference in bytes is returned.
*/
template <typename type1, typename type2>
int getaddressdifference (type1* pointer1, type2* pointer2) noexcept
    { return (int) (((const char*) pointer1) - (const char*) pointer2); }

/** if a pointer is non-null, this returns a new copy of the object that it points to, or safely returns
    nullptr if the pointer is null.
*/
template <class type>
type* createcopyifnotnull (const type* pointer)
    { return pointer != nullptr ? new type (*pointer) : nullptr; }

//==============================================================================
#if beast_mac || beast_ios || doxygen

 /** a handy c++ wrapper that creates and deletes an nsautoreleasepool object using raii.
     you should use the beast_autoreleasepool macro to create a local auto-release pool on the stack.
 */
 class scopedautoreleasepool
 {
 public:
     scopedautoreleasepool();
     ~scopedautoreleasepool();


    scopedautoreleasepool(scopedautoreleasepool const&) = delete;
    scopedautoreleasepool& operator= (scopedautoreleasepool const&) = delete;
    
 private:
     void* pool;
 };

 /** a macro that can be used to easily declare a local scopedautoreleasepool
     object for raii-based obj-c autoreleasing.
     because this may use the \@autoreleasepool syntax, you must follow the macro with
     a set of braces to mark the scope of the pool.
 */
#if (beast_compiler_supports_arc && defined (__objc__)) || doxygen
 #define beast_autoreleasepool  @autoreleasepool
#else
 #define beast_autoreleasepool  const beast::scopedautoreleasepool beast_join_macro (autoreleasepool_, __line__);
#endif

#else
 #define beast_autoreleasepool
#endif

}

#endif

