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
#include <ripple/basics/sustain.h>
#include <ripple/basics/threadname.h>
#include <boost/format.hpp>
    
// for sustain linux variants
// vfalco todo rewrite sustain to use beast::process
#ifdef __linux__
#include <sys/types.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>
#endif
#ifdef __freebsd__
#include <sys/types.h>
#include <sys/wait.h>
#endif

namespace ripple {

#ifdef __unix__

static pid_t pmanager = static_cast<pid_t> (0);
static pid_t pchild = static_cast<pid_t> (0);

static void pass_signal (int a)
{
    kill (pchild, a);
}

static void stop_manager (int)
{
    kill (pchild, sigint);
    _exit (0);
}

bool havesustain ()
{
    return true;
}

std::string stopsustain ()
{
    if (getppid () != pmanager)
        return std::string ();

    kill (pmanager, sighup);
    return "terminating monitor";
}

std::string dosustain (std::string logfile)
{
    int childcount = 0;
    pmanager = getpid ();
    signal (sigint, stop_manager);
    signal (sighup, stop_manager);
    signal (sigusr1, pass_signal);
    signal (sigusr2, pass_signal);

    while (1)
    {
        ++childcount;
        pchild = fork ();

        if (pchild == -1)
            _exit (0);

        if (pchild == 0)
        {
            setcallingthreadname ("main");
            signal (sigint, sig_dfl);
            signal (sighup, sig_dfl);
            signal (sigusr1, sig_dfl);
            signal (sigusr2, sig_dfl);
            return str (boost::format ("launching child %d") % childcount);;
        }

        setcallingthreadname (boost::str (boost::format ("#%d") % childcount).c_str ());

        sleep (9);
        do
        {
            int i;
            sleep (1);
            waitpid (pchild, &i, 0);
        }
        while (kill (pchild, 0) == 0);

        rename ("core", boost::str (boost::format ("core.%d") % static_cast<int> (pchild)).c_str ());
        if (!logfile.empty()) // fixme: logfile hasn't been set yet
            rename (logfile.c_str(),
                    boost::str (boost::format ("%s.%d")
                                % logfile
                                % static_cast<int> (pchild)).c_str ());
    }
}

#else

bool havesustain ()
{
    return false;
}
std::string dosustain (std::string)
{
    return std::string ();
}
std::string stopsustain ()
{
    return std::string ();
}

#endif

} // ripple
