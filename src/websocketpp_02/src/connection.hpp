/*
 * copyright (c) 2011, peter thorson. all rights reserved.
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

#include "common.hpp"
#include "http/parser.hpp"
#include "logger/logger.hpp"

#include "roles/server.hpp"

#include "processors/hybi.hpp"

#include "messages/data.hpp"
#include "messages/control.hpp"

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>

#include <iostream> // temporary?
#include <vector>
#include <string>
#include <queue>
#include <set>

#ifdef min
	#undef min
#endif // #ifdef min

namespace websocketpp_02 {

class endpoint_base;

template <typename t>
struct connection_traits;

template <
    typename endpoint,
    template <class> class role,
    template <class> class socket>
class connection
 : public role< connection<endpoint,role,socket> >,
   public socket< connection<endpoint,role,socket> >,
   public boost::enable_shared_from_this< connection<endpoint,role,socket> >
{
public:
    typedef connection_traits< connection<endpoint,role,socket> > traits;

    // get types that we need from our traits class
    typedef typename traits::type type;
    typedef typename traits::ptr ptr;
    typedef typename traits::role_type role_type;
    typedef typename traits::socket_type socket_type;

    typedef endpoint endpoint_type;

    typedef typename endpoint_type::handler_ptr handler_ptr;
    typedef typename endpoint_type::handler handler;

    // friends (would require c++11) this would enable connection::start to be
    // protected instead of public.
    //friend typename endpoint_traits<endpoint_type>::role_type;
    //friend typename endpoint_traits<endpoint_type>::socket_type;
    //friend class role<endpoint>;
    //friend class socket<endpoint>;

    friend class role< connection<endpoint,role,socket> >;
    friend class socket< connection<endpoint,role,socket> >;

    enum write_state {
        idle = 0,
        writing = 1,
        inturrupt = 2
    };

    enum read_state {
        reading = 0,
        waiting = 1
    };

    connection(endpoint_type& e,handler_ptr h)
     : role_type(e)
     , socket_type(e)
     , m_endpoint(e)
     , m_alog(e.alog_ptr())
     , m_elog(e.elog_ptr())
     , m_handler(h)
     , m_read_threshold(e.get_read_threshold())
     , m_silent_close(e.get_silent_close())
     , m_timer(e.endpoint_base::m_io_service,boost::posix_time::seconds(0))
     , m_state(session::state::connecting)
     , m_protocol_error(false)
     , m_write_buffer(0)
     , m_write_state(idle)
     , m_fail_code(fail::status::good)
     , m_local_close_code(close::status::abnormal_close)
     , m_remote_close_code(close::status::abnormal_close)
     , m_closed_by_me(false)
     , m_failed_by_me(false)
     , m_dropped_by_me(false)
     , m_read_state(reading)
     , m_strand(e.endpoint_base::m_io_service)
     , m_detached(false)
    {
        socket_type::init();

        // this should go away
        m_control_message = message::control_ptr(new message::control());
    }

    /// destroy the connection
    ~connection() {
        try {
            if (m_state != session::state::closed) {
                terminate(true);
            }
        } catch (...) {}
    }

    // copy/assignment constructors require c++11
    // boost::noncopyable is being used in the meantime.
    // connection(connection const&) = delete;
    // connection& operator=(connection const&) = delete

    /// start the websocket connection async read loop
    /**
     * begins the connection's async read loop. first any socket level
     * initialization will happen (tls handshake, etc) then the handshake and
     * frame reads will start.
     *
     * visibility: protected
     * state: should only be called once by the endpoint.
     * concurrency: safe as long as state is valid
     */
    void start() {
        // initialize the socket.
        socket_type::async_init(
            m_strand.wrap(boost::bind(
                &type::handle_socket_init,
                type::shared_from_this(),
                boost::asio::placeholders::error
            ))
        );
    }

    /// return current connection state
    /**
     * visibility: public
     * state: valid always
     * concurrency: callable from anywhere
     *
     * @return current connection state
     */
    session::state::value get_state() const {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);
        return m_state;
    }

    /// detaches the connection from its endpoint
    /**
     * called by the m_endpoint's destructor. in state detached m_endpoint is
     * no longer avaliable. the connection may stick around if the end user
     * application needs to read state from it (ie close reasons, etc) but no
     * operations requring the endpoint can be performed.
     *
     * visibility: protected
     * state: should only be called once by the endpoint.
     * concurrency: safe as long as state is valid
     */
    void detach() {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);

        m_detached = true;
    }

    void send(const std::string& payload, frame::opcode::value op = frame::opcode::text);
    void send(message::data_ptr msg);

    /// close connection
    /**
     * closes the websocket connection with the given status code and reason.
     * from state open a clean connection close is initiated. from any other
     * state the socket will be closed and the connection cleaned up.
     *
     * there is no feedback directly from close. feedback will be provided via
     * the on_fail or on_close callbacks.
     *
     * visibility: public
     * state: valid from open, ignored otherwise
     * concurrency: callable from any thread
     *
     * @param code close code to send
     * @param reason close reason to send
     */
    void close(close::status::value code, const std::string& reason = "") {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);

        if (m_detached) {return;}

        if (m_state == session::state::connecting) {
            m_endpoint.endpoint_base::m_io_service.post(
                m_strand.wrap(boost::bind(
                    &type::terminate,
                    type::shared_from_this(),
                    true
                ))
            );
        } else if (m_state == session::state::open) {
            m_endpoint.endpoint_base::m_io_service.post(
                m_strand.wrap(boost::bind(
                    &type::begin_close,
                    type::shared_from_this(),
                    code,
                    reason
                ))
            );
        } else {
            // in closing state we are already closing, nothing to do
            // in closed state we are already closed, nothing to do
        }
    }

    /// send ping
    /**
     * initiates a ping with the given payload.
     *
     * there is no feedback directly from ping. feedback will be provided via
     * the on_pong or on_pong_timeout callbacks.
     *
     * visibility: public
     * state: valid from open, ignored otherwise
     * concurrency: callable from any thread
     *
     * @param payload payload to be used for the ping
     */
    void ping(const std::string& payload) {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);

        if (m_state != session::state::open) {return;}
        if (m_detached || !m_processor) {return;}

        // todo: optimize control messages and handle case where
        // endpoint is out of messages
        message::data_ptr control = get_control_message2();
        control->reset(frame::opcode::ping);
        control->set_payload(payload);
        m_processor->prepare_frame(control);

        // using strand post here rather than ioservice.post(strand.wrap)
        // to ensure that messages are sent in order
        m_strand.post(boost::bind(
			&type::write_message,
			type::shared_from_this(),
			control
		));
    }

    /// send pong
    /**
     * initiates a pong with the given payload.
     *
     * there is no feedback from pong.
     *
     * visibility: public
     * state: valid from open, ignored otherwise
     * concurrency: callable from any thread
     *
     * @param payload payload to be used for the pong
     */
    void pong(const std::string& payload) {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);

        if (m_state != session::state::open) {return;}
        if (m_detached) {return;}

        // todo: optimize control messages and handle case where
        // endpoint is out of messages
        message::data_ptr control = get_control_message2();
        control->reset(frame::opcode::pong);
        control->set_payload(payload);
        m_processor->prepare_frame(control);

        // using strand post here rather than ioservice.post(strand.wrap)
        // to ensure that messages are sent in order
        m_strand.post(boost::bind(
			&type::write_message,
			type::shared_from_this(),
			control
		));
    }

    /// return send buffer size (payload bytes)
    /**
     * visibility: public
     * state: valid from any state.
     * concurrency: callable from any thread
     *
     * @return the current number of bytes in the outgoing send buffer.
     */
    uint64_t buffered_amount() const {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);

        return m_write_buffer;
    }

    /// get library fail code
    /**
     * returns the internal ws++ fail code. this code starts at a value of
     * websocketpp_02::fail::status::good and will be set to other values as errors
     * occur. some values are direct errors, others point to locations where
     * more specific error information can be found. key values:
     *
     * (all in websocketpp_02::fail::status:: ) namespace
     * good - no error has occurred yet
     * system - a system call failed, the system error code is avaliable via
     *          get_fail_system_code()
     * websocket - the websocket connection close handshake was performed, more
     *             information is avaliable via get_local_close_code()/reason()
     * unknown - terminate was called without a more specific error being set
     *
     * refer to common.hpp for the rest of the individual codes. call
     * get_fail_reason() for a human readable error.
     *
     * visibility: public
     * state: valid from any
     * concurrency: callable from any thread
     *
     * @return fail code supplied by websocket++
     */
    fail::status::value get_fail_code() const {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);

        return m_fail_code;
    }

    /// get library failure reason
    /**
     * returns a human readable library failure reason.
     *
     * visibility: public
     * state: valid from any
     * concurrency: callable from any thread
     *
     * @return fail reason supplied by websocket++
     */
    std::string get_fail_reason() const {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);

        return m_fail_reason;
    }

    /// get system failure code
    /**
     * returns the system error code that caused ws++ to fail the connection
     *
     * visibility: public
     * state: valid from any
     * concurrency: callable from any thread
     *
     * @return error code supplied by system
     */
    boost::system::error_code get_system_fail_code() const {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);

        return m_fail_system;
    }

    /// get websocket close code sent by websocket++
    /**
     * returns the system error code that caused
     *
     * visibility: public
     * state: valid from closed, an exception is thrown otherwise
     * concurrency: callable from any thread
     *
     * @return close code supplied by websocket++
     */
    close::status::value get_local_close_code() const {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);

        if (m_state != session::state::closed) {
            throw exception("get_local_close_code called from state other than closed",
                            error::invalid_state);
        }

        return m_local_close_code;
    }

    /// get local close reason
    /**
     * returns the close reason that websocket++ believes to be the reason the
     * connection closed. this is almost certainly the value passed to the
     * `close()` method.
     *
     * visibility: public
     * state: valid from closed, an exception is thrown otherwise
     * concurrency: callable from any thread
     *
     * @return close reason supplied by websocket++
     */
    std::string get_local_close_reason() const {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);

        if (m_state != session::state::closed) {
            throw exception("get_local_close_reason called from state other than closed",
                            error::invalid_state);
        }

        return m_local_close_reason;
    }

    /// get remote close code
    /**
     * returns the close reason that was received over the wire from the remote peer.
     * this method may return values that are invalid on the wire such as 1005/no close
     * code received, 1006 abnormal closure, or 1015 bad tls handshake.
     *
     * visibility: public
     * state: valid from closed, an exception is thrown otherwise
     * concurrency: callable from any thread
     *
     * @return close code supplied by remote endpoint
     */
    close::status::value get_remote_close_code() const {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);

        if (m_state != session::state::closed) {
            throw exception("get_remote_close_code called from state other than closed",
                            error::invalid_state);
        }

        return m_remote_close_code;
    }

    /// get remote close reason
    /**
     * returns the close reason that was received over the wire from the remote peer.
     *
     * visibility: public
     * state: valid from closed, an exception is thrown otherwise
     * concurrency: callable from any thread
     *
     * @return close reason supplied by remote endpoint
     */
    std::string get_remote_close_reason() const {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);

        if (m_state != session::state::closed) {
            throw exception("get_remote_close_reason called from state other than closed",
                            error::invalid_state);
        }

        return m_remote_close_reason;
    }

    /// get failed_by_me
    /**
     * returns whether or not the connection ending sequence was initiated by this
     * endpoint. will return true when this endpoint chooses to close normally or when it
     * discovers an error and chooses to close the connection (either forcibly or not).
     * will return false when the close handshake was initiated by the remote endpoint or
     * if the tcp connection was dropped or broken prematurely.
     *
     * visibility: public
     * state: valid from closed, an exception is thrown otherwise
     * concurrency: callable from any thread
     *
     * @return whether or not the connection ending sequence was initiated by this endpoint
     */
    bool get_failed_by_me() const {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);

        if (m_state != session::state::closed) {
            throw exception("get_failed_by_me called from state other than closed",
                            error::invalid_state);
        }

        return m_failed_by_me;
    }

    /// get dropped_by_me
    /**
     * returns whether or not the tcp connection was dropped by this endpoint.
     *
     * visibility: public
     * state: valid from closed, an exception is thrown otherwise
     * concurrency: callable from any thread
     *
     * @return whether or not the tcp connection was dropped by this endpoint.
     */
    bool get_dropped_by_me() const {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);

        if (m_state != session::state::closed) {
            throw exception("get_dropped_by_me called from state other than closed",
                            error::invalid_state);
        }

        return m_dropped_by_me;
    }

    /// get closed_by_me
    /**
     * returns whether or not the websocket closing handshake was initiated by this
     * endpoint.
     *
     * visibility: public
     * state: valid from closed, an exception is thrown otherwise
     * concurrency: callable from any thread
     *
     * @return whether or not closing handshake was initiated by this endpoint
     */
    bool get_closed_by_me() const {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);

        if (m_state != session::state::closed) {
            throw exception("get_closed_by_me called from state other than closed",
                            error::invalid_state);
        }

        return m_closed_by_me;
    }

    /// get get_data_message
    /**
     * returns a pointer to an outgoing message buffer. if there are no outgoing messages
     * available, a no_outgoing_messages exception is thrown. this happens when the
     * endpoint has exhausted its resources dedicated to buffering outgoing messages. to
     * deal with this error either increase available outgoing resources or throttle back
     * the rate and size of outgoing messages.
     *
     * visibility: public
     * state: valid from open, an exception is thrown otherwise
     * concurrency: callable from any thread
     *
     * @return a pointer to the message buffer
     */
    message::data_ptr get_data_message() {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);

        if (m_detached) {
            throw exception("get_data_message: endpoint was destroyed",error::endpoint_unavailable);
        }

        if (m_state != session::state::open && m_state != session::state::closing) {
            throw exception("get_data_message called from invalid state",
                            error::invalid_state);
        }

        message::data_ptr msg = m_endpoint.get_data_message();

        if (!msg) {
            throw exception("no outgoing messages available",error::no_outgoing_messages);
        } else {
            return msg;
        }
    }


    message::data_ptr get_control_message2() {
        return m_endpoint.get_control_message();
    }

    message::control_ptr get_control_message() {
        return m_control_message;
    }

    /// set connection handler
    /**
     * sets the handler that will process callbacks for this connection. the switch is
     * processed asynchronously so set_handler will return immediately. the existing
     * handler will receive an on_unload callback immediately before the switch. after
     * on_unload returns the original handler will not receive any more callbacks from
     * this connection. the new handler will receive an on_load callback immediately after
     * the switch and before any other callbacks are processed.
     *
     * visibility: public
     * state: valid from any state
     * concurrency: callable from any thread
     *
     * @param new_handler the new handler to set
     */
    void set_handler(handler_ptr new_handler) {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);
        if (m_detached) {return;}

        if (!new_handler) {
            elog()->at(log::elevel::fatal) << "tried to switch to a null handler."
                                           << log::endl;
            terminate(true);
            return;
        }

        // io_service::strand ordering guarantees are not sufficient here to
        // make sure that this code is run before any messages are processed.
        handler_ptr old_handler = m_handler;

        old_handler->on_unload(type::shared_from_this(),new_handler);
        m_handler = new_handler;
        new_handler->on_load(type::shared_from_this(),old_handler);

        /*m_endpoint.endpoint_base::m_io_service.post(
            m_strand.wrap(boost::bind(
                &type::set_handler_internal,
                type::shared_from_this(),
                new_handler
            ))
        );*/
    }

    /// set connection read threshold
    /**
     * set the read threshold for this connection. see endpoint::set_read_threshold for
     * more information about the read threshold.
     *
     * visibility: public
     * state: valid always
     * concurrency: callable from anywhere
     *
     * @param val size of the threshold in bytes
     */
    void set_read_threshold(size_t val) {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);

        m_read_threshold = val;
    }

    /// get connection read threshold
    /**
     * returns the connection read threshold. see set_read_threshold for more information
     * about the read threshold.
     *
     * visibility: public
     * state: valid always
     * concurrency: callable from anywhere
     *
     * @return size of the threshold in bytes
     */
    size_t get_read_threshold() const {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);

        return m_read_threshold;
    }

    /// set connection silent close setting
    /**
     * see endpoint::set_silent_close for more information.
     *
     * visibility: public
     * state: valid always
     * concurrency: callable from anywhere
     *
     * @param val new silent close value
     */
    void set_silent_close(bool val) {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);

        m_silent_close = val;
    }

    /// get connection silent close setting
    /**
     * visibility: public
     * state: valid always
     * concurrency: callable from anywhere
     *
     * @return current silent close value
     * @see set_silent_close()
     */
    bool get_silent_close() const {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);

        return m_silent_close;
    }

    // todo: deprecated. will change to get_rng?
    int32_t gen() {
        return 0;
    }

    /// returns a reference to the endpoint's access logger.
    /**
     * visibility: public
     * state: valid from any state
     * concurrency: callable from any thread
     *
     * @return a reference to the endpoint's access logger
     */
    typename endpoint::alogger_ptr alog() {
        return m_alog;
    }

    /// returns a reference to the endpoint's error logger.
    /**
     * visibility: public
     * state: valid from any state
     * concurrency: callable from any thread
     *
     * @return a reference to the endpoint's error logger
     */
    typename endpoint::elogger_ptr elog() {
        return m_elog;
    }

    /// returns a pointer to the endpoint's handler.
    /**
     * visibility: public
     * state: valid from any state
     * concurrency: callable from any thread
     *
     * @return a pointer to the endpoint's default handler
     */
    handler_ptr get_handler() {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);
        return m_handler;
    }

    /// returns a reference to the connections strand object.
    /**
     * visibility: public
     * state: valid from any state
     * concurrency: callable from any thread
     *
     * @return a reference to the connection's strand object
     */
    boost::asio::strand& get_strand() {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);
        return m_strand;
    }
