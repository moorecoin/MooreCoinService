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

#ifndef beast_asio_io_latency_probe_h_included
#define beast_asio_io_latency_probe_h_included

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stdexcept>

#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/config.hpp>

namespace beast {

/** measures handler latency on an io_service queue. */
template <class clock>
class io_latency_probe
{
private:
    typedef typename clock::duration duration;
    typedef typename clock::time_point time_point;

    std::recursive_mutex m_mutex;
    std::condition_variable_any m_cond;
    std::size_t m_count;
    duration const m_period;
    boost::asio::io_service& m_ios;
    boost::asio::deadline_timer m_timer;
    bool m_cancel;

public:
    io_latency_probe (duration const& period,
        boost::asio::io_service& ios)
        : m_count (1)
        , m_period (period)
        , m_ios (ios)
        , m_timer (m_ios)
        , m_cancel (false)
    {
    }

    ~io_latency_probe ()
    {
        std::unique_lock <decltype (m_mutex)> lock (m_mutex);
        cancel (lock, true);
    }

    /** return the io_service associated with the latency probe. */
    /** @{ */
    boost::asio::io_service& get_io_service ()
    {
        return m_ios;
    }

    boost::asio::io_service const& get_io_service () const
    {
        return m_ios;
    }
    /** @} */

    /** cancel all pending i/o.
        any handlers which have already been queued will still be called.
    */
    /** @{ */
    void cancel ()
    {
        std::unique_lock <decltype(m_mutex)> lock (m_mutex);
        cancel (lock, true);
    }

    void cancel_async ()
    {
        std::unique_lock <decltype(m_mutex)> lock (m_mutex);
        cancel (lock, false);
    }
    /** @} */

    /** measure one sample of i/o latency.
        handler will be called with this signature:
            void handler (duration d);
    */
    template <class handler>
    void sample_one (handler&& handler)
    {
        std::lock_guard <decltype(m_mutex)> lock (m_mutex);
        if (m_cancel)
            throw std::logic_error ("io_latency_probe is canceled");
        m_ios.post (sample_op <handler> (
            std::forward <handler> (handler),
                clock::now(), false, this));
    }

    /** initiate continuous i/o latency sampling.
        handler will be called with this signature:
            void handler (std::chrono::milliseconds);
    */
    template <class handler>
    void sample (handler&& handler)
    {
        std::lock_guard <decltype(m_mutex)> lock (m_mutex);
        if (m_cancel)
            throw std::logic_error ("io_latency_probe is canceled");
        m_ios.post (sample_op <handler> (
            std::forward <handler> (handler),
                clock::now(), true, this));
    }

private:
    void cancel (std::unique_lock <decltype (m_mutex)>& lock,
        bool wait)
    {
        if (! m_cancel)
        {
            --m_count;
            m_cancel = true;
        }

        if (wait)
#ifdef boost_no_cxx11_lambdas
            while (m_count != 0)
                m_cond.wait (lock);
#else
            m_cond.wait (lock, [this] {
                return this->m_count == 0; });
#endif
    }

    void addref ()
    {
        std::lock_guard <decltype(m_mutex)> lock (m_mutex);
        ++m_count;
    }

    void release ()
    {
        std::lock_guard <decltype(m_mutex)> lock (m_mutex);
        if (--m_count == 0)
            m_cond.notify_all ();
    }

    template <class handler>
    struct sample_op
    {
        handler m_handler;
        time_point m_start;
        bool m_repeat;
        io_latency_probe* m_probe;

        sample_op (handler const& handler, time_point const& start,
            bool repeat, io_latency_probe* probe)
            : m_handler (handler)
            , m_start (start)
            , m_repeat (repeat)
            , m_probe (probe)
        {
            m_probe->addref();
        }

        sample_op (sample_op const& other)
            : m_handler (other.m_handler)
            , m_start (other.m_start)
            , m_probe (other.m_probe)
        {
            m_probe->addref();
        }

        ~sample_op ()
        {
            m_probe->release();
        }

        void operator() () const
        {
            typename clock::time_point const now (clock::now());
            typename clock::duration const elapsed (now - m_start);

            m_handler (elapsed);

            {
                std::lock_guard <decltype (m_probe->m_mutex)
                    > lock (m_probe->m_mutex);
                if (m_probe->m_cancel)
                    return;
            }

            if (m_repeat)
            {
                // calculate when we want to sample again, and
                // adjust for the expected latency.
                //
                typename clock::time_point const when (
                    now + m_probe->m_period - 2 * elapsed);

                if (when <= now)
                {
                    // the latency is too high to maintain the desired
                    // period so don't bother with a timer.
                    //
                    m_probe->m_ios.post (sample_op <handler> (
                        m_handler, now, m_repeat, m_probe));
                }
                else
                {
                    boost::posix_time::microseconds mms (
                        std::chrono::duration_cast <
                            std::chrono::microseconds> (
                                when - now).count ());
                    m_probe->m_timer.expires_from_now (mms);
                    m_probe->m_timer.async_wait (sample_op <handler> (
                        m_handler, now, m_repeat, m_probe));
                }
            }
        }

        void operator () (boost::system::error_code const& ec)
        {
            typename clock::time_point const now (clock::now());
            m_probe->m_ios.post (sample_op <handler> (
                m_handler, now, m_repeat, m_probe));
        }
    };
};

}

#endif
