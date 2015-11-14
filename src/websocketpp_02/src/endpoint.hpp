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

#ifndef websocketpp_endpoint_hpp
#define websocketpp_endpoint_hpp

#include "connection.hpp"
#include "sockets/autotls.hpp" // should this be here?
#include "logger/logger.hpp"

#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/utility.hpp>

#include <iostream>
#include <set>

namespace websocketpp_02 {

/// endpoint_base provides core functionality that needs to be constructed
/// before endpoint policy classes are constructed.
class endpoint_base {
protected:
    /// start the run method of the endpoint's io_service object.
    void run_internal() {
        for (;;) {
            try {
                m_io_service.run();
                break;
            } catch (const std::exception & e) {
                throw e;
            }
        }
    }

    boost::asio::io_service m_io_service;
};

/// describes a configurable websocket endpoint.
/**
 * the endpoint class template provides a configurable websocket endpoint
 * capable that manages websocket connection lifecycles. endpoint is a host
 * class to a series of enriched policy classes that together provide the public
 * interface for a specific type of websocket endpoint.
 *
 * @par thread safety
 * @e distinct @e objects: safe.@n
 * @e shared @e objects: will be safe when complete.
 */
template <
    template <class> class role,
    template <class> class socket = socket::autotls,
    template <class> class logger = log::logger>
class endpoint
 : public endpoint_base,
   public role< endpoint<role,socket,logger> >,
   public socket< endpoint<role,socket,logger> >
{
public:
    /// type of the traits class that stores endpoint related types.
    typedef endpoint_traits< endpoint<role,socket,logger> > traits;

    /// the type of the endpoint itself.
    typedef typename traits::type type;
    /// the type of a shared pointer to the endpoint.
    typedef typename traits::ptr ptr;
    /// the type of the role policy.
    typedef typename traits::role_type role_type;
    /// the type of the socket policy.
    typedef typename traits::socket_type socket_type;
    /// the type of the access logger based on the logger policy.
    typedef typename traits::alogger_type alogger_type;
    typedef typename traits::alogger_ptr alogger_ptr;
    /// the type of the error logger based on the logger policy.
    typedef typename traits::elogger_type elogger_type;
    typedef typename traits::elogger_ptr elogger_ptr;
    /// the type of the connection that this endpoint creates.
    typedef typename traits::connection_type connection_type;
    /// a shared pointer to the type of connection that this endpoint creates.
    typedef typename traits::connection_ptr connection_ptr;
    /// interface (abc) that handlers for this type of endpoint must impliment
    /// role policy and socket policy both may add methods to this interface
    typedef typename traits::handler handler;
    /// a shared pointer to the base class that all handlers for this endpoint
    /// must derive from.
    typedef typename traits::handler_ptr handler_ptr;

    // friend is used here to allow the crtp base classes to access member
    // functions in the derived endpoint. this is done to limit the use of
    // public methods in endpoint and its crtp bases to only those methods
    // intended for end-application use.
#ifdef _websocketpp_cpp11_friend_
    // highly simplified and preferred c++11 version:
    friend role_type;
    friend socket_type;
    friend connection_type;
#else
    friend class role< endpoint<role,socket> >;
    friend class socket< endpoint<role,socket> >;
    friend class connection<type,role< type >::template connection,socket< type >::template connection>;
#endif


    /// construct an endpoint.
    /**
     * this constructor creates an endpoint and registers the default connection
     * handler.
     *
     * @param handler a shared_ptr to the handler to use as the default handler
     * when creating new connections.
     */
    explicit endpoint(handler_ptr handler)
     : role_type(endpoint_base::m_io_service)
     , socket_type(endpoint_base::m_io_service)
     , m_handler(handler)
     , m_read_threshold(default_read_threshold)
     , m_silent_close(default_silent_close)
     , m_state(idle)
     , m_alog(new alogger_type())
     , m_elog(new elogger_type())
     , m_pool(new message::pool<message::data>(1000))
     , m_pool_control(new message::pool<message::data>(size_max))
    {
        m_pool->set_callback(boost::bind(&type::on_new_message,this));
    }

    /// destroy an endpoint
    ~endpoint() {
        m_alog->at(log::alevel::devel) << "endpoint destructor called" << log::endl;
        // tell the memory pool we don't want to be notified about newly freed
        // messages any more (because we wont be here)
        m_pool->set_callback(null);

        // detach any connections that are still alive at this point
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);

        typename std::set<connection_ptr>::iterator it;

        while (!m_connections.empty()) {
            remove_connection(*m_connections.begin());
        }
        m_alog->at(log::alevel::devel) << "endpoint destructor done" << log::endl;
    }

