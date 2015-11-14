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

#ifndef websocketpp_connection_hpp
#define websocketpp_connection_hpp

#include <websocketpp/close.hpp>
#include <websocketpp/common/connection_hdl.hpp>
#include <websocketpp/common/cpp11.hpp>
#include <websocketpp/common/functional.hpp>
#include <websocketpp/error.hpp>
#include <websocketpp/frame.hpp>
#include <websocketpp/http/constants.hpp>
#include <websocketpp/logger/levels.hpp>
#include <websocketpp/processors/processor.hpp>
#include <websocketpp/transport/base/connection.hpp>

#include <algorithm>
#include <iostream>
#include <queue>
#include <sstream>
#include <string>
#include <vector>

namespace websocketpp {

/// the type and function signature of an open handler
/**
 * the open handler is called once for every successful websocket connection
 * attempt. either the fail handler or the open handler will be called for each
 * websocket connection attempt. http connections that did not attempt to
 * upgrade the connection to the websocket protocol will trigger the http
 * handler instead of fail/open.
 */
typedef lib::function<void(connection_hdl)> open_handler;

/// the type and function signature of a close handler
/**
 * the close handler is called once for every successfully established
 * connection after it is no longer capable of sending or receiving new messages
 *
 * the close handler will be called exactly once for every connection for which
 * the open handler was called.
 */
typedef lib::function<void(connection_hdl)> close_handler;

/// the type and function signature of a fail handler
/**
 * the fail handler is called once for every unsuccessful websocket connection
 * attempt. either the fail handler or the open handler will be called for each
 * websocket connection attempt. http connections that did not attempt to
 * upgrade the connection to the websocket protocol will trigger the http
 * handler instead of fail/open.
 */
typedef lib::function<void(connection_hdl)> fail_handler;

/// the type and function signature of an interrupt handler
/**
 * the interrupt handler is called when a connection receives an interrupt
 * request from the application. interrupts allow the application to trigger a
 * handler to be run in the absense of a websocket level handler trigger (like
 * a new message).
 *
 * this is typically used by another application thread to schedule some tasks
 * that can only be run from within the handler chain for thread safety reasons.
 */
typedef lib::function<void(connection_hdl)> interrupt_handler;

/// the type and function signature of a ping handler
/**
 * the ping handler is called when the connection receives a websocket ping
 * control frame. the string argument contains the ping payload. the payload is
 * a binary string up to 126 bytes in length. the ping handler returns a bool,
 * true if a pong response should be sent, false if the pong response should be
 * suppressed.
 */
typedef lib::function<bool(connection_hdl,std::string)> ping_handler;

/// the type and function signature of a pong handler
/**
 * the pong handler is called when the connection receives a websocket pong
 * control frame. the string argument contains the pong payload. the payload is
 * a binary string up to 126 bytes in length.
 */
typedef lib::function<void(connection_hdl,std::string)> pong_handler;

/// the type and function signature of a pong timeout handler
/**
 * the pong timeout handler is called when a ping goes unanswered by a pong for
 * longer than the locally specified timeout period.
 */
typedef lib::function<void(connection_hdl,std::string)> pong_timeout_handler;

/// the type and function signature of a validate handler
/**
 * the validate handler is called after a websocket handshake has been received
 * and processed but before it has been accepted. this gives the application a
 * chance to impliment connection details specific policies for accepting
 * connections and the ability to negotiate extensions and subprotocols.
 *
 * the validate handler return value indicates whether or not the connection
 * should be accepted. additional methods may be called during the function to
 * set response headers, set http return/error codes, etc.
 */
typedef lib::function<bool(connection_hdl)> validate_handler;

/// the type and function signature of a http handler
/**
 * the http handler is called when an http connection is made that does not
 * attempt to upgrade the connection to the websocket protocol. this allows
 * websocket++ servers to respond to these requests with regular http responses.
 *
 * this can be used to deliver error pages & dashboards and to deliver static
 * files such as the base html & javascript for an otherwise single page
 * websocket application.
 *
 * note: websocket++ is designed to be a high performance websocket server. it
 * is not tuned to provide a full featured, high performance, http web server
 * solution. the http handler is appropriate only for low volume http traffic.
 * if you expect to serve high volumes of http traffic a dedicated http web
 * server is strongly recommended.
 *
 * the default http handler will return a 426 upgrade required error. custom
 * handlers may override the response status code to deliver any type of
 * response.
 */
typedef lib::function<void(connection_hdl)> http_handler;

//
typedef lib::function<void(lib::error_code const & ec, size_t bytes_transferred)> read_handler;
typedef lib::function<void(lib::error_code const & ec)> write_frame_handler;

// constants related to the default websocket protocol versions available
#ifdef _websocketpp_initializer_lists_ // simplified c++11 version
    /// container that stores the list of protocol versions supported
    /**
     * @todo move this to configs to allow compile/runtime disabling or enabling
     * of protocol versions
     */
    static std::vector<int> const versions_supported = {0,7,8,13};
#else
    /// helper array to get around lack of initializer lists pre c++11
    static int const helper[] = {0,7,8,13};
    /// container that stores the list of protocol versions supported
    /**
     * @todo move this to configs to allow compile/runtime disabling or enabling
     * of protocol versions
     */
    static std::vector<int> const versions_supported(helper,helper+4);
#endif

namespace session {
namespace state {
    // externally visible session state (states based on the rfc)
    enum value {
        connecting = 0,
        open = 1,
        closing = 2,
        closed = 3
    };
} // namespace state


namespace fail {
namespace status {
    enum value {
        good = 0,           // no failure yet!
        system = 1,         // system call returned error, check that code
        websocket = 2,      // websocket close codes contain error
        unknown = 3,        // no failure information is available
        timeout_tls = 4,    // tls handshake timed out
        timeout_ws = 5      // ws handshake timed out
    };
} // namespace status
} // namespace fail

namespace internal_state {
    // more granular internal states. these are used for multi-threaded
    // connection synchronization and preventing values that are not yet or no
    // longer available from being used.

