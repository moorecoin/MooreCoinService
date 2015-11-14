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

#ifndef json_forwards_h_included
#define json_forwards_h_included

#include <ripple/json/json_config.h>

namespace json
{

// writer.h
class fastwriter;
class styledwriter;

// reader.h
class reader;

// features.h
class features;

// value.h
typedef int int;
typedef unsigned int uint;
class staticstring;
class path;
class pathargument;
class value;
class valueiteratorbase;
class valueiterator;
class valueconstiterator;
#ifdef json_value_use_internal_map
class valueallocator;
class valuemapallocator;
class valueinternallink;
class valueinternalarray;
class valueinternalmap;
#endif // #ifdef json_value_use_internal_map

} // namespace json


#endif // json_forwards_h_included
