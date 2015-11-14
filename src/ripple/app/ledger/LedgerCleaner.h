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

#ifndef ripple_ledgercleaner_h_included
#define ripple_ledgercleaner_h_included

#include <ripple/json/json_value.h>
#include <beast/threads/stoppable.h>
#include <beast/utility/propertystream.h>
#include <beast/utility/journal.h>
#include <memory>

namespace ripple {

/** check the ledger/transaction databases to make sure they have continuity */
class ledgercleaner
    : public beast::stoppable
    , public beast::propertystream::source
{
protected:
    explicit ledgercleaner (stoppable& parent);

public:
    /** destroy the object. */
    virtual ~ledgercleaner () = 0;

    /** start a long running task to clean the ledger.
        the ledger is cleaned asynchronously, on an implementation defined
        thread. this function call does not block. the long running task
        will be stopped if the stoppable stops.

        thread safety:
            safe to call from any thread at any time.

        @param parameters a json object with configurable parameters.
    */
    virtual void doclean (json::value const& parameters) = 0;
};

std::unique_ptr<ledgercleaner> make_ledgercleaner (beast::stoppable& parent,
    beast::journal journal);

} // ripple

#endif
