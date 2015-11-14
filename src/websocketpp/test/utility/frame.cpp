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
//#define boost_test_dyn_link
#define boost_test_module frame
#include <boost/test/unit_test.hpp>

#include <iostream>
#include <string>

#include <websocketpp/frame.hpp>
#include <websocketpp/utilities.hpp>

using namespace websocketpp;

boost_auto_test_case( basic_bits ) {
    frame::basic_header h1(0x00,0x00); // all false
    frame::basic_header h2(0xf0,0x80); // all true

    // read values
    boost_check( frame::get_fin(h1) == false );
    boost_check( frame::get_rsv1(h1) == false );
    boost_check( frame::get_rsv2(h1) == false );
    boost_check( frame::get_rsv3(h1) == false );
    boost_check( frame::get_masked(h1) == false );

    boost_check( frame::get_fin(h2) == true );
    boost_check( frame::get_rsv1(h2) == true );
    boost_check( frame::get_rsv2(h2) == true );
    boost_check( frame::get_rsv3(h2) == true );
    boost_check( frame::get_masked(h2) == true );

    // set values
    frame::set_fin(h1,true);
    boost_check( h1.b0 == 0x80 );

    frame::set_rsv1(h1,true);
    boost_check( h1.b0 == 0xc0 );

    frame::set_rsv2(h1,true);
    boost_check( h1.b0 == 0xe0 );

    frame::set_rsv3(h1,true);
    boost_check( h1.b0 == 0xf0 );

    frame::set_masked(h1,true);
    boost_check( h1.b1 == 0x80 );
}

boost_auto_test_case( basic_constructors ) {
    // read values
    frame::basic_header h1(frame::opcode::text,12,true,false);
    boost_check( frame::get_opcode(h1) == frame::opcode::text );
    boost_check( frame::get_basic_size(h1) == 12 );
    boost_check( frame::get_fin(h1) == true );
    boost_check( frame::get_rsv1(h1) == false );
    boost_check( frame::get_rsv2(h1) == false );
    boost_check( frame::get_rsv3(h1) == false );
    boost_check( frame::get_masked(h1) == false );

    frame::basic_header h2(frame::opcode::binary,0,false,false,false,true);
    boost_check( frame::get_opcode(h2) == frame::opcode::binary );
    boost_check( frame::get_basic_size(h2) == 0 );
    boost_check( frame::get_fin(h2) == false );
    boost_check( frame::get_rsv1(h2) == false );
    boost_check( frame::get_rsv2(h2) == true );
    boost_check( frame::get_rsv3(h2) == false );
    boost_check( frame::get_masked(h2) == false );
}

boost_auto_test_case( basic_size ) {
    frame::basic_header h1(0x00,0x00); // length 0
    frame::basic_header h2(0x00,0x01); // length 1
    frame::basic_header h3(0x00,0x7d); // length 125
    frame::basic_header h4(0x00,0x7e); // length 126
    frame::basic_header h5(0x00,0x7f); // length 127
    frame::basic_header h6(0x00,0x80); // length 0, mask bit set

    boost_check( frame::get_basic_size(h1) == 0 );
    boost_check( frame::get_basic_size(h2) == 1 );
    boost_check( frame::get_basic_size(h3) == 125 );
    boost_check( frame::get_basic_size(h4) == 126 );
    boost_check( frame::get_basic_size(h5) == 127 );
    boost_check( frame::get_basic_size(h6) == 0 );

    /*frame::set_basic_size(h1,1);
    boost_check( h1.b1 == 0x01 );

    frame::set_basic_size(h1,125);
    boost_check( h1.b1 == 0x7d );

    frame::set_basic_size(h1,126);
    boost_check( h1.b1 == 0x7e );

    frame::set_basic_size(h1,127);
    boost_check( h1.b1 == 0x7f );

    frame::set_basic_size(h1,0);
    boost_check( h1.b1 == 0x00 );*/
}

