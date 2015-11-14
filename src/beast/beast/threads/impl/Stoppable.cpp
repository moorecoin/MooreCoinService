//------------------------------------------------------------------------------
/*
    this file is part of beast: https://github.com/vinniefalco/beast
    copyright 2012, vinnie falco <vinnie.falco@gmail.com>

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

#include <beast/threads/stoppable.h>

namespace beast {

stoppable::stoppable (char const* name, rootstoppable& root)
    : m_name (name)
    , m_root (root)
    , m_child (this)
    , m_started (false)
    , m_stopped (false)
    , m_childrenstopped (false)
{
}

stoppable::stoppable (char const* name, stoppable& parent)
    : m_name (name)
    , m_root (parent.m_root)
    , m_child (this)
    , m_started (false)
    , m_stopped (false)
    , m_childrenstopped (false)
{
    // must not have stopping parent.
    bassert (! parent.isstopping());

    parent.m_children.push_front (&m_child);
}

stoppable::~stoppable ()
{
    // children must be stopped.
    bassert (!m_started || m_childrenstopped);
}

bool stoppable::isstopping() const
{
    return m_root.isstopping();
}

bool stoppable::isstopped () const
{
    return m_stopped;
}

bool stoppable::arechildrenstopped () const
{
    return m_childrenstopped;
}

void stoppable::stopped ()
{
    m_stoppedevent.signal();
}

void stoppable::onprepare ()
{
}

void stoppable::onstart ()
{
}

void stoppable::onstop ()
{
    stopped();
}

void stoppable::onchildrenstopped ()
{
}

//------------------------------------------------------------------------------

void stoppable::preparerecursive ()
{
    for (children::const_iterator iter (m_children.cbegin ());
        iter != m_children.cend(); ++iter)
        iter->stoppable->preparerecursive ();
    onprepare ();
}

void stoppable::startrecursive ()
{
    onstart ();
    for (children::const_iterator iter (m_children.cbegin ());
        iter != m_children.cend(); ++iter)
        iter->stoppable->startrecursive ();
}

void stoppable::stopasyncrecursive ()
{
    onstop ();
    for (children::const_iterator iter (m_children.cbegin ());
        iter != m_children.cend(); ++iter)
        iter->stoppable->stopasyncrecursive ();
}

void stoppable::stoprecursive (journal journal)
{
    // block on each child from the bottom of the tree up.
    //
    for (children::const_iterator iter (m_children.cbegin ());
        iter != m_children.cend(); ++iter)
        iter->stoppable->stoprecursive (journal);

    // if we get here then all children have stopped
    //
    m_childrenstopped = true;
    onchildrenstopped ();

    // now block on this stoppable.
    //
    bool const timedout (! m_stoppedevent.wait (1 * 1000)); // milliseconds
    if (timedout)
    {
        journal.warning << "waiting for '" << m_name << "' to stop";
        m_stoppedevent.wait ();
    }

    // once we get here, we know the stoppable has stopped.
    m_stopped = true;
}

//------------------------------------------------------------------------------

rootstoppable::rootstoppable (char const* name)
    : stoppable (name, *this)
    , m_prepared (false)
    , m_calledstop (false)
    , m_calledstopasync (false)
{
}

bool rootstoppable::isstopping() const
{
    return m_calledstopasync;
}

void rootstoppable::prepare ()
{
    if (m_prepared.exchange (true) == false)
        preparerecursive ();
}

void rootstoppable::start ()
{
    // courtesy call to prepare.
    if (m_prepared.exchange (true) == false)
        preparerecursive ();

    if (m_started.exchange (true) == false)
        startrecursive ();
}

void rootstoppable::stop (journal journal)
{
    // must have a prior call to start()
    bassert (m_started);

    if (m_calledstop.exchange (true) == true)
    {
        journal.warning << "stoppable::stop called again";
        return;
    }

    stopasync ();
    stoprecursive (journal);
}

void rootstoppable::stopasync ()
{
    if (m_calledstopasync.exchange (true) == false)
        stopasyncrecursive ();
}

}
