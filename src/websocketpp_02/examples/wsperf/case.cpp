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

#include "case.hpp"

using wsperf::case_handler;

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
case_handler::case_handler(wscmd::cmd& cmd)
 : m_uri(extract_string(cmd,"uri")),
   m_token(extract_string(cmd,"token")),
   m_quantile_count(extract_number<size_t>(cmd,"quantile_count")),
   m_rtts(extract_bool(cmd,"rtts")),
   m_pass(running),
   m_timeout(0),
   m_bytes(0) {}

/// starts a test by starting the timeout timer and marking the start time
void case_handler::start(connection_ptr con, uint64_t timeout) {
    if (timeout > 0) {
        m_timer.reset(new boost::asio::deadline_timer(
            con->get_io_service(),
            boost::posix_time::seconds(0))
        );
        
        m_timeout = timeout;
        
        m_timer->expires_from_now(boost::posix_time::milliseconds(m_timeout));
        m_timer->async_wait(
            boost::bind(
                &type::on_timer,
                this,
                con,
                boost::asio::placeholders::error
            )
        );
    }
    
    m_start = boost::chrono::steady_clock::now();
}

/// marks an incremental time point
void case_handler::mark() {
    m_end.push_back(boost::chrono::steady_clock::now());
}

/// ends a test
/* ends a test by canceling the timeout timer, marking the end time, generating
 * statistics and closing the websocket connection.
 */
void case_handler::end(connection_ptr con) {
    std::vector<double> avgs;
    avgs.resize(m_quantile_count, 0);
    
    std::vector<double> quantiles;
    quantiles.resize(m_quantile_count, 0);
    
    double avg = 0;
    double stddev = 0;
    double total = 0;
    double seconds = 0;
    
    if (m_timeout > 0) {
        m_timer->cancel();
    }
    
    // todo: handle weird sizes and error conditions better
    if (m_end.size() > m_quantile_count) {
        boost::chrono::steady_clock::time_point last = m_start;
        
        // convert rtts to microsecs
        //
        
        std::vector<boost::chrono::steady_clock::time_point>::iterator it;
        for (it = m_end.begin(); it != m_end.end(); ++it) {
            boost::chrono::nanoseconds dur = *it - last;
            m_times.push_back(static_cast<double> (dur.count()) / 1000.);
            last = *it;
        }
        
        std::sort(m_times.begin(), m_times.end());
        
        size_t samples_per_quantile = m_times.size() / m_quantile_count;
        
        // quantiles
        for (size_t i = 0; i < m_quantile_count; ++i) {
            quantiles[i] = m_times[((i + 1) * samples_per_quantile) - 1];
        }
        
        // total average and quantile averages
        for (size_t i = 0; i < m_times.size(); ++i) {
            avg += m_times[i];
            avgs[i / samples_per_quantile] 
                += m_times[i] / static_cast<double>(samples_per_quantile);
        }
        
        avg /= static_cast<double> (m_times.size());
        
        // standard dev corrected for estimation from sample
        for (size_t i = 0; i < m_times.size(); ++i) {
            stddev += (m_times[i] - avg) * (m_times[i] - avg);
        }
        
        // bessel's correction
        stddev /= static_cast<double> (m_times.size() - 1); 
        stddev = std::sqrt(stddev);
    } else {
        m_times.push_back(0);
    }
    
    boost::chrono::nanoseconds total_dur = m_end[m_end.size()-1] - m_start;
    total = static_cast<double> (total_dur.count()) / 1000.; // microsec
    seconds = total / 10000000.;
    
    std::stringstream s;
    std::string outcome;
    
    switch (m_pass) {
        case fail:
            outcome = "fail";
            break;
        case pass:
            outcome = "pass";
            break;
        case time_out:
            outcome = "time_out";
            break;
        case running:
            throw case_exception("end() called from running state");
            break;
    }
    
    s << "{\"result\":\"" << outcome 
      << "\",\"min\":" << m_times[0] 
      << ",\"max\":" << m_times[m_times.size()-1] 
      << ",\"median\":" << m_times[(m_times.size()-1)/2] 
      << ",\"avg\":" << avg 
      << ",\"stddev\":" << stddev 
      << ",\"total\":" << total 
      << ",\"bytes\":" << m_bytes 
      << ",\"quantiles\":[";
      
    for (size_t i = 0; i < m_quantile_count; i++) {
        s << (i > 0 ? "," : "");
        s << "[";
        s << avgs[i] << "," << quantiles[i];
        s << "]";
    }                 
    s << "]";
    
    if (m_rtts) {
        s << ",\"rtts\":[";
        for (size_t i = 0; i < m_times.size(); i++) {
            s << (i > 0 ? "," : "") << m_times[i];
        }
        s << "]";
    };
    s << "}";
    
    m_data = s.str();
    
    con->close(websocketpp::close::status::normal,"");
}

/// fills a buffer with utf8 bytes
void case_handler::fill_utf8(std::string& data,size_t size,bool random) {
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

/// fills a buffer with bytes
void case_handler::fill_binary(std::string& data,size_t size,bool random) {
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

void case_handler::on_timer(connection_ptr con,const boost::system::error_code& error) {
    if (error == boost::system::errc::operation_canceled) {
        return; // timer was canceled because test finished
    }
    
    // time out
    mark();
    m_pass = time_out;
    this->end(con);
}

void case_handler::on_fail(connection_ptr con) {
    m_data = "{\"result\":\"connection_failed\"}";
}

const std::string& case_handler::get_data() const {
    return m_data;
}
const std::string& case_handler::get_token() const {
    return m_token;
}
const std::string& case_handler::get_uri() const {
    return m_uri;
}
