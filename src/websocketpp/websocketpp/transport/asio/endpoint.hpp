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

#ifndef websocketpp_transport_asio_hpp
#define websocketpp_transport_asio_hpp

#include <websocketpp/common/functional.hpp>
#include <websocketpp/logger/levels.hpp>
#include <websocketpp/transport/base/endpoint.hpp>
#include <websocketpp/transport/asio/connection.hpp>
#include <websocketpp/transport/asio/security/none.hpp>

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/system/error_code.hpp>

namespace websocketpp {
namespace transport {
namespace asio {

/// boost asio based endpoint transport component
/**
 * transport::asio::endpoint implements an endpoint transport component using
 * boost asio.
 */
template <typename config>
class endpoint : public config::socket_type {
public:
    /// type of this endpoint transport component
    typedef endpoint<config> type;

    /// type of the concurrency policy
    typedef typename config::concurrency_type concurrency_type;
    /// type of the socket policy
    typedef typename config::socket_type socket_type;
    /// type of the error logging policy
    typedef typename config::elog_type elog_type;
    /// type of the access logging policy
    typedef typename config::alog_type alog_type;

    /// type of the socket connection component
    typedef typename socket_type::socket_con_type socket_con_type;
    /// type of a shared pointer to the socket connection component
    typedef typename socket_con_type::ptr socket_con_ptr;

    /// type of the connection transport component associated with this
    /// endpoint transport component
    typedef asio::connection<config> transport_con_type;
    /// type of a shared pointer to the connection transport component
    /// associated with this endpoint transport component
    typedef typename transport_con_type::ptr transport_con_ptr;

    /// type of a pointer to the asio io_service being used
    typedef boost::asio::io_service* io_service_ptr;
    /// type of a shared pointer to the acceptor being used
    typedef lib::shared_ptr<boost::asio::ip::tcp::acceptor> acceptor_ptr;
    /// type of a shared pointer to the resolver being used
    typedef lib::shared_ptr<boost::asio::ip::tcp::resolver> resolver_ptr;
    /// type of timer handle
    typedef lib::shared_ptr<boost::asio::deadline_timer> timer_ptr;
    /// type of a shared pointer to an io_service work object
    typedef lib::shared_ptr<boost::asio::io_service::work> work_ptr;

    // generate and manage our own io_service
    explicit endpoint()
      : m_io_service(null)
      , m_external_io_service(false)
      , m_listen_backlog(0)
      , m_reuse_addr(false)
      , m_state(uninitialized)
    {
        //std::cout << "transport::asio::endpoint constructor" << std::endl;
    }

    ~endpoint() {
        // clean up our io_service if we were initialized with an internal one.
        m_acceptor.reset();
        if (m_state != uninitialized && !m_external_io_service) {
            delete m_io_service;
        }
    }

    /// transport::asio objects are moveable but not copyable or assignable.
    /// the following code sets this situation up based on whether or not we
    /// have c++11 support or not
#ifdef _websocketpp_deleted_functions_
    endpoint(const endpoint& src) = delete;
    endpoint& operator= (const endpoint & rhs) = delete;
#else
private:
    endpoint(const endpoint& src);
    endpoint& operator= (const endpoint & rhs);
public:
#endif

#ifdef _websocketpp_rvalue_references_
    endpoint (endpoint&& src)
      : m_io_service(src.m_io_service)
      , m_external_io_service(src.m_external_io_service)
      , m_acceptor(src.m_acceptor)
      , m_listen_backlog(boost::asio::socket_base::max_connections)
      , m_reuse_addr(src.m_reuse_addr)
      , m_state(src.m_state)
    {
        src.m_io_service = null;
        src.m_external_io_service = false;
        src.m_acceptor = null;
        src.m_state = uninitialized;
    }

