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

#include <websocketpp/http/parser.hpp>

#include <chrono>

class scoped_timer {
public:
    scoped_timer(std::string i) : m_id(i),m_start(std::chrono::steady_clock::now()) {
        std::cout << "clock " << i << ": ";
    }
    ~scoped_timer() {
        std::chrono::nanoseconds time_taken = std::chrono::steady_clock::now()-m_start;

        //nanoseconds_per_test

        //tests_per_second

        //1000000000.0/(double(time_taken.count())/1000.0)

        std::cout << 1000000000.0/(double(time_taken.count())/1000.0) << std::endl;

        //std::cout << (1.0/double(time_taken.count())) * double(1000000000*1000) << std::endl;
    }

private:
    std::string m_id;
    std::chrono::steady_clock::time_point m_start;
};

int main() {
    std::string raw = "get / http/1.1\r\nhost: www.example.com\r\n\r\n";

    std::string firefox = "get / http/1.1\r\nhost: localhost:5000\r\nuser-agent: mozilla/5.0 (macintosh; intel mac os x 10.7; rv:10.0) gecko/20100101 firefox/10.0\r\naccept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\naccept-language: en-us,en;q=0.5\r\naccept-encoding: gzip, deflate\r\nconnection: keep-alive, upgrade\r\nsec-websocket-version: 8\r\nsec-websocket-origin: http://zaphoyd.com\r\nsec-websocket-key: pfik//fxwfk0rin4zipfjq==\r\npragma: no-cache\r\ncache-control: no-cache\r\nupgrade: websocket\r\n\r\n";

    std::string firefox1 = "get / http/1.1\r\nhost: localhost:5000\r\nuser-agent: mozilla/5.0 (macintosh; intel mac os x 10.7; rv:10.0) gecko/20100101 firefox/10.0\r\naccept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\naccept-language: en-us,en;q=0.5\r\n";

    std::string firefox2 = "accept-encoding: gzip, deflate\r\nconnection: keep-alive, upgrade\r\nsec-websocket-version: 8\r\nsec-websocket-origin: http://zaphoyd.com\r\nsec-websocket-key: pfik//fxwfk0rin4zipfjq==\r\npragma: no-cache\r\ncache-control: no-cache\r\nupgrade: websocket\r\n\r\n";

    {
        scoped_timer timer("simplest 1 chop");
        for (int i = 0; i < 1000; i++) {
            websocketpp::http::parser::request r;

            try {
                r.consume(raw.c_str(),raw.size());
            } catch (...) {
                std::cout << "exception" << std::endl;
            }

            if (!r.ready()) {
                std::cout << "error" << std::endl;
                break;
            }
        }
    }

    {
        scoped_timer timer("firefox, 1 chop, consume old");
        for (int i = 0; i < 1000; i++) {
            websocketpp::http::parser::request r;

            try {
                r.consume2(firefox.c_str(),firefox.size());
            } catch (...) {
                std::cout << "exception" << std::endl;
            }

            if (!r.ready()) {
                std::cout << "error" << std::endl;
                break;
            }
        }
    }

    {
        scoped_timer timer("firefox, 1 chop");
        for (int i = 0; i < 1000; i++) {
            websocketpp::http::parser::request r;

            try {
                r.consume(firefox.c_str(),firefox.size());
            } catch (...) {
                std::cout << "exception" << std::endl;
            }

            if (!r.ready()) {
                std::cout << "error" << std::endl;
                break;
            }
        }
    }



    {
        scoped_timer timer("firefox, 2 chop");
        for (int i = 0; i < 1000; i++) {
            websocketpp::http::parser::request r;

            try {
                r.consume(firefox1.c_str(),firefox1.size());
                r.consume(firefox2.c_str(),firefox2.size());
            } catch (...) {
                std::cout << "exception" << std::endl;
            }

            if (!r.ready()) {
                std::cout << "error" << std::endl;
                break;
            }
        }
    }

    return 0;
}
