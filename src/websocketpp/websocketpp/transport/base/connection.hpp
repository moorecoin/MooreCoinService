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

#ifndef websocketpp_transport_base_con_hpp
#define websocketpp_transport_base_con_hpp

#include <websocketpp/common/cpp11.hpp>
#include <websocketpp/common/connection_hdl.hpp>
#include <websocketpp/common/functional.hpp>
#include <websocketpp/common/memory.hpp>
#include <websocketpp/common/system_error.hpp>

namespace websocketpp {
/// transport policies provide network connectivity and timers
/**
 * ### connection interface
 *
 * transport connection components needs to provide:
 *
 * **init**\n
 * `void init(init_handler handler)`\n
 * called once shortly after construction to give the policy the chance to
 * perform one time initialization. when complete, the policy must call the
 * supplied `init_handler` to continue setup. the handler takes one argument
 * with the error code if any. if an error is returned here setup will fail and
 * the connection will be aborted or terminated.
 *
 * websocket++ will call init only once. the transport must call `handler`
 * exactly once.
 *
 * **async_read_at_least**\n
 * `void async_read_at_least(size_t num_bytes, char *buf, size_t len,
 * read_handler handler)`\n
 * start an async read for at least num_bytes and at most len
 * bytes into buf. call handler when done with number of bytes read.
 *
 * websocket++ promises to have only one async_read_at_least in flight at a
 * time. the transport must promise to only call read_handler once per async
 * read.
 *
 * **async_write**\n
 * `void async_write(const char* buf, size_t len, write_handler handler)`\n
 * `void async_write(std::vector<buffer> & bufs, write_handler handler)`\n
 * start a write of all of the data in buf or bufs. in second case data is
 * written sequentially and in place without copying anything to a temporary
 * location.
 *
 * websocket++ promises to have only one async_write in flight at a time.
 * the transport must promise to only call the write_handler once per async
 * write
 *
 * **set_handle**\n
 * `void set_handle(connection_hdl hdl)`\n
 * called by websocket++ to let this policy know the hdl to the connection. it
 * may be stored for later use or ignored/discarded. this handle should be used
 * if the policy adds any connection handlers. connection handlers must be
 * called with the handle as the first argument so that the handler code knows
 * which connection generated the callback.
 *
 * **set_timer**\n
 * `timer_ptr set_timer(long duration, timer_handler handler)`\n
 * websocket++ uses the timers provided by the transport policy as the
 * implementation of timers is often highly coupled with the implementation of
 * the networking event loops.
 *
 * transport timer support is an optional feature. a transport method may elect
 * to implement a dummy timer object and have this method return an empty
 * pointer. if so, all timer related features of websocket++ core will be
 * disabled. this includes many security features designed to prevent denial of
 * service attacks. use timer-free transport policies with caution.
 *
 * **get_remote_endpoint**\n
 * `std::string get_remote_endpoint()`\n
 * retrieve address of remote endpoint
 *
 * **is_secure**\n
 * `void is_secure()`\n
 * whether or not the connection to the remote endpoint is secure
 *
 * **dispatch**\n
 * `lib::error_code dispatch(dispatch_handler handler)`: invoke handler within
 * the transport's event system if it uses one. otherwise, this method should
 * simply call `handler` immediately.
 *
 * **async_shutdown**\n
 * `void async_shutdown(shutdown_handler handler)`\n
 * perform any cleanup necessary (if any). call `handler` when complete.
 */
namespace transport {

/// the type and signature of the callback passed to the init hook
typedef lib::function<void(lib::error_code const &)> init_handler;

/// the type and signature of the callback passed to the read method
typedef lib::function<void(lib::error_code const &,size_t)> read_handler;

/// the type and signature of the callback passed to the write method
typedef lib::function<void(lib::error_code const &)> write_handler;

/// the type and signature of the callback passed to the read method
typedef lib::function<void(lib::error_code const &)> timer_handler;

/// the type and signature of the callback passed to the shutdown method
typedef lib::function<void(lib::error_code const &)> shutdown_handler;

/// the type and signature of the callback passed to the interrupt method
typedef lib::function<void()> interrupt_handler;

/// the type and signature of the callback passed to the dispatch method
typedef lib::function<void()> dispatch_handler;

/// a simple utility buffer class
struct buffer {
    buffer(char const * b, size_t l) : buf(b),len(l) {}

    char const * buf;
    size_t len;
};

/// generic transport related errors
namespace error {
enum value {
    /// catch-all error for transport policy errors that don't fit in other
    /// categories
    general = 1,

    /// underlying transport pass through
    pass_through,

    /// async_read_at_least call requested more bytes than buffer can store
    invalid_num_bytes,

    /// async_read called while another async_read was in progress
    double_read,

    /// operation aborted
    operation_aborted,

    /// operation not supported
    operation_not_supported,

    /// end of file
    eof,

    /// tls short read
    tls_short_read,

    /// timer expired
    timeout,

    /// read or write after shutdown
    action_after_shutdown,

    /// other tls error
    tls_error,
};

class category : public lib::error_category {
    public:
    category() {}

    char const * name() const _websocketpp_noexcept_token_ {
        return "websocketpp.transport";
    }

    std::string message(int value) const {
        switch(value) {
            case general:
                return "generic transport policy error";
            case pass_through:
                return "underlying transport error";
            case invalid_num_bytes:
                return "async_read_at_least call requested more bytes than buffer can store";
            case operation_aborted:
                return "the operation was aborted";
            case operation_not_supported:
                return "the operation is not supported by this transport";
            case eof:
                return "end of file";
            case tls_short_read:
                return "tls short read";
            case timeout:
                return "timer expired";
            case action_after_shutdown:
                return "a transport action was requested after shutdown";
            case tls_error:
                return "generic tls related error";
            default:
                return "unknown";
        }
    }
};

inline lib::error_category const & get_category() {
    static category instance;
    return instance;
}

inline lib::error_code make_error_code(error::value e) {
    return lib::error_code(static_cast<int>(e), get_category());
}

} // namespace error
} // namespace transport
} // namespace websocketpp
_websocketpp_error_code_enum_ns_start_
template<> struct is_error_code_enum<websocketpp::transport::error::value>
{
    static bool const value = true;
};
_websocketpp_error_code_enum_ns_end_

#endif // websocketpp_transport_base_con_hpp
