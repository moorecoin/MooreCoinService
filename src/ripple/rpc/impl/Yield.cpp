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
#include <ripple/rpc/yield.h>
#include <ripple/rpc/tests/testoutputsuite.test.h>

namespace ripple {
namespace rpc {

output chunkedyieldingoutput (
    output const& output, yield const& yield, std::size_t chunksize)
{
    auto count = std::make_shared <std::size_t> (0);
    return [chunksize, count, output, yield] (boost::string_ref const& bytes)
    {
        if (*count > chunksize)
        {
            yield();
            *count = 0;
        }
        output (bytes);
        *count += bytes.size();
    };
}


countedyield::countedyield (std::size_t yieldcount, yield const& yield)
        : yieldcount_ (yieldcount), yield_ (yield)
{
}

void countedyield::yield()
{
    if (yieldcount_) {
        if (++count_ >= yieldcount_)
        {
            yield_();
            count_ = 0;
        }
    }
}

yieldstrategy makeyieldstrategy (section const& s)
{
    yieldstrategy ys;
    ys.streaming = get<bool> (s, "streaming") ?
            yieldstrategy::streaming::yes :
            yieldstrategy::streaming::no;
    ys.usecoroutines = get<bool> (s, "use_coroutines") ?
            yieldstrategy::usecoroutines::yes :
            yieldstrategy::usecoroutines::no;
    ys.byteyieldcount = get<std::size_t> (s, "byte_yield_count");
    ys.accountyieldcount = get<std::size_t> (s, "account_yield_count");
    ys.transactionyieldcount = get<std::size_t> (s, "transaction_yield_count");

    return ys;
}

} // rpc
} // ripple