    endpoint& operator= (const endpoint && rhs) {
        if (this != &rhs) {
            m_io_service = rhs.m_io_service;
            m_external_io_service = rhs.m_external_io_service;
            m_acceptor = rhs.m_acceptor;
            m_listen_backlog = rhs.m_listen_backlog;
            m_reuse_addr = rhs.m_reuse_addr;
            m_state = rhs.m_state;

            rhs.m_io_service = null;
            rhs.m_external_io_service = false;
            rhs.m_acceptor = null;
            rhs.m_listen_backlog = boost::asio::socket_base::max_connections;
            rhs.m_state = uninitialized;
        }
        return *this;
    }
#endif
    /// return whether or not the endpoint produces secure connections.
    bool is_secure() const {
        return socket_type::is_secure();
    }

    /// initialize asio transport with external io_service (exception free)
    /**
     * initialize the asio transport policy for this endpoint using the provided
     * io_service object. asio_init must be called exactly once on any endpoint
     * that uses transport::asio before it can be used.
     *
     * @param ptr a pointer to the io_service to use for asio events
     * @param ec set to indicate what error occurred, if any.
     */
    void init_asio(io_service_ptr ptr, lib::error_code & ec) {
        if (m_state != uninitialized) {
            m_elog->write(log::elevel::library,
                "asio::init_asio called from the wrong state");
            using websocketpp::error::make_error_code;
            ec = make_error_code(websocketpp::error::invalid_state);
            return;
        }

        m_alog->write(log::alevel::devel,"asio::init_asio");

        m_io_service = ptr;
        m_external_io_service = true;
        m_acceptor = lib::make_shared<boost::asio::ip::tcp::acceptor>(
            lib::ref(*m_io_service));
            
        m_state = ready;
        ec = lib::error_code();
    }

    /// initialize asio transport with external io_service
    /**
     * initialize the asio transport policy for this endpoint using the provided
     * io_service object. asio_init must be called exactly once on any endpoint
     * that uses transport::asio before it can be used.
     *
     * @param ptr a pointer to the io_service to use for asio events
     */
    void init_asio(io_service_ptr ptr) {
        lib::error_code ec;
        init_asio(ptr,ec);
        if (ec) { throw exception(ec); }
    }

    /// initialize asio transport with internal io_service (exception free)
    /**
     * this method of initialization will allocate and use an internally managed
     * io_service.
     *
     * @see init_asio(io_service_ptr ptr)
     *
     * @param ec set to indicate what error occurred, if any.
     */
    void init_asio(lib::error_code & ec) {
        init_asio(new boost::asio::io_service(), ec);
        m_external_io_service = false;
    }

    /// initialize asio transport with internal io_service
    /**
     * this method of initialization will allocate and use an internally managed
     * io_service.
     *
     * @see init_asio(io_service_ptr ptr)
     */
    void init_asio() {
        init_asio(new boost::asio::io_service());
        m_external_io_service = false;
    }

    /// sets the tcp pre init handler
    /**
     * the tcp pre init handler is called after the raw tcp connection has been
     * established but before any additional wrappers (proxy connects, tls
     * handshakes, etc) have been performed.
     *
     * @since 0.3.0
     *
     * @param h the handler to call on tcp pre init.
     */
    void set_tcp_pre_init_handler(tcp_init_handler h) {
        m_tcp_pre_init_handler = h;
    }

    /// sets the tcp pre init handler (deprecated)
    /**
     * the tcp pre init handler is called after the raw tcp connection has been
     * established but before any additional wrappers (proxy connects, tls
     * handshakes, etc) have been performed.
     *
     * @deprecated use set_tcp_pre_init_handler instead
     *
     * @param h the handler to call on tcp pre init.
     */
    void set_tcp_init_handler(tcp_init_handler h) {
        set_tcp_pre_init_handler(h);
    }

    /// sets the tcp post init handler
    /**
     * the tcp post init handler is called after the tcp connection has been
     * established and all additional wrappers (proxy connects, tls handshakes,
     * etc have been performed. this is fired before any bytes are read or any
     * websocket specific handshake logic has been performed.
     *
     * @since 0.3.0
     *
     * @param h the handler to call on tcp post init.
     */
    void set_tcp_post_init_handler(tcp_init_handler h) {
        m_tcp_post_init_handler = h;
    }

