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

#ifndef websocketpp_transport_iostream_con_hpp
#define websocketpp_transport_iostream_con_hpp

#include <websocketpp/common/connection_hdl.hpp>
#include <websocketpp/common/memory.hpp>
#include <websocketpp/common/platforms.hpp>
#include <websocketpp/logger/levels.hpp>

#include <websocketpp/transport/base/connection.hpp>
#include <websocketpp/transport/iostream/base.hpp>

#include <sstream>
#include <vector>

namespace websocketpp {
namespace transport {
namespace iostream {

/// empty timer class to stub out for timer functionality that iostream
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
      : m_output_stream(null)
      , m_reading(false)
      , m_is_server(is_server)
      , m_is_secure(false)
      , m_alog(alog)
      , m_elog(elog)
      , m_remote_endpoint("iostream transport")
    {
        m_alog.write(log::alevel::devel,"iostream con transport constructor");
    }

    /// get a shared pointer to this component
    ptr get_shared() {
        return type::shared_from_this();
    }

    /// register a std::ostream with the transport for writing output
    /**
     * register a std::ostream with the transport. all future writes will be
     * done to this output stream.
     *
     * @param o a pointer to the ostream to use for output.
     */
    void register_ostream(std::ostream * o) {
        // todo: lock transport state?
        scoped_lock_type lock(m_read_mutex);
        m_output_stream = o;
    }

    /// overloaded stream input operator
    /**
     * attempts to read input from the given stream into the transport. bytes
     * will be extracted from the input stream to fulfill any pending reads.
     * input in this manner will only read until the current read buffer has
     * been filled. then it will signal the library to process the input. if the
     * library's input handler adds a new async_read, additional bytes will be
     * read, otherwise the input operation will end.
     *
     * when this function returns one of the following conditions is true:
     * - there is no outstanding read operation
     * - there are no more bytes available in the input stream
     *
     * you can use tellg() on the input stream to determine if all of the input
     * bytes were read or not.
     *
     * if there is no pending read operation when the input method is called, it
     * will return immediately and tellg() will not have changed.
     */
    friend std::istream & operator>> (std::istream & in, type & t) {
        // this serializes calls to external read.
        scoped_lock_type lock(t.m_read_mutex);

        t.read(in);

        return in;
    }

    /// manual input supply (read some)
    /**
     * copies bytes from buf into websocket++'s input buffers. bytes will be
     * copied from the supplied buffer to fulfill any pending library reads. it
     * will return the number of bytes successfully processed. if there are no
     * pending reads read_some will return immediately. not all of the bytes may
     * be able to be read in one call.
     *
     * @since 0.3.0-alpha4
     *
     * @param buf char buffer to read into the websocket
     * @param len length of buf
     * @return the number of characters from buf actually read.
     */
    size_t read_some(char const * buf, size_t len) {
        // this serializes calls to external read.
        scoped_lock_type lock(m_read_mutex);

        return this->read_some_impl(buf,len);
    }
    
    /// manual input supply (read all)
    /**
     * similar to read_some, but continues to read until all bytes in the
     * supplied buffer have been read or the connection runs out of read
     * requests.
     *
     * this method still may not read all of the bytes in the input buffer. if
     * it doesn't it indicates that the connection was most likely closed or
     * is in an error state where it is no longer accepting new input.
     *
     * @since 0.3.0
     *
     * @param buf char buffer to read into the websocket
     * @param len length of buf
     * @return the number of characters from buf actually read.
     */
    size_t read_all(char const * buf, size_t len) {
        // this serializes calls to external read.
        scoped_lock_type lock(m_read_mutex);
        
        size_t total_read = 0;
        size_t temp_read = 0;

        do {
            temp_read = this->read_some_impl(buf+total_read,len-total_read);
            total_read += temp_read;
        } while (temp_read != 0 && total_read < len);

        return total_read;
    }

    /// manual input supply (deprecated)
    /**
     * @deprecated deprecated in favor of read_some()
     * @see read_some()
     */
    size_t readsome(char const * buf, size_t len) {
        return this->read_some(buf,len);
    }

    /// signal eof
    /**
     * signals to the transport that data stream being read has reached eof and
     * that no more bytes may be read or written to/from the transport.
     *
     * @since 0.3.0-alpha4
     */
    void eof() {
        // this serializes calls to external read.
        scoped_lock_type lock(m_read_mutex);

        if (m_reading) {
            complete_read(make_error_code(transport::error::eof));
        }
    }