boost_auto_test_case( basic_header_length ) {
    frame::basic_header h1(0x82,0x00); // short binary frame, unmasked
    frame::basic_header h2(0x82,0x80); // short binary frame, masked
    frame::basic_header h3(0x82,0x7e); // medium binary frame, unmasked
    frame::basic_header h4(0x82,0xfe); // medium binary frame, masked
    frame::basic_header h5(0x82,0x7f); // jumbo binary frame, unmasked
    frame::basic_header h6(0x82,0xff); // jumbo binary frame, masked

    boost_check( frame::get_header_len(h1) == 2);
    boost_check( frame::get_header_len(h2) == 6);
    boost_check( frame::get_header_len(h3) == 4);
    boost_check( frame::get_header_len(h4) == 8);
    boost_check( frame::get_header_len(h5) == 10);
    boost_check( frame::get_header_len(h6) == 14);
}

boost_auto_test_case( basic_opcode ) {
    frame::basic_header h1(0x00,0x00);

    boost_check( is_control(frame::opcode::continuation) == false);
    boost_check( is_control(frame::opcode::text) == false);
    boost_check( is_control(frame::opcode::binary) == false);
    boost_check( is_control(frame::opcode::close) == true);
    boost_check( is_control(frame::opcode::ping) == true);
    boost_check( is_control(frame::opcode::pong) == true);

    boost_check( frame::get_opcode(h1) == frame::opcode::continuation );
}