    /// sets the maximum length of the queue of pending connections.
    /**
     * sets the maximum length of the queue of pending connections. increasing
     * this will allow websocket++ to queue additional incoming connections.
     * setting it higher may prevent failed connections at high connection rates
     * but may cause additional latency.
     *
     * for this value to take effect you may need to adjust operating system
     * settings.
     *
     * new values affect future calls to listen only.
     *
     * a value of zero will use the operating system default. this is the
     * default value.
     *
     * @since 0.3.0
     *
     * @param backlog the maximum length of the queue of pending connections
     */
    void set_listen_backlog(int backlog) {
        m_listen_backlog = backlog;
    }
    
    /// sets whether to use the so_reuseaddr flag when opening listening sockets
    /**
     * specifies whether or not to use the so_reuseaddr tcp socket option. what 
     * this flag does depends on your operating system. please consult operating
     * system documentation for more details.
     *
     * new values affect future calls to listen only.
     *
     * the default is false.
     *
     * @since 0.3.0
     *
     * @param value whether or not to use the so_reuseaddr option
     */
    void set_reuse_addr(bool value) {
        m_reuse_addr = value;
    }

    /// retrieve a reference to the endpoint's io_service
    /**
     * the io_service may be an internal or external one. this may be used to
     * call methods of the io_service that are not explicitly wrapped by the
     * endpoint.
     *
     * this method is only valid after the endpoint has been initialized with
     * `init_asio`. no error will be returned if it isn't.
     *
     * @return a reference to the endpoint's io_service
     */
    boost::asio::io_service & get_io_service() {
        return *m_io_service;
    }

    /// set up endpoint for listening manually (exception free)
    /**
     * bind the internal acceptor using the specified settings. the endpoint
     * must have been initialized by calling init_asio before listening.
     *
     * @param ep an endpoint to read settings from
     * @param ec set to indicate what error occurred, if any.
     */
    void listen(boost::asio::ip::tcp::endpoint const & ep, lib::error_code & ec)
    {
        if (m_state != ready) {
            m_elog->write(log::elevel::library,
                "asio::listen called from the wrong state");
            using websocketpp::error::make_error_code;
            ec = make_error_code(websocketpp::error::invalid_state);
            return;
        }

        m_alog->write(log::alevel::devel,"asio::listen");

        boost::system::error_code bec;

        m_acceptor->open(ep.protocol(),bec);
        if (!bec) {
            m_acceptor->set_option(boost::asio::socket_base::reuse_address(m_reuse_addr),bec);
        }
        if (!bec) {
            m_acceptor->bind(ep,bec);
        }
        if (!bec) {
            m_acceptor->listen(m_listen_backlog,bec);
        }
        if (bec) {
            log_err(log::elevel::info,"asio listen",bec);
            ec = make_error_code(error::pass_through);
        } else {
            m_state = listening;
            ec = lib::error_code();
        }
    }

    /// set up endpoint for listening manually
    /**
     * bind the internal acceptor using the settings specified by the endpoint e
     *
     * @param ep an endpoint to read settings from
     */
    void listen(boost::asio::ip::tcp::endpoint const & ep) {
        lib::error_code ec;
        listen(ep,ec);
        if (ec) { throw exception(ec); }
    }

    /// set up endpoint for listening with protocol and port (exception free)
    /**
     * bind the internal acceptor using the given internet protocol and port.
     * the endpoint must have been initialized by calling init_asio before
     * listening.
     *
     * common options include:
     * - ipv6 with mapped ipv4 for dual stack hosts boost::asio::ip::tcp::v6()
     * - ipv4 only: boost::asio::ip::tcp::v4()
     *
     * @param internet_protocol the internet protocol to use.
     * @param port the port to listen on.
     * @param ec set to indicate what error occurred, if any.
     */
    template <typename internetprotocol>
    void listen(internetprotocol const & internet_protocol, uint16_t port,
        lib::error_code & ec)
    {
        boost::asio::ip::tcp::endpoint ep(internet_protocol, port);
        listen(ep,ec);
    }

