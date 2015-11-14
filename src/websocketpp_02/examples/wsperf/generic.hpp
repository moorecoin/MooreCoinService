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

#ifndef wsperf_case_generic_hpp
#define wsperf_case_generic_hpp

#include "case.hpp"
#include "wscmd.hpp"

namespace wsperf {

enum correctness_mode {
    exact = 0,
    length = 1
};

class message_test : public case_handler {
public: 
    /// construct a message test from a wscmd command
    explicit message_test(wscmd::cmd& cmd);
    
    void on_open(connection_ptr con);
    void on_message(connection_ptr con,websocketpp::message::data_ptr msg);
private:
    // simulation parameters
    uint64_t            m_message_size;
    uint64_t            m_message_count;
    uint64_t            m_timeout;
    bool                m_binary;
    bool                m_sync;
    correctness_mode    m_mode;
    
    // simulation temporaries
    std::string         m_data;
    message_ptr         m_msg;
    uint64_t            m_acks;
};

} // namespace wsperf

#endif // wsperf_case_generic_hpp