public:
//protected:  todo: figure out why vcpp2010 doesn't like protected here

    /// socket initialization callback
    /* this is the async return point for initializing the socket policy. after this point
     * the socket is open and ready.
     *
     * visibility: protected
     * state: valid only once per connection during the initialization sequence.
     * concurrency: must be called within m_strand
     */
    void handle_socket_init(const boost::system::error_code& error) {
        if (error) {
            elog()->at(log::elevel::rerror)
                << "socket initialization failed, error code: " << error
                << log::endl;
            this->terminate(false);
            return;
        }

        role_type::async_init();
    }
public:
    /// asio callback for async_read of more frame data
    /**
     * callback after asio has read some data that needs to be sent to a frame processor
     *
     * todo: think about how receiving a very large buffer would affect concurrency due to
     *       that handler running for an unusually long period of time? is a maximum size
     *       necessary on m_buf?
     * todo: think about how terminate here works with the locks and concurrency
     *
     * visibility: protected
     * state: valid for states open and closing, ignored otherwise
     * concurrency: must be called via strand. only one async_read should be outstanding
     *              at a time. should only be called from inside handle_read_frame or by
     *              the role's init method.
     */
    void handle_read_frame(const boost::system::error_code& error) {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);

        // check if state changed while we were waiting for a read.
        if (m_state == session::state::closed) {
            alog()->at(log::alevel::devel)
                << "handle read returning due to closed connection"
                << log::endl;
            return;
        }
        if (m_state == session::state::connecting) { return; }

        if (error) {
            if (error == boost::asio::error::eof) {
                elog()->at(log::elevel::rerror)
                    << "unexpected eof from remote endpoint, terminating connection."
                    << log::endl;
                terminate(false);
                return;
            } else if (error == boost::asio::error::operation_aborted) {
                // got unexpected abort (likely our server issued an abort on
                // all connections on this io_service)

                elog()->at(log::elevel::rerror)
                    << "connection terminating due to aborted read: "
                    << error << log::endl;
                terminate(true);
                return;
            } else {
                // other unexpected error

                elog()->at(log::elevel::rerror)
                    << "connection terminating due to unknown error: " << error
                    << log::endl;
                terminate(false);
                return;
            }
        }

        // process data from the buffer just read into
        std::istream s(&m_buf);

        while (m_state != session::state::closed && m_buf.size() > 0) {
            try {
                m_processor->consume(s);

                if (m_processor->ready()) {
                    if (m_processor->is_control()) {
                        process_control(m_processor->get_control_message());
                    } else {
                        process_data(m_processor->get_data_message());
                    }
                    m_processor->reset();
                }
            } catch (const processor::exception& e) {
                if (m_processor->ready()) {
                    m_processor->reset();
                }

                switch(e.code()) {
                    // the protocol error flag is set by any processor exception
                    // that indicates that the composition of future bytes in
                    // the read stream cannot be reliably determined. bytes will
                    // no longer be read after that point.
                    case processor::error::protocol_violation:
                        m_protocol_error = true;
                        begin_close(close::status::protocol_error,e.what());
                        break;
                    case processor::error::payload_violation:
                        m_protocol_error = true;
                        begin_close(close::status::invalid_payload,e.what());
                        break;
                    case processor::error::internal_endpoint_error:
                        m_protocol_error = true;
                        begin_close(close::status::internal_endpoint_error,e.what());
                        break;
                    case processor::error::soft_error:
                        continue;
                    case processor::error::message_too_big:
                        m_protocol_error = true;
                        begin_close(close::status::message_too_big,e.what());
                        break;
                    case processor::error::out_of_messages:
                        // we need to wait for a message to be returned by the
                        // client. we exit the read loop. handle_read_frame
                        // will be restarted by recycle()
                        //m_read_state = waiting;
                        m_endpoint.wait(type::shared_from_this());
                        return;
                    default:
                        // fatal error, forcibly end connection immediately.
                        elog()->at(log::elevel::devel)
                            << "terminating connection due to unrecoverable processor exception: "
                            << e.code() << " (" << e.what() << ")" << log::endl;
                        terminate(true);
                }
                break;

            }
        }

        // try and read more
        if (m_state != session::state::closed &&
        	m_processor &&
            m_processor->get_bytes_needed() > 0 &&
            !m_protocol_error)
        {
            // todo: read timeout timer?

            async_read(
                socket_type::get_socket(),
                m_buf,
                boost::asio::transfer_at_least(std::min(
                    m_read_threshold,
                    static_cast<size_t>(m_processor->get_bytes_needed())
                )),
                m_strand.wrap(boost::bind(
                    &type::handle_read_frame,
                    type::shared_from_this(),
                    boost::asio::placeholders::error
                ))
            );
        }
    }
