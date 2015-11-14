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

#ifndef beast_sqlite_h_included
#define beast_sqlite_h_included

/**  a self-contained, serverless, zero configuration, transactional sql engine.

    this external module provides the sqlite embedded database library.

    sqlite is public domain software, visit http://sqlite.org
*/

#include <beast/config/platformconfig.h>

#if beast_ios || beast_mac
# define beast_have_native_sqlite 1
#else
# define beast_have_native_sqlite 0
#endif

#ifndef beast_sqlite_cpp_included
# if beast_use_native_sqlite && beast_have_native_sqlite
#include <sqlite3.h>
# else
#include <beast/module/sqlite/sqlite/sqlite3.h> // amalgamated
# endif
#endif

#endif
