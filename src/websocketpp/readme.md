websocket++ (0.4.0)
==========================

websocket++ is a header only c++ library that implements rfc6455 the websocket
protocol. it allows integrating websocket client and server functionality into
c++ programs. it uses interchangeable network transport modules including one
based on c++ iostreams and one based on boost asio.

major features
==============
* full support for rfc6455
* partial support for hixie 76 / hybi 00, 07-17 draft specs (server only)
* message/event based interface
* supports secure websockets (tls), ipv6, and explicit proxies.
* flexible dependency management (c++11 standard library or boost)
* interchangeable network transport modules (iostream and boost asio)
* portable/cross platform (posix/windows, 32/64bit, intel/arm/ppc)
* thread-safe

get involved
============

[![build status](https://travis-ci.org/zaphoyd/websocketpp.png)](https://travis-ci.org/zaphoyd/websocketpp)

**project website**
http://www.zaphoyd.com/websocketpp/

**user manual**
http://www.zaphoyd.com/websocketpp/manual/

**github repository**
https://github.com/zaphoyd/websocketpp/

**announcements mailing list**
http://groups.google.com/group/websocketpp-announcements/

**irc channel**
 #websocketpp (freenode)

**discussion / development / support mailing list / forum**
http://groups.google.com/group/websocketpp/

author
======
peter thorson - websocketpp@zaphoyd.com
