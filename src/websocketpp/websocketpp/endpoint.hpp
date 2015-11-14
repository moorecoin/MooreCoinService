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

#ifndef websocketpp_endpoint_hpp
#define websocketpp_endpoint_hpp

#include <websocketpp/connection.hpp>
#include <websocketpp/logger/levels.hpp>
#include <websocketpp/version.hpp>

#include <set>

namespace websocketpp {

/// creates and manages connections associated with a websocket endpoint
template <typename connection, typename config>
class endpoint : public config::transport_type, public config::endpoint_base {
public:
    // import appropriate types from our helper class
    // see endpoint_types for more details.
    typedef endpoint<connection,config> type;

    /// type of the transport component of this endpoint
    typedef typename config::transport_type transport_type;
    /// type of the concurrency component of this endpoint
    typedef typename config::concurrency_type concurrency_type;

    /// type of the connections that this endpoint creates
    typedef connection connection_type;
    /// shared pointer to connection_type
    typedef typename connection_type::ptr connection_ptr;
    /// weak pointer to connection type
    typedef typename connection_type::weak_ptr connection_weak_ptr;

    /// type of the transport component of the connections that this endpoint
    /// creates
    typedef typename transport_type::transport_con_type transport_con_type;
    /// type of a shared pointer to the transport component of the connections
    /// that this endpoint creates.
    typedef typename transport_con_type::ptr transport_con_ptr;

    /// type of message_handler
    typedef typename connection_type::message_handler message_handler;
    /// type of message pointers that this endpoint uses
    typedef typename connection_type::message_ptr message_ptr;

    /// type of error logger
    typedef typename config::elog_type elog_type;
    /// type of access logger
    typedef typename config::alog_type alog_type;

    /// type of our concurrency policy's scoped lock object
    typedef typename concurrency_type::scoped_lock_type scoped_lock_type;
    /// type of our concurrency policy's mutex object
    typedef typename concurrency_type::mutex_type mutex_type;

    /// type of rng
    typedef typename config::rng_type rng_type;

    // todo: organize these
    typedef typename connection_type::termination_handler termination_handler;

    typedef lib::shared_ptr<connection_weak_ptr> hdl_type;

    explicit endpoint(bool p_is_server)
      : m_alog(config::alog_level, log::channel_type_hint::access)
      , m_elog(config::elog_level, log::channel_type_hint::error)
      , m_user_agent(::websocketpp::user_agent)
      , m_open_handshake_timeout_dur(config::timeout_open_handshake)
      , m_close_handshake_timeout_dur(config::timeout_close_handshake)
      , m_pong_timeout_dur(config::timeout_pong)
      , m_max_message_size(config::max_message_size)
      , m_is_server(p_is_server)
    {
        m_alog.set_channels(config::alog_level);
        m_elog.set_channels(config::elog_level);

        m_alog.write(log::alevel::devel, "endpoint constructor");

        transport_type::init_logging(&m_alog, &m_elog);
    }

    /// returns the user agent string that this endpoint will use
    /**
     * returns the user agent string that this endpoint will use when creating
     * new connections.
     *
     * the default value for this version is stored in websocketpp::user_agent
     *
     * @return the user agent string.
     */
    std::string get_user_agent() const {
        scoped_lock_type guard(m_mutex);
        return m_user_agent;
    }

    /// sets the user agent string that this endpoint will use
    /**
     * sets the identifier that this endpoint will use when creating new
     * connections. changing this value will only affect future connections.
     * for client endpoints this will be sent as the "user-agent" header in
     * outgoing requests. for server endpoints this will be sent in the "server"
     * response header.
     *
     * setting this value to the empty string will suppress the use of the
     * server and user-agent headers. this is typically done to hide
     * implementation details for security purposes.
     *
     * for best results set this before accepting or opening connections.
     *
     * the default value for this version is stored in websocketpp::user_agent
     *
     * this can be overridden on an individual connection basis by setting a
     * custom "server" header during the validate handler or "user-agent"
     * header on a connection before calling connect().
     *
     * @param ua the string to set the user agent to.
     */
    void set_user_agent(std::string const & ua) {
        scoped_lock_type guard(m_mutex);
        m_user_agent = ua;
    }

    /// returns whether or not this endpoint is a server.
    /**
     * @return whether or not this endpoint is a server
     */
    bool is_server() const {
        return m_is_server;
    }

    /********************************/
    /* pass-through logging adaptor */
    /********************************/

