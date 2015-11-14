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
//#define boost_test_dyn_link
#define boost_test_module message_buffer_alloc
#include <boost/test/unit_test.hpp>

#include <iostream>
#include <string>

#include <websocketpp/message_buffer/alloc.hpp>

template <template <class> class con_msg_manager>
struct stub {
    typedef websocketpp::lib::shared_ptr<stub> ptr;

    typedef con_msg_manager<stub> con_msg_man_type;
    typedef typename con_msg_man_type::ptr con_msg_man_ptr;
    typedef typename con_msg_man_type::weak_ptr con_msg_man_weak_ptr;

    stub(con_msg_man_ptr manager, websocketpp::frame::opcode::value op, size_t size = 128)
      : m_opcode(op)
      , m_manager(manager)
      , m_size(size) {}

    bool recycle() {
        con_msg_man_ptr shared = m_manager.lock();

        if (shared) {
            return shared->recycle(this);
        } else {
            return false;
        }
    }

    websocketpp::frame::opcode::value   m_opcode;
    con_msg_man_weak_ptr                m_manager;
    size_t                              m_size;
};

boost_auto_test_case( basic_get_message ) {
    typedef stub<websocketpp::message_buffer::alloc::con_msg_manager>
        message_type;
    typedef websocketpp::message_buffer::alloc::con_msg_manager<message_type>
        con_msg_man_type;

    con_msg_man_type::ptr manager(new con_msg_man_type());
    message_type::ptr msg = manager->get_message(websocketpp::frame::opcode::text,512);

    boost_check(msg);
    boost_check(msg->m_opcode == websocketpp::frame::opcode::text);
    boost_check(msg->m_manager.lock() == manager);
    boost_check(msg->m_size == 512);
}

boost_auto_test_case( basic_get_manager ) {
    typedef stub<websocketpp::message_buffer::alloc::con_msg_manager>
        message_type;
    typedef websocketpp::message_buffer::alloc::con_msg_manager<message_type>
        con_msg_man_type;
    typedef websocketpp::message_buffer::alloc::endpoint_msg_manager
        <con_msg_man_type> endpoint_manager_type;

    endpoint_manager_type em;
    con_msg_man_type::ptr manager = em.get_manager();
    message_type::ptr msg = manager->get_message(websocketpp::frame::opcode::text,512);

    boost_check(msg);
    boost_check(msg->m_opcode == websocketpp::frame::opcode::text);
    boost_check(msg->m_manager.lock() == manager);
    boost_check(msg->m_size == 512);
}

