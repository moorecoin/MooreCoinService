# peer.cpp

- move magic number constants into tuning.h / peer constants enum 

- wrap lines to 80 columns

- use journal

- pass journal in the ctor argument list

- use m_socket->remote_endpoint() instead of m_remoteaddress in all cases.
  for async_connect pass the ipaddress in the bind to onconnect completion
  handler (so we know the address if the connect fails). for proxy, to recover
  the original remote address (of the elb) use m_socket->next_layer()->remote_endpoint(),
  and use m_socket->remote_endpoint() to get the "real ip" reported in the proxy
  handshake.

- handle operation_aborted correctly, work with peers.cpp to properly handle
  a stop. peers need to be gracefully disconnected, the listening socket closed
  on the stop to prevent new connections (and new connections that slip
  through should be refused). the peers object needs to know when the last
  peer has finished closing either gracefully or from an expired graceful
  close timer.

- handle graceful connection closure (with a graceful close timeout). during
  a graceful close, throttle incoming data (by not issuing new socket reads),
  discard any received messages, drain the outbound queue, and tear down
  the socket when the last send completes.

- peerimp should construct with the socket, using a move-assign or swap.
  peerdoor should not have to create a peer object, it should just create a
  socket, accept the connection, and then construct the peer object.

- no more strings for ip addresses and ports. always use ipaddress. we can
  have a version of connect() that takes a string but it should either convert
  it to ipaddress if its parseable, else perform a name resolution.

- stop calling getnativesocket() this and that, just go through m_socket.

- properly handle operation_aborted in all the callbacks.

- move all the function definitions into the class declaration.

- replace macros with language constants.

- stop checking for exceptions, just handle errors correctly.

- move the business logic out of the peer class. socket operations and business
  logic should not be mixed together. we can declare a new class peerlogic to
  abstract the details of message processing.

- change m_nodepublickey from rippleaddress to ripplepublickey, change the
  header, and modify all call sites to use the new type instead of the old. this
  might require adding some compatibility functions to ripplepublickey.

- remove public functions that are not used outside of peer.cpp

# peers.cpp

- add peer::config instead of using getconfig() to pass in configuration data

# peerset.cpp

- remove to cyclic dependency on inboundledger (logging only)

- convert to journal