    /// set access logging channel
    /**
     * set the access logger's channel value. the value is a number whose
     * interpretation depends on the logging policy in use.
     *
     * @param channels the channel value(s) to set
     */
    void set_access_channels(log::level channels) {
        m_alog.set_channels(channels);
    }

    /// clear access logging channels
    /**
     * clear the access logger's channel value. the value is a number whose
     * interpretation depends on the logging policy in use.
     *
     * @param channels the channel value(s) to clear
     */
    void clear_access_channels(log::level channels) {
        m_alog.clear_channels(channels);
    }

    /// set error logging channel
    /**
     * set the error logger's channel value. the value is a number whose
     * interpretation depends on the logging policy in use.
     *
     * @param channels the channel value(s) to set
     */
    void set_error_channels(log::level channels) {
        m_elog.set_channels(channels);
    }

    /// clear error logging channels
    /**
     * clear the error logger's channel value. the value is a number whose
     * interpretation depends on the logging policy in use.
     *
     * @param channels the channel value(s) to clear
     */
    void clear_error_channels(log::level channels) {
        m_elog.clear_channels(channels);
    }

    /// get reference to access logger
    /**
     * @return a reference to the access logger
     */
    alog_type & get_alog() {
        return m_alog;
    }

    /// get reference to error logger
    /**
     * @return a reference to the error logger
     */
    elog_type & get_elog() {
        return m_elog;
    }

    /*************************/
    /* set handler functions */
    /*************************/

    void set_open_handler(open_handler h) {
        m_alog.write(log::alevel::devel,"set_open_handler");
        scoped_lock_type guard(m_mutex);
        m_open_handler = h;
    }
    void set_close_handler(close_handler h) {
        m_alog.write(log::alevel::devel,"set_close_handler");
        scoped_lock_type guard(m_mutex);
        m_close_handler = h;
    }
    void set_fail_handler(fail_handler h) {
        m_alog.write(log::alevel::devel,"set_fail_handler");
        scoped_lock_type guard(m_mutex);
        m_fail_handler = h;
    }
    void set_ping_handler(ping_handler h) {
        m_alog.write(log::alevel::devel,"set_ping_handler");
        scoped_lock_type guard(m_mutex);
        m_ping_handler = h;
    }
    void set_pong_handler(pong_handler h) {
        m_alog.write(log::alevel::devel,"set_pong_handler");
        scoped_lock_type guard(m_mutex);
        m_pong_handler = h;
    }
    void set_pong_timeout_handler(pong_timeout_handler h) {
        m_alog.write(log::alevel::devel,"set_pong_timeout_handler");
        scoped_lock_type guard(m_mutex);
        m_pong_timeout_handler = h;
    }
    void set_interrupt_handler(interrupt_handler h) {
        m_alog.write(log::alevel::devel,"set_interrupt_handler");
        scoped_lock_type guard(m_mutex);
        m_interrupt_handler = h;
    }
    void set_http_handler(http_handler h) {
        m_alog.write(log::alevel::devel,"set_http_handler");
        scoped_lock_type guard(m_mutex);
        m_http_handler = h;
    }
    void set_validate_handler(validate_handler h) {
        m_alog.write(log::alevel::devel,"set_validate_handler");
        scoped_lock_type guard(m_mutex);
        m_validate_handler = h;
    }
    void set_message_handler(message_handler h) {
        m_alog.write(log::alevel::devel,"set_message_handler");
        scoped_lock_type guard(m_mutex);
        m_message_handler = h;
    }

    //////////////////////////////////////////
    // connection timeouts and other limits //
    //////////////////////////////////////////

    /// set open handshake timeout
    /**
     * sets the length of time the library will wait after an opening handshake
     * has been initiated before cancelling it. this can be used to prevent
     * excessive wait times for outgoing clients or excessive resource usage
     * from broken clients or dos attacks on servers.
     *
     * connections that time out will have their fail handlers called with the
     * open_handshake_timeout error code.
     *
     * the default value is specified via the compile time config value
     * 'timeout_open_handshake'. the default value in the core config
     * is 5000ms. a value of 0 will disable the timer entirely.
     *
     * to be effective, the transport you are using must support timers. see
     * the documentation for your transport policy for details about its
     * timer support.
     *
     * @param dur the length of the open handshake timeout in ms
     */
    void set_open_handshake_timeout(long dur) {
        scoped_lock_type guard(m_mutex);
        m_open_handshake_timeout_dur = dur;
    }

