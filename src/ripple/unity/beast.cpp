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

#if ! beast_compile_objective_cpp

/*  this file includes all of the beast sources needed to link.
    by including them here, we avoid having to muck with the sconstruct
    makefile, project file, or whatever.
*/

// must come first!
#include <beastconfig.h>

// include this to get all the basic includes included, to prevent errors
#include <beast/module/core/core.unity.cpp>
#include <beast/module/asio/asio.unity.cpp>
#include <beast/module/sqdb/sqdb.unity.cpp>

#include <beast/asio/asio.unity.cpp>
#include <beast/boost/boost.unity.cpp>
#include <beast/chrono/chrono.unity.cpp>
#include <beast/container/container.unity.cpp>
#include <beast/crypto/crypto.unity.cpp>
#include <beast/http/http.unity.cpp>
#include <beast/insight/insight.unity.cpp>
#include <beast/net/net.unity.cpp>
//#include <beast/nudb/nudb.cpp>
#include <beast/streams/streams.unity.cpp>
#include <beast/strings/strings.unity.cpp>
#include <beast/threads/threads.unity.cpp>
#include <beast/utility/utility.unity.cpp>

#include <beast/cxx14/cxx14.unity.cpp>

#include <beast/unit_test/define_print.cpp>

#endif
