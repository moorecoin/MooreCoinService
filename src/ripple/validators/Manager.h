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

#ifndef ripple_validators_manager_h_included
#define ripple_validators_manager_h_included

#include <ripple/protocol/protocol.h>
#include <ripple/validators/connection.h>
#include <ripple/protocol/rippleledgerhash.h>
#include <beast/threads/stoppable.h>
#include <beast/http/url.h>
#include <beast/module/core/files/file.h>
#include <beast/utility/propertystream.h>

namespace ripple {
namespace validators {

/** maintains the list of chosen validators.
    the algorithm for acquiring, building, and calculating metadata on
    the list of chosen validators is critical to the health of the network.
    all operations are performed asynchronously on an internal thread.
*/
class manager : public beast::propertystream::source
{
protected:
    manager();

public:
    /** destroy the object.
        any pending source fetch operations are aborted. this will block
        until any pending database i/o has completed and the thread has
        stopped.
    */
    virtual ~manager() = default;

    /** create a new connection. */
    virtual
    std::unique_ptr<connection>
    newconnection (int id) = 0;

    /** called when a ledger is built. */
    virtual
    void
    onledgerclosed (ledgerindex index,
        ledgerhash const& hash, ledgerhash const& parent) = 0;
};

}
}

#endif
