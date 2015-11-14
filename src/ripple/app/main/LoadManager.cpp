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
#include <ripple/app/main/loadmanager.h>
#include <ripple/app/main/application.h>
#include <ripple/app/misc/networkops.h>
#include <ripple/basics/uptimetimer.h>
#include <ripple/core/jobqueue.h>
#include <ripple/core/loadfeetrack.h>
#include <ripple/json/to_string.h>
#include <beast/threads/thread.h>
#include <beast/cxx14/memory.h> // <memory>
#include <mutex>

namespace ripple {

class loadmanagerimp
    : public loadmanager
    , public beast::thread
{
public:
    //--------------------------------------------------------------------------

    beast::journal m_journal;
    using locktype = std::mutex;
    typedef std::lock_guard <locktype> scopedlocktype;
    locktype mlock;

    bool marmed;

    int mdeadlock;              // detect server deadlocks
    //--------------------------------------------------------------------------

    loadmanagerimp (stoppable& parent, beast::journal journal)
        : loadmanager (parent)
        , thread ("loadmgr")
        , m_journal (journal)
        , marmed (false)
        , mdeadlock (0)
    {
        uptimetimer::getinstance ().beginmanualupdates ();
    }

    ~loadmanagerimp ()
    {
        uptimetimer::getinstance ().endmanualupdates ();

        stopthread ();
    }

    //--------------------------------------------------------------------------
    //
    // stoppable
    //
    //--------------------------------------------------------------------------

    void onprepare ()
    {
    }

    void onstart ()
    {
        m_journal.debug << "starting";
        startthread ();
    }

    void onstop ()
    {
        if (isthreadrunning ())
        {
            m_journal.debug << "stopping";
            stopthreadasync ();
        }
        else
        {
            stopped ();
        }
    }

    //--------------------------------------------------------------------------

    void resetdeadlockdetector ()
    {
        scopedlocktype sl (mlock);
        mdeadlock = uptimetimer::getinstance ().getelapsedseconds ();
    }

    void activatedeadlockdetector ()
    {
        marmed = true;
    }

    void logdeadlock (int dltime)
    {
        m_journal.warning << "server stalled for " << dltime << " seconds.";
    }

    // vfalco note where's the thread object? it's not a data member...
    //
    void run ()
    {
        using clock_type = std::chrono::steady_clock;

        // initialize the clock to the current time.
        auto t = clock_type::now();

        while (! threadshouldexit ())
        {
            {
                // vfalco note what is this lock protecting?
                scopedlocktype sl (mlock);

                // vfalco note i think this is to reduce calls to the operating system
                //             for retrieving the current time.
                //
                //        todo instead of incrementing can't we just retrieve the current
                //             time again?
                //
                // manually update the timer.
                uptimetimer::getinstance ().incrementelapsedtime ();

                // measure the amount of time we have been deadlocked, in seconds.
                //
                // vfalco note mdeadlock is a canary for detecting the condition.
                int const timespentdeadlocked = uptimetimer::getinstance ().getelapsedseconds () - mdeadlock;

                // vfalco note i think that "armed" refers to the deadlock detector
                //
                int const reportingintervalseconds = 10;
                if (marmed && (timespentdeadlocked >= reportingintervalseconds))
                {
                    // report the deadlocked condition every 10 seconds
                    if ((timespentdeadlocked % reportingintervalseconds) == 0)
                    {
                        logdeadlock (timespentdeadlocked);
                    }

                    // if we go over 500 seconds spent deadlocked, it means that the
                    // deadlock resolution code has failed, which qualifies as undefined
                    // behavior.
                    //
                    assert (timespentdeadlocked < 500);
                }
            }

            bool change;

            // vfalco todo eliminate the dependence on the application object.
            //             choices include constructing with the job queue / feetracker.
            //             another option is using an observer pattern to invert the dependency.
            if (getapp().getjobqueue ().isoverloaded ())
            {
                if (m_journal.info)
                    m_journal.info << getapp().getjobqueue ().getjson (0);
                change = getapp().getfeetrack ().raiselocalfee ();
            }
            else
            {
                change = getapp().getfeetrack ().lowerlocalfee ();
            }

            if (change)
            {
                // vfalco todo replace this with a listener / observer and subscribe in networkops or application
                getapp().getops ().reportfeechange ();
            }

            t += std::chrono::seconds (1);
            auto const duration = t - clock_type::now();

            if ((duration < std::chrono::seconds (0)) || (duration > std::chrono::seconds (1)))
            {
                m_journal.warning << "time jump";
                t = clock_type::now();
            }
            else
            {
                std::this_thread::sleep_for (duration);
            }
        }

        stopped ();
    }
};

//------------------------------------------------------------------------------

loadmanager::loadmanager (stoppable& parent)
    : stoppable ("loadmanager", parent)
{
}

//------------------------------------------------------------------------------

std::unique_ptr<loadmanager>
make_loadmanager (beast::stoppable& parent, beast::journal journal)
{
    return std::make_unique <loadmanagerimp> (parent, journal);
}

} // ripple
