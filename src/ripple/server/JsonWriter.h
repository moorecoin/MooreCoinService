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

#ifndef ripple_server_jsonwriter_h_included
#define ripple_server_jsonwriter_h_included

#include <ripple/server/writer.h>
#include <ripple/json/json_value.h>
#include <beast/asio/streambuf.h>
#include <beast/http/message.h>
#include <algorithm>
#include <cstddef>
#include <sstream>

namespace ripple {
namespace http {

namespace detail {

/** writer that sends to streambufs sequentially. */
template <class streambuf>
class message_writer : public writer
{
private:
    streambuf prebody_;
    streambuf body_;
    std::size_t hint_ = 0;
    std::vector<boost::asio::const_buffer> data_;

public:
    message_writer (streambuf&& prebody, streambuf&& body);

    bool
    complete() override;

    bool
    prepare (std::size_t n, std::function<void(void)>) override;

    std::vector<boost::asio::const_buffer>
    data() override;

    void
    consume (std::size_t n) override;

private:
    std::vector<boost::asio::const_buffer>
    data(streambuf& buf);
};

template <class streambuf>
message_writer<streambuf>::message_writer (
        streambuf&& prebody, streambuf&& body)
    : prebody_(std::move(prebody))
    , body_(std::move(body))
{
}

template <class streambuf>
bool
message_writer<streambuf>::complete()
{
    return prebody_.size() == 0 && body_.size() == 0;
}

template <class streambuf>
bool
message_writer<streambuf>::prepare (
    std::size_t n, std::function<void(void)>)
{
    hint_ = n;
    return true;
}

template <class streambuf>
std::vector<boost::asio::const_buffer>
message_writer<streambuf>::data()
{
    return (prebody_.size() > 0) ?
        data(prebody_) : data(body_);
}

template <class streambuf>
void
message_writer<streambuf>::consume (std::size_t n)
{
    if (prebody_.size() > 0)
        return prebody_.consume(n);
    body_.consume(n);
}

template <class streambuf>
std::vector<boost::asio::const_buffer>
message_writer<streambuf>::data(streambuf& buf)
{
    data_.resize(0);
    for (auto iter = buf.data().begin();
        hint_ > 0 && iter != buf.data().end(); ++iter)
    {
        auto const n = std::min(hint_,
            boost::asio::buffer_size(*iter));
        data_.emplace_back(boost::asio::buffer_cast<
            void const*>(*iter), n);
        hint_ -= n;
    }
    return data_;
}

} // detail

using streambufs_writer = detail::message_writer<beast::asio::streambuf>;

//------------------------------------------------------------------------------

/** write a json::value to a streambuf. */
template <class streambuf>
void
write(streambuf& buf, json::value const& json)
{
    stream(json,
        [&buf](void const* data, std::size_t n)
        {
            buf.commit(boost::asio::buffer_copy(
                buf.prepare(n), boost::asio::buffer(data, n)));
        });
}

/** returns a writer that streams the provided http message and json body.
    the message is modified to include the correct headers.
*/
template <class = void>
std::shared_ptr<writer>
make_jsonwriter (beast::http::message& m, json::value const& json)
{
    beast::asio::streambuf prebody;
    beast::asio::streambuf body;
    write(body, json);
    // vfalco todo better way to set a field
    m.headers.erase ("content-length");
    m.headers.append("content-length", std::to_string(body.size()));
    m.headers.erase ("content-type");
    m.headers.append("content-type", "application/json");
    write(prebody, m);
    return std::make_shared<streambufs_writer>(
        std::move(prebody), std::move(body));
}

}
}

#endif
