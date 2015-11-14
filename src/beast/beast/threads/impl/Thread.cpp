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

#include <beast/threads/thread.h>
#include <beast/smart_ptr/sharedobject.h>
#include <beast/smart_ptr/sharedptr.h>
#include <beast/module/core/time/time.h>

#include <cassert>
#include <thread>

namespace beast {

thread::thread (std::string const& threadname_)
    : threadname (threadname_),
      threadhandle (nullptr),
      shouldexit (false)
{
}

thread::~thread()
{
    /* if your thread class's destructor has been called without first stopping the thread, that
       means that this partially destructed object is still performing some work - and that's
       probably a bad thing!

       to avoid this type of nastiness, always make sure you call stopthread() before or during
       your subclass's destructor.
    */
    assert (! isthreadrunning());

    stopthread ();
}

//==============================================================================
void thread::threadentrypoint()
{
    if (!threadname.empty ())
        setcurrentthreadname (threadname);

    if (startsuspensionevent.wait (10000))
        run();

    closethreadhandle();
}

// used to wrap the incoming call from the platform-specific code
void beast_threadentrypoint (void* userdata)
{
    static_cast <thread*> (userdata)->threadentrypoint();
}

//==============================================================================
void thread::startthread()
{
    const recursivemutex::scopedlocktype  sl (startstoplock);

    shouldexit = false;

    if (threadhandle == nullptr)
    {
        launchthread();
        startsuspensionevent.signal();
    }
}

bool thread::isthreadrunning() const
{
    return threadhandle != nullptr;
}

//==============================================================================
void thread::signalthreadshouldexit()
{
    shouldexit = true;
}

void thread::waitforthreadtoexit () const
{
    while (isthreadrunning())
        std::this_thread::sleep_for (std::chrono::milliseconds (10));
}

void thread::stopthread ()
{
    const recursivemutex::scopedlocktype sl (startstoplock);

    if (isthreadrunning())
    {
        signalthreadshouldexit();
        notify();
        waitforthreadtoexit ();
    }
}

void thread::stopthreadasync ()
{
    const recursivemutex::scopedlocktype sl (startstoplock);

    if (isthreadrunning())
    {
        signalthreadshouldexit();
        notify();
    }
}

//==============================================================================
bool thread::wait (const int timeoutmilliseconds) const
{
    return defaultevent.wait (timeoutmilliseconds);
}

void thread::notify() const
{
    defaultevent.signal();
}

}

//------------------------------------------------------------------------------

#if beast_windows

#include <windows.h>
#include <process.h>
#include <tchar.h>

namespace beast {

hwnd beast_messagewindowhandle = 0;  // (this is used by other parts of the codebase)

void beast_threadentrypoint (void*);

static unsigned int __stdcall threadentryproc (void* userdata)
{
    if (beast_messagewindowhandle != 0)
        attachthreadinput (getwindowthreadprocessid (beast_messagewindowhandle, 0),
                           getcurrentthreadid(), true);

    beast_threadentrypoint (userdata);

    _endthreadex (0);
    return 0;
}

void thread::launchthread()
{
    unsigned int newthreadid;
    threadhandle = (void*) _beginthreadex (0, 0, &threadentryproc, this, 0, &newthreadid);
}

void thread::closethreadhandle()
{
    closehandle ((handle) threadhandle);
    threadhandle = 0;
}

void thread::setcurrentthreadname (std::string const& name)
{
   #if beast_debug && beast_msvc
    struct
    {
        dword dwtype;
        lpcstr szname;
        dword dwthreadid;
        dword dwflags;
    } info;

    info.dwtype = 0x1000;
    info.szname = name.c_str ();
    info.dwthreadid = getcurrentthreadid();
    info.dwflags = 0;

    __try
    {
        raiseexception (0x406d1388 /*ms_vc_exception*/, 0, sizeof (info) / sizeof (ulong_ptr), (ulong_ptr*) &info);
    }
    __except (exception_continue_execution)
    {}
   #else
    (void) name;
   #endif
}

}

//------------------------------------------------------------------------------

#else

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <time.h>
#if beast_bsd
 // ???
#elif beast_mac || beast_ios
#include <foundation/nsthread.h>
#include <foundation/nsstring.h>
#import <objc/message.h>
namespace beast{
#include <beast/module/core/native/osx_objchelpers.h>
}

#else
#include <sys/prctl.h>

#endif

namespace beast {

void beast_threadentrypoint (void*);

extern "c" void* threadentryprocbeast (void*);
extern "c" void* threadentryprocbeast (void* userdata)
{
    beast_autoreleasepool
    {
       #if beast_android
        struct androidthreadscope
        {
            androidthreadscope()   { threadlocaljnienvholder.attach(); }
            ~androidthreadscope()  { threadlocaljnienvholder.detach(); }
        };

        const androidthreadscope androidenv;
       #endif

        beast_threadentrypoint (userdata);
    }

    return nullptr;
}

void thread::launchthread()
{
    threadhandle = 0;
    pthread_t handle = 0;

    if (pthread_create (&handle, 0, threadentryprocbeast, this) == 0)
    {
        pthread_detach (handle);
        threadhandle = (void*) handle;
    }
}

void thread::closethreadhandle()
{
    threadhandle = 0;
}

void thread::setcurrentthreadname (std::string const& name)
{
   #if beast_ios || (beast_mac && defined (mac_os_x_version_10_5) && mac_os_x_version_min_required >= mac_os_x_version_10_5)
    beast_autoreleasepool
    {
        [[nsthread currentthread] setname: beaststringtons (beast::string (name))];
    }
   #elif beast_linux
    #if (__glibc__ * 1000 + __glibc_minor__) >= 2012
     pthread_setname_np (pthread_self(), name.c_str ());
    #else
     prctl (pr_set_name, name.c_str (), 0, 0, 0);
    #endif
   #endif
}

}

//------------------------------------------------------------------------------

#endif