    /// set close handshake timeout
    /**
     * sets the length of time the library will wait after a closing handshake
     * has been initiated before cancelling it. this can be used to prevent
     * excessive wait times for outgoing clients or excessive resource usage
     * from broken clients or dos attacks on servers.
     *
     * connections that time out will have their close handlers called with the
     * close_handshake_timeout error code.
     *
     * the default value is specified via the compile time config value
     * 'timeout_close_handshake'. the default value in the core config
     * is 5000ms. a value of 0 will disable the timer entirely.
     *
     * to be effective, the transport you are using must support timers. see
     * the documentation for your transport policy for details about its
     * timer support.
     *
     * @param dur the length of the close handshake timeout in ms
     */
    void set_close_handshake_timeout(long dur) {
        scoped_lock_type guard(m_mutex);
        m_close_handshake_timeout_dur = dur;
    }

    /// set pong timeout
    /**
     * sets the length of time the library will wait for a pong response to a
     * ping. this can be used as a keepalive or to detect broken  connections.
     *
     * pong responses that time out will have the pong timeout handler called.
     *
     * the default value is specified via the compile time config value
     * 'timeout_pong'. the default value in the core config
     * is 5000ms. a value of 0 will disable the timer entirely.
     *
     * to be effective, the transport you are using must support timers. see
     * the documentation for your transport policy for details about its
     * timer support.
     *
     * @param dur the length of the pong timeout in ms
     */
    void set_pong_timeout(long dur) {
        scoped_lock_type guard(m_mutex);
        m_pong_timeout_dur = dur;
    }

    /// get default maximum message size
    /**
     * get the default maximum message size that will be used for new connections created
     * by this endpoint. the maximum message size determines the point at which the
     * connection will fail a connection with the message_too_big protocol error.
     *
     * the default is set by the max_message_size value from the template config
     *
     * @since 0.3.0
     */
    size_t get_max_message_size() const {
        return m_max_message_size;
    }
    
    /// set default maximum message size
    /**
     * set the default maximum message size that will be used for new connections created
     * by this endpoint. maximum message size determines the point at which the connection
     * will fail a connection with the message_too_big protocol error.
     *
     * the default is set by the max_message_size value from the template config
     *
     * @since 0.3.0
     *
     * @param new_value the value to set as the maximum message size.
     */
    void set_max_message_size(size_t new_value) {
        m_max_message_size = new_value;
    }

    /*************************************/
    /* connection pass through functions */
    /*************************************/

    /**
     * these functions act as adaptors to their counterparts in connection. they
     * can produce one additional type of error, the bad_connection error, that
     * indicates that the conversion from connection_hdl to connection_ptr
     * failed due to the connection not existing anymore. each method has a
     * default and an exception free varient.
     */

    void interrupt(connection_hdl hdl, lib::error_code & ec);
    void interrupt(connection_hdl hdl);

    /// pause reading of new data (exception free)
    /**
     * signals to the connection to halt reading of new data. while reading is paused, 
     * the connection will stop reading from its associated socket. in turn this will 
     * result in tcp based flow control kicking in and slowing data flow from the remote
     * endpoint.
     *
     * this is useful for applications that push new requests to a queue to be processed
     * by another thread and need a way to signal when their request queue is full without
     * blocking the network processing thread.
     *
     * use `resume_reading()` to resume.
     *
     * if supported by the transport this is done asynchronously. as such reading may not
     * stop until the current read operation completes. typically you can expect to
     * receive no more bytes after initiating a read pause than the size of the read 
     * buffer.
     *
     * if reading is paused for this connection already nothing is changed.
     */
    void pause_reading(connection_hdl hdl, lib::error_code & ec);
    
    /// pause reading of new data
    void pause_reading(connection_hdl hdl);

    /// resume reading of new data (exception free)
    /**
     * signals to the connection to resume reading of new data after it was paused by
     * `pause_reading()`.
     *
     * if reading is not paused for this connection already nothing is changed.
     */
    void resume_reading(connection_hdl hdl, lib::error_code & ec);

    /// resume reading of new data
    void resume_reading(connection_hdl hdl);

    /// create a message and add it to the outgoing send queue (exception free)
    /**
     * convenience method to send a message given a payload string and an opcode
     *
     * @param [in] hdl the handle identifying the connection to send via.
     * @param [in] payload the payload string to generated the message with
     * @param [in] op the opcode to generated the message with.
     * @param [out] ec a code to fill in for errors
     */
    void send(connection_hdl hdl, std::string const & payload,
        frame::opcode::value op, lib::error_code & ec);
    /// create a message and add it to the outgoing send queue
    /**
     * convenience method to send a message given a payload string and an opcode
     *
     * @param [in] hdl the handle identifying the connection to send via.
     * @param [in] payload the payload string to generated the message with
     * @param [in] op the opcode to generated the message with.
     * @param [out] ec a code to fill in for errors
     */
    void send(connection_hdl hdl, std::string const & payload,
        frame::opcode::value op);