    /// set up endpoint for listening with protocol and port
    /**
     * bind the internal acceptor using the given internet protocol and port.
     * the endpoint must have been initialized by calling init_asio before
     * listening.
     *
     * common options include:
     * - ipv6 with mapped ipv4 for dual stack hosts boost::asio::ip::tcp::v6()
     * - ipv4 only: boost::asio::ip::tcp::v4()
     *
     * @param internet_protocol the internet protocol to use.
     * @param port the port to listen on.
     */
    template <typename internetprotocol>
    void listen(internetprotocol const & internet_protocol, uint16_t port)
    {
        boost::asio::ip::tcp::endpoint ep(internet_protocol, port);
        listen(ep);
    }

    /// set up endpoint for listening on a port (exception free)
    /**
     * bind the internal acceptor using the given port. the ipv6 protocol with
     * mapped ipv4 for dual stack hosts will be used. if you need ipv4 only use
     * the overload that allows specifying the protocol explicitly.
     *
     * the endpoint must have been initialized by calling init_asio before
     * listening.
     *
     * @param port the port to listen on.
     * @param ec set to indicate what error occurred, if any.
     */
    void listen(uint16_t port, lib::error_code & ec) {
        listen(boost::asio::ip::tcp::v6(), port, ec);
    }

    /// set up endpoint for listening on a port
    /**
     * bind the internal acceptor using the given port. the ipv6 protocol with
     * mapped ipv4 for dual stack hosts will be used. if you need ipv4 only use
     * the overload that allows specifying the protocol explicitly.
     *
     * the endpoint must have been initialized by calling init_asio before
     * listening.
     *
     * @param port the port to listen on.
     * @param ec set to indicate what error occurred, if any.
     */
    void listen(uint16_t port) {
        listen(boost::asio::ip::tcp::v6(), port);
    }

    /// set up endpoint for listening on a host and service (exception free)
    /**
     * bind the internal acceptor using the given host and service. more details
     * about what host and service can be are available in the boost asio
     * documentation for ip::basic_resolver_query::basic_resolver_query's
     * constructors.
     *
     * the endpoint must have been initialized by calling init_asio before
     * listening.
     *
     * @param host a string identifying a location. may be a descriptive name or
     * a numeric address string.
     * @param service a string identifying the requested service. this may be a
     * descriptive name or a numeric string corresponding to a port number.
     * @param ec set to indicate what error occurred, if any.
     */
    void listen(std::string const & host, std::string const & service,
        lib::error_code & ec)
    {
        using boost::asio::ip::tcp;
        tcp::resolver r(*m_io_service);
        tcp::resolver::query query(host, service);
        tcp::resolver::iterator endpoint_iterator = r.resolve(query);
        tcp::resolver::iterator end;
        if (endpoint_iterator == end) {
            m_elog->write(log::elevel::library,
                "asio::listen could not resolve the supplied host or service");
            ec = make_error_code(error::invalid_host_service);
            return;
        }
        listen(*endpoint_iterator,ec);
    }

    /// set up endpoint for listening on a host and service
    /**
     * bind the internal acceptor using the given host and service. more details
     * about what host and service can be are available in the boost asio
     * documentation for ip::basic_resolver_query::basic_resolver_query's
     * constructors.
     *
     * the endpoint must have been initialized by calling init_asio before
     * listening.
     *
     * @param host a string identifying a location. may be a descriptive name or
     * a numeric address string.
     * @param service a string identifying the requested service. this may be a
     * descriptive name or a numeric string corresponding to a port number.
     * @param ec set to indicate what error occurred, if any.
     */
    void listen(std::string const & host, std::string const & service)
    {
        lib::error_code ec;
        listen(host,service,ec);
        if (ec) { throw exception(ec); }
    }

