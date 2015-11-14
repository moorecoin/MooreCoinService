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

#ifndef beast_asio_waitable_executor_h_included
#define beast_asio_waitable_executor_h_included

#include <boost/asio/handler_alloc_hook.hpp>
#include <boost/asio/handler_continuation_hook.hpp>
#include <boost/asio/handler_invoke_hook.hpp>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <beast/cxx14/type_traits.h> // <type_traits>
#include <utility>
#include <vector>

namespace beast {
namespace asio {

namespace detail {

template <class owner, class handler>
class waitable_executor_wrapped_handler
{
private:
    static_assert (std::is_same <std::decay_t <owner>, owner>::value,
        "owner cannot be a const or reference type");

    handler handler_;
    std::reference_wrapper <owner> owner_;
    bool cont_;

public:
    waitable_executor_wrapped_handler (owner& owner,
        handler&& handler, bool continuation = false)
        : handler_ (std::move(handler))
        , owner_ (owner)
    {
        using boost::asio::asio_handler_is_continuation;
        cont_ = continuation ? true :
            asio_handler_is_continuation(
                std::addressof(handler_));
        owner_.get().increment();
    }

    waitable_executor_wrapped_handler (owner& owner,
        handler const& handler, bool continuation = false)
        : handler_ (handler)
        , owner_ (owner)
    {
        using boost::asio::asio_handler_is_continuation;
        cont_ = continuation ? true :
            asio_handler_is_continuation(
                std::addressof(handler_));
        owner_.get().increment();
    }

    ~waitable_executor_wrapped_handler()
    {
        owner_.get().decrement();
    }

    waitable_executor_wrapped_handler (
            waitable_executor_wrapped_handler const& other)
        : handler_ (other.handler_)
        , owner_ (other.owner_)
        , cont_ (other.cont_)
    {
        owner_.get().increment();
    }

    waitable_executor_wrapped_handler (
            waitable_executor_wrapped_handler&& other)
        : handler_ (std::move(other.handler_))
        , owner_ (other.owner_)
        , cont_ (other.cont_)
    {
        owner_.get().increment();
    }

    waitable_executor_wrapped_handler& operator=(
        waitable_executor_wrapped_handler const&) = delete;

    template <class... args>
    void
    operator()(args&&... args)
    {
        handler_(std::forward<args>(args)...);
    }

    template <class... args>
    void
    operator()(args&&... args) const
    {
        handler_(std::forward<args>(args)...);
    }

    template <class function>
    friend
    void
    asio_handler_invoke (function& f,
        waitable_executor_wrapped_handler* h)
    {
        using boost::asio::asio_handler_invoke;
        asio_handler_invoke(f,
            std::addressof(h->handler_));
    }

    template <class function>
    friend
    void
    asio_handler_invoke (function const& f,
        waitable_executor_wrapped_handler* h)
    {
        using boost::asio::asio_handler_invoke;
        asio_handler_invoke(f,
            std::addressof(h->handler_));
    }

    friend
    void*
    asio_handler_allocate (std::size_t size,
        waitable_executor_wrapped_handler* h)
    {
        using boost::asio::asio_handler_allocate;
        return asio_handler_allocate(
            size, std::addressof(h->handler_));
    }

    friend
    void
    asio_handler_deallocate (void* p, std::size_t size,
        waitable_executor_wrapped_handler* h)
    {
        using boost::asio::asio_handler_deallocate;
        asio_handler_deallocate(
            p, size, std::addressof(h->handler_));
    }

    friend
    bool
    asio_handler_is_continuation (
        waitable_executor_wrapped_handler* h)
    {
        return h->cont_;
    }
};

} // detail

//------------------------------------------------------------------------------

/** executor which provides blocking until all handlers are called. */
class waitable_executor
{
private:
    template <class, class>
    friend class detail::waitable_executor_wrapped_handler;

    std::mutex mutex_;
    std::condition_variable cond_;
    std::size_t count_ = 0;
    std::vector<std::function<void(void)>> notify_;

public:
    /** block until all handlers are called. */
    template <class = void>
    void
    wait();

    /** blocks until all handlers are called or time elapses.
        @return `true` if all handlers are done or `false` if the time elapses.
    */
    template <class rep, class period>
    bool
    wait_for (std::chrono::duration<
        rep, period> const& elapsed_time);

    /** blocks until all handlers are called or a time is reached.
        @return `true` if all handlers are done or `false` on timeout.
    */
    template <class clock, class duration>
    bool
    wait_until (std::chrono::time_point<
        clock, duration> const& timeout_time);

    /** call a function asynchronously after all handlers are called.
        the function may be called on the callers thread.
    */
    template <class = void>
    void
    async_wait(std::function<void(void)> f);

    /** create a new handler that dispatches the wrapped handler on the context. */
    template <class handler>
    detail::waitable_executor_wrapped_handler<waitable_executor,
        std::remove_reference_t<handler>>
    wrap (handler&& handler);

private:
    template <class = void>
    void
    increment();

    template <class = void>
    void
    decrement();
};

template <class>
void
waitable_executor::wait()
{
    std::unique_lock<std::mutex> lock(mutex_);
    cond_.wait(lock,
        [this]() { return count_ == 0; });
}

template <class rep, class period>
bool
waitable_executor::wait_for (std::chrono::duration<
    rep, period> const& elapsed_time)
{
    std::unique_lock<std::mutex> lock(mutex_);
    return cond_.wait_for(lock, elapsed_time,
        [this]() { return count_ == 0; }) ==
            std::cv_status::no_timeout;
}

template <class clock, class duration>
bool
waitable_executor::wait_until (std::chrono::time_point<
    clock, duration> const& timeout_time)
{
    std::unique_lock<std::mutex> lock(mutex_);
    return cond_.wait_until(lock, timeout_time,
        [this]() { return count_ == 0; }) ==
            std::cv_status::no_timeout;
    return true;
}

template <class>
void
waitable_executor::async_wait(std::function<void(void)> f)
{
    bool busy;
    {
        std::lock_guard<std::mutex> _(mutex_);
        busy = count_ > 0;
        if (busy)
            notify_.emplace_back(std::move(f));
    }
    if (! busy)
        f();
}

template <class handler>
detail::waitable_executor_wrapped_handler<waitable_executor,
    std::remove_reference_t<handler>>
waitable_executor::wrap (handler&& handler)
{
    return detail::waitable_executor_wrapped_handler<
        waitable_executor, std::remove_reference_t<handler>>(
            *this, std::forward<handler>(handler));
}

template <class>
void
waitable_executor::increment()
{
    std::lock_guard<std::mutex> _(mutex_);
    ++count_;
}

template <class>
void
waitable_executor::decrement()
{
    bool notify;
    std::vector<std::function<void(void)>> list;
    {
        std::lock_guard<std::mutex> _(mutex_);
        notify = --count_ == 0;
        if (notify)
            std::swap(list, notify_);
    }
    if (notify)
    {
        cond_.notify_all();
        for(auto& _ : list)
            _();
    }
}

} // asio
} // beast

#endif