public:
//protected:  todo: figure out why vcpp2010 doesn't like protected here
    /// internal implementation for set_handler
    /**
     * visibility: protected
     * state: valid for all states
     * concurrency: must be called within m_strand
     *
     * @param new_handler the handler to switch to
     */
    void set_handler_internal(handler_ptr new_handler) {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);

        if (!new_handler) {
            elog()->at(log::elevel::fatal)
                << "terminating connection due to attempted switch to null handler."
                << log::endl;
            // todo: unserialized call to terminate?
            terminate(true);
            return;
        }

        handler_ptr old_handler = m_handler;

        old_handler->on_unload(type::shared_from_this(),new_handler);
        m_handler = new_handler;
        new_handler->on_load(type::shared_from_this(),old_handler);
    }

    /// dispatch a data message
    /**
     * visibility: private
     * state: no state checking, should only be called within handle_read_frame
     * concurrency: must be called within m_stranded method
     */
    void process_data(message::data_ptr msg) {
        m_handler->on_message(type::shared_from_this(),msg);
    }

    /// dispatch or handle a control message
    /**
     * visibility: private
     * state: no state checking, should only be called within handle_read_frame
     * concurrency: must be called within m_stranded method
     */
    void process_control(message::control_ptr msg) {
        bool response;
        switch (msg->get_opcode()) {
            case frame::opcode::ping:
                response = m_handler->on_ping(type::shared_from_this(),
                                              msg->get_payload());
                if (response) {
                    pong(msg->get_payload());
                }
                break;
            case frame::opcode::pong:
                m_handler->on_pong(type::shared_from_this(),
                                   msg->get_payload());
                // todo: disable ping response timer

                break;
            case frame::opcode::close:
                m_remote_close_code = msg->get_close_code();
                m_remote_close_reason = msg->get_close_reason();

                // check that the codes we got over the wire are valid

                if (m_state == session::state::open) {
                    // other end is initiating
                    alog()->at(log::alevel::debug_close)
                        << "sending close ack" << log::endl;

                    // todo:
                    send_close_ack();
                } else if (m_state == session::state::closing) {
                    // ack of our close
                    alog()->at(log::alevel::debug_close)
                        << "got close ack" << log::endl;

                    terminate(false);
                    // todo: start terminate timer (if client)
                }
                break;
            default:
                throw processor::exception("invalid opcode",
                    processor::error::protocol_violation);
                break;
        }
    }

    /// begins a clean close handshake
    /**
     * initiates a close handshake by sending a close frame with the given code
     * and reason.
     *
     * visibility: protected
     * state: valid for open, ignored otherwise.
     * concurrency: must be called within m_strand
     *
     * @param code the code to send
     * @param reason the reason to send
     */
    void begin_close(close::status::value code, const std::string& reason) {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);

        alog()->at(log::alevel::debug_close) << "begin_close called" << log::endl;

        if (m_detached) {return;}

        if (m_state != session::state::open) {
            elog()->at(log::elevel::warn)
                << "tried to disconnect a session that wasn't open" << log::endl;
            return;
        }

        if (close::status::invalid(code)) {
            elog()->at(log::elevel::warn)
                << "tried to close a connection with invalid close code: "
                << code << log::endl;
            return;
        } else if (close::status::reserved(code)) {
            elog()->at(log::elevel::warn)
                << "tried to close a connection with reserved close code: "
                << code << log::endl;
            return;
        }

        m_state = session::state::closing;

        m_closed_by_me = true;

        if (m_silent_close) {
            m_local_close_code = close::status::no_status;
            m_local_close_reason = "";

            // in case of protocol error in silent mode just drop the connection.
            // this is allowed by the spec and improves robustness over sending an
            // empty close frame that we would ignore the ack to.
            if (m_protocol_error) {
                terminate(false);
                return;
            }
        } else {
            m_local_close_code = code;
            m_local_close_reason = reason;
        }

        // wait for close timer
        // todo: configurable value
        register_timeout(5000,fail::status::websocket,"timeout on close handshake");

        // todo: optimize control messages and handle case where endpoint is
        // out of messages
        message::data_ptr msg = get_control_message2();

        if (!msg) {
            // server endpoint is out of control messages. force drop connection
            elog()->at(log::elevel::rerror)
                << "request for control message failed (out of resources). terminating connection."
                << log::endl;
            terminate(true);
            return;
        }

        msg->reset(frame::opcode::close);
        m_processor->prepare_close_frame(msg,m_local_close_code,m_local_close_reason);

        // using strand post here rather than ioservice.post(strand.wrap)
        // to ensure that messages are sent in order
        m_strand.post(boost::bind(
			&type::write_message,
			type::shared_from_this(),
			msg
		));
    }

    /// send an acknowledgement close frame
    /**
     * visibility: private
     * state: no state checking, should only be called within process_control
     * concurrency: must be called within m_stranded method
     */
    void send_close_ack() {
        alog()->at(log::alevel::debug_close) << "send_close_ack called" << log::endl;

        // echo close value unless there is a good reason not to.
        if (m_silent_close) {
            m_local_close_code = close::status::no_status;
            m_local_close_reason = "";
        } else if (m_remote_close_code == close::status::no_status) {
            m_local_close_code = close::status::normal;
            m_local_close_reason = "";
        } else if (m_remote_close_code == close::status::abnormal_close) {
            // todo: can we possibly get here? this means send_close_ack was
            //       called after a connection ended without getting a close
            //       frame
            throw "shouldn't be here";
        } else if (close::status::invalid(m_remote_close_code)) {
            // todo: shouldn't be able to get here now either
            m_local_close_code = close::status::protocol_error;
            m_local_close_reason = "status code is invalid";
        } else if (close::status::reserved(m_remote_close_code)) {
            // todo: shouldn't be able to get here now either
            m_local_close_code = close::status::protocol_error;
            m_local_close_reason = "status code is reserved";
        } else {
            m_local_close_code = m_remote_close_code;
            m_local_close_reason = m_remote_close_reason;
        }

        // todo: check whether we should cancel the current in flight write.
        //       if not canceled the close message will be sent as soon as the
        //       current write completes.


        // todo: optimize control messages and handle case where endpoint is
        // out of messages
        message::data_ptr msg = get_control_message2();

        if (!msg) {
            // server is out of resources, close connection.
            elog()->at(log::elevel::rerror)
                << "request for control message failed (out of resources). terminating connection."
                << log::endl;
            terminate(true);
            return;
        }

        msg->reset(frame::opcode::close);
        m_processor->prepare_close_frame(msg,m_local_close_code,m_local_close_reason);

        // using strand post here rather than ioservice.post(strand.wrap)
        // to ensure that messages are sent in order
        m_strand.post(boost::bind(
			&type::write_message,
			type::shared_from_this(),
			msg
		));

        //m_write_state = inturrupt;
    }

    /// push message to write queue and start writer if it was idle
    /**
     * visibility: protected (called only by asio dispatcher)
     * state: valid from open and closing, ignored otherwise
     * concurrency: must be called within m_stranded method
     */
    void write_message(message::data_ptr msg) {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);
        if (m_state != session::state::open && m_state != session::state::closing) {return;}
        if (m_write_state == inturrupt) {return;}

        m_write_buffer += msg->get_payload().size();
        m_write_queue.push(msg);

        write();
    }

    /// begin async write of next message in list
    /**
     * visibility: private
     * state: valid only as called from write_message or handle_write
     * concurrency: must be called within m_stranded method
     */
    void write() {
        switch (m_write_state) {
            case idle:
                break;
            case writing:
                // already writing. write() will get called again by the write
                // handler once it is ready.
                return;
            case inturrupt:
                // clear the queue except for the last message
                while (m_write_queue.size() > 1) {
                    m_write_buffer -= m_write_queue.front()->get_payload().size();
                    m_write_queue.pop();
                }
                break;
            default:
                // todo: assert shouldn't be here
                break;
        }

        if (!m_write_queue.empty()) {
            if (m_write_state == idle) {
                m_write_state = writing;
            }

            m_write_buf.push_back(boost::asio::buffer(m_write_queue.front()->get_header()));
            m_write_buf.push_back(boost::asio::buffer(m_write_queue.front()->get_payload()));

            //m_endpoint.alog().at(log::alevel::devel) << "write header: " << zsutil::to_hex(m_write_queue.front()->get_header()) << log::endl;

            async_write(socket_type::get_socket(),
                m_write_buf,
                m_strand.wrap(boost::bind(
                    &type::handle_write,
                    type::shared_from_this(),
                    boost::asio::placeholders::error
                ))
            );
        } else {
            // if we are in an inturrupted state and had nothing else to write
            // it is safe to terminate the connection.
            if (m_write_state == inturrupt) {
                alog()->at(log::alevel::debug_close)
                    << "exit after inturrupt" << log::endl;
                terminate(false);
            } else {
		m_handler->on_send_empty(type::shared_from_this());
	    }
        }
    }

    /// async write callback
    /**
     * visibility: protected (called only by asio dispatcher)
     * state: valid from open and closing, ignored otherwise
     * concurrency: must be called within m_stranded method
     */
    void handle_write(const boost::system::error_code& error) {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);



        if (error) {
            if (m_state == session::state::closed) {
                alog()->at(log::alevel::debug_close)
                        << "handle_write error in closed state. ignoring."
                        << log::endl;
                return;
            }

            if (error == boost::asio::error::operation_aborted) {
                // previous write was aborted
                alog()->at(log::alevel::debug_close)
                    << "asio write was aborted. exiting write loop."
                    << log::endl;
                //terminate(false);
                return;
            } else {
                log_error("asio write failed with unknown error. terminating connection.",error);
                terminate(false);
                return;
            }
        }



        if (m_write_queue.size() == 0) {
            alog()->at(log::alevel::debug_close)
                << "handle_write called with empty queue" << log::endl;
	    return;
        }

        m_write_buffer -= m_write_queue.front()->get_payload().size();
        m_write_buf.clear();

        frame::opcode::value code = m_write_queue.front()->get_opcode();

        m_write_queue.pop();

        if (m_write_state == writing) {
            m_write_state = idle;
        }




        if (code != frame::opcode::close) {
            // only continue next write if the connection is still open
            if (m_state == session::state::open || m_state == session::state::closing) {
                write();
            }
        } else {
            if (m_closed_by_me) {
                alog()->at(log::alevel::debug_close)
                        << "initial close frame sent" << log::endl;
                // this is my close message
                // no more writes allowed after this. will hang on to read their
                // close response unless i just sent a protocol error close, in
                // which case i assume the other end is too broken to return
                // a meaningful response.
                if (m_protocol_error) {
                    terminate(false);
                }
            } else {
                // this is a close ack
                // now that it has been written close the connection

                if (m_endpoint.is_server()) {
                    // if i am a server close immediately
                    alog()->at(log::alevel::debug_close)
                        << "close ack sent. terminating immediately." << log::endl;
                    terminate(false);
                } else {
                    // if i am a client give the server a moment to close.
                    alog()->at(log::alevel::debug_close)
                        << "close ack sent. termination queued." << log::endl;
                    // todo: set timer here and close afterwards
                    terminate(false);
                }
            }
        }
    }

    /// ends the connection by cleaning up based on current state
    /** terminate will review the outstanding resources and close each
     * appropriately. attached handlers will recieve an on_fail or on_close call
     *
     * todo: should we protect against long running handlers?
     *
     * visibility: protected
     * state: valid from any state except closed.
     * concurrency: must be called from within m_strand
     *
     * @param failed_by_me whether or not to mark the connection as failed by me
     */
    void terminate(bool failed_by_me) {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);

        alog()->at(log::alevel::devel)
                << "terminate called from state: " << m_state << log::endl;

        // if state is closed it either means terminate was called twice or
        // something other than this library called it. in either case running
        // it will only cause problems
        if (m_state == session::state::closed) {return;}

        // todo: ensure any other timers are accounted for here.
        // cancel the close timeout
        cancel_timeout();

        // version -1 is an http (non-websocket) connection.
        if (role_type::get_version() != -1) {
            // todo: note, calling shutdown on the ssl socket for an http
            // connection seems to cause shutdown to block for a very long time.
            // not calling it for a websocket connection causes the connection
            // to time out. behavior now is correct but i am not sure why.
            m_dropped_by_me = socket_type::shutdown();

            m_failed_by_me = failed_by_me;

            session::state::value old_state = m_state;
            m_state = session::state::closed;

            if (old_state == session::state::connecting) {
                m_handler->on_fail(type::shared_from_this());

                if (m_fail_code == fail::status::good) {
                    m_fail_code = fail::status::unknown;
                    m_fail_reason = "terminate called in connecting state without more specific error.";
                }
            } else if (old_state == session::state::open ||
                       old_state == session::state::closing)
            {
                m_handler->on_close(type::shared_from_this());

                if (m_fail_code == fail::status::good) {
                    m_fail_code = fail::status::websocket;
                    m_fail_reason = "terminate called in open state without more specific error.";
                }
            }

            log_close_result();
        }

        // finally remove this connection from the endpoint's list. this will
        // remove the last shared pointer to the connection held by ws++. if we
        // are detached this has already been done and can't be done again.
        if (!m_detached) {
            alog()->at(log::alevel::devel)
                << "terminate removing connection" << log::endl;
            m_endpoint.remove_connection(type::shared_from_this());

            /*m_endpoint.endpoint_base::m_io_service.post(
                m_strand.wrap(boost::bind(
                    &type::remove_connection,
                    type::shared_from_this()
                ))
            );*/
        }
    }

    // this was an experiment in deferring the detachment of the connection
    // until all handlers had been cleaned up. with the switch to a local logger
    // this can probably be removed entirely.
    void remove_connection() {
        // finally remove this connection from the endpoint's list. this will
        // remove the last shared pointer to the connection held by ws++. if we
        // are detached this has already been done and can't be done again.
        /*if (!m_detached) {
            alog()->at(log::alevel::devel) << "terminate removing connection: start" << log::endl;
            m_endpoint.remove_connection(type::shared_from_this());
        }*/
    }

    // this is called when an async asio call encounters an error
    void log_error(std::string msg,const boost::system::error_code& e) {
        elog()->at(log::elevel::rerror) << msg << "(" << e << ")" << log::endl;
    }

    void log_close_result() {
        alog()->at(log::alevel::disconnect)
            //<< "disconnect " << (m_was_clean ? "clean" : "unclean")
            << "disconnect "
            << " close local:[" << m_local_close_code
            << (m_local_close_reason == "" ? "" : ","+m_local_close_reason)
            << "] remote:[" << m_remote_close_code
            << (m_remote_close_reason == "" ? "" : ","+m_remote_close_reason) << "]"
            << log::endl;
     }

    void register_timeout(size_t ms,fail::status::value s, std::string msg) {
        m_timer.expires_from_now(boost::posix_time::milliseconds(ms));
        m_timer.async_wait(
            m_strand.wrap(boost::bind(
                &type::fail_on_expire,
                type::shared_from_this(),
                boost::asio::placeholders::error,
                s,
                msg
            ))
        );
    }

    void cancel_timeout() {
         m_timer.cancel();
    }

    void fail_on_expire(const boost::system::error_code& error,
                        fail::status::value status,
                        std::string& msg)
    {
        if (error) {
            if (error != boost::asio::error::operation_aborted) {
                elog()->at(log::elevel::devel)
                    << "fail_on_expire timer ended in unknown error" << log::endl;
                terminate(false);
            }
            return;
        }

        m_fail_code = status;
        m_fail_system = error;
        m_fail_reason = msg;

        alog()->at(log::alevel::disconnect)
            << "fail_on_expire timer expired with message: " << msg << log::endl;
        terminate(true);
    }

    boost::asio::streambuf& buffer()  {
        return m_buf;
    }
