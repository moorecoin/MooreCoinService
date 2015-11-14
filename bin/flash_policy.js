#!/usr/bin/node
//
// this program allows ie 9 ripple-clients to make websocket connections to
// rippled using flash.  as ie 9 does not have websocket support, this required
// if you wish to support ie 9 ripple-clients.
//
// http://www.lightsphere.com/dev/articles/flash_socket_policy.html
//
// for better security, be sure to set the port below to the port of your
// [websocket_public_port].
//

var net	    = require("net"),
    port    = "*",
    domains = ["*:"+port]; // domain:port

net.createserver(
  function(socket) {
    socket.write("<?xml version='1.0' ?>\n");
    socket.write("<!doctype cross-domain-policy system 'http://www.macromedia.com/xml/dtds/cross-domain-policy.dtd'>\n");
    socket.write("<cross-domain-policy>\n");
    domains.foreach(
      function(domain) {
        var parts = domain.split(':');
        socket.write("\t<allow-access-from domain='" + parts[0] + "' to-ports='" + parts[1] + "' />\n");
      }
    );
    socket.write("</cross-domain-policy>\n");
    socket.end();
  }
).listen(843);
