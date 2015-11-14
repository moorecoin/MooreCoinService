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

#ifndef rippled_ripple_rpc_yield_h
#define rippled_ripple_rpc_yield_h

#include <ripple/rpc/output.h>
#include <boost/coroutine/all.hpp>
#include <functional>

namespace ripple {

class section;

namespace rpc {

/** yield is a generic placeholder for a function that yields control of
    execution - perhaps to another coroutine.

    when code calls yield, it might block for an indeterminate period of time.

    by convention you must not be holding any locks or any resource that would
    prevent any other task from making forward progress when you call yield.
*/
using yield = std::function <void ()>;

/** wrap an output so it yields after approximately `chunksize` bytes.

    chunkedyieldingoutput() only yields after a call to output(), so there might
    more than chunksize bytes sent between calls to yield().

    chunkedyieldingoutput() also only yields before it's about to output more
    data.  this is to avoid the case where you yield after outputting data, but
    then never send more data.
 */
output chunkedyieldingoutput (
    output const&, yield const&, std::size_t chunksize);

/** yield every yieldcount calls.  if yieldcount is 0, never yield. */
class countedyield
{
public:
    countedyield (std::size_t yieldcount, yield const& yield);
    void yield();

private:
    std::size_t count_ = 0;
    std::size_t const yieldcount_;
    yield const yield_;
};

/** when do we yield when performing a ledger computation? */
struct yieldstrategy
{
    enum class streaming {no, yes};
    enum class usecoroutines {no, yes};

    /** is the data streamed, or generated monolithically? */
    streaming streaming = streaming::no;

    /** are results generated in a coroutine?  if this is no, then the code can
        never yield. */
    usecoroutines usecoroutines = usecoroutines::no;

    /** how many bytes do we emit before yielding?  0 means "never yield due to
        number of bytes sent". */
    std::size_t byteyieldcount = 0;

    /** how many accounts do we process before yielding?  0 means "never yield
        due to number of accounts processed." */
    std::size_t accountyieldcount = 0;

    /** how many transactions do we process before yielding? 0 means "never
        yield due to number of transactions processed." */
    std::size_t transactionyieldcount = 0;
};

/** create a yield strategy from a configuration section. */
yieldstrategy makeyieldstrategy (section const&);

} // rpc
} // ripple

#endif
