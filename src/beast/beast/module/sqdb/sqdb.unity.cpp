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

#if beast_include_beastconfig
#include <beastconfig.h>
#endif

#include <beast/module/sqdb/sqdb.h>

#include <beast/module/sqlite/sqlite.h>

#if beast_msvc
#pragma warning (push)
#pragma warning (disable: 4100) // unreferenced formal parmaeter
#pragma warning (disable: 4355) // 'this' used in base member
#endif

// implementation headers
#include <beast/module/sqdb/detail/error_codes.h>
#include <beast/module/sqdb/detail/statement_imp.h>

#include <beast/module/sqdb/source/blob.cpp>
#include <beast/module/sqdb/source/error_codes.cpp>
#include <beast/module/sqdb/source/into_type.cpp>
#include <beast/module/sqdb/source/once_temp_type.cpp>
#include <beast/module/sqdb/source/prepare_temp_type.cpp>
#include <beast/module/sqdb/source/ref_counted_prepare_info.cpp>
#include <beast/module/sqdb/source/ref_counted_statement.cpp>
#include <beast/module/sqdb/source/session.cpp>
#include <beast/module/sqdb/source/statement.cpp>
#include <beast/module/sqdb/source/statement_imp.cpp>
#include <beast/module/sqdb/source/transaction.cpp>
#include <beast/module/sqdb/source/use_type.cpp>

#if beast_msvc
#pragma warning (pop)
#endif