    enum value {
        user_init = 0,
        transport_init = 1,
        read_http_request = 2,
        write_http_request = 3,
        read_http_response = 4,
        write_http_response = 5,
        process_http_request = 6,
        process_connection = 7
    };
} // namespace internal_state
} // namespace session

/// represents an individual websocket connection
template <typename config>
class connection
 : public config::transport_type::transport_con_type
 , public config::connection_base
{
public:
    /// type of this connection
    typedef connection<config> type;
    /// type of a shared pointer to this connection
    typedef lib::shared_ptr<type> ptr;
    /// type of a weak pointer to this connection
    typedef lib::weak_ptr<type> weak_ptr;

    /// type of the concurrency component of this connection
    typedef typename config::concurrency_type concurrency_type;
    /// type of the access logging policy
    typedef typename config::alog_type alog_type;
    /// type of the error logging policy
    typedef typename config::elog_type elog_type;

    /// type of the transport component of this connection
    typedef typename config::transport_type::transport_con_type
        transport_con_type;
    /// type of a shared pointer to the transport component of this connection
    typedef typename transport_con_type::ptr transport_con_ptr;

    typedef lib::function<void(ptr)> termination_handler;

    typedef typename concurrency_type::scoped_lock_type scoped_lock_type;
    typedef typename concurrency_type::mutex_type mutex_type;

    typedef typename config::request_type request_type;
    typedef typename config::response_type response_type;

    typedef typename config::message_type message_type;
    typedef typename message_type::ptr message_ptr;

    typedef typename config::con_msg_manager_type con_msg_manager_type;
    typedef typename con_msg_manager_type::ptr con_msg_manager_ptr;

    /// type of rng
    typedef typename config::rng_type rng_type;

    typedef processor::processor<config> processor_type;
    typedef lib::shared_ptr<processor_type> processor_ptr;

    // message handler (needs to know message type)
    typedef lib::function<void(connection_hdl,message_ptr)> message_handler;

    /// type of a pointer to a transport timer handle
    typedef typename transport_con_type::timer_ptr timer_ptr;

    // misc convenience types
    typedef session::internal_state::value istate_type;

private:
    enum terminate_status {
        failed = 1,
        closed,
        unknown
    };
public:

    explicit connection(bool p_is_server, std::string const & ua, alog_type& alog,
        elog_type& elog, rng_type & rng)
      : transport_con_type(p_is_server, alog, elog)
      , m_handle_read_frame(lib::bind(
            &type::handle_read_frame,
            this,
            lib::placeholders::_1,
            lib::placeholders::_2
        ))
      , m_write_frame_handler(lib::bind(
            &type::handle_write_frame,
            this,
            lib::placeholders::_1
        ))
      , m_user_agent(ua)
      , m_open_handshake_timeout_dur(config::timeout_open_handshake)
      , m_close_handshake_timeout_dur(config::timeout_close_handshake)
      , m_pong_timeout_dur(config::timeout_pong)
      , m_max_message_size(config::max_message_size)
      , m_state(session::state::connecting)
      , m_internal_state(session::internal_state::user_init)
      , m_msg_manager(new con_msg_manager_type())
      , m_send_buffer_size(0)
      , m_write_flag(false)
      , m_read_flag(true)
      , m_is_server(p_is_server)
      , m_alog(alog)
      , m_elog(elog)
      , m_rng(rng)
      , m_local_close_code(close::status::abnormal_close)
      , m_remote_close_code(close::status::abnormal_close)
      , m_was_clean(false)
    {
        m_alog.write(log::alevel::devel,"connection constructor");
    }

    /// get a shared pointer to this component
    ptr get_shared() {
        return lib::static_pointer_cast<type>(transport_con_type::get_shared());
    }

    ///////////////////////////
    // set handler callbacks //
    ///////////////////////////

    /// set open handler
    /**
     * the open handler is called after the websocket handshake is complete and
     * the connection is considered open.
     *
     * @param h the new open_handler
     */
    void set_open_handler(open_handler h) {
        m_open_handler = h;
    }

    /// set close handler
    /**
     * the close handler is called immediately after the connection is closed.
     *
     * @param h the new close_handler
     */
    void set_close_handler(close_handler h) {
        m_close_handler = h;
    }

    /// set fail handler
    /**
     * the fail handler is called whenever the connection fails while the
     * handshake is bring processed.
     *
     * @param h the new fail_handler
     */
    void set_fail_handler(fail_handler h) {
        m_fail_handler = h;
    }

    /// set ping handler
    /**
     * the ping handler is called whenever the connection receives a ping
     * control frame. the ping payload is included.
     *
     * the ping handler's return time controls whether or not a pong is
     * sent in response to this ping. returning false will suppress the
     * return pong. if no ping handler is set a pong will be sent.
     *
     * @param h the new ping_handler
     */
    void set_ping_handler(ping_handler h) {
        m_ping_handler = h;
    }

    /// set pong handler
    /**
     * the pong handler is called whenever the connection receives a pong
     * control frame. the pong payload is included.
     *
     * @param h the new pong_handler
     */
    void set_pong_handler(pong_handler h) {
        m_pong_handler = h;
    }

    /// set pong timeout handler
    /**
     * if the transport component being used supports timers, the pong timeout
     * handler is called whenever a pong control frame is not received with the
     * configured timeout period after the application sends a ping.
     *
     * the config setting `timeout_pong` controls the length of the timeout
     * period. it is specified in milliseconds.
     *
     * this can be used to probe the health of the remote endpoint's websocket
     * implementation. this does not guarantee that the remote application
     * itself is still healthy but can be a useful diagnostic.
     *
     * note: receipt of this callback doesn't mean the pong will never come.
     * this functionality will not suppress delivery of the pong in question
     * should it arrive after the timeout.
     *
     * @param h the new pong_timeout_handler
     */
    void set_pong_timeout_handler(pong_timeout_handler h) {
        m_pong_timeout_handler = h;
    }

    /// set interrupt handler
    /**
     * the interrupt handler is called whenever the connection is manually
     * interrupted by the application.
     *
     * @param h the new interrupt_handler
     */
    void set_interrupt_handler(interrupt_handler h) {
        m_interrupt_handler = h;
    }

    /// set http handler
    /**
     * the http handler is called after an http request other than a websocket
     * upgrade request is received. it allows a websocket++ server to respond
     * to regular http requests on the same port as it processes websocket
     * connections. this can be useful for hosting error messages, flash
     * policy files, status pages, and other simple http responses. it is not
     * intended to be used as a primary web server.
     *
     * @param h the new http_handler
     */
    void set_http_handler(http_handler h) {
        m_http_handler = h;
    }

    /// set validate handler
    /**
     * the validate handler is called after a websocket handshake has been
     * parsed but before a response is returned. it provides the application
     * a chance to examine the request and determine whether or not it wants
     * to accept the connection.
     *
     * returning false from the validate handler will reject the connection.
     * if no validate handler is present, all connections will be allowed.
     *
     * @param h the new validate_handler
     */
    void set_validate_handler(validate_handler h) {
        m_validate_handler = h;
    }

    /// set message handler
    /**
     * the message handler is called after a new message has been received.
     *
     * @param h the new message_handler
     */
    void set_message_handler(message_handler h) {
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
        m_pong_timeout_dur = dur;
    }

    /// get maximum message size
    /**
     * get maximum message size. maximum message size determines the point at which the
     * connection will fail a connection with the message_too_big protocol error.
     *
     * the default is set by the endpoint that creates the connection.
     *
     * @since 0.3.0
     */
    size_t get_max_message_size() const {
        return m_max_message_size;
    }
    
    /// set maximum message size
    /**
     * set maximum message size. maximum message size determines the point at which the
     * connection will fail a connection with the message_too_big protocol error. this
     * value may be changed during the connection.
     *
     * the default is set by the endpoint that creates the connection.
     *
     * @since 0.3.0
     *
     * @param new_value the value to set as the maximum message size.
     */
    void set_max_message_size(size_t new_value) {
        m_max_message_size = new_value;
        if (m_processor) {
            m_processor->set_max_message_size(new_value);
        }
    }

    //////////////////////////////////
    // uncategorized public methods //
    //////////////////////////////////

    /// get the size of the outgoing write buffer (in payload bytes)
    /**
     * retrieves the number of bytes in the outgoing write buffer that have not
     * already been dispatched to the transport layer. this represents the bytes
     * that are presently cancelable without uncleanly ending the websocket
     * connection
     *
     * this method invokes the m_write_lock mutex
     *
     * @return the current number of bytes in the outgoing send buffer.
     */
    size_t get_buffered_amount() const;

    /// deprecated: use get_buffered_amount instead
    size_t buffered_amount() const {
        return get_buffered_amount();
    }

    ////////////////////
    // action methods //
    ////////////////////

    /// create a message and then add it to the outgoing send queue
    /**
     * convenience method to send a message given a payload string and
     * optionally an opcode. default opcode is utf8 text.
     *
     * this method locks the m_write_lock mutex
     *
     * @param payload the payload string to generated the message with
     *
     * @param op the opcode to generated the message with. default is
     * frame::opcode::text
     */
    lib::error_code send(std::string const & payload, frame::opcode::value op =
        frame::opcode::text);

    /// send a message (raw array overload)
    /**
     * convenience method to send a message given a raw array and optionally an
     * opcode. default opcode is binary.
     *
     * this method locks the m_write_lock mutex
     *
     * @param payload a pointer to the array containing the bytes to send.
     *
     * @param len length of the array.
     *
     * @param op the opcode to generated the message with. default is
     * frame::opcode::binary
     */
    lib::error_code send(void const * payload, size_t len, frame::opcode::value
        op = frame::opcode::binary);

    /// add a message to the outgoing send queue
    /**
     * if presented with a prepared message it is added without validation or
     * framing. if presented with an unprepared message it is validated, framed,
     * and then added
     *
     * errors are returned via an exception
     * \todo make exception system_error rather than error_code
     *
     * this method invokes the m_write_lock mutex
     *
     * @param msg a message_ptr to the message to send.
     */
    lib::error_code send(message_ptr msg);

    /// asyncronously invoke handler::on_inturrupt
    /**
     * signals to the connection to asyncronously invoke the on_inturrupt
     * callback for this connection's handler once it is safe to do so.
     *
     * when the on_inturrupt handler callback is called it will be from
     * within the transport event loop with all the thread safety features
     * guaranteed by the transport to regular handlers
     *
     * multiple inturrupt signals can be active at once on the same connection
     *
     * @return an error code
     */
    lib::error_code interrupt();
    
    /// transport inturrupt callback
    void handle_interrupt();
    
    /// pause reading of new data
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
    lib::error_code pause_reading();

    /// pause reading callback
    void handle_pause_reading();

    /// resume reading of new data
    /**
     * signals to the connection to resume reading of new data after it was paused by
     * `pause_reading()`.
     *
     * if reading is not paused for this connection already nothing is changed.
     */
    lib::error_code resume_reading();

    /// resume reading callback
    void handle_resume_reading();

    /// send a ping
    /**
     * initiates a ping with the given payload/
     *
     * there is no feedback directly from ping except in cases of immediately
     * detectable errors. feedback will be provided via on_pong or
     * on_pong_timeout callbacks.
     *
     * ping locks the m_write_lock mutex
     *
     * @param payload payload to be used for the ping
     */
    void ping(std::string const & payload);

    /// exception free variant of ping
    void ping(std::string const & payload, lib::error_code & ec);

    /// utility method that gets called back when the ping timer expires
    void handle_pong_timeout(std::string payload, lib::error_code const & ec);

    /// send a pong
    /**
     * initiates a pong with the given payload.
     *
     * there is no feedback from a pong once sent.
     *
     * pong locks the m_write_lock mutex
     *
     * @param payload payload to be used for the pong
     */
    void pong(std::string const & payload);

    /// exception free variant of pong
    void pong(std::string const & payload, lib::error_code & ec);

    /// close the connection
    /**
     * initiates the close handshake process.
     *
     * if close returns successfully the connection will be in the closing
     * state and no additional messages may be sent. all messages sent prior
     * to calling close will be written out before the connection is closed.
     *
     * if no reason is specified none will be sent. if no code is specified
     * then no code will be sent.
     *
     * the handler's on_close callback will be called once the close handshake
     * is complete.
     *
     * reasons will be automatically truncated to the maximum length (123 bytes)
     * if necessary.
     *
     * @param code the close code to send
     * @param reason the close reason to send
     */
    void close(close::status::value const code, std::string const & reason);

    /// exception free variant of close
    void close(close::status::value const code, std::string const & reason,
        lib::error_code & ec);

    ////////////////////////////////////////////////
    // pass-through access to the uri information //
    ////////////////////////////////////////////////

    /// returns the secure flag from the connection uri
    /**
     * this value is available after the http request has been fully read and
     * may be called from any thread.
     *
     * @return whether or not the connection uri is flagged secure.
     */
    bool get_secure() const;

    /// returns the host component of the connection uri
    /**
     * this value is available after the http request has been fully read and
     * may be called from any thread.
     *
     * @return the host component of the connection uri
     */
    std::string const & get_host() const;

    /// returns the resource component of the connection uri
    /**
     * this value is available after the http request has been fully read and
     * may be called from any thread.
     *
     * @return the resource component of the connection uri
     */
    std::string const & get_resource() const;

    /// returns the port component of the connection uri
    /**
     * this value is available after the http request has been fully read and
     * may be called from any thread.
     *
     * @return the port component of the connection uri
     */
    uint16_t get_port() const;

    /// gets the connection uri
    /**
     * this should really only be called by internal library methods unless you
     * really know what you are doing.
     *
     * @return a pointer to the connection's uri
     */
    uri_ptr get_uri() const;

    /// sets the connection uri
    /**
     * this should really only be called by internal library methods unless you
     * really know what you are doing.
     *
     * @param uri the new uri to set
     */
    void set_uri(uri_ptr uri);

    /////////////////////////////
    // subprotocol negotiation //
    /////////////////////////////

    /// gets the negotated subprotocol
    /**
     * retrieves the subprotocol that was negotiated during the handshake. this
     * method is valid in the open handler and later.
     *
     * @return the negotiated subprotocol
     */
    std::string const & get_subprotocol() const;

    /// gets all of the subprotocols requested by the client
    /**
     * retrieves the subprotocols that were requested during the handshake. this
     * method is valid in the validate handler and later.
     *
     * @return a vector of the requested subprotocol
     */
    std::vector<std::string> const & get_requested_subprotocols() const;

    /// adds the given subprotocol string to the request list (exception free)
    /**
     * adds a subprotocol to the list to send with the opening handshake. this
     * may be called multiple times to request more than one. if the server
     * supports one of these, it may choose one. if so, it will return it
     * in it's handshake reponse and the value will be available via
     * get_subprotocol(). subprotocol requests should be added in order of
     * preference.
     *
     * @param request the subprotocol to request
     * @param ec a reference to an error code that will be filled in the case of
     * errors
     */
    void add_subprotocol(std::string const & request, lib::error_code & ec);

    /// adds the given subprotocol string to the request list
    /**
     * adds a subprotocol to the list to send with the opening handshake. this
     * may be called multiple times to request more than one. if the server
     * supports one of these, it may choose one. if so, it will return it
     * in it's handshake reponse and the value will be available via
     * get_subprotocol(). subprotocol requests should be added in order of
     * preference.
     *
     * @param request the subprotocol to request
     */
    void add_subprotocol(std::string const & request);

    /// select a subprotocol to use (exception free)
    /**
     * indicates which subprotocol should be used for this connection. valid
     * only during the validate handler callback. subprotocol selected must have
     * been requested by the client. consult get_requested_subprotocols() for a
     * list of valid subprotocols.
     *
     * this member function is valid on server endpoints/connections only
     *
     * @param value the subprotocol to select
     * @param ec a reference to an error code that will be filled in the case of
     * errors
     */
    void select_subprotocol(std::string const & value, lib::error_code & ec);

    /// select a subprotocol to use
    /**
     * indicates which subprotocol should be used for this connection. valid
     * only during the validate handler callback. subprotocol selected must have
     * been requested by the client. consult get_requested_subprotocols() for a
     * list of valid subprotocols.
     *
     * this member function is valid on server endpoints/connections only
     *
     * @param value the subprotocol to select
     */
    void select_subprotocol(std::string const & value);

    /////////////////////////////////////////////////////////////
    // pass-through access to the request and response objects //
    /////////////////////////////////////////////////////////////

    /// retrieve a request header
    /**
     * retrieve the value of a header from the handshake http request.
     *
     * @param key name of the header to get
     * @return the value of the header
     */
    std::string const & get_request_header(std::string const & key);

    /// retrieve a response header
    /**
     * retrieve the value of a header from the handshake http request.
     *
     * @param key name of the header to get
     * @return the value of the header
     */
    std::string const & get_response_header(std::string const & key);

    /// set response status code and message
    /**
     * sets the response status code to `code` and looks up the corresponding
     * message for standard codes. non-standard codes will be entered as unknown
     * use set_status(status_code::value,std::string) overload to set both
     * values explicitly.
     *
     * this member function is valid only from the http() and validate() handler
     * callbacks.
     *
     * @param code code to set
     * @param msg message to set
     * @see websocketpp::http::response::set_status
     */
    void set_status(http::status_code::value code);

    /// set response status code and message
    /**
     * sets the response status code and message to independent custom values.
     * use set_status(status_code::value) to set the code and have the standard
     * message be automatically set.
     *
     * this member function is valid only from the http() and validate() handler
     * callbacks.
     *
     * @param code code to set
     * @param msg message to set
     * @see websocketpp::http::response::set_status
     */
    void set_status(http::status_code::value code, std::string const & msg);

    /// set response body content
    /**
     * set the body content of the http response to the parameter string. note
     * set_body will also set the content-length http header to the appropriate
     * value. if you want the content-length header to be something else set it
     * to something else after calling set_body
     *
     * this member function is valid only from the http() and validate() handler
     * callbacks.
     *
     * @param value string data to include as the body content.
     * @see websocketpp::http::response::set_body
     */
    void set_body(std::string const & value);

    /// append a header
    /**
     * if a header with this name already exists the value will be appended to
     * the existing header to form a comma separated list of values. use
     * `connection::replace_header` to overwrite existing values.
     *
     * this member function is valid only from the http() and validate() handler
     * callbacks, or to a client connection before connect has been called.
     *
     * @param key name of the header to set
     * @param val value to add
     * @see replace_header
     * @see websocketpp::http::parser::append_header
     */
    void append_header(std::string const & key, std::string const & val);

    /// replace a header
    /**
     * if a header with this name already exists the old value will be replaced
     * use `connection::append_header` to append to a list of existing values.
     *
     * this member function is valid only from the http() and validate() handler
     * callbacks, or to a client connection before connect has been called.
     *
     * @param key name of the header to set
     * @param val value to set
     * @see append_header
     * @see websocketpp::http::parser::replace_header
     */
    void replace_header(std::string const & key, std::string const & val);

    /// remove a header
    /**
     * removes a header from the response.
     *
     * this member function is valid only from the http() and validate() handler
     * callbacks, or to a client connection before connect has been called.
     *
     * @param key the name of the header to remove
     * @see websocketpp::http::parser::remove_header
     */
    void remove_header(std::string const & key);

    /// get request object
    /**
     * direct access to request object. this can be used to call methods of the
     * request object that are not part of the standard request api that
     * connection wraps.
     *
     * note use of this method involves using behavior specific to the
     * configured http policy. such behavior may not work with alternate http
     * policies.
     *
     * @since 0.3.0-alpha3
     *
     * @return a const reference to the raw request object
     */
    request_type const & get_request() const {
        return m_request;
    }

    /////////////////////////////////////////////////////////////
    // pass-through access to the other connection information //
    /////////////////////////////////////////////////////////////

    /// get connection handle
    /**
     * the connection handle is a token that can be shared outside the
     * websocket++ core for the purposes of identifying a connection and
     * sending it messages.
     *
     * @return a handle to the connection
     */
    connection_hdl get_handle() const {
        return m_connection_hdl;
    }

    /// get whether or not this connection is part of a server or client
    /**
     * @return whether or not the connection is attached to a server endpoint
     */
    bool is_server() const {
        return m_is_server;
    }

    /// return the same origin policy origin value from the opening request.
    /**
     * this value is available after the http request has been fully read and
     * may be called from any thread.
     *
     * @return the connection's origin value from the opening handshake.
     */
    std::string const & get_origin() const;

    /// return the connection state.
    /**
     * values can be connecting, open, closing, and closed
     *
     * @return the connection's current state.
     */
    session::state::value get_state() const;


    /// get the websocket close code sent by this endpoint.
    /**
     * @return the websocket close code sent by this endpoint.
     */
    close::status::value get_local_close_code() const {
        return m_local_close_code;
    }

    /// get the websocket close reason sent by this endpoint.
    /**
     * @return the websocket close reason sent by this endpoint.
     */
    std::string const & get_local_close_reason() const {
        return m_local_close_reason;
    }

    /// get the websocket close code sent by the remote endpoint.
    /**
     * @return the websocket close code sent by the remote endpoint.
     */
    close::status::value get_remote_close_code() const {
        return m_remote_close_code;
    }

    /// get the websocket close reason sent by the remote endpoint.
    /**
     * @return the websocket close reason sent by the remote endpoint.
     */
    std::string const & get_remote_close_reason() const {
        return m_remote_close_reason;
    }

    /// get the internal error code for a closed/failed connection
    /**
     * retrieves a machine readable detailed error code indicating the reason
     * that the connection was closed or failed. valid only after the close or
     * fail handler is called.
     *
     * @return error code indicating the reason the connection was closed or
     * failed
     */
    lib::error_code get_ec() const {
        return m_ec;
    }

    ////////////////////////////////////////////////////////////////////////
    // the remaining public member functions are for internal/policy use  //
    // only. do not call from application code unless you understand what //
    // you are doing.                                                     //
    ////////////////////////////////////////////////////////////////////////

    /// set connection handle
    /**
     * the connection handle is a token that can be shared outside the
     * websocket++ core for the purposes of identifying a connection and
     * sending it messages.
     *
     * @param hdl a connection_hdl that the connection will use to refer
     * to itself.
     */
    void set_handle(connection_hdl hdl) {
        m_connection_hdl = hdl;
        transport_con_type::set_handle(hdl);
    }

    /// get a message buffer
    /**
     * warning: the api related to directly sending message buffers may change
     * before the 1.0 release. if you plan to use it, please keep an eye on any
     * breaking changes notifications in future release notes. also if you have
     * any feedback about usage and capabilities now is a great time to provide
     * it.
     *
     * message buffers are used to store message payloads and other message
     * metadata.
     *
     * the size parameter is a hint only. your final payload does not need to
     * match it. there may be some performance benefits if the initial size
     * guess is equal to or slightly higher than the final payload size.
     *
     * @param op the opcode for the new message
     * @param size a hint to optimize the initial allocation of payload space.
     * @return a new message buffer
     */
    message_ptr get_message(websocketpp::frame::opcode::value op, size_t size)
        const
    {
        return m_msg_manager->get_message(op, size);
    }

    void start();

    void read_handshake(size_t num_bytes);

    void handle_read_handshake(lib::error_code const & ec,
        size_t bytes_transferred);
    void handle_read_http_response(lib::error_code const & ec,
        size_t bytes_transferred);

    void handle_send_http_response(lib::error_code const & ec);
    void handle_send_http_request(lib::error_code const & ec);

    void handle_open_handshake_timeout(lib::error_code const & ec);
    void handle_close_handshake_timeout(lib::error_code const & ec);

    void handle_read_frame(lib::error_code const & ec, size_t bytes_transferred);
    void read_frame();

    /// get array of websocket protocol versions that this connection supports.
    const std::vector<int>& get_supported_versions() const;

    /// sets the handler for a terminating connection. should only be used
    /// internally by the endpoint class.
    void set_termination_handler(termination_handler new_handler);

    void terminate(lib::error_code const & ec);
    void handle_terminate(terminate_status tstat, lib::error_code const & ec);

    /// checks if there are frames in the send queue and if there are sends one
    /**
     * \todo unit tests
     *
     * this method locks the m_write_lock mutex
     */
    void write_frame();

    /// process the results of a frame write operation and start the next write
    /**
     * \todo unit tests
     *
     * this method locks the m_write_lock mutex
     *
     * @param terminate whether or not to terminate the connection upon
     * completion of this write.
     *
     * @param ec a status code from the transport layer, zero on success,
     * non-zero otherwise.
     */
    void handle_write_frame(lib::error_code const & ec);
protected:
    void handle_transport_init(lib::error_code const & ec);

    /// set m_processor based on information in m_request. set m_response
    /// status and return false on error.
    bool initialize_processor();

    /// perform websocket handshake validation of m_request using m_processor.
    /// set m_response and return false on error.
    bool process_handshake_request();

    /// atomically change the internal connection state.
    /**
     * @param req the required starting state. if the internal state does not
     * match req an exception is thrown.
     *
     * @param dest the state to change to.
     *
     * @param msg the message to include in the exception thrown
     */
    void atomic_state_change(istate_type req, istate_type dest,
        std::string msg);

    /// atomically change the internal and external connection state.
    /**
     * @param ireq the required starting internal state. if the internal state
     * does not match ireq an exception is thrown.
     *
     * @param idest the internal state to change to.
     *
     * @param ereq the required starting external state. if the external state
     * does not match ereq an exception is thrown.
     *
     * @param edest the external state to change to.
     *
     * @param msg the message to include in the exception thrown
     */
    void atomic_state_change(istate_type ireq, istate_type idest,
        session::state::value ereq, session::state::value edest,
        std::string msg);

    /// atomically read and compared the internal state.
    /**
     * @param req the state to test against. if the internal state does not
     * match req an exception is thrown.
     *
     * @param msg the message to include in the exception thrown
     */
    void atomic_state_check(istate_type req, std::string msg);
private:
    /// completes m_response, serializes it, and sends it out on the wire.
    void send_http_response();

    /// sends an opening websocket connect request
    void send_http_request();

    /// alternate path for send_http_response in error conditions
    void send_http_response_error();

    /// process control message
    /**
     *
     */
    void process_control_frame(message_ptr msg);

    /// send close acknowledgement
    /**
     * if no arguments are present no close code/reason will be specified.
     *
     * note: the close code/reason values provided here may be overrided by
     * other settings (such as silent close).
     *
     * @param code the close code to send
     * @param reason the close reason to send
     * @return a status code, zero on success, non-zero otherwise
     */
    lib::error_code send_close_ack(close::status::value code =
        close::status::blank, std::string const & reason = "");

    /// send close frame
    /**
     * if no arguments are present no close code/reason will be specified.
     *
     * note: the close code/reason values provided here may be overrided by
     * other settings (such as silent close).
     *
     * the ack flag determines what to do in the case of a blank status and
     * whether or not to terminate the tcp connection after sending it.
     *
     * @param code the close code to send
     * @param reason the close reason to send
     * @param ack whether or not this is an acknowledgement close frame
     * @return a status code, zero on success, non-zero otherwise
     */
    lib::error_code send_close_frame(close::status::value code =
        close::status::blank, std::string const & reason = "", bool ack = false,
        bool terminal = false);

    /// get a pointer to a new websocket protocol processor for a given version
    /**
     * @param version version number of the websocket protocol to get a
     * processor for. negative values indicate invalid/unknown versions and will
     * always return a null ptr
     *
     * @return a shared_ptr to a new instance of the appropriate processor or a
     * null ptr if there is no installed processor that matches the version
     * number.
     */
    processor_ptr get_processor(int version) const;

    /// add a message to the write queue
    /**
     * adds a message to the write queue and updates any associated shared state
     *
     * must be called while holding m_write_lock
     *
     * @todo unit tests
     *
     * @param msg the message to push
     */
    void write_push(message_ptr msg);

    /// pop a message from the write queue
    /**
     * removes and returns a message from the write queue and updates any
     * associated shared state.
     *
     * must be called while holding m_write_lock
     *
     * @todo unit tests
     *
     * @return the message_ptr at the front of the queue
     */
    message_ptr write_pop();

    /// prints information about the incoming connection to the access log
    /**
     * prints information about the incoming connection to the access log.
     * includes: connection type, websocket version, remote endpoint, user agent
     * path, status code.
     */
    void log_open_result();

    /// prints information about a connection being closed to the access log
    /**
     * includes: local and remote close codes and reasons
     */
    void log_close_result();

    /// prints information about a connection being failed to the access log
    /**
     * includes: error code and message for why it was failed
     */
    void log_fail_result();

    /// prints information about an arbitrary error code on the specified channel
    template <typename error_type>
    void log_err(log::level l, char const * msg, error_type const & ec) {
        std::stringstream s;
        s << msg << " error: " << ec << " (" << ec.message() << ")";
        m_elog.write(l, s.str());
    }

    // internal handler functions
    read_handler            m_handle_read_frame;
    write_frame_handler     m_write_frame_handler;

    // static settings
    std::string const       m_user_agent;

    /// pointer to the connection handle
    connection_hdl          m_connection_hdl;

    /// handler objects
    open_handler            m_open_handler;
    close_handler           m_close_handler;
    fail_handler            m_fail_handler;
    ping_handler            m_ping_handler;
    pong_handler            m_pong_handler;
    pong_timeout_handler    m_pong_timeout_handler;
    interrupt_handler       m_interrupt_handler;
    http_handler            m_http_handler;
    validate_handler        m_validate_handler;
    message_handler         m_message_handler;

    /// constant values
    long                    m_open_handshake_timeout_dur;
    long                    m_close_handshake_timeout_dur;
    long                    m_pong_timeout_dur;
    size_t                  m_max_message_size;

    /// external connection state
    /**
     * lock: m_connection_state_lock
     */
    session::state::value   m_state;

    /// internal connection state
    /**
     * lock: m_connection_state_lock
     */
    istate_type             m_internal_state;

    mutable mutex_type      m_connection_state_lock;

    /// the lock used to protect the message queue
    /**
     * serializes access to the write queue as well as shared state within the
     * processor.
     */
    mutex_type              m_write_lock;

    // connection resources
    char                    m_buf[config::connection_read_buffer_size];
    size_t                  m_buf_cursor;
    termination_handler     m_termination_handler;
    con_msg_manager_ptr     m_msg_manager;
    timer_ptr               m_handshake_timer;
    timer_ptr               m_ping_timer;

    /// @todo this is not memory efficient. this value is not used after the
    /// handshake.
    std::string m_handshake_buffer;

    /// pointer to the processor object for this connection
    /**
     * the processor provides functionality that is specific to the websocket
     * protocol version that the client has negotiated. it also contains all of
     * the state necessary to encode and decode the incoming and outgoing
     * websocket byte streams
     *
     * use of the prepare_data_frame method requires lock: m_write_lock
     */
    processor_ptr           m_processor;

    /// queue of unsent outgoing messages
    /**
     * lock: m_write_lock
     */
    std::queue<message_ptr> m_send_queue;

    /// size in bytes of the outstanding payloads in the write queue
    /**
     * lock: m_write_lock
     */
    size_t m_send_buffer_size;

    /// buffer holding the various parts of the current message being writen
    /**
     * lock m_write_lock
     */
    std::vector<transport::buffer> m_send_buffer;

    /// a list of pointers to hold on to the messages being written to keep them
    /// from going out of scope before the write is complete.
    std::vector<message_ptr> m_current_msgs;

    /// true if there is currently an outstanding transport write
    /**
     * lock m_write_lock
     */
    bool m_write_flag;

    /// true if this connection is presently reading new data
    bool m_read_flag;

    // connection data
    request_type            m_request;
    response_type           m_response;
    uri_ptr                 m_uri;
    std::string             m_subprotocol;

    // connection data that might not be necessary to keep around for the life
    // of the whole connection.
    std::vector<std::string> m_requested_subprotocols;

    bool const              m_is_server;
    alog_type& m_alog;
    elog_type& m_elog;

    rng_type & m_rng;

    // close state
    /// close code that was sent on the wire by this endpoint
    close::status::value    m_local_close_code;

    /// close reason that was sent on the wire by this endpoint
    std::string             m_local_close_reason;

    /// close code that was received on the wire from the remote endpoint
    close::status::value    m_remote_close_code;

    /// close reason that was received on the wire from the remote endpoint
    std::string             m_remote_close_reason;

    /// detailed internal error code
    lib::error_code m_ec;

    bool m_was_clean;

    /// whether or not this endpoint initiated the closing handshake.
    bool                    m_closed_by_me;

    /// ???
    bool                    m_failed_by_me;

    /// whether or not this endpoint initiated the drop of the tcp connection
    bool                    m_dropped_by_me;
};

} // namespace websocketpp

#include <websocketpp/impl/connection_impl.hpp>

#endif // websocketpp_connection_hpp
