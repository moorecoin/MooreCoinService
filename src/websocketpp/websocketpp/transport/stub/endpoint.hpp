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

#ifndef websocketpp_transport_stub_hpp
#define websocketpp_transport_stub_hpp

#include <websocketpp/common/memory.hpp>
#include <websocketpp/logger/levels.hpp>

#include <websocketpp/transport/base/endpoint.hpp>
#include <websocketpp/transport/stub/connection.hpp>

namespace websocketpp {
namespace transport {
namespace stub {

template <typename config>
class endpoint {
public:
    /// type of this endpoint transport component
    typedef endpoint type;
    /// type of a pointer to this endpoint transport component
    typedef lib::shared_ptr<type> ptr;

    /// type of this endpoint's concurrency policy
    typedef typename config::concurrency_type concurrency_type;
    /// type of this endpoint's error logging policy
    typedef typename config::elog_type elog_type;
    /// type of this endpoint's access logging policy
    typedef typename config::alog_type alog_type;

    /// type of this endpoint transport component's associated connection
    /// transport component.
    typedef iostream::connection<config> transport_con_type;
    /// type of a shared pointer to this endpoint transport component's
    /// associated connection transport component
    typedef typename transport_con_type::ptr transport_con_ptr;

    // generate and manage our own io_service
    explicit endpoint()
    {
        //std::cout << "transport::iostream::endpoint constructor" << std::endl;
    }

    /// set whether or not endpoint can create secure connections
    /**
     * todo: docs
     *
     * setting this value only indicates whether or not the endpoint is capable
     * of producing and managing secure connections. connections produced by
     * this endpoint must also be individually flagged as secure if they are.
     *
     * @since 0.3.0-alpha4
     *
     * @param value whether or not the endpoint can create secure connections.
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
     *
     * @param a a pointer to the access logger to use.
     * @param e a pointer to the error logger to use.
     */
    void init_logging(alog_type * a, elog_type * e) {}

    /// initiate a new connection
    /**
     * @param tcon a pointer to the transport connection component of the
     * connection to connect.
     * @param u a uri pointer to the uri to connect to.
     * @param cb the function to call back with the results when complete.
     */
    void async_connect(transport_con_ptr tcon, uri_ptr u, connect_handler cb) {
        cb(make_error_code(error::not_implimented));
    }

    /// initialize a connection
    /**
     * init is called by an endpoint once for each newly created connection.
     * it's purpose is to give the transport policy the chance to perform any
     * transport specific initialization that couldn't be done via the default
     * constructor.
     *
     * @param tcon a pointer to the transport portion of the connection.
     * @return a status code indicating the success or failure of the operation
     */
    lib::error_code init(transport_con_ptr tcon) {
        cb(make_error_code(error::not_implimented));
    }
private:

};

} // namespace stub
} // namespace transport
} // namespace websocketpp

#endif // websocketpp_transport_stub_hpp
