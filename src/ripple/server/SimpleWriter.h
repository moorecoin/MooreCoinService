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

#ifndef ripple_server_simplewriter_h_included
#define ripple_server_simplewriter_h_included

#include <ripple/server/writer.h>
#include <beast/asio/streambuf.h>
#include <beast/http/message.h>
#include <utility>

namespace ripple {
namespace http {

/** writer that sends a simple http response with a message body. */
class simplewriter : public writer
{
private:
    beast::http::message message_;
    beast::asio::streambuf streambuf_;
    std::string body_;
    bool prepared_ = false;

public:
    explicit
    simplewriter(beast::http::message&& message)
        : message_(std::forward<beast::http::message>(message))
    {
    }

    beast::http::message&
    message()
    {
        return message_;
    }

    bool
    complete() override
    {
        return streambuf_.size() == 0;
    }

    void
    consume (std::size_t bytes) override
    {
        streambuf_.consume(bytes);
    }

    bool
    prepare (std::size_t bytes,
        std::function<void(void)>) override
    {
        if (! prepared_)
            do_prepare();
        return true;
    }

    std::vector<boost::asio::const_buffer>
    data() override
    {
        auto const& buf = streambuf_.data();
        std::vector<boost::asio::const_buffer> result;
        result.reserve(std::distance(buf.begin(), buf.end()));
        for (auto const& b : buf)
            result.push_back(b);
        return result;
    }

    /** set the content body. */
    void
    body (std::string const& s)
    {
        body_ = s;
    }

private:
    void
    do_prepare()
    {
        prepared_ = true;
        message_.headers.erase("content-length");
        message_.headers.append("content-length",
            std::to_string(body_.size()));
        write(streambuf_, message_);
        write(streambuf_, body_;
    }
};

}
}

#endif
