//------------------------------------------------------------------------------
/*
    this file is part of rippled: https://github.com/ripple/rippled
    copyright (c) 2012, 2013 ripple labs inc.

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

#ifndef ripple_overlay_zerocopystream_h_included
#define ripple_overlay_zerocopystream_h_included

#include <google/protobuf/io/zero_copy_stream.h>
#include <boost/asio/buffer.hpp>
#include <cstdint>

namespace ripple {

/** implements zerocopyinputstream around a buffer sequence.
    @tparam buffers a type meeting the requirements of constbuffersequence.
    @see https://developers.google.com/protocol-buffers/docs/reference/cpp/google.protobuf.io.zero_copy_stream
*/
template <class buffers>
class zerocopyinputstream
    : public ::google::protobuf::io::zerocopyinputstream
{
private:
    using iterator = typename buffers::const_iterator;
    using const_buffer = boost::asio::const_buffer;

    std::int64_t count_ = 0;
    iterator last_;
    iterator first_;    // where pos_ comes from
    const_buffer pos_;  // what next() will return

public:
    explicit
    zerocopyinputstream (buffers const& buffers);

    bool
    next (const void** data, int* size) override;

    void
    backup (int count) override;

    bool
    skip (int count) override;

    std::int64_t
    bytecount() const override
    {
        return count_;
    }
};

//------------------------------------------------------------------------------

template <class buffers>
zerocopyinputstream<buffers>::zerocopyinputstream (
        buffers const& buffers)
    : last_ (buffers.end())
    , first_ (buffers.begin())
    , pos_ ((first_ != last_) ?
        *first_ : const_buffer(nullptr, 0))
{
}

template <class buffers>
bool
zerocopyinputstream<buffers>::next (
    const void** data, int* size)
{
    *data = boost::asio::buffer_cast<void const*>(pos_);
    *size = boost::asio::buffer_size(pos_);
    if (first_ == last_)
        return false;
    count_ += *size;
    pos_ = (++first_ != last_) ? *first_ :
        const_buffer(nullptr, 0);
    return true;
}

template <class buffers>
void
zerocopyinputstream<buffers>::backup (int count)
{
    --first_;
    pos_ = *first_ +
        (boost::asio::buffer_size(*first_) - count);
    count_ -= count;
}

template <class buffers>
bool
zerocopyinputstream<buffers>::skip (int count)
{
    if (first_ == last_)
        return false;
    while (count > 0)
    {
        auto const size =
            boost::asio::buffer_size(pos_);
        if (count < size)
        {
            pos_ = pos_ + count;
            count_ += count;
            return true;
        }
        count_ += size;
        if (++first_ == last_)
            return false;
        count -= size;
        pos_ = *first_;
    }
    return true;
}

//------------------------------------------------------------------------------

/** implements zerocopyoutputstream around a streambuf.
    streambuf matches the public interface defined by boost::asio::streambuf.
    @tparam streambuf a type meeting the requirements of streambuf.
*/
template <class streambuf>
class zerocopyoutputstream
    : public ::google::protobuf::io::zerocopyoutputstream
{
private:
    using buffers_type = typename streambuf::mutable_buffers_type;
    using iterator = typename buffers_type::const_iterator;
    using mutable_buffer = boost::asio::mutable_buffer;

    streambuf& streambuf_;
    std::size_t blocksize_;
    std::size_t count_ = 0;
    std::size_t commit_ = 0;
    buffers_type buffers_;
    iterator pos_;

public:
    explicit
    zerocopyoutputstream (streambuf& streambuf,
        std::size_t blocksize);

    ~zerocopyoutputstream();

    bool
    next (void** data, int* size) override;

    void
    backup (int count) override;

    std::int64_t
    bytecount() const override
    {
        return count_;
    }
};

//------------------------------------------------------------------------------

template <class streambuf>
zerocopyoutputstream<streambuf>::zerocopyoutputstream(
        streambuf& streambuf, std::size_t blocksize)
    : streambuf_ (streambuf)
    , blocksize_ (blocksize)
    , buffers_ (streambuf_.prepare(blocksize_))
    , pos_ (buffers_.begin())
{
}

template <class streambuf>
zerocopyoutputstream<streambuf>::~zerocopyoutputstream()
{
    if (commit_ != 0)
        streambuf_.commit(commit_);
}

template <class streambuf>
bool
zerocopyoutputstream<streambuf>::next(
    void** data, int* size)
{
    if (commit_ != 0)
    {
        streambuf_.commit(commit_);
        count_ += commit_;
    }

    if (pos_ == buffers_.end())
    {
        buffers_ = streambuf_.prepare (blocksize_);
        pos_ = buffers_.begin();
    }

    *data = boost::asio::buffer_cast<void*>(*pos_);
    *size = boost::asio::buffer_size(*pos_);
    commit_ = *size;
    ++pos_;
    return true;
}

template <class streambuf>
void
zerocopyoutputstream<streambuf>::backup (int count)
{
    assert(count <= commit_);
    auto const n = commit_ - count;
    streambuf_.commit(n);
    count_ += n;
    commit_ = 0;
}

} // ripple

#endif