    /// stop listening (exception free)
    /**
     * stop listening and accepting new connections. this will not end any
     * existing connections.
     *
     * @since 0.3.0-alpha4
     * @param ec a status code indicating an error, if any.
     */
    void stop_listening(lib::error_code & ec) {
        if (m_state != listening) {
            m_elog->write(log::elevel::library,
                "asio::listen called from the wrong state");
            using websocketpp::error::make_error_code;
            ec = make_error_code(websocketpp::error::invalid_state);
            return;
        }

        m_acceptor->close();
        m_state = ready;
        ec = lib::error_code();
    }

    /// stop listening
    /**
     * stop listening and accepting new connections. this will not end any
     * existing connections.
     *
     * @since 0.3.0-alpha4
     */
    void stop_listening() {
        lib::error_code ec;
        stop_listening(ec);
        if (ec) { throw exception(ec); }
    }

    /// check if the endpoint is listening
    /**
     * @return whether or not the endpoint is listening.
     */
    bool is_listening() const {
        return (m_state == listening);
    }

    /// wraps the run method of the internal io_service object
    std::size_t run() {
        return m_io_service->run();
    }

    /// wraps the run_one method of the internal io_service object
    /**
     * @since 0.3.0-alpha4
     */
    std::size_t run_one() {
        return m_io_service->run_one();
    }

    /// wraps the stop method of the internal io_service object
    void stop() {
        m_io_service->stop();
    }

    /// wraps the poll method of the internal io_service object
    std::size_t poll() {
        return m_io_service->poll();
    }

    /// wraps the poll_one method of the internal io_service object
    std::size_t poll_one() {
        return m_io_service->poll_one();
    }

    /// wraps the reset method of the internal io_service object
    void reset() {
        m_io_service->reset();
    }

    /// wraps the stopped method of the internal io_service object
    bool stopped() const {
        return m_io_service->stopped();
    }

    /// marks the endpoint as perpetual, stopping it from exiting when empty
    /**
     * marks the endpoint as perpetual. perpetual endpoints will not
     * automatically exit when they run out of connections to process. to stop
     * a perpetual endpoint call `end_perpetual`.
     *
     * an endpoint may be marked perpetual at any time by any thread. it must be
     * called either before the endpoint has run out of work or before it was
     * started
     *
     * @since 0.3.0
     */
    void start_perpetual() {
        m_work = lib::make_shared<boost::asio::io_service::work>(
            lib::ref(*m_io_service)
        );
    }

    /// clears the endpoint's perpetual flag, allowing it to exit when empty
    /**
     * clears the endpoint's perpetual flag. this will cause the endpoint's run
     * method to exit normally when it runs out of connections. if there are
     * currently active connections it will not end until they are complete.
     *
     * @since 0.3.0
     */
    void stop_perpetual() {
        m_work.reset();
    }

    /// call back a function after a period of time.
    /**
     * sets a timer that calls back a function after the specified period of
     * milliseconds. returns a handle that can be used to cancel the timer.
     * a cancelled timer will return the error code error::operation_aborted
     * a timer that expired will return no error.
     *
     * @param duration length of time to wait in milliseconds
     * @param callback the function to call back when the timer has expired
     * @return a handle that can be used to cancel the timer if it is no longer
     * needed.
     */
    timer_ptr set_timer(long duration, timer_handler callback) {
        timer_ptr new_timer = lib::make_shared<boost::asio::deadline_timer>(
            *m_io_service,
            boost::posix_time::milliseconds(duration)
        );

        new_timer->async_wait(
            lib::bind(
                &type::handle_timer,
                this,
                new_timer,
                callback,
                lib::placeholders::_1
            )
        );

        return new_timer;
    }

