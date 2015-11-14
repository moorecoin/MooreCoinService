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

#include <beast/threads/recursivemutex.h>

#if beast_windows

#include <windows.h>
#undef check
#undef direct
#undef max
#undef min
#undef type_bool

namespace beast {

recursivemutex::recursivemutex()
{
    // (just to check the ms haven't changed this structure and broken things...)
    static_assert (sizeof (critical_section) <= sizeof (section), "");

    initializecriticalsection ((critical_section*) section);
}

recursivemutex::~recursivemutex()
{
    deletecriticalsection ((critical_section*) section);
}

void recursivemutex::lock() const
{
    entercriticalsection ((critical_section*) section);
}

void recursivemutex::unlock() const
{
    leavecriticalsection ((critical_section*) section);
}

bool recursivemutex::try_lock() const
{
    return tryentercriticalsection ((critical_section*) section) != false;
}

}

#else

namespace beast {

recursivemutex::recursivemutex()
{
    pthread_mutexattr_t atts;
    pthread_mutexattr_init (&atts);
    pthread_mutexattr_settype (&atts, pthread_mutex_recursive);
#if ! beast_android
    pthread_mutexattr_setprotocol (&atts, pthread_prio_inherit);
#endif
    pthread_mutex_init (&mutex, &atts);
    pthread_mutexattr_destroy (&atts);
}

recursivemutex::~recursivemutex()
{
    pthread_mutex_destroy (&mutex);
}

void recursivemutex::lock() const
{
    pthread_mutex_lock (&mutex);
}

void recursivemutex::unlock() const
{
    pthread_mutex_unlock (&mutex);
}

bool recursivemutex::try_lock() const
{
    return pthread_mutex_trylock (&mutex) == 0;
}

}

#endif