    /// signal transport error
    /**
     * signals to the transport that a fatal data stream error has occurred and
     * that no more bytes may be read or written to/from the transport.
     *
     * @since 0.3.0-alpha4
     */
    void fatal_error() {
        // this serializes calls to external read.
        scoped_lock_type lock(m_read_mutex);

        if (m_reading) {
            complete_read(make_error_code(transport::error::pass_through));
        }
    }

    /// set whether or not this connection is secure
    /**
     * the iostream transport does not provide any security features. as such
     * it defaults to returning false when `is_secure` is called. however, the
     * iostream transport may be used to wrap an external socket api that may
     * provide secure transport. this method allows that external api to flag
     * whether or not this connection is secure so that users of the websocket++
     * api will get more accurate information.
     *
     * @since 0.3.0-alpha4
     *
     * @param value whether or not this connection is secure.
     */
    void set_secure(bool value) {
        m_is_secure = value;
    }

    /// tests whether or not the underlying transport is secure
    /**
     * iostream transport will return false always because it has no information
     * about the ultimate remote endpoint. this may or may not be accurate
     * depending on the real source of bytes being input. the `set_secure`
     * method may be used to flag connections that are secured by an external
     * api
     *
     * @return whether or not the underlying transport is secure
     */
    bool is_secure() const {
        return m_is_secure;
    }

    /// set human readable remote endpoint address
    /**
     * sets the remote endpoint address returned by `get_remote_endpoint`. this
     * value should be a human readable string that describes the remote
     * endpoint. typically an ip address or hostname, perhaps with a port. but
     * may be something else depending on the nature of the underlying
     * transport.
     *
     * if none is set the default is "iostream transport".
     *
     * @since 0.3.0-alpha4
     *
     * @param value the remote endpoint address to set.
     */
    void set_remote_endpoint(std::string value) {
        m_remote_endpoint = value;
    }

    /// get human readable remote endpoint address
    /**
     * the iostream transport has no information about the ultimate remote
     * endpoint. it will return the string "iostream transport". the
     * `set_remote_endpoint` method may be used by external network code to set
     * a more accurate value.
     *
     * this value is used in access and error logs and is available to the end
     * application for including in user facing interfaces and messages.
     *
     * @return a string identifying the address of the remote endpoint
     */
    std::string get_remote_endpoint() const {
        return m_remote_endpoint;
    }

