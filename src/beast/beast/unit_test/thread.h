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

#ifndef beast_unit_test_thread_h_included
#define beast_unit_test_thread_h_included

#include <beast/utility/noexcept.h>
#include <beast/unit_test/suite.h>
#include <functional>
#include <thread>
#include <utility>

namespace beast {
namespace unit_test {

/** replacement for std::thread that handles exceptions in unit tests. */
class thread
{
private:
    suite* s_ = nullptr;
    std::thread t_;

public:
    using id = std::thread::id;
    using native_handle_type = std::thread::native_handle_type;

    thread() = default;
    thread (thread const&) = delete;
    thread& operator= (thread const&) = delete;

    thread (thread&& other)
        : s_ (other.s_)
        , t_ (std::move(other.t_))
    {
    }

    thread& operator= (thread&& other)
    {
        s_ = other.s_;
        t_ = std::move(other.t_);
        return *this;
    }

    template <class f, class... args>
    explicit
    thread (suite& s, f&& f, args&&... args)
        : s_ (&s)
    {
        std::function<void(void)> b = 
            std::bind(std::forward<f>(f),
                std::forward<args>(args)...);
        t_ = std::thread (&thread::run, this,
            std::move(b));
    }

    bool
    joinable() const
    {
        return t_.joinable();
    }

    std::thread::id
    get_id() const
    {
        return t_.get_id();
    }

    static
    unsigned
    hardware_concurrency() noexcept
    {
        return std::thread::hardware_concurrency();
    }

    void
    join()
    {
        t_.join();
        s_->propagate_abort();
    }

    void
    swap (thread& other)
    {
        std::swap(s_, other.s_);
        std::swap(t_, other.t_);
    }

private:
    void
    run (std::function <void(void)> f)
    {
        try
        {
            f();
        }
        catch (suite::abort_exception const&)
        {
        }
    }
};

} // unit_test
} // beast

#endif
