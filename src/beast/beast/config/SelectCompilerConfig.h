//------------------------------------------------------------------------------
/*
    this file is part of beast: https://github.com/vinniefalco/beast
    copyright 2013, vinnie falco <vinnie.falco@gmail.com>

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

// ideas from boost

// intel
#if defined(__intel_compiler) || defined(__icl) || defined(__icc) || defined(__ecc)
#define beast_compiler_config "config/compiler/intel.h"

// clang c++ emulates gcc, so it has to appear early.
#elif defined __clang__
#define beast_compiler_config "config/compiler/clang.h"

//  gnu c++:
#elif defined __gnuc__
#define beast_compiler_config "config/compiler/gcc.h"

//  microsoft visual c++
//
//  must remain the last #elif since some other vendors (metrowerks, for
//  example) also #define _msc_ver
#elif defined _msc_ver
#define beast_compiler_config "config/compiler/visualc.h"

#else
#error "unsupported compiler."
#endif

