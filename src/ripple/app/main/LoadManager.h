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

#ifndef ripple_loadmanager_h_included
#define ripple_loadmanager_h_included

#include <beast/threads/stoppable.h>
#include <beast/cxx14/memory.h> // <memory>

namespace ripple {

/** manages load sources.

    this object creates an associated thread to maintain a clock.

    when the server is overloaded by a particular peer it issues a warning
    first. this allows friendly peers to reduce their consumption of resources,
    or disconnect from the server.

    the warning system is used instead of merely dropping, because hostile
    peers can just reconnect anyway.
*/
class loadmanager : public beast::stoppable
{
protected:
    explicit loadmanager (stoppable& parent);

public:
    /** destroy the manager.

        the destructor returns only after the thread has stopped.
    */
    virtual ~loadmanager () = default;

    /** turn on deadlock detection.

        the deadlock detector begins in a disabled state. after this function
        is called, it will report deadlocks using a separate thread whenever
        the reset function is not called at least once per 10 seconds.

        @see resetdeadlockdetector
    */
    // vfalco note it seems that the deadlock detector has an "armed" state to prevent it
    //             from going off during program startup if there's a lengthy initialization
    //             operation taking place?
    //
    virtual void activatedeadlockdetector () = 0;

    /** reset the deadlock detection timer.

        a dedicated thread monitors the deadlock timer, and if too much
        time passes it will produce log warnings.
    */
    virtual void resetdeadlockdetector () = 0;
};

std::unique_ptr<loadmanager>
make_loadmanager (beast::stoppable& parent, beast::journal journal);

} // ripple

#endif
