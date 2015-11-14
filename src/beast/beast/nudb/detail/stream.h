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

#ifndef beast_nudb_stream_h_included
#define beast_nudb_stream_h_included

#include <beast/nudb/common.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>

namespace beast {
namespace nudb {
namespace detail {

// input stream from bytes
template <class = void>
class istream_t
{
private:
    std::uint8_t const* buf_;
    std::size_t size_ = 0;

public:
    istream_t (istream_t const&) = default;
    istream_t& operator= (istream_t const&) = default;

    istream_t (void const* data, std::size_t size)
        : buf_(reinterpret_cast<
            std::uint8_t const*>(data))
        , size_(size)
    {
    }

    template <std::size_t n>
    istream_t (std::array<std::uint8_t, n> const& a)
        : buf_ (a.data())
        , size_ (a.size())
    {
    }

    std::uint8_t const*
    data (std::size_t bytes);

    std::uint8_t const*
    operator()(std::size_t bytes)
    {
        return data(bytes);
    }
};

template <class _>
std::uint8_t const*
istream_t<_>::data (std::size_t bytes)
{
    if (size_ < bytes)
        throw short_read_error();
    auto const data = buf_;
    buf_ = buf_ + bytes;
    size_ -= bytes;
    return data;
}

using istream = istream_t<>;

//------------------------------------------------------------------------------

// output stream to bytes
template <class = void>
class ostream_t
{
private:
    std::uint8_t* buf_;
    std::size_t size_ = 0;

public:
    ostream_t (ostream_t const&) = default;
    ostream_t& operator= (ostream_t const&) = default;

    ostream_t (void* data, std::size_t)
        : buf_ (reinterpret_cast<std::uint8_t*>(data))
    {
    }

    template <std::size_t n>
    ostream_t (std::array<std::uint8_t, n>& a)
        : buf_ (a.data())
    {
    }

    // returns the number of bytes written
    std::size_t
    size() const
    {
        return size_;
    }

    std::uint8_t*
    data (std::size_t bytes);

    std::uint8_t*
    operator()(std::size_t bytes)
    {
        return data(bytes);
    }
};

template <class _>
std::uint8_t*
ostream_t<_>::data (std::size_t bytes)
{
    auto const data = buf_;
    buf_ = buf_ + bytes;
    size_ += bytes;
    return data;
}

using ostream = ostream_t<>;

//------------------------------------------------------------------------------

// read blob
inline
void
read (istream& is,
    void* buffer, std::size_t bytes)
{
    std::memcpy (buffer, is.data(bytes), bytes);
}

// write blob
inline
void
write (ostream& os,
    void const* buffer, std::size_t bytes)
{
    std::memcpy (os.data(bytes), buffer, bytes);
}

} // detail
} // nudb
} // beast

#endif
