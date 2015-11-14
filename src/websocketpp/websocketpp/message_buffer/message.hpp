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

#ifndef websocketpp_message_buffer_message_hpp
#define websocketpp_message_buffer_message_hpp

#include <websocketpp/common/memory.hpp>
#include <websocketpp/frame.hpp>

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


/// represents a buffer for a single websocket message.
/**
 *
 *
 */
template <template<class> class con_msg_manager>
class message {
public:
    typedef lib::shared_ptr<message> ptr;

    typedef con_msg_manager<message> con_msg_man_type;
    typedef typename con_msg_man_type::ptr con_msg_man_ptr;
    typedef typename con_msg_man_type::weak_ptr con_msg_man_weak_ptr;

    /// construct an empty message
    /**
     * construct an empty message
     */
    message(const con_msg_man_ptr manager)
      : m_manager(manager)
      , m_prepared(false)
      , m_fin(true)
      , m_terminal(false)
      , m_compressed(false) {}

    /// construct a message and fill in some values
    /**
     *
     */
    message(const con_msg_man_ptr manager, frame::opcode::value op, size_t size = 128)
      : m_manager(manager)
      , m_opcode(op)
      , m_prepared(false)
      , m_fin(true)
      , m_terminal(false)
      , m_compressed(false)
    {
        m_payload.reserve(size);
    }

    /// return whether or not the message has been prepared for sending
    /**
     * the prepared flag indicates that the message has been prepared by a
     * websocket protocol processor and is ready to be written to the wire.
     *
     * @return whether or not the message has been prepared for sending
     */
    bool get_prepared() const {
        return m_prepared;
    }

    /// set or clear the flag that indicates that the message has been prepared
    /**
     * this flag should not be set by end user code without a very good reason.
     *
     * @param value the value to set the prepared flag to
     */
    void set_prepared(bool value) {
        m_prepared = value;
    }

    /// return whether or not the message is flagged as compressed
    /**
     * @return whether or not the message is/should be compressed
     */
    bool get_compressed() const {
        return m_compressed;
    }

    /// set or clear the compression flag
    /**
     * the compression flag is used to indicate whether or not the message is
     * or should be compressed. compression is not guaranteed. both endpoints
     * must support a compression extension and the connection must have had
     * that extension negotiated in its handshake.
     *
     * @param value the value to set the compressed flag to
     */
    void set_compressed(bool value) {
        m_compressed = value;
    }

    /// get whether or not the message is terminal
    /**
     * messages can be flagged as terminal, which results in the connection
     * being close after they are written rather than the implementation going
     * on to the next message in the queue. this is typically used internally
     * for close messages only.
     *
     * @return whether or not this message is marked terminal
     */
    bool get_terminal() const {
        return m_terminal;
    }

    /// set the terminal flag
    /**
     * this flag should not be set by end user code without a very good reason.
     *
     * @see get_terminal()
     *
     * @param value the value to set the terminal flag to.
     */
    void set_terminal(bool value) {
        m_terminal = value;
    }
    /// read the fin bit
    /**
     * a message with the fin bit set will be sent as the last message of its
     * sequence. a message with the fin bit cleared will require subsequent
     * frames of opcode continuation until one of them has the fin bit set.
     *
     * the remote end likely will not deliver any bytes until the frame with the fin
     * bit set has been received.
     *
     * @return whether or not the fin bit is set
     */
    bool get_fin() const {
        return m_fin;
    }

    /// set the fin bit
    /**
     * @see get_fin for a more detailed explaination of the fin bit
     *
     * @param value the value to set the fin bit to.
     */
    void set_fin(bool value) {
        m_fin = value;
    }

    /// return the message opcode
    frame::opcode::value get_opcode() const {
        return m_opcode;
    }

    /// set the opcode
    void set_opcode(frame::opcode::value op) {
        m_opcode = op;
    }

    /// return the prepared frame header
    /**
     * this value is typically set by a websocket protocol processor
     * and shouldn't be tampered with.
     */
    std::string const & get_header() const {
        return m_header;
    }

    /// set prepared frame header
    /**
     * under normal circumstances this should not be called by end users
     *
     * @param header a string to set the header to.
     */
    void set_header(std::string const & header) {
        m_header = header;
    }

    std::string const & get_extension_data() const {
        return m_extension_data;
    }

    /// get a reference to the payload string
    /**
     * @return a const reference to the message's payload string
     */
    std::string const & get_payload() const {
        return m_payload;
    }

    /// get a non-const reference to the payload string
    /**
     * @return a reference to the message's payload string
     */
    std::string & get_raw_payload() {
        return m_payload;
    }

    /// set payload data
    /**
     * set the message buffer's payload to the given value.
     *
     * @param payload a string to set the payload to.
     */
    void set_payload(std::string const & payload) {
        m_payload = payload;
    }

    /// set payload data
    /**
     * set the message buffer's payload to the given value.
     *
     * @param payload a pointer to a data array to set to.
     * @param len the length of new payload in bytes.
     */
    void set_payload(void const * payload, size_t len) {
        m_payload.reserve(len);
        char const * pl = static_cast<char const *>(payload);
        m_payload.assign(pl, pl + len);
    }

    /// append payload data
    /**
     * append data to the message buffer's payload.
     *
     * @param payload a string containing the data array to append.
     */
    void append_payload(std::string const & payload) {
        m_payload.append(payload);
    }

    /// append payload data
    /**
     * append data to the message buffer's payload.
     *
     * @param payload a pointer to a data array to append
     * @param len the length of payload in bytes
     */
    void append_payload(void const * payload, size_t len) {
        m_payload.reserve(m_payload.size()+len);
        m_payload.append(static_cast<char const *>(payload),len);
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
        con_msg_man_ptr shared = m_manager.lock();

        if (shared) {
            return shared->recycle(this);
        } else {
            return false;
        }
    }
private:
    con_msg_man_weak_ptr        m_manager;
    std::string                 m_header;
    std::string                 m_extension_data;
    std::string                 m_payload;
    frame::opcode::value        m_opcode;
    bool                        m_prepared;
    bool                        m_fin;
    bool                        m_terminal;
    bool                        m_compressed;
};

} // namespace message_buffer
} // namespace websocketpp

#endif // websocketpp_message_buffer_message_hpp
