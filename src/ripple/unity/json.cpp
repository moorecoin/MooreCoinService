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

#include <beastconfig.h>

#include <cassert>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <string>

// for json/
//
#ifdef json_use_cpptl
#include <cpptl/conststring.h>
#endif
#ifndef json_use_simple_internal_allocator
#include <ripple/json/impl/json_batchallocator.h>
#endif

#define json_assert_unreachable assert( false )
#define json_assert( condition ) assert( condition );  // @todo <= change this into an exception throw
#define json_assert_message( condition, message ) if (!( condition )) throw std::runtime_error( message );

#include <ripple/json/impl/json_reader.cpp>
#include <ripple/json/impl/json_value.cpp>
#include <ripple/json/impl/json_writer.cpp>
#include <ripple/json/impl/to_string.cpp>
#include <ripple/json/impl/jsonpropertystream.cpp>

#include <ripple/json/tests/jsoncpp.test.cpp>