    // copy/assignment constructors require c++11
    // boost::noncopyable is being used in the meantime.
    // endpoint(endpoint const&) = delete;
    // endpoint& operator=(endpoint const&) = delete

    /// returns a reference to the endpoint's access logger.
    /**
     * visibility: public
     * state: any
     * concurrency: callable from anywhere
     *
     * @return a reference to the endpoint's access logger. see @ref logger
     * for more details about websocket++ logging policy classes.
     *
     * @par example
     * to print a message to the access log of endpoint e at access level devel:
     * @code
     * e.alog().at(log::alevel::devel) << "message" << log::endl;
     * @endcode
     */
    alogger_type& alog() {
        return *m_alog;
    }
    alogger_ptr alog_ptr() {
        return m_alog;
    }

    /// returns a reference to the endpoint's error logger.
    /**
     * @returns a reference to the endpoint's error logger. see @ref logger
     * for more details about websocket++ logging policy classes.
     *
     * @par example
     * to print a message to the error log of endpoint e at access level devel:
     * @code
     * e.elog().at(log::elevel::devel) << "message" << log::endl;
     * @endcode
     */
    elogger_type& elog() {
        return *m_elog;
    }
    elogger_ptr elog_ptr() {
        return m_elog;
    }

    /// get default handler
    /**
     * visibility: public
     * state: valid always
     * concurrency: callable from anywhere
     *
     * @return a pointer to the default handler
     */
    handler_ptr get_handler() const {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);

        return m_handler;
    }

    /// sets the default handler to be used for future connections
    /**
     * does not affect existing connections.
     *
     * @param new_handler a shared pointer to the new default handler. must not
     * be null.
     */
    void set_handler(handler_ptr new_handler) {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);

        if (!new_handler) {
            elog().at(log::elevel::fatal)
                << "tried to switch to a null handler." << log::endl;
            throw websocketpp_02::exception("todo: handlers can't be null");
        }

        m_handler = new_handler;
    }

    /// set endpoint read threshold
    /**
     * sets the default read threshold value that will be passed to new connections.
     * changing this value will only affect new connections, not existing ones. the read
     * threshold represents the largest block of payload bytes that will be processed in
     * a single async read. lower values may experience better callback latency at the
     * expense of additional asio context switching overhead. this value also affects the
     * maximum number of bytes to be buffered before performing utf8 and other streaming
     * validation.
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

    /// get endpoint read threshold
    /**
     * returns the endpoint read threshold. see set_read_threshold for more information
     * about the read threshold.
     *
     * visibility: public
     * state: valid always
     * concurrency: callable from anywhere
     *
     * @return size of the threshold in bytes
     * @see set_read_threshold()
     */
    size_t get_read_threshold() const {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);

        return m_read_threshold;
    }

    /// set connection silent close setting
    /**
     * silent close suppresses the return of detailed connection close information during
     * the closing handshake. this information is critically useful for debugging but may
     * be undesirable for security reasons for some production environments. close reasons
     * could be used to by an attacker to confirm that the implementation is out of
     * resources or be used to identify the websocket library in use.
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

    /// cleanly closes all websocket connections
    /**
     * sends a close signal to every connection with the specified code and
     * reason. the default code is 1001/going away and the default reason is
     * blank.
     *
     * @param code the websocket close code to send to remote clients as the
     * reason that the connection is being closed.
     * @param reason the websocket close reason to send to remote clients as the
     * text reason that the connection is being closed. must be valid utf-8.
     */
    void close_all(close::status::value code = close::status::going_away,
                   const std::string& reason = "")
    {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);

        m_alog->at(log::alevel::endpoint)
        << "endpoint received signal to close all connections cleanly with code "
        << code << " and reason " << reason << log::endl;

        // todo: is there a more elegant way to do this? in some code paths
        // close can call terminate immediately which removes the connection
        // from m_connections, invalidating the iterator.
        typename std::set<connection_ptr>::iterator it;

        for (it = m_connections.begin(); it != m_connections.end();) {
            const connection_ptr con = *it++;
            con->close(code,reason);
        }
    }

    /// stop the endpoint's asio loop
    /**
     * signals the endpoint to call the io_service stop member function. if
     * clean is true the endpoint will be put into an intermediate state where
     * it signals all connections to close cleanly and only calls stop once that
     * process is complete. otherwise stop is called immediately and all
     * io_service operations will be aborted.
     *
     * if clean is true stop will use code and reason for the close code and
     * close reason when it closes open connections. the default code is
     * 1001/going away and the default reason is blank.
     *
     * visibility: public
     * state: valid from running only
     * concurrency: callable from anywhere
     *
     * @param clean whether or not to wait until all connections have been
     * cleanly closed to stop io_service operations.
     * @param code the websocket close code to send to remote clients as the
     * reason that the connection is being closed.
     * @param reason the websocket close reason to send to remote clients as the
     * text reason that the connection is being closed. must be valid utf-8.
     */
    void stop(bool clean = true,
              close::status::value code = close::status::going_away,
              const std::string& reason = "")
    {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);

        if (clean) {
            m_alog->at(log::alevel::endpoint)
                << "endpoint is stopping cleanly" << log::endl;

            m_state = stopping;
            close_all(code,reason);
        } else {
            m_alog->at(log::alevel::endpoint)
                << "endpoint is stopping immediately" << log::endl;

            endpoint_base::m_io_service.stop();
            m_state = stopped;
        }
    }
