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

namespace beast
{

class deadlinetimer::manager
    : public leakchecked <manager>
    , protected thread
{
private:
    typedef criticalsection locktype;
    typedef list <deadlinetimer> items;

public:
    manager () : thread ("deadlinetimer::manager")
    {
        startthread ();
    }

    ~manager ()
    {
        signalthreadshouldexit ();
        notify ();
        waitforthreadtoexit ();
        bassert (m_items.empty ());
    }

    // okay to call on an active timer.
    // however, an extra notification may still happen due to concurrency.
    //
    void activate (deadlinetimer& timer,
        double secondsrecurring, relativetime const& when)
    {
        bassert (secondsrecurring >= 0);

        std::lock_guard <locktype> lock (m_mutex);

        if (timer.m_isactive)
        {
            m_items.erase (m_items.iterator_to (timer));

            timer.m_isactive = false;
        }

        timer.m_secondsrecurring = secondsrecurring;
        timer.m_notificationtime = when;

        insertsorted (timer);
        timer.m_isactive = true;

        notify ();
    }

    // okay to call this on an inactive timer.
    // this can happen naturally based on concurrency.
    //
    void deactivate (deadlinetimer& timer)
    {
        std::lock_guard <locktype> lock (m_mutex);

        if (timer.m_isactive)
        {
            m_items.erase (m_items.iterator_to (timer));

            timer.m_isactive = false;

            notify ();
        }
    }

    void run ()
    {
        while (! threadshouldexit ())
        {
            relativetime const currenttime (
                relativetime::fromstartup ());

            double seconds (0);
            deadlinetimer* timer (nullptr);

            {
                std::lock_guard <locktype> lock (m_mutex);

                // see if a timer expired
                if (! m_items.empty ())
                {
                    timer = &m_items.front ();

                    // has this timer expired?
                    if (timer->m_notificationtime <= currenttime)
                    {
                        // expired, remove it from the list.
                        bassert (timer->m_isactive);
                        m_items.pop_front ();

                        // is the timer recurring?
                        if (timer->m_secondsrecurring > 0)
                        {
                            // yes so set the timer again.
                            timer->m_notificationtime =
                                currenttime + timer->m_secondsrecurring;

                            // put it back into the list as active
                            insertsorted (*timer);
                        }
                        else
                        {
                            // not a recurring timer, deactivate it.
                            timer->m_isactive = false;
                        }

                        timer->m_listener->ondeadlinetimer (*timer);

                        // re-loop
                        seconds = -1;
                    }
                    else
                    {
                        seconds = (
                            timer->m_notificationtime - currenttime).inseconds ();

                        // can't be zero and come into the else clause.
                        bassert (seconds != 0);

                        // don't call the listener
                        timer = nullptr;
                    }
                }
            }

            // note that we have released the lock here.

            if (seconds > 0)
            {
                // wait until interrupt or next timer.
                //
                int const milliseconds (std::max (
                    static_cast <int> (seconds * 1000 + 0.5), 1));
                bassert (milliseconds > 0);
                wait (milliseconds);
            }
            else if (seconds == 0)
            {
                // wait until interrupt
                //
                wait ();
            }
            else
            {
                // do not wait. this can happen if the recurring timer duration
                // is extremely short, or if a listener wastes too much time in
                // their callback.
            }
        }
    }

    // caller is responsible for locking
    void insertsorted (deadlinetimer& timer)
    {
        if (! m_items.empty ())
        {
            items::iterator before = m_items.begin ();

            for (;;)
            {
                if (before->m_notificationtime >= timer.m_notificationtime)
                {
                    m_items.insert (before, timer);
                    break;
                }

                ++before;

                if (before == m_items.end ())
                {
                    m_items.push_back (timer);
                    break;
                }
            }
        }
        else
        {
            m_items.push_back (timer);
        }
    }

private:
    criticalsection m_mutex;
    items m_items;
};

//------------------------------------------------------------------------------

deadlinetimer::deadlinetimer (listener* listener)
    : m_listener (listener)
    , m_manager (sharedsingleton <manager>::getinstance ())
    , m_isactive (false)
{
}

deadlinetimer::~deadlinetimer ()
{
    m_manager->deactivate (*this);
}

void deadlinetimer::cancel ()
{
    m_manager->deactivate (*this);
}

void deadlinetimer::setexpiration (double secondsuntildeadline)
{
    bassert (secondsuntildeadline != 0);

    relativetime const when (
        relativetime::fromstartup() + secondsuntildeadline);

    m_manager->activate (*this, 0, when);
}

void deadlinetimer::setrecurringexpiration (double secondsuntildeadline)
{
    bassert (secondsuntildeadline != 0);

    relativetime const when (
        relativetime::fromstartup() + secondsuntildeadline);

    m_manager->activate (*this, secondsuntildeadline, when);
}

} // beast
