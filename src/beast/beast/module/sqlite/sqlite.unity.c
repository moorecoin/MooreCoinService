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

/** add this to get the @ref vf_sqlite external module.

    @file beast_sqlite.c
    @ingroup beast_sqlite
*/

#if beast_include_beastconfig
#include <beastconfig.h>
#endif

// prevents sqlite.h from being included, since it screws up the .c
#define beast_sqlite_cpp_included

#include <beast/module/sqlite/sqlite.h>

#if ! (beast_use_native_sqlite && beast_have_native_sqlite)

#if beast_msvc
#pragma warning (push)
#pragma warning (disable: 4100) /* unreferenced formal parameter */
#pragma warning (disable: 4127) /* conditional expression is constant */
#pragma warning (disable: 4232) /* nonstandard extension used: dllimport address */
#pragma warning (disable: 4244) /* conversion from 'int': possible loss of data */
#pragma warning (disable: 4701) /* potentially uninitialized variable */
#pragma warning (disable: 4706) /* assignment within conditional expression */
#pragma warning (disable: 4996) /* 'getversionexa' was declared deprecated */
#endif

/* when compiled with sqlite_threadsafe=1, sqlite operates in serialized mode.
   in this mode, sqlite can be safely used by multiple threads with no restriction.

   vfalco note this implies a global mutex!
*/
#define sqlite_threadsafe 1

/* when compiled with sqlite_threadsafe=2, sqlite can be used in a
   multithreaded program so long as no two threads attempt to use the
   same database connection at the same time.

   vfalco note this is the preferred threading model.
*/
//#define sqlite_threadsafe 2

#if defined (beast_sqlite_use_ndebug) && beast_sqlite_use_ndebug && !defined (ndebug)
#define ndebug
#endif

#include <beast/module/sqlite/sqlite/sqlite3.c>

#if beast_msvc
#pragma warning (pop)
#endif

#endif
