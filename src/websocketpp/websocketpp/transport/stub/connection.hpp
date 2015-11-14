/*
 * copyright (c) 2014, peter thorson. all rights reserved.
 *
 * redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * neither the name of the websocket++ project nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * this software is provided by the copyright holders and contributors "as is"
 * and any express or implied warranties, including, but not limited to, the
 * implied warranties of merchantability and fitness for a particular purpose
 * are disclaimed. in no event shall peter thorson be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential damages
 * (including, but not limited to, procurement of substitute goods or services;
 * loss of use, data, or profits; or business interruption) however caused and
 * on any theory of liability, whether in contract, strict liability, or tort
 * (including negligence or otherwise) arising in any way out of the use of this
 * software, even if advised of the possibility of such damage.
 *
 */

#ifndef websocketpp_transport_stub_con_hpp
#define websocketpp_transport_stub_con_hpp

#include <websocketpp/common/connection_hdl.hpp>
#include <websocketpp/common/memory.hpp>
#include <websocketpp/common/platforms.hpp>
#include <websocketpp/logger/levels.hpp>

#include <websocketpp/transport/base/connection.hpp>
#include <websocketpp/transport/stub/base.hpp>

namespace websocketpp {
namespace transport {
namespace stub {

/// empty timer class to stub out for timer functionality that stub
/// transport doesn't support
struct timer {
    void cancel() {}
};

template <typename config>
class connection : public lib::enable_shared_from_this< connection<config> > {
public:
    /// type of this connection transport component
    typedef connection<config> type;
    /// type of a shared pointer to this connection transport component
    typedef lib::shared_ptr<type> ptr;

    /// transport concurrency policy
    typedef typename config::concurrency_type concurrency_type;
    /// type of this transport's access logging policy
    typedef typename config::alog_type alog_type;
    /// type of this transport's error logging policy
    typedef typename config::elog_type elog_type;

    // concurrency policy types
    typedef typename concurrency_type::scoped_lock_type scoped_lock_type;
    typedef typename concurrency_type::mutex_type mutex_type;

    typedef lib::shared_ptr<timer> timer_ptr;

    explicit connection(bool is_server, alog_type & alog, elog_type & elog)
    {
        m_alog.write(log::alevel::devel,"stub con transport constructor");
    }

    /// get a shared pointer to this component
    ptr get_shared() {
        return type::shared_from_this();
    }

    /// set whether or not this connection is secure
    /**
     * todo: docs
     *
     * @since 0.3.0-alpha4
     *
     * @param value whether or not this connection is secure.
     */
    void set_secure(bool value) {}

    /// tests whether or not the underlying transport is secure
    /**
     * todo: docs
     *
     * @return whether or not the underlying transport is secure
     */
    bool is_secure() const {
        return false;
    }

    /// set human readable remote endpoint address
    /**
     * sets the remote endpoint address returned by `get_remote_endpoint`. this
     * value should be a human readable string that describes the remote
     * endpoint. typically an ip address or hostname, perhaps with a port. but
     * may be something else depending on the nature of the underlying
     * transport.
     *
     * if none is set a default is returned.
     *
     * @since 0.3.0-alpha4
     *
     * @param value the remote endpoint address to set.
     */
    void set_remote_endpoint(std::string value) {}

    /// get human readable remote endpoint address
    /**
     * todo: docs
     *
     * this value is used in access and error logs and is available to the end
     * application for including in user facing interfaces and messages.
     *
     * @return a string identifying the address of the remote endpoint
     */
    std::string get_remote_endpoint() const {
        return "unknown (stub transport)";
    }

    /// get the connection handle
    /**
     * @return the handle for this connection.
     */
    connection_hdl get_handle() const {
        return connection_hdl();
    }

    /// call back a function after a period of time.
    /**
     * timers are not implemented in this transport. the timer pointer will
     * always be empty. the handler will never be called.
     *
     * @param duration length of time to wait in milliseconds
     * @param callback the function to call back when the timer has expired
     * @return a handle that can be used to cancel the timer if it is no longer
     * needed.
     */
    timer_ptr set_timer(long duration, timer_handler handler) {
        return timer_ptr();
    }
protected:
    /// initialize the connection transport
    /**
     * initialize the connection's transport component.
     *
     * @param handler the `init_handler` to call when initialization is done
     */
    void init(init_handler handler) {
        m_alog.write(log::alevel::devel,"stub connection init");
        handler(make_error_code(error::not_implimented));
    }

    /// initiate an async_read for at least num_bytes bytes into buf
    /**
     * initiates an async_read request for at least num_bytes bytes. the input
     * will be read into buf. a maximum of len bytes will be input. when the
     * operation is complete, handler will be called with the status and number
     * of bytes read.
     *
     * this method may or may not call handler from within the initial call. the
     * application should be prepared to accept either.
     *
     * the application should never call this method a second time before it has
     * been called back for the first read. if this is done, the second read
     * will be called back immediately with a double_read error.
     *
     * if num_bytes or len are zero handler will be called back immediately
     * indicating success.
     *
     * @param num_bytes don't call handler until at least this many bytes have
     * been read.
     * @param buf the buffer to read bytes into
     * @param len the size of buf. at maximum, this many bytes will be read.
     * @param handler the callback to invoke when the operation is complete or
     * ends in an error
     */
    void async_read_at_least(size_t num_bytes, char *buf, size_t len,
        read_handler handler)
    {
        m_alog.write(log::alevel::devel, "stub_con async_read_at_least");
        handler(make_error_code(error::not_implimented));
    }

    /// asyncronous transport write
    /**
     * write len bytes in buf to the output stream. call handler to report
     * success or failure. handler may or may not be called during async_write,
     * but it must be safe for this to happen.
     *
     * will return 0 on success.
     *
     * @param buf buffer to read bytes from
     * @param len number of bytes to write
     * @param handler callback to invoke with operation status.
     */
    void async_write(char const * buf, size_t len, write_handler handler) {
        m_alog.write(log::alevel::devel,"stub_con async_write");
        handler(make_error_code(error::not_implimented));
    }

    /// asyncronous transport write (scatter-gather)
    /**
     * write a sequence of buffers to the output stream. call handler to report
     * success or failure. handler may or may not be called during async_write,
     * but it must be safe for this to happen.
     *
     * will return 0 on success.
     *
     * @param bufs vector of buffers to write
     * @param handler callback to invoke with operation status.
     */
    void async_write(std::vector<buffer> const & bufs, write_handler handler) {
        m_alog.write(log::alevel::devel,"stub_con async_write buffer list");
        handler(make_error_code(error::not_implimented));
    }

    /// set connection handle
    /**
     * @param hdl the new handle
     */
    void set_handle(connection_hdl hdl) {}

    /// call given handler back within the transport's event system (if present)
    /**
     * invoke a callback within the transport's event system if it has one. if
     * it doesn't, the handler will be invoked immediately before this function
     * returns.
     *
     * @param handler the callback to invoke
     *
     * @return whether or not the transport was able to register the handler for
     * callback.
     */
    lib::error_code dispatch(dispatch_handler handler) {
        handler();
        return lib::error_code();
    }

    /// perform cleanup on socket shutdown_handler
    /**
     * @param h the `shutdown_handler` to call back when complete
     */
    void async_shutdown(shutdown_handler handler) {
        handler(lib::error_code());
    }
private:
    // member variables!
};


} // namespace stub
} // namespace transport
} // namespace websocketpp

#endif // websocketpp_transport_stub_con_hpp