protected:
    /// creates and returns a new connection
    /**
     * this function creates a new connection of the type and passes it a
     * reference to this as well as a shared pointer to the default connection
     * handler. the newly created connection is added to the endpoint's
     * management list. the endpoint will retain this pointer until
     * remove_connection is called to remove it.
     *
     * if the endpoint is in a state where it is trying to stop or has already
     * stopped an empty shared pointer is returned.
     *
     * visibility: protected
     * state: always valid, behavior differs based on state
     * concurrency: callable from anywhere
     *
     * @return a shared pointer to the newly created connection or an empty
     * shared pointer if one could not be created.
     */
    connection_ptr create_connection() {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);

        if (m_state == stopping || m_state == stopped) {
            return connection_ptr();
        }

        connection_ptr new_connection(new connection_type(*this,m_handler));
        m_connections.insert(new_connection);

        m_alog->at(log::alevel::devel) << "connection created: count is now: "
                                       << m_connections.size() << log::endl;

        return new_connection;
    }

    /// removes a connection from the list managed by this endpoint.
    /**
     * this function erases a connection from the list managed by the endpoint.
     * after this function returns, endpoint all async events related to this
     * connection should be canceled and neither asio nor this endpoint should
     * have a pointer to this connection. unless the end user retains a copy of
     * the shared pointer the connection will be freed and any state it
     * contained (close code status, etc) will be lost.
     *
     * visibility: protected
     * state: always valid, behavior differs based on state
     * concurrency: callable from anywhere
     *
     * @param con a shared pointer to a connection created by this endpoint.
     */
    void remove_connection(connection_ptr con) {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);

        // todo: is this safe to use?
        // detaching signals to the connection that the endpoint is no longer aware of it
        // and it is no longer safe to assume the endpoint exists.
        con->detach();

        m_connections.erase(con);

        m_alog->at(log::alevel::devel) << "connection removed: count is now: "
                                       << m_connections.size() << log::endl;

        if (m_state == stopping && m_connections.empty()) {
            // if we are in the process of stopping and have reached zero
            // connections stop the io_service.
            m_alog->at(log::alevel::endpoint)
                << "endpoint has reached zero connections in stopping state. stopping io_service now."
                << log::endl;
            stop(false);
        }
    }

    /// gets a shared pointer to a read/write data message.
    // todo: thread safety
    message::data::ptr get_data_message() {
		return m_pool->get();
	}

    /// gets a shared pointer to a read/write control message.
    // todo: thread safety
    message::data::ptr get_control_message() {
		return m_pool_control->get();
	}

    /// asks the endpoint to restart this connection's handle_read_frame loop
    /// when there are avaliable data messages.
    void wait(connection_ptr con) {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);

        m_read_waiting.push(con);
        m_alog->at(log::alevel::devel) << "connection " << con << " is waiting. " << m_read_waiting.size() << log::endl;
    }

    /// message pool callback indicating that there is a free data message
    /// avaliable. causes one waiting connection to get restarted.
    void on_new_message() {
        boost::lock_guard<boost::recursive_mutex> lock(m_lock);

        if (!m_read_waiting.empty()) {
            connection_ptr next = m_read_waiting.front();

            m_alog->at(log::alevel::devel) << "waking connection " << next << ". " << m_read_waiting.size()-1 << log::endl;

            (*next).handle_read_frame(boost::system::error_code());
            m_read_waiting.pop();


        }
    }
