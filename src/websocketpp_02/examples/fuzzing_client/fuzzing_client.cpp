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

#include "../../src/roles/client.hpp"
#include "../../src/websocketpp.hpp"

#include <cstdlib>
#include <ctime>
#include <iostream>

using websocketpp::client;

// base test case handler
class test_case_handler : public client::handler {
public:
    typedef test_case_handler type;
    
    void start(connection_ptr con, int timeout) {
        m_timer.reset(new boost::asio::deadline_timer(
            con->get_io_service(),
            boost::posix_time::seconds(0))
        );
        m_timer->expires_from_now(boost::posix_time::milliseconds(timeout));
        m_timer->async_wait(boost::bind(&type::on_timer,
                                        this,
                                        con,
                                        boost::asio::placeholders::error));
        
        m_start_time = boost::posix_time::microsec_clock::local_time();
    }
    
    virtual void end(connection_ptr con) {
        // test is over, give control back to controlling handler
        boost::posix_time::time_period len(m_start_time,m_end_time);
        
        switch (m_pass) {
            case fail:
                std::cout << " fails in " << len.length() << std::endl;
                break;
            case pass:
                std::cout << " passes in " << len.length();
                
                if (m_iterations > 1) {
                    std::cout << " (avg: " 
                        << (len.length().total_milliseconds())/m_iterations 
                        << "ms)";
                }
                
                std::cout << std::endl;
                
                break;
            case time_out:
                std::cout << " times out in " << len.length() << std::endl;
                break;
        }
        
        con->close(websocketpp::close::status::normal,"");
    }
    
    // just does random ascii right now. true random utf8 with multi-byte stuff
    // would probably be better
    void fill_utf8(std::string& data,size_t size,bool random = true) {
        if (random) {
            uint32_t val;
            for (size_t i = 0; i < size; i++) {
                if (i%4 == 0) {
                    val = uint32_t(rand());
                }
                
                data.push_back(char(((reinterpret_cast<uint8_t*>(&val)[i%4])%95)+32));
            }
        } else {
            data.assign(size,'*');
        }
    }
    
    void fill_binary(std::string& data,size_t size,bool random = true) {
        if (random) {
            int32_t val;
            for (size_t i = 0; i < size; i++) {
                if (i%4 == 0) {
                    val = rand();
                }
                
                data.push_back((reinterpret_cast<char*>(&val))[i%4]);
            }
        } else {
            data.assign(size,'*');
        }
    }
    
    void on_timer(connection_ptr con,const boost::system::error_code& error) {
        if (error == boost::system::errc::operation_canceled) {
            return; // timer was canceled because test finished
        }
        
        // time out
        m_end_time = boost::posix_time::microsec_clock::local_time();
        m_pass = time_out;
        this->end(con);
    }
    
    void on_close(connection_ptr con) {
        //std::cout << " fails in " << len.length() << std::endl;
    }
    void on_fail(connection_ptr con) {
        std::cout << " fails to connect." << std::endl;
    }
protected:
    enum status {
        fail = 0,
        pass = 1,
        time_out = 2
    };
    
    status                      m_pass;
    int                         m_iterations;
    boost::posix_time::ptime    m_start_time;
    boost::posix_time::ptime    m_end_time;
    boost::shared_ptr<boost::asio::deadline_timer> m_timer;
};

// test class for 9.1.* and 9.2.*
class test_9_1_x : public test_case_handler {
public: 
    test_9_1_x(int minor, int subtest) 
     : m_minor(minor),
       m_subtest(subtest)
    {
        // needs more c++11 intializer lists =(
        m_test_sizes[0] = 65536;
        m_test_sizes[1] = 262144;
        m_test_sizes[2] = 1048576;
        m_test_sizes[3] = 4194304;
        m_test_sizes[4] = 8388608;
        m_test_sizes[5] = 16777216;
        
        m_iterations = 1;
    }
    
    void on_open(connection_ptr con) {
        std::cout << "test 9." << m_minor << "." << m_subtest;
        
        m_data.reserve(m_test_sizes[m_subtest-1]);
        
        int timeout = 10000; // 10 sec
        
        // extend timeout to 100 sec for the longer tests
        if ((m_minor == 1 && m_subtest >= 3) || (m_minor == 2 && m_subtest >= 5)) {
            timeout = 100000;
        }
        
        if (m_minor == 1) {
            fill_utf8(m_data,m_test_sizes[m_subtest-1],true);
            start(con,timeout);
            con->send(m_data,websocketpp::frame::opcode::text);
        } else if (m_minor == 2) {
            fill_binary(m_data,m_test_sizes[m_subtest-1],true);
            start(con,timeout);
            con->send(m_data,websocketpp::frame::opcode::binary);
        } else {
            std::cout << " has unknown definition." << std::endl;
        }
    }
    