    /// timer handler
    /**
     * the timer pointer is included to ensure the timer isn't destroyed until
     * after it has expired.
     *
     * @param t pointer to the timer in question
     * @param callback the function to call back
     * @param ec a status code indicating an error, if any.
     */
    void handle_timer(timer_ptr, timer_handler callback,
        boost::system::error_code const & ec)
    {
        if (ec) {
            if (ec == boost::asio::error::operation_aborted) {
                callback(make_error_code(transport::error::operation_aborted));
            } else {
                m_elog->write(log::elevel::info,
                    "asio handle_timer error: "+ec.message());
                log_err(log::elevel::info,"asio handle_timer",ec);
                callback(make_error_code(error::pass_through));
            }
        } else {
            callback(lib::error_code());
        }
    }

    /// accept the next connection attempt and assign it to con (exception free)
    /**
     * @param tcon the connection to accept into.
     * @param callback the function to call when the operation is complete.
     * @param ec a status code indicating an error, if any.
     */
    void async_accept(transport_con_ptr tcon, accept_handler callback,
        lib::error_code & ec)
    {
        if (m_state != listening) {
            using websocketpp::error::make_error_code;
            ec = make_error_code(websocketpp::error::async_accept_not_listening);
            return;
        }

        m_alog->write(log::alevel::devel, "asio::async_accept");

        if (config::enable_multithreading) {
            m_acceptor->async_accept(
                tcon->get_raw_socket(),
                tcon->get_strand()->wrap(lib::bind(
                    &type::handle_accept,
                    this,
                    callback,
                    lib::placeholders::_1
                ))
            );
        } else {
            m_acceptor->async_accept(
                tcon->get_raw_socket(),
                lib::bind(
                    &type::handle_accept,
                    this,
                    callback,
                    lib::placeholders::_1
                )
            );
        }
    }

    /// accept the next connection attempt and assign it to con.
    /**
     * @param tcon the connection to accept into.
     * @param callback the function to call when the operation is complete.
     */
    void async_accept(transport_con_ptr tcon, accept_handler callback) {
        lib::error_code ec;
        async_accept(tcon,callback,ec);
        if (ec) { throw exception(ec); }
    }
protected:
    /// initialize logging
    /**
     * the loggers are located in the main endpoint class. as such, the
     * transport doesn't have direct access to them. this method is called
     * by the endpoint constructor to allow shared logging from the transport
     * component. these are raw pointers to member variables of the endpoint.
     * in particular, they cannot be used in the transport constructor as they
     * haven't been constructed yet, and cannot be used in the transport
     * destructor as they will have been destroyed by then.
     */
    void init_logging(alog_type* a, elog_type* e) {
        m_alog = a;
        m_elog = e;
    }

    void handle_accept(accept_handler callback, boost::system::error_code const
        & boost_ec)
    {
        lib::error_code ret_ec;

        m_alog->write(log::alevel::devel, "asio::handle_accept");

        if (boost_ec) {
            if (boost_ec == boost::system::errc::operation_canceled) {
                ret_ec = make_error_code(websocketpp::error::operation_canceled);
            } else {
                log_err(log::elevel::info,"asio handle_accept",boost_ec);
                ret_ec = make_error_code(error::pass_through);
            }
        }

        callback(ret_ec);
    }