private:
    enum state {
        idle = 0,
        running = 1,
        stopping = 2,
        stopped = 3
    };

    // default settings to pass to connections
    handler_ptr                 m_handler;
    size_t                      m_read_threshold;
    bool                        m_silent_close;

    // other stuff
    state                       m_state;

    std::set<connection_ptr>    m_connections;
    alogger_ptr                 m_alog;
    elogger_ptr                 m_elog;

    // resource pools for read/write message buffers
    message::pool<message::data>::ptr   m_pool;
    message::pool<message::data>::ptr   m_pool_control;
    std::queue<connection_ptr>          m_read_waiting;

    // concurrency support
    mutable boost::recursive_mutex      m_lock;
};

/// traits class that allows looking up relevant endpoint types by the fully
/// defined endpoint type.
template <
    template <class> class role,
    template <class> class socket,
    template <class> class logger>
struct endpoint_traits< endpoint<role, socket, logger> > {
    /// the type of the endpoint itself.
    typedef endpoint<role,socket,logger> type;
    typedef boost::shared_ptr<type> ptr;

    /// the type of the role policy.
    typedef role< type > role_type;
    /// the type of the socket policy.
    typedef socket< type > socket_type;

    /// the type of the access logger based on the logger policy.
    typedef logger<log::alevel::value> alogger_type;
    typedef boost::shared_ptr<alogger_type> alogger_ptr;
    /// the type of the error logger based on the logger policy.
    typedef logger<log::elevel::value> elogger_type;
    typedef boost::shared_ptr<elogger_type> elogger_ptr;

    /// the type of the connection that this endpoint creates.
    typedef connection<type,
                       role< type >::template connection,
                       socket< type >::template connection> connection_type;
    /// a shared pointer to the type of connection that this endpoint creates.
    typedef boost::shared_ptr<connection_type> connection_ptr;

    class handler;

    /// a shared pointer to the base class that all handlers for this endpoint
    /// must derive from.
    typedef boost::shared_ptr<handler> handler_ptr;

    /// interface (abc) that handlers for this type of endpoint may impliment.
    /// role policy and socket policy both may add methods to this interface
    class handler : public role_type::handler_interface,
                    public socket_type::handler_interface
    {
    public:
        // convenience typedefs for use in end application handlers.
        // todo: figure out how to not duplicate the definition of connection_ptr
        typedef boost::shared_ptr<handler> ptr;
        typedef typename connection_type::ptr connection_ptr;
        typedef typename message::data::ptr message_ptr;

        virtual ~handler() {}

        /// on_load is the first callback called for a handler after a new
        /// connection has been transferred to it mid flight.
        /**
         * @param connection a shared pointer to the connection that was transferred
         * @param old_handler a shared pointer to the previous handler
         */
        virtual void on_load(connection_ptr con, handler_ptr old_handler) {}
        /// on_unload is the last callback called for a handler before control
        /// of a connection is handed over to a new handler mid flight.
        /**
         * @param connection a shared pointer to the connection being transferred
         * @param old_handler a shared pointer to the new handler
         */
        virtual void on_unload(connection_ptr con, handler_ptr new_handler) {}
    };

};

} // namespace websocketpp_02

#endif // websocketpp_endpoint_hpp