boost_auto_test_case( extended_header_basics ) {
    frame::extended_header h1;
    uint8_t h1_solution[12] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                               0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    frame::extended_header h2(uint16_t(255));
    uint8_t h2_solution[12] = {0x00, 0xff, 0x00, 0x00, 0x00, 0x00,
                               0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    frame::extended_header h3(uint16_t(256),htonl(0x8040201));
    uint8_t h3_solution[12] = {0x01, 0x00, 0x08, 0x04, 0x02, 0x01,
                               0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    frame::extended_header h4(uint64_t(0x0807060504030201ll));
    uint8_t h4_solution[12] = {0x08, 0x07, 0x06, 0x05, 0x04, 0x03,
                               0x02, 0x01, 0x00, 0x00, 0x00, 0x00};

    frame::extended_header h5(uint64_t(0x0807060504030201ll),htonl(0x8040201));
    uint8_t h5_solution[12] = {0x08, 0x07, 0x06, 0x05, 0x04, 0x03,
                               0x02, 0x01, 0x08, 0x04, 0x02, 0x01};

    boost_check( std::equal(h1_solution,h1_solution+12,h1.bytes) );
    boost_check( std::equal(h2_solution,h2_solution+12,h2.bytes) );
    boost_check( std::equal(h3_solution,h3_solution+12,h3.bytes) );
    boost_check( std::equal(h4_solution,h4_solution+12,h4.bytes) );
    boost_check( std::equal(h5_solution,h5_solution+12,h5.bytes) );
}

boost_auto_test_case( extended_header_extractors ) {
    frame::basic_header h1(0x00,0x7e);
    frame::extended_header e1(uint16_t(255));
    boost_check( get_extended_size(e1) == 255 );
    boost_check( get_payload_size(h1,e1) == 255 );
    boost_check( get_masking_key_offset(h1) == 2 );
    boost_check( get_masking_key(h1,e1).i == 0 );

    frame::basic_header h2(0x00,0x7f);
    frame::extended_header e2(uint64_t(0x0807060504030201ll));
    boost_check( get_jumbo_size(e2) == 0x0807060504030201ll );
    boost_check( get_payload_size(h2,e2) == 0x0807060504030201ll );
    boost_check( get_masking_key_offset(h2) == 8 );
    boost_check( get_masking_key(h2,e2).i == 0 );

    frame::basic_header h3(0x00,0xfe);
    frame::extended_header e3(uint16_t(255),0x08040201);
    boost_check( get_extended_size(e3) == 255 );
    boost_check( get_payload_size(h3,e3) == 255 );
    boost_check( get_masking_key_offset(h3) == 2 );
    boost_check( get_masking_key(h3,e3).i == 0x08040201 );

    frame::basic_header h4(0x00,0xff);
    frame::extended_header e4(uint64_t(0x0807060504030201ll),0x08040201);
    boost_check( get_jumbo_size(e4) == 0x0807060504030201ll );
    boost_check( get_payload_size(h4,e4) == 0x0807060504030201ll );
    boost_check( get_masking_key_offset(h4) == 8 );
    boost_check( get_masking_key(h4,e4).i == 0x08040201 );

    frame::basic_header h5(0x00,0x7d);
    frame::extended_header e5;
    boost_check( get_payload_size(h5,e5) == 125 );
}

boost_auto_test_case( header_preparation ) {
    frame::basic_header h1(0x81,0xff); //
    frame::extended_header e1(uint64_t(0xfffffll),htonl(0xd5fb70ee));
    std::string p1 = prepare_header(h1, e1);
    uint8_t s1[14] = {0x81, 0xff,
                     0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xff, 0xff,
                     0xd5, 0xfb, 0x70, 0xee};

    boost_check( p1.size() == 14);
    boost_check( std::equal(p1.begin(),p1.end(),reinterpret_cast<char*>(s1)) );

    frame::basic_header h2(0x81,0x7e); //
    frame::extended_header e2(uint16_t(255));
    std::string p2 = prepare_header(h2, e2);
    uint8_t s2[4] = {0x81, 0x7e, 0x00, 0xff};

    boost_check( p2.size() == 4);
    boost_check( std::equal(p2.begin(),p2.end(),reinterpret_cast<char*>(s2)) );
}

boost_auto_test_case( prepare_masking_key ) {
    frame::masking_key_type key;

    key.i = htonl(0x12345678);

    if (sizeof(size_t) == 8) {
        boost_check(
            frame::prepare_masking_key(key) == lib::net::_htonll(0x1234567812345678ll)
        );
    } else {
        boost_check( frame::prepare_masking_key(key) == htonl(0x12345678) );
    }
}

boost_auto_test_case( prepare_masking_key2 ) {
    frame::masking_key_type key;

    key.i = htonl(0xd5fb70ee);

    // one call
    if (sizeof(size_t) == 8) {
        boost_check(
            frame::prepare_masking_key(key) == lib::net::_htonll(0xd5fb70eed5fb70eell)
        );
    } else {
        boost_check( frame::prepare_masking_key(key) == htonl(0xd5fb70ee) );
    }
}

// todo: figure out a way to run/test both 4 and 8 byte versions.
boost_auto_test_case( circshift ) {
    /*if (sizeof(size_t) == 8) {
        size_t test = 0x0123456789abcdef;

        boost_check( frame::circshift_prepared_key(test,0) == 0x0123456789abcdef);
        boost_check( frame::circshift_prepared_key(test,1) == 0xef0123456789abcd);
        boost_check( frame::circshift_prepared_key(test,2) == 0xcdef0123456789ab);
        boost_check( frame::circshift_prepared_key(test,3) == 0xabcdef0123456789);
    } else {
        size_t test = 0x01234567;

        boost_check( frame::circshift_prepared_key(test,0) == 0x01234567);
        boost_check( frame::circshift_prepared_key(test,1) == 0x67012345);
        boost_check( frame::circshift_prepared_key(test,2) == 0x45670123);
        boost_check( frame::circshift_prepared_key(test,3) == 0x23456701);
    }*/
}

boost_auto_test_case( block_byte_mask ) {
    uint8_t input[15] = {0x00, 0x00, 0x00, 0x00,
                         0x00, 0x00, 0x00, 0x00,
                         0x00, 0x00, 0x00, 0x00,
                         0x00, 0x00, 0x00};

    uint8_t output[15];

    uint8_t masked[15] = {0x00, 0x01, 0x02, 0x03,
                          0x00, 0x01, 0x02, 0x03,
                          0x00, 0x01, 0x02, 0x03,
                          0x00, 0x01, 0x02};

    frame::masking_key_type key;
    key.c[0] = 0x00;
    key.c[1] = 0x01;
    key.c[2] = 0x02;
    key.c[3] = 0x03;

    byte_mask(input,input+15,output,key);

    boost_check( std::equal(output,output+15,masked) );
}

boost_auto_test_case( block_byte_mask_inplace ) {
    uint8_t buffer[15] = {0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00};

    uint8_t masked[15] = {0x00, 0x01, 0x02, 0x03,
                          0x00, 0x01, 0x02, 0x03,
                          0x00, 0x01, 0x02, 0x03,
                          0x00, 0x01, 0x02};

    frame::masking_key_type key;
    key.c[0] = 0x00;
    key.c[1] = 0x01;
    key.c[2] = 0x02;
    key.c[3] = 0x03;

    byte_mask(buffer,buffer+15,key);

    boost_check( std::equal(buffer,buffer+15,masked) );
}

boost_auto_test_case( block_word_mask ) {
    uint8_t input[15] =  {0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00};

    uint8_t output[15];

    uint8_t masked[15] = {0x00, 0x01, 0x02, 0x03,
                          0x00, 0x01, 0x02, 0x03,
                          0x00, 0x01, 0x02, 0x03,
                          0x00, 0x01, 0x02};

    frame::masking_key_type key;
    key.c[0] = 0x00;
    key.c[1] = 0x01;
    key.c[2] = 0x02;
    key.c[3] = 0x03;

    word_mask_exact(input,output,15,key);

    boost_check( std::equal(output,output+15,masked) );
}

boost_auto_test_case( block_word_mask_inplace ) {
    uint8_t buffer[15] = {0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00};

    uint8_t masked[15] = {0x00, 0x01, 0x02, 0x03,
                          0x00, 0x01, 0x02, 0x03,
                          0x00, 0x01, 0x02, 0x03,
                          0x00, 0x01, 0x02};

    frame::masking_key_type key;
    key.c[0] = 0x00;
    key.c[1] = 0x01;
    key.c[2] = 0x02;
    key.c[3] = 0x03;

    word_mask_exact(buffer,15,key);

    boost_check( std::equal(buffer,buffer+15,masked) );
}

boost_auto_test_case( continuous_word_mask ) {
    uint8_t input[16];
    uint8_t output[16];

    uint8_t masked[16] = {0x00, 0x01, 0x02, 0x03,
                          0x00, 0x01, 0x02, 0x03,
                          0x00, 0x01, 0x02, 0x03,
                          0x00, 0x01, 0x02, 0x00};

    frame::masking_key_type key;
    key.c[0] = 0x00;
    key.c[1] = 0x01;
    key.c[2] = 0x02;
    key.c[3] = 0x03;

    // one call
    size_t pkey,pkey_temp;
    pkey = frame::prepare_masking_key(key);
    std::fill_n(input,16,0x00);
    std::fill_n(output,16,0x00);
    frame::word_mask_circ(input,output,15,pkey);
    boost_check( std::equal(output,output+16,masked) );

    // calls not split on word boundaries
    pkey = frame::prepare_masking_key(key);
    std::fill_n(input,16,0x00);
    std::fill_n(output,16,0x00);

    pkey_temp = frame::word_mask_circ(input,output,7,pkey);
    boost_check( std::equal(output,output+7,masked) );
    boost_check( pkey_temp == frame::circshift_prepared_key(pkey,3) );

    pkey_temp = frame::word_mask_circ(input+7,output+7,8,pkey_temp);
    boost_check( std::equal(output,output+16,masked) );
    boost_check_equal( pkey_temp, frame::circshift_prepared_key(pkey,3) );
}

boost_auto_test_case( continuous_byte_mask ) {
    uint8_t input[16];
    uint8_t output[16];

    uint8_t masked[16] = {0x00, 0x01, 0x02, 0x03,
                          0x00, 0x01, 0x02, 0x03,
                          0x00, 0x01, 0x02, 0x03,
                          0x00, 0x01, 0x02, 0x00};

    frame::masking_key_type key;
    key.c[0] = 0x00;
    key.c[1] = 0x01;
    key.c[2] = 0x02;
    key.c[3] = 0x03;

    // one call
    size_t pkey,pkey_temp;
    pkey = frame::prepare_masking_key(key);
    std::fill_n(input,16,0x00);
    std::fill_n(output,16,0x00);
    frame::byte_mask_circ(input,output,15,pkey);
    boost_check( std::equal(output,output+16,masked) );

    // calls not split on word boundaries
    pkey = frame::prepare_masking_key(key);
    std::fill_n(input,16,0x00);
    std::fill_n(output,16,0x00);

    pkey_temp = frame::byte_mask_circ(input,output,7,pkey);
    boost_check( std::equal(output,output+7,masked) );
    boost_check( pkey_temp == frame::circshift_prepared_key(pkey,3) );

    pkey_temp = frame::byte_mask_circ(input+7,output+7,8,pkey_temp);
    boost_check( std::equal(output,output+16,masked) );
    boost_check_equal( pkey_temp, frame::circshift_prepared_key(pkey,3) );
}

boost_auto_test_case( continuous_word_mask_inplace ) {
    uint8_t buffer[16];

    uint8_t masked[16] = {0x00, 0x01, 0x02, 0x03,
                          0x00, 0x01, 0x02, 0x03,
                          0x00, 0x01, 0x02, 0x03,
                          0x00, 0x01, 0x02, 0x00};

    frame::masking_key_type key;
    key.c[0] = 0x00;
    key.c[1] = 0x01;
    key.c[2] = 0x02;
    key.c[3] = 0x03;

    // one call
    size_t pkey,pkey_temp;
    pkey = frame::prepare_masking_key(key);
    std::fill_n(buffer,16,0x00);
    frame::word_mask_circ(buffer,15,pkey);
    boost_check( std::equal(buffer,buffer+16,masked) );

    // calls not split on word boundaries
    pkey = frame::prepare_masking_key(key);
    std::fill_n(buffer,16,0x00);

    pkey_temp = frame::word_mask_circ(buffer,7,pkey);
    boost_check( std::equal(buffer,buffer+7,masked) );
    boost_check_equal( pkey_temp, frame::circshift_prepared_key(pkey,3) );

    pkey_temp = frame::word_mask_circ(buffer+7,8,pkey_temp);
    boost_check( std::equal(buffer,buffer+16,masked) );
    boost_check_equal( pkey_temp, frame::circshift_prepared_key(pkey,3) );
}

boost_auto_test_case( continuous_byte_mask_inplace ) {
    uint8_t buffer[16];

    uint8_t masked[16] = {0x00, 0x01, 0x02, 0x03,
                          0x00, 0x01, 0x02, 0x03,
                          0x00, 0x01, 0x02, 0x03,
                          0x00, 0x01, 0x02, 0x00};

    frame::masking_key_type key;
    key.c[0] = 0x00;
    key.c[1] = 0x01;
    key.c[2] = 0x02;
    key.c[3] = 0x03;

    // one call
    size_t pkey,pkey_temp;
    pkey = frame::prepare_masking_key(key);
    std::fill_n(buffer,16,0x00);
    frame::byte_mask_circ(buffer,15,pkey);
    boost_check( std::equal(buffer,buffer+16,masked) );

    // calls not split on word boundaries
    pkey = frame::prepare_masking_key(key);
    std::fill_n(buffer,16,0x00);

    pkey_temp = frame::byte_mask_circ(buffer,7,pkey);
    boost_check( std::equal(buffer,buffer+7,masked) );
    boost_check_equal( pkey_temp, frame::circshift_prepared_key(pkey,3) );

    pkey_temp = frame::byte_mask_circ(buffer+7,8,pkey_temp);
    boost_check( std::equal(buffer,buffer+16,masked) );
    boost_check_equal( pkey_temp, frame::circshift_prepared_key(pkey,3) );
}

boost_auto_test_case( continuous_word_mask2 ) {
    uint8_t buffer[12] = {0xa6, 0x15, 0x97, 0xb9,
                          0x81, 0x50, 0xac, 0xba,
                          0x9c, 0x1c, 0x9f, 0xf4};

    uint8_t unmasked[12] = {0x48, 0x65, 0x6c, 0x6c,
                            0x6f, 0x20, 0x57, 0x6f,
                            0x72, 0x6c, 0x64, 0x21};

    frame::masking_key_type key;
    key.c[0] = 0xee;
    key.c[1] = 0x70;
    key.c[2] = 0xfb;
    key.c[3] = 0xd5;

    // one call
    size_t pkey;
    pkey = frame::prepare_masking_key(key);
    frame::word_mask_circ(buffer,12,pkey);
    boost_check( std::equal(buffer,buffer+12,unmasked) );
}
