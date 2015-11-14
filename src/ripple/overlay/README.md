# overlay

## introduction

the _ripple payment network_ consists of a collection of _peers_ running
**rippled**. each peer maintains multiple outgoing connections and optional
incoming connections to other peers. these connections are made over both
the public internet and private local area networks. this network defines a
fully connected directed graph of nodes where vertices are instances of rippled
and edges are persistent tcp/ip connections. peers send and receive messages to
other connected peers. this peer to peer network, layered on top of the public
and private internet, forms an [_overlay network_][overlay_network]. the
contents of the messages and the behavior of peers in response to the messages,
plus the information exchanged during the handshaking phase of connection
establishment, defines the _ripple peer protocol_ (_protocol_ in this context).

## overview

each connection is represented by a _peer_ object. the overlay manager
establishes, receives, and maintains connections to peers. protocol
messages are exchanged between peers and serialized using
[_google protocol buffers_][protocol_buffers].

### structure

each connection between peers is identified by its connection type, which
affects the behavior of message routing:

* leaf

* peer

## roles

depending on the type of connection desired, the peers will modify their
behavior according to certain roles:

### leaf or superpeer

a peer in the leaf role does not route messages. in the superpeer role, a
peer accepts incoming connections from other leaves and superpeers up to the
configured slot limit. it also routes messages. for a particular connection,
the choice of leaf or superpeer is mutually exclusive. however, a peer can
operate in both the leaf and superpeer role for different connections. one of
the requirements 

### client handler

while not part of the responsibilities of the overlay module, a peer
operating in the client handler role accepts incoming connections from clients
and services them through the json-rpc interface. a peer can operate in either
the leaf or superpeer roles while also operating as a client handler.

## handshake

to establish a protocol connection, a peer makes an outgoing tls encrypted
connection to a remote peer, then sends a http request with no message body.
the request uses the [_http/1.1 upgrade_][upgrade_header] mechanism with some
custom fields to communicate protocol specific information:

```
get / http/1.1
user-agent: rippled-0.27.0
local-address: 192.168.0.101:8421
upgrade: rtxp/1.2, rtxp/1.3
connection: upgrade
connect-as: leaf, peer
accept-encoding: identity, zlib, snappy
public-key: abroqibi2jpdofohoofuzzi9nezkw9zdfc4exvnmuxhajpsph8uj
session-signature: 71ed064155ffadfa38782c5e0158cb26
```

upon receipt of a well-formed http request the remote peer will send back a
http response indicating the connection status:

```
http/1.1 101 switching protocols
server: rippled-0.27.0
remote-address: 63.104.209.13
upgrade: rtxp/1.2
connection: upgrade
connect-as: leaf
transfer-encoding: snappy
public-key: abroqibi2jpdofohoofuzzi9nezkw9zdfc4exvnmuxhajpsph8uj
session-signature: 71ed064155ffadfa38782c5e0158cb26
```

if the remote peer has no available slots, the http status code 503 (service
unavailable) is returned, with an optional content body in json format that
may contain additional information such as ip and port addresses of other
servers that may have open slots:

```
http/1.1 503 service unavailable
server: rippled-0.27.0
remote-address: 63.104.209.13
content-length: 253
content-type: application/json
{"peer-ips":["54.68.219.39:51235","54.187.191.179:51235",
"107.150.55.21:6561","54.186.230.77:51235","54.187.110.243:51235",
"85.127.34.221:51235","50.43.33.236:51235","54.187.138.75:51235"]}
```

### fields

* *url*

    the url in the request line must be a single forward slash character
    ("/"). requests with any other url must be rejected. different url strings
    are reserved for future protocol implementations.

* *http version*

    the minimum required http version is 1.1. requests for http versions
    earlier than 1.1 must be rejected.

* `user-agent`

    contains information about the software originating the request.
    the specification is identical to rfc2616 section 14.43.

* `server`

    contains information about the software providing the response. the
    specification is identical to rfc2616 section 14.38.

* `remote-address` (optional)

    this optional field contains the string representation of the ip
    address of the remote end of the connection as seen by the peer.
    by observing values of this field from a sufficient number of different
    servers, a peer making outgoing connections can deduce its own ip address.

* `upgrade`

    this field must be present and for requests consist of a comma delimited
    list of at least one element where each element is of the form "rtxp/"
    followed by the dotted major and minor protocol version number. for
    responses the value must be a single element matching one of the elements
    provided in the corresponding request field. if the server does not
    understand any of the requested protocols, 

* `connection`

    this field must be present, containing the value 'upgrade'.

* `connect-as`

    for requests the value consists of a comma delimited list of elements
    where each element describes a possible connection type. current connection
    types are:

    - leaf
    - peer

    if this field is omitted or the value is the empty string, then 'leaf' is
    assumed.

    for responses, the value must consist of exactly one element from the list
    of elements specified in the request. if a server does not recognize any
    of the connection types it must return a http error response.

* `public-key`

    this field value must be present, and contain a base 64 encoded value used
    as a server public key identifier.

* `session-signature`

    this field must be present. it contains a cryptographic token formed
    from the sha512 hash of the shared data exchanged during ssl handshaking.
    for more details see the corresponding source code.

* `crawl` (optional)

    if present, and the value is "public" then neighbors will report the ip
    address to crawler requests. if absent, neighbor's default behavior is to
    not report ip addresses.

* _user defined_ (unimplemented)

    the rippled operator may specify additional, optional fields and values
    through the configuration. these headers will be transmitted in the
    corresponding request or response messages.

---

[overlay_network]: http://en.wikipedia.org/wiki/overlay_network
[protocol_buffers]: https://developers.google.com/protocol-buffers/
[upgrade_header]: http://en.wikipedia.org/wiki/http/1.1_upgrade_header
