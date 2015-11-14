//------------------------------------------------------------------------------
/*
    this file is part of beast: https://github.com/vinniefalco/beast
    copyright 2013, vinnie falco <vinnie.falco@gmail.com>

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

#ifndef beast_threads_stoppable_h_included
#define beast_threads_stoppable_h_included

#include <beast/intrusive/lockfreestack.h>
#include <beast/utility/journal.h>

#include <beast/threads/waitableevent.h>

#include <atomic>

namespace beast {

class rootstoppable;

/** provides an interface for starting and stopping.

    a common method of structuring server or peer to peer code is to isolate
    conceptual portions of functionality into individual classes, aggregated
    into some larger "application" or "core" object which holds all the parts.
    frequently, these components are dependent on each other in unavoidably
    complex ways. they also often use threads and perform asynchronous i/o
    operations involving sockets or other operating system objects. the process
    of starting and stopping such a system can be complex. this interface
    provides a set of behaviors for ensuring that the start and stop of a
    composite application-style object is well defined.

    upon the initialization of the composite object these steps are peformed:

    1.  construct sub-components.

        these are all typically derived from stoppable. there can be a deep
        hierarchy: stoppable objects may themselves have stoppable child
        objects. this captures the relationship of dependencies.

    2.  prepare()

        because some components may depend on others, preparatory steps require
        that all objects be first constructed. the prepare step calls all
        stoppable objects in the tree starting from the leaves and working up
        to the root. in this stage we are guaranteed that all objects have been
        constructed and are in a well-defined state.

    3.  onprepare()

        this override is called for all stoppable objects in the hierarchy
        during the prepare stage. it is guaranteed that all child stoppable
        objects have already been prepared when this is called.
        
        objects are called children first.

    4.  start()

        at this point all sub-components have been constructed and prepared,
        so it should be safe for them to be started. while some stoppable
        objects may do nothing in their start function, others will start
        threads or call asynchronous i/o initiating functions like timers or
        sockets.

    5.  onstart()

        this override is called for all stoppable objects in the hierarchy
        during the start stage. it is guaranteed that no child stoppable
        objects have been started when this is called.

        objects are called parent first.

    this is the sequence of events involved in stopping:

    6.  stopasync() [optional]

        this notifies the root stoppable and all its children that a stop is
        requested.

    7.  stop()

        this first calls stopasync(), and then blocks on each child stoppable in
        the in the tree from the bottom up, until the stoppable indicates it has
        stopped. this will usually be called from the main thread of execution
        when some external signal indicates that the process should stop. for
        example, an rpc 'stop' command, or a sigint posix signal.

    8.  onstop()

        this override is called for the root stoppable and all its children when
        stopasync() is called. derived classes should cancel pending i/o and
        timers, signal that threads should exit, queue cleanup jobs, and perform
        any other necessary final actions in preparation for exit.

        objects are called parent first.

    9.  onchildrenstopped()

        this override is called when all the children have stopped. this informs
        the stoppable that there should not be any more dependents making calls
        into its member functions. a stoppable that has no children will still
        have this function called.

        objects are called children first.

    10. stopped()

        the derived class calls this function to inform the stoppable api that
        it has completed the stop. this unblocks the caller of stop().

        for stoppables which are only considered stopped when all of their
        children have stopped, and their own internal logic indicates a stop, it
        will be necessary to perform special actions in onchildrenstopped(). the
        funtion arechildrenstopped() can be used after children have stopped,
        but before the stoppable logic itself has stopped, to determine if the
        stoppable's logic is a true stop.
        
        pseudo code for this process is as follows:

        @code

        // returns `true` if derived logic has stopped.
        //
        // when the logic stops, logicprocessingstop() is no longer called.
        // if children are still active we need to wait until we get a
        // notification that the children have stopped.
        //
        bool logichasstopped ();

        // called when children have stopped
        void onchildrenstopped ()
        {
            // we have stopped when the derived logic stops and children stop.
            if (logichasstopped)
                stopped();
        }

        // derived-specific logic that executes periodically
        void logicprocessingstep ()
        {
            // process
            // ...

            // now see if we've stopped
            if (logichasstopped() && arechildrenstopped())
                stopped();
        }

        @endcode

        derived class that manage one or more threads should typically notify
        those threads in onstop that they should exit. in the thread function,
        when the last thread is about to exit it would call stopped().

    @note a stoppable may not be restarted.
*/
/** @{ */
class stoppable
{
protected:
    stoppable (char const* name, rootstoppable& root);

public:
    /** create the stoppable. */
    stoppable (char const* name, stoppable& parent);

