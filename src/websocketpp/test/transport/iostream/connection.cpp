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
#define boost_test_module transport_iostream_connection
#include <boost/test/unit_test.hpp>

#include <iostream>
#include <cstring>
#include <string>

#include <websocketpp/common/memory.hpp>

#include <websocketpp/error.hpp>
#include <websocketpp/transport/iostream/connection.hpp>

// policies
#include <websocketpp/concurrency/basic.hpp>
#include <websocketpp/logger/basic.hpp>

struct config {
    typedef websocketpp::concurrency::basic concurrency_type;
    typedef websocketpp::log::basic<concurrency_type,
        websocketpp::log::elevel> elog_type;
    typedef websocketpp::log::basic<concurrency_type,
        websocketpp::log::alevel> alog_type;
};

typedef websocketpp::transport::iostream::connection<config> iostream_con;

using websocketpp::transport::iostream::error::make_error_code;

struct stub_con : public iostream_con {
    typedef stub_con type;
    typedef websocketpp::lib::shared_ptr<type> ptr;
    typedef iostream_con::timer_ptr timer_ptr;

    stub_con(bool is_server, config::alog_type & a, config::elog_type & e)
        : iostream_con(is_server,a,e)
        // set the error to a known code that is unused by the library
        // this way we can easily confirm that the handler was run at all.
        , ec(websocketpp::error::make_error_code(websocketpp::error::test))
        , indef_read_total(0)
    {}

    /// get a shared pointer to this component
    ptr get_shared() {
        return websocketpp::lib::static_pointer_cast<type>(iostream_con::get_shared());
    }

    void write(std::string msg) {
        iostream_con::async_write(
            msg.data(),
            msg.size(),
            websocketpp::lib::bind(
                &stub_con::handle_op,
                type::get_shared(),
                websocketpp::lib::placeholders::_1
            )
        );
    }

    void write(std::vector<websocketpp::transport::buffer> & bufs) {
        iostream_con::async_write(
            bufs,
            websocketpp::lib::bind(
                &stub_con::handle_op,
                type::get_shared(),
                websocketpp::lib::placeholders::_1
            )
        );
    }

    void async_read_at_least(size_t num_bytes, char *buf, size_t len)
    {
        iostream_con::async_read_at_least(
            num_bytes,
            buf,
            len,
            websocketpp::lib::bind(
                &stub_con::handle_op,
                type::get_shared(),
                websocketpp::lib::placeholders::_1
            )
        );
    }

    void handle_op(websocketpp::lib::error_code const & e) {
        ec = e;
    }

    void async_read_indef(size_t num_bytes, char *buf, size_t len)
    {
        indef_read_size = num_bytes;
        indef_read_buf = buf;
        indef_read_len = len;
        
        indef_read();
    }

    void indef_read() {
        iostream_con::async_read_at_least(
            indef_read_size,
            indef_read_buf,
            indef_read_len,
            websocketpp::lib::bind(
                &stub_con::handle_indef,
                type::get_shared(),
                websocketpp::lib::placeholders::_1,
                websocketpp::lib::placeholders::_2
            )
        );
    }

    void handle_indef(websocketpp::lib::error_code const & e, size_t amt_read) {
        ec = e;
        indef_read_total += amt_read;
        
        indef_read();
    }

    websocketpp::lib::error_code ec;
    size_t indef_read_size;
    char * indef_read_buf;
    size_t indef_read_len;
    size_t indef_read_total;
};

// stubs
config::alog_type alogger;
config::elog_type elogger;

boost_auto_test_case( const_methods ) {
    iostream_con::ptr con(new iostream_con(true,alogger,elogger));

    boost_check( con->is_secure() == false );
    boost_check( con->get_remote_endpoint() == "iostream transport" );
}

boost_auto_test_case( write_before_ostream_set ) {
    stub_con::ptr con(new stub_con(true,alogger,elogger));

    con->write("foo");
    boost_check( con->ec == make_error_code(websocketpp::transport::iostream::error::output_stream_required) );

    std::vector<websocketpp::transport::buffer> bufs;
    con->write(bufs);
    boost_check( con->ec == make_error_code(websocketpp::transport::iostream::error::output_stream_required) );
}

boost_auto_test_case( async_write ) {
    stub_con::ptr con(new stub_con(true,alogger,elogger));

    std::stringstream output;

    con->register_ostream(&output);

    con->write("foo");

    boost_check( !con->ec );
    boost_check( output.str() == "foo" );
}

