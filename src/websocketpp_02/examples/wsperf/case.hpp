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

#ifndef wsperf_case_hpp
#define wsperf_case_hpp

#include "wscmd.hpp"

#include "../../src/roles/client.hpp"
#include "../../src/websocketpp.hpp"

#include <boost/chrono.hpp>

#include <exception>

using websocketpp::client;

namespace wsperf {

class case_exception : public std::exception {
public: 
    case_exception(const std::string& msg) : m_msg(msg) {}
    ~case_exception() throw() {}
    
    virtual const char* what() const throw() {
        return m_msg.c_str();
    }
    
    std::string m_msg;
};

class case_handler : public client::handler {
public:
    typedef case_handler type;
    
    /// construct a message test from a wscmd command
    explicit case_handler(wscmd::cmd& cmd);
    
    void start(connection_ptr con, uint64_t timeout);
    void end(connection_ptr con);
    
    void mark();
    
    // just does random ascii right now. true random utf8 with multi-byte stuff
    // would probably be better
    void fill_utf8(std::string& data,size_t size,bool random = true);
    void fill_binary(std::string& data,size_t size,bool random = true);
    
    void on_timer(connection_ptr con,const boost::system::error_code& error);
    
    void on_close(connection_ptr con) {
        con->alog()->at(websocketpp::log::alevel::devel) 
            << "case_handler::on_close" 
            << websocketpp::log::endl;
    }
    void on_fail(connection_ptr con);
    
    const std::string& get_data() const;
    const std::string& get_token() const;
    const std::string& get_uri() const;
    
    // todo: refactor these three extract methods into wscmd
    std::string extract_string(wscmd::cmd command,std::string key) {
        if (command.args[key] != "") {
           return command.args[key];
        } else {
            throw case_exception("invalid " + key + " parameter.");
        }
    }
    
    template <typename t>
    t extract_number(wscmd::cmd command,std::string key) {
        if (command.args[key] != "") {
            std::istringstream buf(command.args[key]);
            t val;
            
            buf >> val;
            
            if (buf) {return val;}
        }
        throw case_exception("invalid " + key + " parameter.");
    }
    
    bool extract_bool(wscmd::cmd command,std::string key) {
        if (command.args[key] != "") {
            if (command.args[key] == "true") {
                return true;
            } else if (command.args[key] == "false") {
                return false;
            }
        }
        throw case_exception("invalid " + key + " parameter.");
    }
protected:
    enum status {
        fail = 0,
        pass = 1,
        time_out = 2,
        running = 3
    };
    
    std::string                 m_uri;
    std::string                 m_token;
    size_t                      m_quantile_count;
    bool                        m_rtts;
    std::string                 m_data;
    
    status                      m_pass;
    
    uint64_t                                                m_timeout;
    boost::shared_ptr<boost::asio::deadline_timer>          m_timer;
    
    boost::chrono::steady_clock::time_point                 m_start;
    std::vector<boost::chrono::steady_clock::time_point>    m_end;
    std::vector<double>                                     m_times;
    
    uint64_t                    m_bytes;
};

typedef boost::shared_ptr<case_handler> case_handler_ptr;

} // namespace wsperf

#endif // wsperf_case_hpp
