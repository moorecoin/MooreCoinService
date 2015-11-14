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

#ifndef beast_nudb_fail_file_h_included
#define beast_nudb_fail_file_h_included

#include <beast/nudb/common.h>
#include <atomic>
#include <cstddef>
#include <string>
#include <utility>

namespace beast {
namespace nudb {

/** thrown when a test failure mode occurs. */
struct fail_error : std::exception
{
    char const*
    what() const noexcept override
    {
        return "test failure";
    }
};

/** countdown to test failure modue. */
class fail_counter
{
private:
    std::size_t target_;
    std::atomic<std::size_t> count_;

public:
    fail_counter (fail_counter const&) = delete;
    fail_counter& operator= (fail_counter const&) = delete;

    explicit
    fail_counter (std::size_t target = 0)
    {
        reset (target);
    }

    /** reset the counter to fail at the nth step, or 0 for no failure. */
    void
    reset (std::size_t n = 0)
    {
        target_ = n;
        count_.store(0);
    }

    bool
    fail()
    {
        return target_ && (++count_ >= target_);
    }
};

/** wrapper to simulate file system failures. */
template <class file>
class fail_file
{
private:
    file f_;
    fail_counter* c_ = nullptr;

public:
    fail_file() = default;
    fail_file (fail_file const&) = delete;
    fail_file& operator= (fail_file const&) = delete;
    ~fail_file() = default;

    fail_file (fail_file&&);

    fail_file&
    operator= (fail_file&& other);

    explicit
    fail_file (fail_counter& c);

    bool
    is_open() const
    {
        return f_.is_open();
    }

    path_type const&
    path() const
    {
        return f_.path();
    }

    std::size_t
    actual_size() const
    {
        return f_.actual_size();
    }

    void
    close()
    {
        f_.close();
    }

    bool
    create (file_mode mode,
        path_type const& path)
    {
        return f_.create(mode, path);
    }

    bool
    open (file_mode mode,
        path_type const& path)
    {
        return f_.open(mode, path);
    }

    static
    void
    erase (path_type const& path)
    {
        file::erase(path);
    }

    void
    read (std::size_t offset,
        void* buffer, std::size_t bytes)
    {
        f_.read(offset, buffer, bytes);
    }

    void
    write (std::size_t offset,
        void const* buffer, std::size_t bytes);

    void
    sync();

    void
    trunc (std::size_t length);

private:
    bool
    fail();

    void
    do_fail();
};

template <class file>
fail_file<file>::fail_file (fail_file&& other)
    : f_ (std::move(other.f_))
    , c_ (other.c_)
{
    other.c_ = nullptr;
}

template <class file>
fail_file<file>&
fail_file<file>::operator= (fail_file&& other)
{
    f_ = std::move(other.f_);
    c_ = other.c_;
    other.c_ = nullptr;
    return *this;
}

template <class file>
fail_file<file>::fail_file (fail_counter& c)
    : c_ (&c)
{
}

template <class file>
void
fail_file<file>::write (std::size_t offset,
    void const* buffer, std::size_t bytes)
{
    if (fail())
        do_fail();
    if (fail())
    {
        f_.write(offset, buffer, (bytes + 1) / 2);
        do_fail();
    }
    f_.write(offset, buffer, bytes);
}

template <class file>
void
fail_file<file>::sync()
{
    if (fail())
        do_fail();
    // we don't need a real sync for
    // testing, it just slows things down.
    //f_.sync();
}

template <class file>
void
fail_file<file>::trunc (std::size_t length)
{
    if (fail())
        do_fail();
    f_.trunc(length);
}

template <class file>
bool
fail_file<file>::fail()
{
    if (c_)
        return c_->fail();
    return false;
}

template <class file>
void
fail_file<file>::do_fail()
{
    throw fail_error();
}

}
}

#endif