    /// initiate a new connection
    // todo: there have to be some more failure conditions here
    void async_connect(transport_con_ptr tcon, uri_ptr u, connect_handler cb) {
        using namespace boost::asio::ip;

        // create a resolver
        if (!m_resolver) {
            m_resolver = lib::make_shared<boost::asio::ip::tcp::resolver>(
                lib::ref(*m_io_service));
        }

        std::string proxy = tcon->get_proxy();
        std::string host;
        std::string port;

        if (proxy.empty()) {
            host = u->get_host();
            port = u->get_port_str();
        } else {
            lib::error_code ec;

            uri_ptr pu = lib::make_shared<uri>(proxy);

            if (!pu->get_valid()) {
                cb(make_error_code(error::proxy_invalid));
                return;
            }

            ec = tcon->proxy_init(u->get_authority());
            if (ec) {
                cb(ec);
                return;
            }

            host = pu->get_host();
            port = pu->get_port_str();
        }

        tcp::resolver::query query(host,port);

        if (m_alog->static_test(log::alevel::devel)) {
            m_alog->write(log::alevel::devel,
                "starting async dns resolve for "+host+":"+port);
        }

        timer_ptr dns_timer;

        dns_timer = tcon->set_timer(
            config::timeout_dns_resolve,
            lib::bind(
                &type::handle_resolve_timeout,
                this,
                dns_timer,
                cb,
                lib::placeholders::_1
            )
        );

        if (config::enable_multithreading) {
            m_resolver->async_resolve(
                query,
                tcon->get_strand()->wrap(lib::bind(
                    &type::handle_resolve,
                    this,
                    tcon,
                    dns_timer,
                    cb,
                    lib::placeholders::_1,
                    lib::placeholders::_2
                ))
            );
        } else {
            m_resolver->async_resolve(
                query,
                lib::bind(
                    &type::handle_resolve,
                    this,
                    tcon,
                    dns_timer,
                    cb,
                    lib::placeholders::_1,
                    lib::placeholders::_2
                )
            );
        }
    }

    /// dns resolution timeout handler
    /**
     * the timer pointer is included to ensure the timer isn't destroyed until
     * after it has expired.
     *
     * @param dns_timer pointer to the timer in question
     * @param callback the function to call back
     * @param ec a status code indicating an error, if any.
     */
    void handle_resolve_timeout(timer_ptr, connect_handler callback,
        lib::error_code const & ec)
    {
        lib::error_code ret_ec;

        if (ec) {
            if (ec == transport::error::operation_aborted) {
                m_alog->write(log::alevel::devel,
                    "asio handle_resolve_timeout timer cancelled");
                return;
            }

            log_err(log::elevel::devel,"asio handle_resolve_timeout",ec);
            ret_ec = ec;
        } else {
            ret_ec = make_error_code(transport::error::timeout);
        }

        m_alog->write(log::alevel::devel,"dns resolution timed out");
        m_resolver->cancel();
        callback(ret_ec);
    }

    void handle_resolve(transport_con_ptr tcon, timer_ptr dns_timer,
        connect_handler callback, boost::system::error_code const & ec,
        boost::asio::ip::tcp::resolver::iterator iterator)
    {
        if (ec == boost::asio::error::operation_aborted ||
            dns_timer->expires_from_now().is_negative())
        {
            m_alog->write(log::alevel::devel,"async_resolve cancelled");
            return;
        }

        dns_timer->cancel();

        if (ec) {
            log_err(log::elevel::info,"asio async_resolve",ec);
            callback(make_error_code(error::pass_through));
            return;
        }

        if (m_alog->static_test(log::alevel::devel)) {
            std::stringstream s;
            s << "async dns resolve successful. results: ";

            boost::asio::ip::tcp::resolver::iterator it, end;
            for (it = iterator; it != end; ++it) {
                s << (*it).endpoint() << " ";
            }

            m_alog->write(log::alevel::devel,s.str());
        }

        m_alog->write(log::alevel::devel,"starting async connect");

        timer_ptr con_timer;

        con_timer = tcon->set_timer(
            config::timeout_connect,
            lib::bind(
                &type::handle_connect_timeout,
                this,
                tcon,
                con_timer,
                callback,
                lib::placeholders::_1
            )
        );

        if (config::enable_multithreading) {
            boost::asio::async_connect(
                tcon->get_raw_socket(),
                iterator,
                tcon->get_strand()->wrap(lib::bind(
                    &type::handle_connect,
                    this,
                    tcon,
                    con_timer,
                    callback,
                    lib::placeholders::_1
                ))
            );
        } else {
            boost::asio::async_connect(
                tcon->get_raw_socket(),
                iterator,
                lib::bind(
                    &type::handle_connect,
                    this,
                    tcon,
                    con_timer,
                    callback,
                    lib::placeholders::_1
                )
            );
        }
    }

