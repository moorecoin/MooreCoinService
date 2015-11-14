//------------------------------------------------------------------------------
/*
    this file is part of beast: https://github.com/vinniefalco/beast
    copyright 2013, vinnie falco <vinnie.falco@gmail.com>

    portions of this file are from juce.
    copyright (c) 2013 - raw material software ltd.
    please visit http://www.juce.com

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

#include <beast/threads/waitableevent.h>

#if beast_windows

#include <windows.h>
#undef check
#undef direct
#undef max
#undef min
#undef type_bool

namespace beast {

waitableevent::waitableevent (const bool manualreset, bool initiallysignaled)
    : handle (createevent (0, manualreset ? true : false, initiallysignaled ? true : false, 0))
{
}

waitableevent::~waitableevent()
{
    closehandle (handle);
}

void waitableevent::signal() const
{
    setevent (handle);
}

void waitableevent::reset() const
{
    resetevent (handle);
}

bool waitableevent::wait () const
{
    return waitforsingleobject (handle, infinite) == wait_object_0;
}

bool waitableevent::wait (const int timeoutms) const
{
    if (timeoutms >= 0)
        return waitforsingleobject (handle,
            (dword) timeoutms) == wait_object_0;
    return wait ();
}

}

#else

namespace beast {

waitableevent::waitableevent (const bool usemanualreset, bool initiallysignaled)
    : triggered (false), manualreset (usemanualreset)
{
    pthread_cond_init (&condition, 0);

    pthread_mutexattr_t atts;
    pthread_mutexattr_init (&atts);
   #if ! beast_android
    pthread_mutexattr_setprotocol (&atts, pthread_prio_inherit);
   #endif
    pthread_mutex_init (&mutex, &atts);

    if (initiallysignaled)
        signal ();
}

waitableevent::~waitableevent()
{
    pthread_cond_destroy (&condition);
    pthread_mutex_destroy (&mutex);
}

bool waitableevent::wait () const
{
    return wait (-1);
}

bool waitableevent::wait (const int timeoutmillisecs) const
{
    pthread_mutex_lock (&mutex);

    if (! triggered)
    {
        if (timeoutmillisecs < 0)
        {
            do
            {
                pthread_cond_wait (&condition, &mutex);
            }
            while (! triggered);
        }
        else
        {
            struct timeval now;
            gettimeofday (&now, 0);

            struct timespec time;
            time.tv_sec  = now.tv_sec  + (timeoutmillisecs / 1000);
            time.tv_nsec = (now.tv_usec + ((timeoutmillisecs % 1000) * 1000)) * 1000;

            if (time.tv_nsec >= 1000000000)
            {
                time.tv_nsec -= 1000000000;
                time.tv_sec++;
            }

            do
            {
                if (pthread_cond_timedwait (&condition, &mutex, &time) == etimedout)
                {
                    pthread_mutex_unlock (&mutex);
                    return false;
                }
            }
            while (! triggered);
        }
    }

    if (! manualreset)
        triggered = false;

    pthread_mutex_unlock (&mutex);
    return true;
}

void waitableevent::signal() const
{
    pthread_mutex_lock (&mutex);
    triggered = true;
    pthread_cond_broadcast (&condition);
    pthread_mutex_unlock (&mutex);
}

void waitableevent::reset() const
{
    pthread_mutex_lock (&mutex);
    triggered = false;
    pthread_mutex_unlock (&mutex);
}

}

#endif
