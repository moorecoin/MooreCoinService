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
 
#include "generic.hpp"
 
using wsperf::message_test;

// construct a message_test from a wscmd command
/* reads values from the wscmd object into member variables. the cmd object is
 * passed to the parent constructor for extracting values common to all test
 * cases.
 * 
 * any of the constructors may throw a `case_exception` if required parameters
 * are not found or default values don't make sense.
 *
 * values that message_test checks for:
 *
 * size=[interger];
 * example: size=4096;
 * size of messages to send in bytes. valid values 0 - max size_t
 *
 * count=[integer];
 * example: count=1000;
 * number of test messages to send. valid values 0-2^64
 * 
 * timeout=[integer];
 * example: timeout=10000;
 * how long to wait (in ms) for a response before failing the test.
 * 
 * binary=[bool];
 * example: binary=true;
 * whether or not to use binary websocket frames. (true=binary, false=utf8)
 * 
 * sync=[bool];
 * example: sync=true;
 * syncronize messages. when sync is on wsperf will wait for a response before
 * sending the next message. when sync is off, messages will be sent as quickly
 * as possible.
 * 
 * correctness=[string];
 * example: correctness=exact;
 * example: correctness=length;
 * how to evaluate the correctness of responses. exact checks each response for
 * exact correctness. length checks only that the response has the correct 
 * length. length mode is faster but won't catch invalid implimentations. length
 * mode can be used to test situations where you deliberately return incorrect
 * bytes in order to compare performance (ex: performance with/without masking)
 */
message_test::message_test(wscmd::cmd& cmd) 
 : case_handler(cmd), 
   m_message_size(extract_number<uint64_t>(cmd,"size")),
   m_message_count(extract_number<uint64_t>(cmd,"count")),
   m_timeout(extract_number<uint64_t>(cmd,"timeout")),
   m_binary(extract_bool(cmd,"binary")),
   m_sync(extract_bool(cmd,"sync")),
   m_acks(0)
{
    if (cmd.args["correctness"] == "exact") {
        m_mode = exact;
    } else if (cmd.args["correctness"] == "length") {
        m_mode = length;
    } else {
        throw case_exception("invalid correctness parameter.");
    }
}

void message_test::on_open(connection_ptr con) {
    con->alog()->at(websocketpp::log::alevel::devel) 
        << "message_test::on_open" << websocketpp::log::endl;
    
    m_msg = con->get_data_message();
    
    m_data.reserve(static_cast<size_t>(m_message_size));
    
    if (!m_binary) {
        fill_utf8(m_data,static_cast<size_t>(m_message_size),true);
        m_msg->reset(websocketpp::frame::opcode::text);
    } else {
        fill_binary(m_data,static_cast<size_t>(m_message_size),true);
        m_msg->reset(websocketpp::frame::opcode::binary);
    }
    
    m_msg->set_payload(m_data);
    
    start(con,m_timeout);
    
    if (m_sync) {
        con->send(m_msg);
    } else {
        for (uint64_t i = 0; i < m_message_count; i++) {
            con->send(m_msg);
        }
    }
}

void message_test::on_message(connection_ptr con,websocketpp::message::data_ptr msg) {
    if ((m_mode == length && msg->get_payload().size() == m_data.size()) || 
        (m_mode == exact && msg->get_payload() == m_data))
    {
        m_acks++;
        m_bytes += m_message_size;
        mark();
    } else {
        mark();
        m_msg.reset();
        m_pass = fail;
        
        this->end(con);
    }
    
    if (m_acks == m_message_count) {
        m_pass = pass;
        m_msg.reset();
        this->end(con);
    } else if (m_sync && m_pass == running) {
        con->send(m_msg);
    }
}
