//------------------------------------------------------------------------------
/*
    this file is part of beast: https://github.com/vinniefalco/beast
    copyright 2014, vinnie falco <vinnie.falco@gmail.com>

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

#ifndef beast_nudb_gentex_h_included
#define beast_nudb_gentex_h_included

#include <beast/utility/noexcept.h>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <system_error>

namespace beast {
namespace nudb {
namespace detail {

//  generation counting mutex
//
template <class = void>
class gentex_t
{
private:
    std::mutex m_;
    std::size_t gen_ = 0;
    std::size_t cur_ = 0;
    std::size_t prev_ = 0;
    std::condition_variable cond_;

public:
    gentex_t() = default;
    gentex_t (gentex_t const&) = delete;
    gentex_t& operator= (gentex_t const&) = delete;

    void
    lock();

    void
    unlock();

    std::size_t
    lock_gen();

    void
    unlock_gen (std::size_t gen);
};

template <class _>
void
gentex_t<_>::lock()
{
    std::lock_guard<
        std::mutex> l(m_);
    prev_ += cur_;
    cur_ = 0;
    ++gen_;
}

template <class _>
void
gentex_t<_>::unlock()
{
    std::unique_lock<
        std::mutex> l(m_);
    while (prev_ > 0)
        cond_.wait(l);
}

template <class _>
std::size_t
gentex_t<_>::lock_gen()
{
    std::lock_guard<
        std::mutex> l(m_);
    ++cur_;
    return gen_;
}

template <class _>
void
gentex_t<_>::unlock_gen (
    std::size_t gen)
{
    std::lock_guard<
        std::mutex> l(m_);
    if (gen == gen_)
    {
        --cur_;
    }
    else
    {
        --prev_;
        if (prev_ == 0)
                cond_.notify_all();
    }
}

using gentex = gentex_t<>;

//------------------------------------------------------------------------------

template <class generationlockable>
class genlock
{
private:
    bool owned_ = false;
    generationlockable* g_ = nullptr;
    std::size_t gen_;

public:
    using mutex_type = generationlockable;

    genlock() = default;
    genlock (genlock const&) = delete;
    genlock& operator= (genlock const&) = delete;

    genlock (genlock&& other);

    genlock& operator= (genlock&& other);

    explicit
    genlock (mutex_type& g);

    genlock (mutex_type& g, std::defer_lock_t);

    ~genlock();

    mutex_type*
    mutex() noexcept
    {
        return g_;
    }

    bool
    owns_lock() const noexcept
    {
        return g_ && owned_;
    }

    explicit
    operator bool() const noexcept
    {
        return owns_lock();
    }

    void
    lock();

    void
    unlock();

    mutex_type*
    release() noexcept;

    template <class u>
    friend
    void
    swap (genlock<u>& lhs, genlock<u>& rhs) noexcept;
};

template <class g>
genlock<g>::genlock (genlock&& other)
    : owned_ (other.owned_)
    , g_ (other.g_)
{
    other.owned_ = false;
    other.g_ = nullptr;
}

template <class g>
genlock<g>&
genlock<g>::operator= (genlock&& other)
{
    if (owns_lock())
        unlock();
    owned_ = other.owned_;
    g_ = other.g_;
    other.owned_ = false;
    other.g_ = nullptr;
    return *this;
}

template <class g>
genlock<g>::genlock (mutex_type& g)
    : g_ (&g)
{
    lock();
}

template <class g>
genlock<g>::genlock (
        mutex_type& g, std::defer_lock_t)
    : g_ (&g)
{
}

template <class g>
genlock<g>::~genlock()
{
    if (owns_lock())
        unlock();
}

template <class g>
void
genlock<g>::lock()
{
    if (! g_)
        throw std::system_error(std::make_error_code(
            std::errc::operation_not_permitted),
                "genlock: no associated mutex");
    if (owned_)
        throw std::system_error(std::make_error_code(
            std::errc::resource_deadlock_would_occur),
                "genlock: already owned");
    gen_ = g_->lock_gen();
    owned_ = true;
}

template <class g>
void
genlock<g>::unlock()
{
    if (! g_)
        throw std::system_error(std::make_error_code(
            std::errc::operation_not_permitted),
                "genlock: no associated mutex");
    if (! owned_)
        throw std::system_error(std::make_error_code(
            std::errc::operation_not_permitted),
                "genlock: not owned");
    g_->unlock_gen (gen_);
    owned_ = false;
}

template <class g>
auto
genlock<g>::release() noexcept ->
    mutex_type*
{
    mutex_type* const g = g_;
    g_ = nullptr;
    return g;
}

template <class g>
void
swap (genlock<g>& lhs, genlock<g>& rhs) noexcept
{
    using namespace std;
    swap (lhs.owned_, rhs.owned_);
    swap (lhs.g_, rhs.g_);
}

} // detail
} // nudb
} // beast


#endif