boost_auto_test_case( async_write_vector_0 ) {
    std::stringstream output;

    stub_con::ptr con(new stub_con(true,alogger,elogger));
    con->register_ostream(&output);

    std::vector<websocketpp::transport::buffer> bufs;

    con->write(bufs);

    boost_check( !con->ec );
    boost_check( output.str() == "" );
}

boost_auto_test_case( async_write_vector_1 ) {
    std::stringstream output;

    stub_con::ptr con(new stub_con(true,alogger,elogger));
    con->register_ostream(&output);

    std::vector<websocketpp::transport::buffer> bufs;

    std::string foo = "foo";

    bufs.push_back(websocketpp::transport::buffer(foo.data(),foo.size()));

    con->write(bufs);

    boost_check( !con->ec );
    boost_check( output.str() == "foo" );
}

boost_auto_test_case( async_write_vector_2 ) {
    std::stringstream output;

    stub_con::ptr con(new stub_con(true,alogger,elogger));
    con->register_ostream(&output);

    std::vector<websocketpp::transport::buffer> bufs;

    std::string foo = "foo";
    std::string bar = "bar";

    bufs.push_back(websocketpp::transport::buffer(foo.data(),foo.size()));
    bufs.push_back(websocketpp::transport::buffer(bar.data(),bar.size()));

    con->write(bufs);

    boost_check( !con->ec );
    boost_check( output.str() == "foobar" );
}

boost_auto_test_case( async_read_at_least_too_much ) {
    stub_con::ptr con(new stub_con(true,alogger,elogger));

    char buf[10];

    con->async_read_at_least(11,buf,10);
    boost_check( con->ec == make_error_code(websocketpp::transport::iostream::error::invalid_num_bytes) );
}

boost_auto_test_case( async_read_at_least_double_read ) {
    stub_con::ptr con(new stub_con(true,alogger,elogger));

    char buf[10];

    con->async_read_at_least(5,buf,10);
    con->async_read_at_least(5,buf,10);
    boost_check( con->ec == make_error_code(websocketpp::transport::iostream::error::double_read) );
}

boost_auto_test_case( async_read_at_least ) {
    stub_con::ptr con(new stub_con(true,alogger,elogger));

    char buf[10];

    memset(buf,'x',10);

    con->async_read_at_least(5,buf,10);
    boost_check( con->ec == make_error_code(websocketpp::error::test) );

    std::stringstream channel;
    channel << "abcd";
    channel >> *con;
    boost_check( channel.tellg() == -1 );
    boost_check( con->ec == make_error_code(websocketpp::error::test) );

    std::stringstream channel2;
    channel2 << "e";
    channel2 >> *con;
    boost_check( channel2.tellg() == -1 );
    boost_check( !con->ec );
    boost_check( std::string(buf,10) == "abcdexxxxx" );

    std::stringstream channel3;
    channel3 << "f";
    channel3 >> *con;
    boost_check( channel3.tellg() == 0 );
    boost_check( !con->ec );
    boost_check( std::string(buf,10) == "abcdexxxxx" );
    con->async_read_at_least(1,buf+5,5);
    channel3 >> *con;
    boost_check( channel3.tellg() == -1 );
    boost_check( !con->ec );
    boost_check( std::string(buf,10) == "abcdefxxxx" );
}

boost_auto_test_case( async_read_at_least2 ) {
    stub_con::ptr con(new stub_con(true,alogger,elogger));

    char buf[10];

    memset(buf,'x',10);

    con->async_read_at_least(5,buf,5);
    boost_check( con->ec == make_error_code(websocketpp::error::test) );

    std::stringstream channel;
    channel << "abcdefg";
    channel >> *con;
    boost_check( channel.tellg() == 5 );
    boost_check( !con->ec );
    boost_check( std::string(buf,10) == "abcdexxxxx" );

    con->async_read_at_least(1,buf+5,5);
    channel >> *con;
    boost_check( channel.tellg() == -1 );
    boost_check( !con->ec );
    boost_check( std::string(buf,10) == "abcdefgxxx" );
}

void timer_callback_stub(websocketpp::lib::error_code const &) {}

boost_auto_test_case( set_timer ) {
   stub_con::ptr con(new stub_con(true,alogger,elogger));

    stub_con::timer_ptr tp = con->set_timer(1000,timer_callback_stub);

    boost_check( !tp );
}