public:
//protected:  todo: figure out why vcpp2010 doesn't like protected here
    endpoint_type&                  m_endpoint;
    typename endpoint::alogger_ptr  m_alog;
    typename endpoint::elogger_ptr  m_elog;

    // overridable connection specific settings
    handler_ptr                 m_handler;          // object to dispatch callbacks to
    size_t                      m_read_threshold;   // maximum number of bytes to fetch in
                                                    //   a single async read.
    bool                        m_silent_close;     // suppresses the return of detailed
                                                    //   close codes.

    // network resources
    boost::asio::streambuf      m_buf;
    boost::asio::deadline_timer m_timer;

    // websocket connection state
    session::state::value       m_state;
    bool                        m_protocol_error;

    // stuff that actually does the work
    processor::ptr              m_processor;

    // write queue
    std::vector<boost::asio::const_buffer> m_write_buf;
    std::queue<message::data_ptr>   m_write_queue;
    uint64_t                        m_write_buffer;
    write_state                     m_write_state;

    // close state
    fail::status::value         m_fail_code;
    boost::system::error_code   m_fail_system;
    std::string                 m_fail_reason;
    close::status::value        m_local_close_code;
    std::string                 m_local_close_reason;
    close::status::value        m_remote_close_code;
    std::string                 m_remote_close_reason;
    bool                        m_closed_by_me;
    bool                        m_failed_by_me;
    bool                        m_dropped_by_me;

    // read queue
    read_state                  m_read_state;
    message::control_ptr        m_control_message;

    // concurrency support
    mutable boost::recursive_mutex      m_lock;
    boost::asio::strand         m_strand;
    bool                        m_detached; // todo: this should be atomic
};