    /// get the connection handle
    /**
     * @return the handle for this connection.
     */
    connection_hdl get_handle() const {
        return m_connection_hdl;
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
    timer_ptr set_timer(long, timer_handler) {
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
        m_alog.write(log::alevel::devel,"iostream connection init");
        handler(lib::error_code());
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
        std::stringstream s;
        s << "iostream_con async_read_at_least: " << num_bytes;
        m_alog.write(log::alevel::devel,s.str());

        if (num_bytes > len) {
            handler(make_error_code(error::invalid_num_bytes),size_t(0));
            return;
        }

        if (m_reading == true) {
            handler(make_error_code(error::double_read),size_t(0));
            return;
        }

        if (num_bytes == 0 || len == 0) {
            handler(lib::error_code(),size_t(0));
            return;
        }

        m_buf = buf;
        m_len = len;
        m_bytes_needed = num_bytes;
        m_read_handler = handler;
        m_cursor = 0;
        m_reading = true;
    }

    /// asyncronous transport write
    /**
     * write len bytes in buf to the output stream. call handler to report
     * success or failure. handler may or may not be called during async_write,
     * but it must be safe for this to happen.
     *
     * will return 0 on success. other possible errors (not exhaustive)
     * output_stream_required: no output stream was registered to write to
     * bad_stream: a ostream pass through error
     *
     * @param buf buffer to read bytes from
     * @param len number of bytes to write
     * @param handler callback to invoke with operation status.
     */
    void async_write(char const * buf, size_t len, write_handler handler) {
        m_alog.write(log::alevel::devel,"iostream_con async_write");
        // todo: lock transport state?

        if (!m_output_stream) {
            handler(make_error_code(error::output_stream_required));
            return;
        }

        m_output_stream->write(buf,len);

        if (m_output_stream->bad()) {
            handler(make_error_code(error::bad_stream));
        } else {
            handler(lib::error_code());
        }
    }

    /// asyncronous transport write (scatter-gather)
    /**
     * write a sequence of buffers to the output stream. call handler to report
     * success or failure. handler may or may not be called during async_write,
     * but it must be safe for this to happen.
     *
     * will return 0 on success. other possible errors (not exhaustive)
     * output_stream_required: no output stream was registered to write to
     * bad_stream: a ostream pass through error
     *
     * @param bufs vector of buffers to write
     * @param handler callback to invoke with operation status.
     */
    void async_write(std::vector<buffer> const & bufs, write_handler handler) {
        m_alog.write(log::alevel::devel,"iostream_con async_write buffer list");
        // todo: lock transport state?

        if (!m_output_stream) {
            handler(make_error_code(error::output_stream_required));
            return;
        }

        std::vector<buffer>::const_iterator it;
        for (it = bufs.begin(); it != bufs.end(); it++) {
            m_output_stream->write((*it).buf,(*it).len);

            if (m_output_stream->bad()) {
                handler(make_error_code(error::bad_stream));
            }
        }

        handler(lib::error_code());
    }

    /// set connection handle
    /**
     * @param hdl the new handle
     */
    void set_handle(connection_hdl hdl) {
        m_connection_hdl = hdl;
    }

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
    void read(std::istream &in) {
        m_alog.write(log::alevel::devel,"iostream_con read");

        while (in.good()) {
            if (!m_reading) {
                m_elog.write(log::elevel::devel,"write while not reading");
                break;
            }

            in.read(m_buf+m_cursor,static_cast<std::streamsize>(m_len-m_cursor));

            if (in.gcount() == 0) {
                m_elog.write(log::elevel::devel,"read zero bytes");
                break;
            }

            m_cursor += static_cast<size_t>(in.gcount());

            // todo: error handling
            if (in.bad()) {
                m_reading = false;
                complete_read(make_error_code(error::bad_stream));
            }

            if (m_cursor >= m_bytes_needed) {
                m_reading = false;
                complete_read(lib::error_code());
            }
        }
    }

    size_t read_some_impl(char const * buf, size_t len) {
        m_alog.write(log::alevel::devel,"iostream_con read_some");

        if (!m_reading) {
            m_elog.write(log::elevel::devel,"write while not reading");
            return 0;
        }

        size_t bytes_to_copy = (std::min)(len,m_len-m_cursor);

        std::copy(buf,buf+bytes_to_copy,m_buf+m_cursor);

        m_cursor += bytes_to_copy;

        if (m_cursor >= m_bytes_needed) {
            complete_read(lib::error_code());
        }

        return bytes_to_copy;
    }

    /// signal that a requested read is complete
    /**
     * sets the reading flag to false and returns the handler that should be
     * called back with the result of the read. the cursor position that is sent
     * is whatever the value of m_cursor is.
     *
     * it must not be called when m_reading is false.
     * it must be called while holding the read lock
     *
     * it is important to use this method rather than directly setting/calling
     * m_read_handler back because this function makes sure to delete the
     * locally stored handler which contains shared pointers that will otherwise
     * cause circular reference based memory leaks.
     *
     * @param ec the error code to forward to the read handler
     */
    void complete_read(lib::error_code const & ec) {
        m_reading = false;

        read_handler handler = m_read_handler;
        m_read_handler = read_handler();

        handler(ec,m_cursor);
    }

    // read space (protected by m_read_mutex)
    char *          m_buf;
    size_t          m_len;
    size_t          m_bytes_needed;
    read_handler    m_read_handler;
    size_t          m_cursor;

    // transport resources
    std::ostream *  m_output_stream;
    connection_hdl  m_connection_hdl;

    bool            m_reading;
    bool const      m_is_server;
    bool            m_is_secure;
    alog_type &     m_alog;
    elog_type &     m_elog;
    std::string     m_remote_endpoint;

    // this lock ensures that only one thread can edit read data for this
    // connection. this is a very coarse lock that is basically locked all the
    // time. the nature of the connection is such that it cannot be
    // parallelized, the locking is here to prevent intra-connection concurrency
    // in order to allow inter-connection concurrency.
    mutex_type      m_read_mutex;
};


} // namespace iostream
} // namespace transport
} // namespace websocketpp

#endif // websocketpp_transport_iostream_con_hpp
