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
#include <websocketpp/frame.hpp>

namespace websocketpp {
namespace message_buffer {
namespace alloc {

/// a connection message manager that allocates a new message for each
/// request.
template <typename message>
class con_msg_manager
  : public lib::enable_shared_from_this<con_msg_manager<message> >
{
public:
    typedef con_msg_manager<message> type;
    typedef lib::shared_ptr<con_msg_manager> ptr;
    typedef lib::weak_ptr<con_msg_manager> weak_ptr;

    typedef typename message::ptr message_ptr;

    /// get an empty message buffer
    /**
     * @return a shared pointer to an empty new message
     */
    message_ptr get_message() {
        return message_ptr(lib::make_shared<message>(type::shared_from_this()));
    }

    /// get a message buffer with specified size and opcode
    /**
     * @param op the opcode to use
     * @param size minimum size in bytes to request for the message payload.
     *
     * @return a shared pointer to a new message with specified size.
     */
    message_ptr get_message(frame::opcode::value op,size_t size) {
        return message_ptr(lib::make_shared<message>(type::shared_from_this(),op,size));
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
    bool recycle(message *) {
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
        return con_msg_man_ptr(lib::make_shared<con_msg_manager>());
    }
};

} // namespace alloc
} // namespace message_buffer
} // namespace websocketpp

#endif // websocketpp_message_buffer_alloc_hpp