    /// asio connect timeout handler
    /**
     * the timer pointer is included to ensure the timer isn't destroyed until
     * after it has expired.
     *
     * @param tcon pointer to the transport connection that is being connected
     * @param con_timer pointer to the timer in question
     * @param callback the function to call back
     * @param ec a status code indicating an error, if any.
     */
    void handle_connect_timeout(transport_con_ptr tcon, timer_ptr,
        connect_handler callback, lib::error_code const & ec)
    {
        lib::error_code ret_ec;

        if (ec) {
            if (ec == transport::error::operation_aborted) {
                m_alog->write(log::alevel::devel,
                    "asio handle_connect_timeout timer cancelled");
                return;
            }

            log_err(log::elevel::devel,"asio handle_connect_timeout",ec);
            ret_ec = ec;
        } else {
            ret_ec = make_error_code(transport::error::timeout);
        }

        m_alog->write(log::alevel::devel,"tcp connect timed out");
        tcon->cancel_socket();
        callback(ret_ec);
    }

    void handle_connect(transport_con_ptr tcon, timer_ptr con_timer,
        connect_handler callback, boost::system::error_code const & ec)
    {
        if (ec == boost::asio::error::operation_aborted ||
            con_timer->expires_from_now().is_negative())
        {
            m_alog->write(log::alevel::devel,"async_connect cancelled");
            return;
        }

        con_timer->cancel();

        if (ec) {
            log_err(log::elevel::info,"asio async_connect",ec);
            callback(make_error_code(error::pass_through));
            return;
        }

        if (m_alog->static_test(log::alevel::devel)) {
            m_alog->write(log::alevel::devel,
                "async connect to "+tcon->get_remote_endpoint()+" successful.");
        }

        callback(lib::error_code());
    }

    /// initialize a connection
    /**
     * init is called by an endpoint once for each newly created connection.
     * it's purpose is to give the transport policy the chance to perform any
     * transport specific initialization that couldn't be done via the default
     * constructor.
     *
     * @param tcon a pointer to the transport portion of the connection.
     *
     * @return a status code indicating the success or failure of the operation
     */
    lib::error_code init(transport_con_ptr tcon) {
        m_alog->write(log::alevel::devel, "transport::asio::init");

        // initialize the connection socket component
        socket_type::init(lib::static_pointer_cast<socket_con_type,
            transport_con_type>(tcon));

        lib::error_code ec;

        ec = tcon->init_asio(m_io_service);
        if (ec) {return ec;}

        tcon->set_tcp_pre_init_handler(m_tcp_pre_init_handler);
        tcon->set_tcp_post_init_handler(m_tcp_post_init_handler);

        return lib::error_code();
    }
private:
    /// convenience method for logging the code and message for an error_code
    template <typename error_type>
    void log_err(log::level l, char const * msg, error_type const & ec) {
        std::stringstream s;
        s << msg << " error: " << ec << " (" << ec.message() << ")";
        m_elog->write(l,s.str());
    }

    enum state {
        uninitialized = 0,
        ready = 1,
        listening = 2
    };

    // handlers
    tcp_init_handler    m_tcp_pre_init_handler;
    tcp_init_handler    m_tcp_post_init_handler;

    // network resources
    io_service_ptr      m_io_service;
    bool                m_external_io_service;
    acceptor_ptr        m_acceptor;
    resolver_ptr        m_resolver;
    work_ptr            m_work;

    // network constants
    int                 m_listen_backlog;
    bool                m_reuse_addr;

    elog_type* m_elog;
    alog_type* m_alog;

    // transport state
    state               m_state;
};

} // namespace asio
} // namespace transport
} // namespace websocketpp

#endif // websocketpp_transport_asio_hpp