// connection related types that it and its policy classes need.
template <
    typename endpoint,
    template <class> class role,
    template <class> class socket>
struct connection_traits< connection<endpoint,role,socket> > {
    // type of the connection itself
    typedef connection<endpoint,role,socket> type;
    typedef boost::shared_ptr<type> ptr;

    // types of the connection policies
    typedef endpoint endpoint_type;
    typedef role< type > role_type;
    typedef socket< type > socket_type;
};

/// convenience overload for sending a one off message.
/**
 * creates a message, fills in payload, and queues a write as a message of
 * type op. default type is text.
 *
 * visibility: public
 * state: valid from open, ignored otherwise
 * concurrency: callable from any thread
 *
 * @param payload payload to write_state
 * @param op opcode to send the message as
 */
template <typename endpoint,template <class> class role,template <class> class socket>
void
connection<endpoint,role,socket>::send(const std::string& payload,frame::opcode::value op)
{
    {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);
        if (m_state != session::state::open) {return;}
    }

    websocketpp_02::message::data::ptr msg = get_control_message2();

    if (!msg) {
        throw exception("endpoint send queue is full",error::send_queue_full);
    }
    if (op != frame::opcode::text && op != frame::opcode::binary) {
        throw exception("opcode must be either text or binary",error::generic);
    }

    msg->reset(op);
    msg->set_payload(payload);
    send(msg);
}

/// send message
/**
 * prepares (if necessary) and sends the given message
 *
 * visibility: public
 * state: valid from open, ignored otherwise
 * concurrency: callable from any thread
 *
 * @param msg a pointer to a data message buffer to send.
 */
template <typename endpoint,template <class> class role,template <class> class socket>
void connection<endpoint,role,socket>::send(message::data_ptr msg) {
    boost::lock_guard<boost::recursive_mutex> lock(m_lock);
    if (m_state != session::state::open) {return;}

    m_processor->prepare_frame(msg);

    // using strand post here rather than ioservice.post(strand.wrap)
	// to ensure that messages are sent in order
	m_strand.post(boost::bind(
		&type::write_message,
		type::shared_from_this(),
		msg
	));
}

} // namespace websocketpp_02

#endif // websocketpp_connection_hpp
