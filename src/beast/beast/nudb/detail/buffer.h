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

#ifndef beast_nudb_detail_buffer_h_included
#define beast_nudb_detail_buffer_h_included

#include <atomic>
#include <cstdint>
#include <memory>

namespace beast {
namespace nudb {
namespace detail {

// simple growable memory buffer
class buffer
{
private:
    std::size_t size_ = 0;
    std::unique_ptr<std::uint8_t[]> buf_;

public:
    ~buffer() = default;
    buffer() = default;
    buffer (buffer const&) = delete;
    buffer& operator= (buffer const&) = delete;

    explicit
    buffer (std::size_t n)
        : size_ (n)
        , buf_ (new std::uint8_t[n])
    {
    }

    buffer (buffer&& other)
        : size_ (other.size_)
        , buf_ (std::move(other.buf_))
    {
        other.size_ = 0;
    }

    buffer& operator= (buffer&& other)
    {
        size_ = other.size_;
        buf_ = std::move(other.buf_);
        other.size_ = 0;
        return *this;
    }

    std::size_t
    size() const
    {
        return size_;
    }

    std::uint8_t*
    get() const
    {
        return buf_.get();
    }

    void
    reserve (std::size_t n)
    {
        if (size_ < n)
            buf_.reset (new std::uint8_t[n]);
        size_ = n;
    }

    // bufferfactory
    void*
    operator() (std::size_t n)
    {
        reserve(n);
        return buf_.get();
    }
};

} // detail
} // nudb
} // beast

#endif
