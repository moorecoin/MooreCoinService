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

#ifndef ripple_nodestore_types_h_included
#define ripple_nodestore_types_h_included

#include <ripple/nodestore/nodeobject.h>
#include <beast/module/core/text/stringpairarray.h>
#include <vector>

namespace ripple {
namespace nodestore {

enum
{
    // this is only used to pre-allocate the array for
    // batch objects and does not affect the amount written.
    //
    batchwritepreallocationsize = 128
};

/** return codes from backend operations. */
enum status
{
    ok,
    notfound,
    datacorrupt,
    unknown,

    customcode = 100
};

/** a batch of nodeobjects to write at once. */
typedef std::vector <nodeobject::ptr> batch;

/** a list of key/value parameter pairs passed to the backend. */
// vfalco todo use std::string, pair, vector
typedef beast::stringpairarray parameters;

}
}

#endif
