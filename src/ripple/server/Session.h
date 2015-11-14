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

#ifndef ripple_server_session_h_included
#define ripple_server_session_h_included

#include <ripple/server/writer.h>
#include <beast/http/body.h>
#include <beast/http/message.h>
#include <beast/net/ipendpoint.h>
#include <beast/utility/journal.h>
#include <functional>
#include <memory>
#include <ostream>
#include <vector>

namespace ripple {

namespace http {

class session;

/** persistent state information for a connection session.
    these values are preserved between calls for efficiency.
    some fields are input parameters, some are output parameters,
    and all only become defined during specific callbacks.
*/
class session
{
public:
    session() = default;
    session (session const&) = delete;

    /** a user-definable pointer.
        the initial value is always zero.
        changes to the value are persisted between calls.
    */
    void* tag = nullptr;

    /** returns the journal to use for logging. */
    virtual
    beast::journal
    journal() = 0;

    /** returns the port settings for this connection. */
    virtual
    port const&
    port() = 0;

    /** returns the remote address of the connection. */
    virtual
    beast::ip::endpoint
    remoteaddress() = 0;

    /** returns the current http request. */
    virtual
    beast::http::message&
    request() = 0;

    /** returns the content-body of the current http request. */
    virtual
    beast::http::body const&
    body() = 0;

    /** send a copy of data asynchronously. */
    /** @{ */
    void
    write (std::string const& s)
    {
        if (! s.empty())
            write (&s[0],
                std::distance (s.begin(), s.end()));
    }

    template <typename buffersequence>
    void
    write (buffersequence const& buffers)
    {
        for (typename buffersequence::const_iterator iter (buffers.begin());
            iter != buffers.end(); ++iter)
        {
            typename buffersequence::value_type const& buffer (*iter);
            write (boost::asio::buffer_cast <void const*> (buffer),
                boost::asio::buffer_size (buffer));
        }
    }

    virtual
    void
    write (void const* buffer, std::size_t bytes) = 0;

    virtual
    void
    write (std::shared_ptr <writer> const& writer,
        bool keep_alive) = 0;

    /** @} */

    /** detach the session.
        this holds the session open so that the response can be sent
        asynchronously. calls to io_service::run made by the server
        will not return until all detached sessions are closed.
    */
    virtual
    std::shared_ptr<session>
    detach() = 0;

    /** indicate that the response is complete.
        the handler should call this when it has completed writing
        the response. if keep-alive is indicated on the connection,
        this will trigger a read for the next request; else, the
        connection will be closed when all remaining data has been sent.
    */
    virtual
    void
    complete() = 0;

    /** close the session.
        this will be performed asynchronously. the session will be
        closed gracefully after all pending writes have completed.
        @param graceful `true` to wait until all data has finished sending.
    */
    virtual
    void
    close (bool graceful) = 0;
};

}  // namespace http
}  // ripple

#endif
