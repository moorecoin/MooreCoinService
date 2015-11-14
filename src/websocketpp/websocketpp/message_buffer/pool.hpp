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

#ifndef websocketpp_message_buffer_alloc_hpp
#define websocketpp_message_buffer_alloc_hpp

#include <websocketpp/common/memory.hpp>

#include <string>

namespace websocketpp {
namespace message_buffer {

/* # message:
 * object that stores a message while it is being sent or received. contains
 * the message payload itself, the message header, the extension data, and the
 * opcode.
 *
 * # connection_message_manager:
 * an object that manages all of the message_buffers associated with a given
 * connection. impliments the get_message_buffer(size) method that returns
 * a message buffer at least size bytes long.
 *
 * message buffers are reference counted with shared ownership semantics. once
 * requested from the manager the requester and it's associated downstream code
 * may keep a pointer to the message indefinitely at a cost of extra resource
 * usage. once the reference count drops to the point where the manager is the
 * only reference the messages is recycled using whatever method is implemented
 * in the manager.
 *
 * # endpoint_message_manager:
 * an object that manages connection_message_managers. impliments the
 * get_message_manager() method. this is used once by each connection to
 * request the message manager that they are supposed to use to manage message
 * buffers for their own use.
 *
 * types of connection_message_managers
 * - allocate a message with the exact size every time one is requested
 * - maintain a pool of pre-allocated messages and return one when needed.
 *   recycle previously used messages back into the pool
 *
 * types of endpoint_message_managers
 *  - allocate a new connection manager for each connection. message pools
 *    become connection specific. this increases memory usage but improves
 *    concurrency.
 *  - allocate a single connection manager and share a pointer to it with all
 *    connections created by this endpoint. the message pool will be shared
 *    among all connections, improving memory usage and performance at the cost
 *    of reduced concurrency
 */

/// custom deleter for use in shared_ptrs to message.
/**
 * this is used to catch messages about to be deleted and offer the manager the
 * ability to recycle them instead. message::recycle will return true if it was
 * successfully recycled and false otherwise. in the case of exceptions or error
 * this deleter frees the memory.
 */
template <typename t>
void message_deleter(t* msg) {
    try {
        if (!msg->recycle()) {
            delete msg;
        }
    } catch (...) {
        // todo: is there a better way to ensure this function doesn't throw?
        delete msg;
    }
}

/// represents a buffer for a single websocket message.
/**
 *
 *
 */
template <typename con_msg_manager>
class message {
public:
    typedef lib::shared_ptr<message> ptr;

    typedef typename con_msg_manager::weak_ptr con_msg_man_ptr;

    message(con_msg_man_ptr manager, size_t size = 128)
      : m_manager(manager)
      , m_payload(size) {}

    frame::opcode::value get_opcode() const {
        return m_opcode;
    }
    const std::string& get_header() const {
        return m_header;
    }
    const std::string& get_extension_data() const {
        return m_extension_data;
    }
    const std::string& get_payload() const {
        return m_payload;
    }

    /// recycle the message
    /**
     * a request to recycle this message was received. forward that request to
     * the connection message manager for processing. errors and exceptions
     * from the manager's recycle member function should be passed back up the
     * call chain. the caller to message::recycle will deal with them.
     *
     * recycle must *only* be called by the message shared_ptr's destructor.
     * once recycled successfully, ownership of the memory has been passed to
     * another system and must not be accessed again.
     *
     * @return true if the message was successfully recycled, false otherwise.
     */
    bool recycle() {
        typename con_msg_manager::ptr shared = m_manager.lock();

        if (shared) {
            return shared->(recycle(this));
        } else {
            return false;
        }
    }
private:
    con_msg_man_ptr             m_manager;

    frame::opcode::value        m_opcode;
    std::string                 m_header;
    std::string                 m_extension_data;
    std::string                 m_payload;
};

namespace alloc {

/// a connection message manager that allocates a new message for each
/// request.
template <typename message>
class con_msg_manager {
public:
    typedef lib::shared_ptr<con_msg_manager> ptr;
    typedef lib::weak_ptr<con_msg_manager> weak_ptr;

    typedef typename message::ptr message_ptr;

    /// get a message buffer with specified size
    /**
     * @param size minimum size in bytes to request for the message payload.
     *
     * @return a shared pointer to a new message with specified size.
     */
    message_ptr get_message(size_t size) const {
        return lib::make_shared<message>(size);
    }

    /// recycle a message
    /**
     * this method shouldn't be called. if it is, return false to indicate an
     * error. the rest of the method recycle chain should notice this and free
     * the memory.
     *
     * @param msg the message to be recycled.
     *
     * @return true if the message was successfully recycled, false otherwse.
     */
    bool recycle(message * msg) {
        return false;
    }
};

/// an endpoint message manager that allocates a new manager for each
/// connection.
template <typename con_msg_manager>
class endpoint_msg_manager {
public:
    typedef typename con_msg_manager::ptr con_msg_man_ptr;

    /// get a pointer to a connection message manager
    /**
     * @return a pointer to the requested connection message manager.
     */
    con_msg_man_ptr get_manager() const {
        return lib::make_shared<con_msg_manager>();
    }
};

} // namespace alloc

namespace pool {

/// a connection messages manager that maintains a pool of messages that is
/// used to fulfill get_message requests.
class con_msg_manager {

};

/// an endpoint manager that maintains a shared pool of connection managers
/// and returns an appropriate one for the requesting connection.
class endpoint_msg_manager {

};

} // namespace pool

} // namespace message_buffer
} // namespace websocketpp

#endif // websocketpp_message_buffer_alloc_hpp
