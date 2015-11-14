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

#ifndef rippled_ripple_rpc_coroutine_h
#define rippled_ripple_rpc_coroutine_h

#include <ripple/rpc/yield.h>

namespace ripple {
namespace rpc {

/** runs a function that takes a yield as a coroutine. */
class coroutine
{
public:
    using yieldfunction = std::function <void (yield const&)>;

    explicit coroutine (yieldfunction const&);
    ~coroutine();

    /** is the coroutine finished? */
    operator bool() const;

    /** run one more step of the coroutine. */
    void operator()() const;

private:
    struct impl;

    std::shared_ptr<impl> impl_;
    // we'd prefer to use std::unique_ptr here, but unfortunately, in c++11
    // move semantics don't work well with `std::bind` or lambdas.
};

} // rpc
} // ripple

#endif
