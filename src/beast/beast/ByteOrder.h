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

#ifndef beast_byteorder_h_included
#define beast_byteorder_h_included

#include <beast/config.h>

#include <cstdint>

namespace beast {

//==============================================================================
/** contains static methods for converting the byte order between different
    endiannesses.
*/
class byteorder
{
public:
    //==============================================================================
    /** swaps the upper and lower bytes of a 16-bit integer. */
    static std::uint16_t swap (std::uint16_t value);

    /** reverses the order of the 4 bytes in a 32-bit integer. */
    static std::uint32_t swap (std::uint32_t value);

    /** reverses the order of the 8 bytes in a 64-bit integer. */
    static std::uint64_t swap (std::uint64_t value);

    //==============================================================================
    /** swaps the byte order of a 16-bit int if the cpu is big-endian */
    static std::uint16_t swapifbigendian (std::uint16_t value);

    /** swaps the byte order of a 32-bit int if the cpu is big-endian */
    static std::uint32_t swapifbigendian (std::uint32_t value);

    /** swaps the byte order of a 64-bit int if the cpu is big-endian */
    static std::uint64_t swapifbigendian (std::uint64_t value);

    /** swaps the byte order of a 16-bit int if the cpu is little-endian */
    static std::uint16_t swapiflittleendian (std::uint16_t value);

    /** swaps the byte order of a 32-bit int if the cpu is little-endian */
    static std::uint32_t swapiflittleendian (std::uint32_t value);

    /** swaps the byte order of a 64-bit int if the cpu is little-endian */
    static std::uint64_t swapiflittleendian (std::uint64_t value);

    //==============================================================================
    /** turns 2 bytes into a little-endian integer. */
    static std::uint16_t littleendianshort (const void* bytes);

    /** turns 4 bytes into a little-endian integer. */
    static std::uint32_t littleendianint (const void* bytes);

    /** turns 4 bytes into a little-endian integer. */
    static std::uint64_t littleendianint64 (const void* bytes);

    /** turns 2 bytes into a big-endian integer. */
    static std::uint16_t bigendianshort (const void* bytes);

    /** turns 4 bytes into a big-endian integer. */
    static std::uint32_t bigendianint (const void* bytes);

    /** turns 4 bytes into a big-endian integer. */
    static std::uint64_t bigendianint64 (const void* bytes);

    //==============================================================================
    /** converts 3 little-endian bytes into a signed 24-bit value (which is sign-extended to 32 bits). */
    static int littleendian24bit (const char* bytes);

    /** converts 3 big-endian bytes into a signed 24-bit value (which is sign-extended to 32 bits). */
    static int bigendian24bit (const char* bytes);

    /** copies a 24-bit number to 3 little-endian bytes. */
    static void littleendian24bittochars (int value, char* destbytes);

    /** copies a 24-bit number to 3 big-endian bytes. */
    static void bigendian24bittochars (int value, char* destbytes);