    void send(connection_hdl hdl, void const * payload, size_t len,
        frame::opcode::value op, lib::error_code & ec);
    void send(connection_hdl hdl, void const * payload, size_t len,
        frame::opcode::value op);

    void send(connection_hdl hdl, message_ptr msg, lib::error_code & ec);
    void send(connection_hdl hdl, message_ptr msg);

    void close(connection_hdl hdl, close::status::value const code,
        std::string const & reason, lib::error_code & ec);
    void close(connection_hdl hdl, close::status::value const code,
        std::string const & reason);

    /// send a ping to a specific connection
    /**
     * @since 0.3.0-alpha3
     *
     * @param [in] hdl the connection_hdl of the connection to send to.
     * @param [in] payload the payload string to send.
     * @param [out] ec a reference to an error code to fill in
     */
    void ping(connection_hdl hdl, std::string const & payload,
        lib::error_code & ec);
    /// send a ping to a specific connection
    /**
     * exception variant of `ping`
     *
     * @since 0.3.0-alpha3
     *
     * @param [in] hdl the connection_hdl of the connection to send to.
     * @param [in] payload the payload string to send.
     */
    void ping(connection_hdl hdl, std::string const & payload);

    /// send a pong to a specific connection
    /**
     * @since 0.3.0-alpha3
     *
     * @param [in] hdl the connection_hdl of the connection to send to.
     * @param [in] payload the payload string to send.
     * @param [out] ec a reference to an error code to fill in
     */
    void pong(connection_hdl hdl, std::string const & payload,
        lib::error_code & ec);
    /// send a pong to a specific connection
    /**
     * exception variant of `pong`
     *
     * @since 0.3.0-alpha3
     *
     * @param [in] hdl the connection_hdl of the connection to send to.
     * @param [in] payload the payload string to send.
     */
    void pong(connection_hdl hdl, std::string const & payload);

    /// retrieves a connection_ptr from a connection_hdl (exception free)
    /**
     * converting a weak pointer to shared_ptr is not thread safe because the
     * pointer could be deleted at any time.
     *
     * note: this method may be called by handler to upgrade its handle to a
     * full connection_ptr. that full connection may then be used safely for the
     * remainder of the handler body. get_con_from_hdl and the resulting
     * connection_ptr are not safe to use outside the handler loop.
     *
     * @param hdl the connection handle to translate
     *
     * @return the connection_ptr. may be null if the handle was invalid.
     */
    connection_ptr get_con_from_hdl(connection_hdl hdl, lib::error_code & ec) {
        scoped_lock_type lock(m_mutex);
        connection_ptr con = lib::static_pointer_cast<connection_type>(
            hdl.lock());
        if (!con) {
            ec = error::make_error_code(error::bad_connection);
        }
        return con;
    }

    /// retrieves a connection_ptr from a connection_hdl (exception version)
    connection_ptr get_con_from_hdl(connection_hdl hdl) {
        lib::error_code ec;
        connection_ptr con = this->get_con_from_hdl(hdl,ec);
        if (ec) {
            throw exception(ec);
        }
        return con;
    }
protected:
    connection_ptr create_connection();

    alog_type m_alog;
    elog_type m_elog;
private:
    // dynamic settings
    std::string                 m_user_agent;

    open_handler                m_open_handler;
    close_handler               m_close_handler;
    fail_handler                m_fail_handler;
    ping_handler                m_ping_handler;
    pong_handler                m_pong_handler;
    pong_timeout_handler        m_pong_timeout_handler;
    interrupt_handler           m_interrupt_handler;
    http_handler                m_http_handler;
    validate_handler            m_validate_handler;
    message_handler             m_message_handler;

    long                        m_open_handshake_timeout_dur;
    long                        m_close_handshake_timeout_dur;
    long                        m_pong_timeout_dur;
    size_t                      m_max_message_size;

    rng_type m_rng;

    // static settings
    bool const                  m_is_server;

    // endpoint state
    mutable mutex_type          m_mutex;
};

} // namespace websocketpp

#include <websocketpp/impl/endpoint_impl.hpp>

#endif // websocketpp_endpoint_hpp
