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

#ifndef beast_http_chunk_encode_h_included
#define beast_http_chunk_encode_h_included

#include <boost/asio/buffer.hpp>
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <iterator>
#include <beast/cxx14/type_traits.h> // <type_traits>

namespace beast {
namespace http {

namespace detail {

template <class buffers>
class chunk_encoded_buffers
{
private:
    using const_buffer = boost::asio::const_buffer;

    buffers buffers_;
    const_buffer head_;
    const_buffer tail_;

    // storage for the longest hex string we might need, plus delimiters.
    std::array<char, 2 * sizeof(std::size_t) + 2> data_;

public:
    using value_type = boost::asio::const_buffer;

    class const_iterator;

    chunk_encoded_buffers() = delete;
    chunk_encoded_buffers (chunk_encoded_buffers const&) = default;
    chunk_encoded_buffers& operator= (chunk_encoded_buffers const&) = default;

    chunk_encoded_buffers (buffers const& buffers, bool final_chunk);

    const_iterator
    begin() const
    {
        return const_iterator(*this, false);
    }

    const_iterator
    end() const
    {
        return const_iterator(*this, true);
    }

private:
    // unchecked conversion of unsigned to hex string
    template<class outiter, class unsigned>
    static
    std::enable_if_t<std::is_unsigned<unsigned>::value, outiter>
    to_hex(outiter const first, outiter const last, unsigned n);
};

template <class buffers>
class chunk_encoded_buffers<buffers>::const_iterator
    : public std::iterator<std::bidirectional_iterator_tag, const_buffer>
{
private:
    using iterator = typename buffers::const_iterator;
    enum class where { head, input, end };
    chunk_encoded_buffers const* buffers_;
    where where_;
    iterator iter_;

public:
    const_iterator();
    const_iterator (const_iterator const&) = default;
    const_iterator& operator= (const_iterator const&) = default;
    bool operator== (const_iterator const& other) const;
    bool operator!= (const_iterator const& other) const;
    const_iterator& operator++();
    const_iterator& operator--();
    const_iterator operator++(int) const;
    const_iterator operator--(int) const;
    const_buffer operator*() const;

private:
    friend class chunk_encoded_buffers;
    const_iterator(chunk_encoded_buffers const& buffers, bool past_the_end);
};

//------------------------------------------------------------------------------

template <class buffers>
chunk_encoded_buffers<buffers>::chunk_encoded_buffers (
        buffers const& buffers, bool final_chunk)
    : buffers_(buffers)
{
    auto const size = boost::asio::buffer_size(buffers);
    data_[data_.size() - 2] = '\r';
    data_[data_.size() - 1] = '\n';
    auto pos = to_hex(data_.begin(), data_.end() - 2, size);
    head_ = const_buffer(&*pos,
        std::distance(pos, data_.end()));
    if (size > 0 && final_chunk)
        tail_ = const_buffer("\r\n0\r\n\r\n", 7);
    else
        tail_ = const_buffer("\r\n", 2);
}

template <class buffers>
template <class outiter, class unsigned>
std::enable_if_t<std::is_unsigned<unsigned>::value, outiter>
chunk_encoded_buffers<buffers>::to_hex(
    outiter const first, outiter const last, unsigned n)
{
    assert(first != last);
    outiter iter = last;
    if(n == 0)
    {
        *--iter = '0';
        return iter;
    }
    while(n)
    {
        assert(iter != first);
        *--iter = "0123456789abcdef"[n&0xf];
        n>>=4;
    }
    return iter;
}

template <class buffers>
chunk_encoded_buffers<buffers>::const_iterator::const_iterator()
    : where_(where::end)
    , buffers_(nullptr)
{
}

template <class buffers>
bool
chunk_encoded_buffers<buffers>::const_iterator::operator==(
    const_iterator const& other) const
{
    return buffers_ == other.buffers_ &&
        where_ == other.where_ && iter_ == other.iter_;
}

template <class buffers>
bool
chunk_encoded_buffers<buffers>::const_iterator::operator!=(
    const_iterator const& other) const
{
    return buffers_ != other.buffers_ ||
        where_ != other.where_ || iter_ != other.iter_;
}

template <class buffers>
auto
chunk_encoded_buffers<buffers>::const_iterator::operator++() ->
    const_iterator&
{
    assert(buffers_);
    assert(where_ != where::end);
    if (where_ == where::head)
        where_ = where::input;
    else if (iter_ != buffers_->buffers_.end())
        ++iter_;
    else
        where_ = where::end;
    return *this;
}

template <class buffers>
auto
chunk_encoded_buffers<buffers>::const_iterator::operator--() ->
    const_iterator&
{
    assert(buffers_);
    assert(where_ != where::begin);
    if (where_ == where::end)
        where_ = where::input;
    else if (iter_ != buffers_->buffers_.begin())
        --iter_;
    else
        where_ = where::head;
    return *this;
}

template <class buffers>
auto
chunk_encoded_buffers<buffers>::const_iterator::operator++(int) const ->
    const_iterator
{
    auto iter = *this;
    ++iter;
    return iter;
}

template <class buffers>
auto
chunk_encoded_buffers<buffers>::const_iterator::operator--(int) const ->
    const_iterator
{
    auto iter = *this;
    --iter;
    return iter;
}

template <class buffers>
auto
chunk_encoded_buffers<buffers>::const_iterator::operator*() const ->
    const_buffer
{
    assert(buffers_);
    assert(where_ != where::end);
    if (where_ == where::head)
        return buffers_->head_;
    if (iter_ != buffers_->buffers_.end())
        return *iter_;
    return buffers_->tail_;
}

template <class buffers>
chunk_encoded_buffers<buffers>::const_iterator::const_iterator(
        chunk_encoded_buffers const& buffers, bool past_the_end)
    : buffers_(&buffers)
    , where_(past_the_end ? where::end : where::head)
    , iter_(past_the_end ? buffers_->buffers_.end() :
        buffers_->buffers_.begin())
{
}

}

/** returns a chunk-encoded buffersequence.

    see:
        http://www.w3.org/protocols/rfc2616/rfc2616-sec3.html#sec3.6.1

    @tparam buffers a type meeting the requirements of buffersequence.
    @param buffers the input buffer sequence.
    @param final_chunk `true` if this should include a final-chunk.
    @return a chunk-encoded constbufferseqeunce representing the input.
*/
/** @{ */
template <class buffers>
detail::chunk_encoded_buffers<buffers>
chunk_encode (buffers const& buffers,
    bool final_chunk = false)
{
    return detail::chunk_encoded_buffers<
        buffers>(buffers, final_chunk);
}

// returns a chunked encoding final chunk.
inline
boost::asio::const_buffers_1
chunk_encode_final()
{
    return boost::asio::const_buffers_1(
        "0\r\n\r\n", 5);
}
/** @} */

} // http
} // beast

#endif
