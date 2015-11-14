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
#define boost_test_module message
#include <boost/test/unit_test.hpp>

#include <iostream>
#include <string>

#include <websocketpp/message_buffer/message.hpp>

template <typename message>
struct stub {
    typedef websocketpp::lib::weak_ptr<stub> weak_ptr;
    typedef websocketpp::lib::shared_ptr<stub> ptr;

    stub() : recycled(false) {}

    bool recycle(message *) {
        this->recycled = true;
        return false;
    }

    bool recycled;
};

boost_auto_test_case( basic_size_check ) {
    typedef websocketpp::message_buffer::message<stub> message_type;
    typedef stub<message_type> stub_type;

    stub_type::ptr s(new stub_type());
    message_type::ptr msg(new message_type(s,websocketpp::frame::opcode::text,500));

    boost_check(msg->get_payload().capacity() >= 500);
}

boost_auto_test_case( recycle ) {
    typedef websocketpp::message_buffer::message<stub> message_type;
    typedef stub<message_type> stub_type;

    stub_type::ptr s(new stub_type());
    message_type::ptr msg(new message_type(s,websocketpp::frame::opcode::text,500));

    boost_check(s->recycled == false);
    boost_check(msg->recycle() == false);
    boost_check(s->recycled == true);
}