    void on_message(connection_ptr con,websocketpp::message::data_ptr msg) {
        m_timer->cancel();
        m_end_time = boost::posix_time::microsec_clock::local_time();
        
        // check whether the echoed data matches exactly
        m_pass = ((msg->get_payload() == m_data) ? pass : fail);
        this->end(con);
    }
private:
    int         m_minor;
    int         m_subtest;
    int         m_test_sizes[6];
    std::string m_data;
};

// test class for 9.7.* and 9.8.*
class test_9_7_x : public test_case_handler {
public: 
    test_9_7_x(int minor, int subtest) 
     : m_minor(minor),
       m_subtest(subtest),
       m_acks(0)
    {
        // needs more c++11 intializer lists =(
        m_test_sizes[0] = 0;
        m_test_sizes[1] = 16;
        m_test_sizes[2] = 64;
        m_test_sizes[3] = 256;
        m_test_sizes[4] = 1024;
        m_test_sizes[5] = 4096;
        
        m_test_timeouts[0] = 60000;
        m_test_timeouts[1] = 60000;
        m_test_timeouts[2] = 60000;
        m_test_timeouts[3] = 120000;
        m_test_timeouts[4] = 240000;
        m_test_timeouts[5] = 480000;
        
        m_iterations = 1000;
    }
    
    void on_open(connection_ptr con) {
        std::cout << "test 9." << m_minor << "." << m_subtest;
        
        // get a message buffer from the connection
        m_msg = con->get_data_message();
        
        // reserve space in our local data buffer
        m_data.reserve(m_test_sizes[m_subtest-1]);
        
        // reset message to appropriate opcode and fill local buffer with
        // appropriate types of random data.
        if (m_minor == 7) {
            fill_utf8(m_data,m_test_sizes[m_subtest-1],true);
            m_msg->reset(websocketpp::frame::opcode::text);
        } else if (m_minor == 8) {
            fill_binary(m_data,m_test_sizes[m_subtest-1],true);
            m_msg->reset(websocketpp::frame::opcode::binary);
        } else {
            std::cout << " has unknown definition." << std::endl;
            return;
        }
        
        m_msg->set_payload(m_data);
        
        // start test timer with 60-480 second timeout based on test length
        start(con,m_test_timeouts[m_subtest-1]);
        
        con->send(m_msg);
        
        // send 1000 copies of prepared message
        /*for (int i = 0; i < m_iterations; i++) {
            con->send(msg);
        }*/
    }
    
    void on_message(connection_ptr con, message_ptr msg) {
        if (msg->get_payload() == m_data) {
            m_acks++;
        } else {
            m_end_time = boost::posix_time::microsec_clock::local_time();
            m_timer->cancel();
            m_msg.reset();
            this->end(con);
        }
        
        if (m_acks == m_iterations) {
            m_pass = pass;
            m_end_time = boost::posix_time::microsec_clock::local_time();
            m_timer->cancel();
            m_msg.reset();
            this->end(con);
        } else {
            con->send(m_msg);
        }
    }
private:
    int         m_minor;
    int         m_subtest;
    int         m_test_timeouts[6];
    size_t      m_test_sizes[6];
    std::string m_data;
    int         m_acks;
    message_ptr m_msg;
    
};

int main(int argc, char* argv[]) {
    std::string uri;
    
    srand(time(null));
    
    if (argc != 2) {
        uri = "ws://localhost:9002/";
    } else {
        uri = argv[1];
    }
    
    
    
    std::vector<client::handler_ptr>  tests;
    
    // 9.1.x and 9.2.x tests
    for (int i = 1; i <= 2; i++) {
        for (int j = 1; j <= 6; j++) {
            tests.push_back(client::handler_ptr(new test_9_1_x(i,j)));
        }
    }
    
    // 9.7.x and 9.8.x tests
    for (int i = 7; i <= 8; i++) {
        for (int j = 1; j <= 6; j++) {
            tests.push_back(client::handler_ptr(new test_9_7_x(i,j)));
        }
    }
    
    try {
        client endpoint(tests[0]);
        
        endpoint.alog().unset_level(websocketpp::log::alevel::all);
        endpoint.elog().unset_level(websocketpp::log::elevel::all);
        
        for (size_t i = 0; i < tests.size(); i++) {
            if (i > 0) {
                endpoint.reset();
                endpoint.set_handler(tests[i]);
            }
            
            endpoint.connect(uri);
            endpoint.run();
        }
    } catch (std::exception& e) {
        std::cerr << "exception: " << e.what() << std::endl;
    }
    
    return 0;
}
