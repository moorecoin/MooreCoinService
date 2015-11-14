//------------------------------------------------------------------------------
/*
    this file is part of rippled: https://github.com/ripple/rippled
    copyright (c) 2012, 2013 ripple labs inc.

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

#ifndef ripple_basics_byteorder_h
#define ripple_basics_byteorder_h

#include <beast/config.h>

// for byteorder
#if beast_win32
// (nothing)
#elif __apple__
#include <libkern/osbyteorder.h>
#elif defined(__freebsd__) || defined(__netbsd__)
#include <sys/endian.h>
#elif defined(__openbsd__)
#include <sys/types.h>
#endif
#include <cstdint>

namespace ripple {

// vfalco these are all deprecated please use the ones in <beast/byteorder.h>
// routines for converting endianness

// reference: http://www.mail-archive.com/licq-commits@googlegroups.com/msg02334.html

#ifdef _msc_ver
extern std::uint64_t htobe64 (uint64_t value);
extern std::uint64_t be64toh (uint64_t value);
extern std::uint32_t htobe32 (uint32_t value);
extern std::uint32_t be32toh (uint32_t value);

#elif __apple__
#define htobe16(x) osswaphosttobigint16(x)
#define htole16(x) osswaphosttolittleint16(x)
#define be16toh(x) osswapbigtohostint16(x)
#define le16toh(x) osswaplittletohostint16(x)

#define htobe32(x) osswaphosttobigint32(x)
#define htole32(x) osswaphosttolittleint32(x)
#define be32toh(x) osswapbigtohostint32(x)
#define le32toh(x) osswaplittletohostint32(x)

#define htobe64(x) osswaphosttobigint64(x)
#define htole64(x) osswaphosttolittleint64(x)
#define be64toh(x) osswapbigtohostint64(x)
#define le64toh(x) osswaplittletohostint64(x)

#elif defined(__freebsd__) || defined(__netbsd__)

#elif defined(__openbsd__)
#define be16toh(x) betoh16(x)
#define be32toh(x) betoh32(x)
#define be64toh(x) betoh64(x)

#endif

}

#endif
