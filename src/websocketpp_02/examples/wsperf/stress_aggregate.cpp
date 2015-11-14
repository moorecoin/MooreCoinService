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

#include "stress_aggregate.hpp"

using wsperf::stress_aggregate;

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
 * uri=[string];
 * example: uri=ws://localhost:9000;
 * uri of the server to connect to
 * 
 * token=[string];
 * example: token=foo;
 * string value that will be returned in the `token` field of all test related
 * messages. a separate token should be sent for each unique test.
 *
 * quantile_count=[integer];
 * example: quantile_count=10;
 * how many histogram quantiles to return in the test results
 * 
 * rtts=[bool];
 * example: rtts:true;
 * whether or not to return the full list of round trip times for each message
 * primarily useful for debugging.
 */
stress_aggregate::stress_aggregate(wscmd::cmd& cmd)
 : stress_handler(cmd)
{}

void stress_aggregate::start(connection_ptr con) {
    
}

/*void stress_aggregate::on_message(connection_ptr con,websocketpp::message::data_ptr msg) {
    std::string hash = websocketpp::md5_hash_hex(msg->get_payload());
    
    boost::lock_guard<boost::mutex> lock(m_lock);
    m_msg_stats[hash]++;
}*/

/*std::string stress_aggregate::get_data() const {
    std::stringstream data;
    
    std::string sep = "";
    
    data << "{";
    
    std::map<std::string,size_t>::iterator it;
    
    {
        boost::lock_guard<boost::mutex> lock(m_lock);
        for (it = m_msg_stats.begin(); it != m_msg_stats.end(); it++) {
            data << sep << "\"" << (*it)->first << "\":" << (*it)->second;
            sep = ",";
        }
    }
    
    data << "}";
    
    return data;
}*/

