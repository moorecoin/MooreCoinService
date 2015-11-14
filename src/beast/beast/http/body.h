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

#ifndef beast_http_body_h_included
#define beast_http_body_h_included

#include <cstdint>
#include <memory>
#include <boost/asio/buffer.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/optional.hpp>
#include <beast/cxx14/memory.h> // <memory>
#include <string>

namespace beast {
namespace http {

/** container for the http content-body. */
class body
{
private:
    typedef boost::asio::streambuf buffer_type;

    // hack: use unique_ptr because streambuf cant be moved
    std::unique_ptr <buffer_type> buf_;

public:
    typedef buffer_type::const_buffers_type const_buffers_type;

    body();
    body (body&& other);
    body& operator= (body&& other);

    body (body const&) = delete;
    body& operator= (body const&) = delete;

    template <class = void>
    void
    clear();

    void
    write (void const* data, std::size_t bytes);

    template <class constbuffersequence>
    void
    write (constbuffersequence const& buffers);

    std::size_t
    size() const;

    const_buffers_type
    data() const;
};

template <class = void>
std::string
to_string (body const& b)
{
    std::string s;
    auto const& data (b.data());
    auto const n (boost::asio::buffer_size (data));
    s.resize (n);
    boost::asio::buffer_copy (
        boost::asio::buffer (&s[0], n), data);
    return s;
}

//------------------------------------------------------------------------------

inline
body::body()
    : buf_ (std::make_unique <buffer_type>())
{
}

inline
body::body (body&& other)
    : buf_ (std::move(other.buf_))
{
    other.clear();
}

inline
body&
body::operator= (body&& other)
{
    buf_ = std::move(other.buf_);
    other.clear();
    return *this;
}

template <class>
void
body::clear()
{
    buf_ = std::make_unique <buffer_type>();
}

inline
void
body::write (void const* data, std::size_t bytes)
{
    buf_->commit (boost::asio::buffer_copy (buf_->prepare (bytes),
        boost::asio::const_buffers_1 (data, bytes)));
}

template <class constbuffersequence>
void
body::write (constbuffersequence const& buffers)
{
    for (auto const& buffer : buffers)
        write (boost::asio::buffer_cast <void const*> (buffer),
            boost::asio::buffer_size (buffer));
}

inline
std::size_t
body::size() const
{
    return buf_->size();
}

inline
auto
body::data() const
    -> const_buffers_type
{
    return buf_->data();
}

} // http
} // beast

#endif
