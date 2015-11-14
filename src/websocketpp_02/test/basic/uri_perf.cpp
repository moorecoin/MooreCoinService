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
#include <boost/date_time.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <iostream>

#include "../../src/uri.hpp"

int main() {
    boost::posix_time::ptime start = boost::posix_time::microsec_clock::local_time();
    
    long m = 100000;
    long n = 3;
    
    for (long i = 0; i < m; i++) {
        websocketpp::uri uri1("wss://thor-websocket.zaphoyd.net:9002/foo/bar/baz?a=b&c=d");
        websocketpp::uri uri2("ws://[::1]");
        websocketpp::uri uri3("ws://localhost:9000/chat");
    }
    boost::posix_time::ptime end = boost::posix_time::microsec_clock::local_time();
    boost::posix_time::time_period period(start,end);
    int ms = period.length().total_milliseconds();
    
    std::cout << "created " << m*n << " uris in " << ms << "ms" << " (" << (m*n)/(ms/1000.0) << "/s)" << std::endl;
}