boost_auto_test_case( async_read_at_least_read_some ) {
    stub_con::ptr con(new stub_con(true,alogger,elogger));

    char buf[10];
    memset(buf,'x',10);

    con->async_read_at_least(5,buf,5);
    boost_check( con->ec == make_error_code(websocketpp::error::test) );

    char input[10] = "abcdefg";
    boost_check_equal(con->read_some(input,5), 5);
    boost_check( !con->ec );
    boost_check_equal( std::string(buf,10), "abcdexxxxx" );

    boost_check_equal(con->read_some(input+5,2), 0);
    boost_check( !con->ec );
    boost_check_equal( std::string(buf,10), "abcdexxxxx" );

    con->async_read_at_least(1,buf+5,5);
    boost_check_equal(con->read_some(input+5,2), 2);
    boost_check( !con->ec );
    boost_check_equal( std::string(buf,10), "abcdefgxxx" );
}

boost_auto_test_case( async_read_at_least_read_some_indef ) {
    stub_con::ptr con(new stub_con(true,alogger,elogger));

    char buf[20];
    memset(buf,'x',20);

    con->async_read_indef(5,buf,5);
    boost_check( con->ec == make_error_code(websocketpp::error::test) );

    // here we expect to return early from read some because the outstanding
    // read was for 5 bytes and we were called with 10.
    char input[11] = "aaaaabbbbb";
    boost_check_equal(con->read_some(input,10), 5);
    boost_check( !con->ec );
    boost_check_equal( std::string(buf,10), "aaaaaxxxxx" );
    boost_check_equal( con->indef_read_total, 5 );
    
    // a subsequent read should read 5 more because the indef read refreshes
    // itself. the new read will start again at the beginning of the buffer.
    boost_check_equal(con->read_some(input+5,5), 5);
    boost_check( !con->ec );
    boost_check_equal( std::string(buf,10), "bbbbbxxxxx" );
    boost_check_equal( con->indef_read_total, 10 );
}

boost_auto_test_case( async_read_at_least_read_all ) {
    stub_con::ptr con(new stub_con(true,alogger,elogger));

    char buf[20];
    memset(buf,'x',20);

    con->async_read_indef(5,buf,5);
    boost_check( con->ec == make_error_code(websocketpp::error::test) );

    char input[11] = "aaaaabbbbb";
    boost_check_equal(con->read_all(input,10), 10);
    boost_check( !con->ec );
    boost_check_equal( std::string(buf,10), "bbbbbxxxxx" );
    boost_check_equal( con->indef_read_total, 10 );
}

boost_auto_test_case( eof_flag ) {
    stub_con::ptr con(new stub_con(true,alogger,elogger));
    char buf[10];
    con->async_read_at_least(5,buf,5);
    boost_check( con->ec == make_error_code(websocketpp::error::test) );
    con->eof();
    boost_check_equal( con->ec, make_error_code(websocketpp::transport::error::eof) );
}

boost_auto_test_case( fatal_error_flag ) {
    stub_con::ptr con(new stub_con(true,alogger,elogger));
    char buf[10];
    con->async_read_at_least(5,buf,5);
    boost_check( con->ec == make_error_code(websocketpp::error::test) );
    con->fatal_error();
    boost_check_equal( con->ec, make_error_code(websocketpp::transport::error::pass_through) );
}

boost_auto_test_case( shared_pointer_memory_cleanup ) {
    stub_con::ptr con(new stub_con(true,alogger,elogger));

    boost_check_equal(con.use_count(), 1);

    char buf[10];
    memset(buf,'x',10);
    con->async_read_at_least(5,buf,5);
    boost_check( con->ec == make_error_code(websocketpp::error::test) );
    boost_check_equal(con.use_count(), 2);

    char input[10] = "foo";
    con->read_some(input,3);
    boost_check_equal(con.use_count(), 2);

    con->read_some(input,2);
    boost_check_equal( std::string(buf,10), "foofoxxxxx" );
    boost_check_equal(con.use_count(), 1);

    con->async_read_at_least(5,buf,5);
    boost_check_equal(con.use_count(), 2);

    con->eof();
    boost_check_equal( con->ec, make_error_code(websocketpp::transport::error::eof) );
    boost_check_equal(con.use_count(), 1);
}