    /** destroy the stoppable. */
    virtual ~stoppable ();

    /** returns `true` if the stoppable should stop. */
    bool isstopping () const;

    /** returns `true` if the requested stop has completed. */
    bool isstopped () const;

    /** returns `true` if all children have stopped. */
    bool arechildrenstopped () const;

protected:
    /** called by derived classes to indicate that the stoppable has stopped. */
    void stopped ();

private:
    /** override called during preparation.
        since all other stoppable objects in the tree have already been
        constructed, this provides an opportunity to perform initialization which
        depends on calling into other stoppable objects.
        this call is made on the same thread that called prepare().
        the default implementation does nothing.
        guaranteed to only be called once.
    */
    virtual void onprepare ();

    /** override called during start. */
    virtual void onstart ();

    /** override called when the stop notification is issued.

        the call is made on an unspecified, implementation-specific thread.
        onstop and onchildrenstopped will never be called concurrently, across
        all stoppable objects descended from the same root, inclusive of the
        root.

        it is safe to call isstopping, isstopped, and  arechildrenstopped from
        within this function; the values returned will always be valid and never
        change during the callback.

        the default implementation simply calls stopped(). this is applicable
        when the stoppable has a trivial stop operation (or no stop operation),
        and we are merely using the stoppable api to position it as a dependency
        of some parent service.

        thread safety:
            may not block for long periods.
            guaranteed only to be called once.
            must be safe to call from any thread at any time.
    */
    virtual void onstop ();

    /** override called when all children have stopped.

        the call is made on an unspecified, implementation-specific thread.
        onstop and onchildrenstopped will never be called concurrently, across
        all stoppable objects descended from the same root, inclusive of the
        root.
        
        it is safe to call isstopping, isstopped, and  arechildrenstopped from
        within this function; the values returned will always be valid and never
        change during the callback.

        the default implementation does nothing.

        thread safety:
            may not block for long periods.
            guaranteed only to be called once.
            must be safe to call from any thread at any time.
    */
    virtual void onchildrenstopped ();

    friend class rootstoppable;

    struct child;
    typedef lockfreestack <child> children;

    struct child : children::node
    {
        child (stoppable* stoppable_) : stoppable (stoppable_)
        {
        }

        stoppable* stoppable;
    };

    void preparerecursive ();
    void startrecursive ();
    void stopasyncrecursive ();
    void stoprecursive (journal journal);

    std::string m_name;
    rootstoppable& m_root;
    child m_child;
    std::atomic<bool> m_started;
    std::atomic<bool> m_stopped;
    std::atomic<bool> m_childrenstopped;
    children m_children;
    waitableevent m_stoppedevent;
};

//------------------------------------------------------------------------------

class rootstoppable : public stoppable
{
public:
    explicit rootstoppable (char const* name);

    ~rootstoppable () = default;

    bool isstopping() const;

    /** prepare all contained stoppable objects.
        this calls onprepare for all stoppable objects in the tree.
        calls made after the first have no effect.
        thread safety:
            may be called from any thread.
    */
    void prepare ();

    /** start all contained stoppable objects.
        the default implementation does nothing.
        calls made after the first have no effect.
        thread safety:
            may be called from any thread.
    */
    void start ();

    /** notify a root stoppable and children to stop, and block until stopped.
        has no effect if the stoppable was already notified.
        this blocks until the stoppable and all of its children have stopped.
        undefined behavior results if stop() is called without a previous call
        to start().
        thread safety:
            safe to call from any thread not associated with a stoppable.
    */
    void stop (journal journal = journal());
private:
    /** notify a root stoppable and children to stop, without waiting.
        has no effect if the stoppable was already notified.

        thread safety:
            safe to call from any thread at any time.
    */
    void stopasync ();

    std::atomic<bool> m_prepared;
    std::atomic<bool> m_calledstop;
    std::atomic<bool> m_calledstopasync;
};
/** @} */

}

#endif
