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

#ifndef ripple_hyperleveldb_h_included
#define ripple_hyperleveldb_h_included

#include <beast/config.h>

#if ! beast_win32

#define ripple_hyperleveldb_available 1

#include <hyperleveldb/hyperleveldb/cache.h>
#include <hyperleveldb/hyperleveldb/filter_policy.h>
#include <hyperleveldb/hyperleveldb/db.h>
#include <hyperleveldb/hyperleveldb/write_batch.h>

#else

#define ripple_hyperleveldb_available 0

#endif

#endif
