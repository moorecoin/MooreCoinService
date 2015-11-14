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

#ifndef beast_chrono_basic_seconds_clock_h_included
#define beast_chrono_basic_seconds_clock_h_included

#include <beast/chrono/chrono_util.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

namespace beast {

namespace detail {

class seconds_clock_worker
{
public:
    virtual void sample () = 0;
};

//------------------------------------------------------------------------------

// updates the clocks
class seconds_clock_thread
{
public:
    using mutex = std::mutex;
    using cond_var = std::condition_variable;
    using lock_guard = std::lock_guard <mutex>;
    using unique_lock = std::unique_lock <mutex>;
    using clock_type = std::chrono::steady_clock;
    using seconds = std::chrono::seconds;
    using thread = std::thread;
    using workers = std::vector <seconds_clock_worker*>;

    bool stop_;
    mutex mutex_;
    cond_var cond_;
    workers workers_;
    thread thread_;

    seconds_clock_thread ()
        : stop_ (false)
    {
        thread_ = thread (std::bind(
            &seconds_clock_thread::run, this));
    }

    ~seconds_clock_thread ()
    {
        stop();
    }

    void add (seconds_clock_worker& w)
    {
        lock_guard lock (mutex_);
        workers_.push_back (&w);
    }

    void remove (seconds_clock_worker& w)
    {
        lock_guard lock (mutex_);
        workers_.erase (std::find (
            workers_.begin (), workers_.end(), &w));
    }

    void stop()
    {
        if (thread_.joinable())
        {
            {
                lock_guard lock (mutex_);
                stop_ = true;
            }
            cond_.notify_all();
            thread_.join();
        }
    }

    void run()
    {
        unique_lock lock (mutex_);;

        for (;;)
        {
            for (auto iter : workers_)
                iter->sample();

            clock_type::time_point const when (
                floor <seconds> (
                    clock_type::now().time_since_epoch()) +
                        seconds (1));

            if (cond_.wait_until (lock, when, [this]{ return stop_; }))
                return;
        }
    }

    static seconds_clock_thread& instance ()
    {
        static seconds_clock_thread singleton;
        return singleton;
    }
};

}

//------------------------------------------------------------------------------

/** called before main exits to terminate the utility thread.
    this is a workaround for visual studio 2013:
    http://connect.microsoft.com/visualstudio/feedback/details/786016/creating-a-global-c-object-that-used-thread-join-in-its-destructor-causes-a-lockup
    http://stackoverflow.com/questions/10915233/stdthreadjoin-hangs-if-called-after-main-exits-when-using-vs2012-rc
*/
inline
void
basic_seconds_clock_main_hook()
{
#ifdef _msc_ver
    detail::seconds_clock_thread::instance().stop();
#endif
}

/** a clock whose minimum resolution is one second.

    the purpose of this class is to optimize the performance of the now()
    member function call. it uses a dedicated thread that wakes up at least
    once per second to sample the requested trivial clock.

    @tparam clock a type meeting these requirements:
        http://en.cppreference.com/w/cpp/concept/clock
*/
template <class clock>
class basic_seconds_clock
{
public:
    using rep = typename clock::rep;
    using period = typename clock::period;
    using duration = typename clock::duration;
    using time_point = typename clock::time_point;

    static bool const is_steady = clock::is_steady;

    static time_point now()
    {
        // make sure the thread is constructed before the
        // worker otherwise we will crash during destruction
        // of objects with static storage duration.
        struct initializer
        {
            initializer ()
            {
                detail::seconds_clock_thread::instance();
            }
        };
        static initializer init;

        struct worker : detail::seconds_clock_worker
        {
            time_point m_now;
            std::mutex mutex_;

            worker()
                : m_now(clock::now())
            {
                detail::seconds_clock_thread::instance().add(*this);
            }

            ~worker()
            {
                detail::seconds_clock_thread::instance().remove(*this);
            }

            time_point now()
            {
                std::lock_guard<std::mutex> lock (mutex_);
                return m_now;
            }

            void sample()
            {
                std::lock_guard<std::mutex> lock (mutex_);
                m_now = clock::now();
            }
        };

        static worker w;

        return w.now();
    }
};

}

#endif
