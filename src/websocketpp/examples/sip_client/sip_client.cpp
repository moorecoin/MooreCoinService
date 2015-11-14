#include <condition_variable>

#include <websocketpp/config/asio_no_tls_client.hpp>

#include <websocketpp/client.hpp>

#include <iostream>

#include <boost/thread/thread.hpp>

typedef websocketpp::client<websocketpp::config::asio_client> client;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

// pull out the type of messages sent by our config
typedef websocketpp::config::asio_client::message_type::ptr message_ptr;

// create a server endpoint
client sip_client;


bool received;

void on_open(client* c, websocketpp::connection_hdl hdl) {
    // now it is safe to use the connection
    std::cout << "connection ready" << std::endl;

    received=false;
    // send a sip options message to the server:
    std::string sip_msg="options sip:carol@chicago.com sip/2.0\r\nvia: sip/2.0/ws df7jal23ls0d.invalid;rport;branch=z9hg4bkhjhs8ass877\r\nmax-forwards: 70\r\nto: <sip:carol@chicago.com>\r\nfrom: alice <sip:alice@atlanta.com>;tag=1928301774\r\ncall-id: a84b4c76e66710\r\ncseq: 63104 options\r\ncontact: <sip:alice@pc33.atlanta.com>\r\naccept: application/sdp\r\ncontent-length: 0\r\n\r\n";
    sip_client.send(hdl, sip_msg.c_str(), websocketpp::frame::opcode::text);
}

void on_message(client* c, websocketpp::connection_hdl hdl, message_ptr msg) {
    client::connection_ptr con = sip_client.get_con_from_hdl(hdl);

    std::cout << "received a reply:" << std::endl;
    fwrite(msg->get_payload().c_str(), msg->get_payload().size(), 1, stdout);
    received=true;
}

int main(int argc, char* argv[]) {

    std::string uri = "ws://localhost:9001";

    if (argc == 2) {
        uri = argv[1];
    }

    try {
        // we expect there to be a lot of errors, so suppress them
        sip_client.clear_access_channels(websocketpp::log::alevel::all);
        sip_client.clear_error_channels(websocketpp::log::elevel::all);

        // initialize asio
        sip_client.init_asio();

        // register our handlers
        sip_client.set_open_handler(bind(&on_open,&sip_client,::_1));
        sip_client.set_message_handler(bind(&on_message,&sip_client,::_1,::_2));

        websocketpp::lib::error_code ec;
        client::connection_ptr con = sip_client.get_connection(uri, ec);

        // specify the sip subprotocol:
        con->add_subprotocol("sip");

        sip_client.connect(con);

        // start the asio io_service run loop
        sip_client.run();

        while(!received) {
            boost::this_thread::sleep(boost::posix_time::milliseconds(100));
        }

        std::cout << "done" << std::endl;

    } catch (const std::exception & e) {
        std::cout << e.what() << std::endl;
    } catch (websocketpp::lib::error_code e) {
        std::cout << e.message() << std::endl;
    } catch (...) {
        std::cout << "other exception" << std::endl;
    }
}