    //==============================================================================
    /** returns true if the current cpu is big-endian. */
    static bool isbigendian();

private:
    byteorder();
    byteorder(byteorder const&) = delete;
    byteorder& operator= (byteorder const&) = delete;
};

//==============================================================================
#if beast_use_intrinsics && ! defined (__intel_compiler)
 #pragma intrinsic (_byteswap_ulong)
#endif

inline std::uint16_t byteorder::swap (std::uint16_t n)
{
   #if beast_use_intrinsicsxxx // agh - the ms compiler has an internal error when you try to use this intrinsic!
    return static_cast <std::uint16_t> (_byteswap_ushort (n));
   #else
    return static_cast <std::uint16_t> ((n << 8) | (n >> 8));
   #endif
}

inline std::uint32_t byteorder::swap (std::uint32_t n)
{
   #if beast_mac || beast_ios
    return osswapint32 (n);
   #elif beast_gcc && beast_intel && ! beast_no_inline_asm
    asm("bswap %%eax" : "=a"(n) : "a"(n));
    return n;
   #elif beast_use_intrinsics
    return _byteswap_ulong (n);
   #elif beast_msvc && ! beast_no_inline_asm
    __asm {
        mov eax, n
        bswap eax
        mov n, eax
    }
    return n;
   #elif beast_android
    return bswap_32 (n);
   #else
    return (n << 24) | (n >> 24) | ((n & 0xff00) << 8) | ((n & 0xff0000) >> 8);
   #endif
}

inline std::uint64_t byteorder::swap (std::uint64_t value)
{
   #if beast_mac || beast_ios
    return osswapint64 (value);
   #elif beast_use_intrinsics
    return _byteswap_uint64 (value);
   #else
    return (((std::int64_t) swap ((std::uint32_t) value)) << 32) | swap ((std::uint32_t) (value >> 32));
   #endif
}

#if beast_little_endian
 inline std::uint16_t byteorder::swapifbigendian (const std::uint16_t v)                                  { return v; }
 inline std::uint32_t byteorder::swapifbigendian (const std::uint32_t v)                                  { return v; }
 inline std::uint64_t byteorder::swapifbigendian (const std::uint64_t v)                                  { return v; }
 inline std::uint16_t byteorder::swapiflittleendian (const std::uint16_t v)                               { return swap (v); }
 inline std::uint32_t byteorder::swapiflittleendian (const std::uint32_t v)                               { return swap (v); }
 inline std::uint64_t byteorder::swapiflittleendian (const std::uint64_t v)                               { return swap (v); }
 inline std::uint16_t byteorder::littleendianshort (const void* const bytes)                       { return *static_cast <const std::uint16_t*> (bytes); }
 inline std::uint32_t byteorder::littleendianint (const void* const bytes)                         { return *static_cast <const std::uint32_t*> (bytes); }
 inline std::uint64_t byteorder::littleendianint64 (const void* const bytes)                       { return *static_cast <const std::uint64_t*> (bytes); }
 inline std::uint16_t byteorder::bigendianshort (const void* const bytes)                          { return swap (*static_cast <const std::uint16_t*> (bytes)); }
 inline std::uint32_t byteorder::bigendianint (const void* const bytes)                            { return swap (*static_cast <const std::uint32_t*> (bytes)); }
 inline std::uint64_t byteorder::bigendianint64 (const void* const bytes)                          { return swap (*static_cast <const std::uint64_t*> (bytes)); }
 inline bool byteorder::isbigendian()                                                       { return false; }
#else
 inline std::uint16_t byteorder::swapifbigendian (const std::uint16_t v)                                  { return swap (v); }
 inline std::uint32_t byteorder::swapifbigendian (const std::uint32_t v)                                  { return swap (v); }
 inline std::uint64_t byteorder::swapifbigendian (const std::uint64_t v)                                  { return swap (v); }
 inline std::uint16_t byteorder::swapiflittleendian (const std::uint16_t v)                               { return v; }
 inline std::uint32_t byteorder::swapiflittleendian (const std::uint32_t v)                               { return v; }
 inline std::uint64_t byteorder::swapiflittleendian (const std::uint64_t v)                               { return v; }
 inline std::uint32_t byteorder::littleendianint (const void* const bytes)                         { return swap (*static_cast <const std::uint32_t*> (bytes)); }
 inline std::uint16_t byteorder::littleendianshort (const void* const bytes)                       { return swap (*static_cast <const std::uint16_t*> (bytes)); }
 inline std::uint16_t byteorder::bigendianshort (const void* const bytes)                          { return *static_cast <const std::uint16_t*> (bytes); }
 inline std::uint32_t byteorder::bigendianint (const void* const bytes)                            { return *static_cast <const std::uint32_t*> (bytes); }
 inline std::uint64_t byteorder::bigendianint64 (const void* const bytes)                          { return *static_cast <const std::uint64_t*> (bytes); }
 inline bool byteorder::isbigendian()                                                       { return true; }
#endif

inline int  byteorder::littleendian24bit (const char* const bytes)                          { return (((int) bytes[2]) << 16) | (((int) (std::uint8_t) bytes[1]) << 8) | ((int) (std::uint8_t) bytes[0]); }
inline int  byteorder::bigendian24bit (const char* const bytes)                             { return (((int) bytes[0]) << 16) | (((int) (std::uint8_t) bytes[1]) << 8) | ((int) (std::uint8_t) bytes[2]); }
inline void byteorder::littleendian24bittochars (const int value, char* const destbytes)    { destbytes[0] = (char)(value & 0xff); destbytes[1] = (char)((value >> 8) & 0xff); destbytes[2] = (char)((value >> 16) & 0xff); }
inline void byteorder::bigendian24bittochars (const int value, char* const destbytes)       { destbytes[0] = (char)((value >> 16) & 0xff); destbytes[1] = (char)((value >> 8) & 0xff); destbytes[2] = (char)(value & 0xff); }

namespace detail
{

/** specialized helper class template for swapping bytes.
    
    normally you won't use this directly, use the helper function
    byteswap instead. you can specialize this class for your
    own user defined types, as was done for uint24.

    @see swapbytes, uint24
*/
template <typename integraltype>
struct swapbytes
{
    inline integraltype operator() (integraltype value) const noexcept 
    {
        return byteorder::swap (value);
    }
};

// specializations for signed integers

template <>
struct swapbytes <std::int16_t>
{
    inline std::int16_t operator() (std::int16_t value) const noexcept 
    {
        return static_cast <std::int16_t> (byteorder::swap (static_cast <std::uint16_t> (value)));
    }
};

template <>
struct swapbytes <std::int32_t>
{
    inline std::int32_t operator() (std::int32_t value) const noexcept 
    {
        return static_cast <std::int32_t> (byteorder::swap (static_cast <std::uint32_t> (value)));
    }
};

template <>
struct swapbytes <std::int64_t>
{
    inline std::int64_t operator() (std::int64_t value) const noexcept 
    {
        return static_cast <std::int64_t> (byteorder::swap (static_cast <std::uint64_t> (value)));
    }
};

}

//------------------------------------------------------------------------------

/** returns a type with the bytes swapped.
    little endian becomes big endian and vice versa. the underlying
    type must be an integral type or behave like one.
*/
template <class integraltype>
inline integraltype swapbytes (integraltype value) noexcept
{
    return detail::swapbytes <integraltype> () (value);
}

/** returns the machine byte-order value to little-endian byte order. */
template <typename integraltype>
inline integraltype tolittleendian (integraltype value) noexcept
{
#if beast_little_endian
    return value;
#else
    return swapbytes (value);
#endif
}

/** returns the machine byte-order value to big-endian byte order. */
template <typename integraltype>
inline integraltype tobigendian (integraltype value) noexcept
{
#if beast_little_endian
    return swapbytes (value);
#else
    return value;
#endif
}

/** returns the machine byte-order value to network byte order. */
template <typename integraltype>
inline integraltype tonetworkbyteorder (integraltype value) noexcept
{
    return tobigendian (value);
}

/** converts from network byte order to machine byte order. */
template <typename integraltype>
inline integraltype fromnetworkbyteorder (integraltype value) noexcept
{
#if beast_little_endian
    return swapbytes (value);
#else
    return value;
#endif
}

}

#endif
