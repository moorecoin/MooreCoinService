
# peerfinder

## introduction

the _ripple payment network_ consists of a collection of _peers_ running the
**rippled software**. each peer maintains multiple outgoing connections and
optional incoming connections to other peers. these connections are made over
both the public internet and private local area networks. this network defines
a fully connected directed graph of nodes. peers send and receive messages to
other connected peers. this peer to peer network, layered on top of the public
and private internet, forms an [_overlay network_][overlay_network].

## bootstrapping

when a peer comes online it needs a set of ip addresses to connect to in order to
gain initial entry into the overlay in a process called _bootstrapping_. once they
have established an initial set of these outbound peer connections, they need to
gain additional addresses to establish more outbound peer connections until the
desired limit is reached. furthermore, they need a mechanism to advertise their
ip address to new or existing peers in the overlay so they may receive inbound
connections up to some desired limit. and finally, they need a mechanism to provide
inbound connection requests with an alternate set of ip addresses to try when they
have already reached their desired maximum number of inbound connections.

peerfinder is a self contained module that provides these services, along with some
additional overlay network management services such as _fixed slots_ and _cluster
slots_.

## features

peerfinder has these responsibilities

* maintain a persistent set of endpoint addresses suitable for bootstrapping
  into the peer to peer overlay, ranked by relative locally observed utility.

* send and receive protocol messages for discovery of endpoint addresses.

* provide endpoint addresses to new peers that need them.

* maintain connections to a configured set of fixed peers.

* impose limits on the various slots consumed by peer connections.

* initiate outgoing connection attempts to endpoint addresses to maintain the
  overlay connectivity and fixed peer policies.

* verify the connectivity of neighbors who advertise inbound connection slots.

* prevent duplicate connections and connections to self.

---

# concepts

## manager

the `manager` is an application singleton which provides the primary interface
to interaction with the peerfinder.

### autoconnect

the autoconnect feature of peerfinder automatically establishes outgoing
connections using addresses learned from various sources including the
configuration file, the result of domain name lookups, and messages received
from the overlay itself.

### callback

peerfinder is an isolated code module with few external dependencies. to perform
socket specific activities such as establishing outgoing connections or sending
messages to connected peers, the manager is constructed with an abstract
interface called the `callback`. an instance of this interface performs the
actual required operations, making peerfinder independent of the calling code.

### config

the `config` structure defines the operational parameters of the peerfinder.
some values come from the configuration file while others are calculated via
tuned heuristics. the fields are as follows:

* `autoconnect`
 
  a flag indicating whether or not the autoconnect feature is enabled.

* `wantincoming`

  a flag indicating whether or not the peer desires inbound connections. when
  this flag is turned off, a peer will not advertise itself in endpoint
  messages.

* `listeningport`

  the port number to use when creating the listening socket for peer
  connections.

* `maxpeers`

  the largest number of active peer connections to allow. this includes inbound
  and outbound connections, but excludes fixed and cluster peers. there is an
  implementation defined floor on this value.

* `outpeers`

  the number of automatic outbound connections that peerfinder will maintain
  when the autoconnect feature is enabled. the value is computed with fractional
  precision as an implementation defined percentage of `maxpeers` subject to
  an implementation defined floor. an instance of the peerfinder rounds the
  fractional part up or down using a uniform random number generated at
  program startup. this allows the outdegree of the overlay network to be
  controlled with fractional precision, ensuring that all inbound network
  connection slots are not consumed (which would it difficult for new
  participants to enter the network).

here's an example of how the network might be structured with a fractional
value for outpeers:

**(need example here)**

### livecache

the livecache holds relayed ip addresses that have been received recently in
the form of endpoint messages via the peer to peer overlay. a peer periodically
broadcasts the endpoint message to its neighbors when it has open inbound
connection slots. peers store these messages in the livecache and periodically
forward their neighbors a handful of random entries from their livecache, with
an incremented hop count for each forwarded entry.

the algorithm for sending a neighbor a set of endpoint messages chooses evenly
from all available hop counts on each send. this ensures that each peer
will see some entries with the farthest hops at each iteration. the result is
to expand a peer's horizon with respect to which overlay endpoints are visible.
this is designed to force the overlay to become highly connected and reduce
the network diameter with each connection establishment.

when a peer receives an endpoint message that originates from a neighbor
(identified by a hop count of zero) for the first time, it performs an incoming
connection test on that neighbor by initiating an outgoing connection to the
remote ip address as seen on the connection combined with the port advertised
in the endpoint message. if the test fails, then the peer considers its neighbor
firewalled (intentionally or due to misconfiguration) and no longer forwards
endpoint messages for that peer. this prevents poor quality unconnectible
addresses from landing in the caches. if the incoming connection test passes,
then the peer fills in the endpoint message with the remote address as seen on
the connection before storing it in its cache and forwarding it to other peers.
this relieves the neighbor from the responsibility of knowing its own ip address
before it can start receiving incoming connections.

livecache entries expire quickly. since a peer stops advertising itself when
it no longer has available inbound slots, its address will shortly after stop
being handed out by other peers. livecache entries are very likely to result
in both a successful connection establishment and the acquisition of an active
outbound slot. compare this with bootcache addresses, which are very likely to
be connectible but unlikely to have an open slot.

because entries in the livecache are ephemeral, they are not persisted across
launches in the database. the livecache is continually updated and expired as
endpoint messages are received from the overlay over time.

### bootcache

the `bootcache` stores ip addresses useful for gaining initial connections.
each address is associated with the following metadata:
 
* **valence**

  a signed integer which represents the number of successful
  consecutive connection attempts when positive, and the number of
  failed consecutive connection attempts when negative. if an outgoing
  connection attempt to the corresponding ip address fails to complete the
  handshake the valence is reset to negative one. this harsh penalty is
  intended to prevent popular servers from forever remaining top ranked in
  all peer databases.

when choosing addresses from the boot cache for the purpose of
establishing outgoing connections, addresses are ranked in decreasing order of
valence. the bootcache is persistent. entries are periodically inserted and
updated in the corresponding sqlite database during program operation. when
**rippled** is launched, the existing bootcache database data is accessed and
loaded to accelerate the bootstrap process.

desirable entries in the bootcache are addresses for servers which are known to
have high uptimes, and for which connection attempts usually succeed. however,
these servers do not necessarily have available inbound connection slots.
however, it is assured that these servers will have a well populated livecache
since they will have moved towards the core of the overlay over their high
uptime. when a connected server is full it will return a handful of new
addresses from its livecache and gracefully close the connection. addresses
from the livecache are highly likely to have inbound connection slots and be
connectible.

for security, all information that contributes to the ranking of bootcache
entries is observed locally. peerfinder never trusts external sources of information.

### slot

each tcp/ip socket that can participate in the peer to peer overlay occupies
a slot. slots have properties and state associated with them:

#### state (slot)

the slot state represents the current stage of the connection as it passes
through the business logic for establishing peer connections.

* `accept`

  the accept state is an initial state resulting from accepting an incoming
  connection request on a listening socket. the remote ip address and port
  are known, and a handshake is expected next.

* `connect`

  the connect state is an initial state used when actively establishing outbound
  connection attempts. the desired remote ip address and port are known.

* `connected`

  when an outbound connection attempt succeeds, it moves to the connected state.
  the handshake is initiated but not completed.

* `active`

  the state becomes active when a connection in either the accepted or connected
  state completes the handshake process, and a slot is available based on the
  properties. if no slot is available when the handshake completes, the socket
  is gracefully closed.

* `closing`

  the closing state represents a connected socket in the process of being
  gracefully closed.

#### properties (slot)

slot properties may be combined and are not mutually exclusive.

* **inbound**

  an inbound slot is the condition of a socket which has accepted an incoming
  connection request. a connection which is not inbound is by definition
  outbound.

* **fixed**

  a fixed slot is a desired connection to a known peer identified by ip address,
  usually entered manually in the configuration file. for the purpose of
  establishing outbound connections, the peer also has an associated port number
  although only the ip address is checked to determine if the fixed peer is
  already connected. fixed slots do not count towards connection limits.

* **cluster**

  a cluster slot is a connection which has completed the handshake stage, whose
  public key matches a known public key usually entered manually in the
  configuration file or learned through overlay messages from other trusted
  peers. cluster slots do not count towards connection limits.

* **superpeer** (forthcoming)

  a superpeer slot is a connection to a peer which can accept incoming
  connections, meets certain resource availaibility requirements (such as
  bandwidth, cpu, and storage capacity), and operates full duplex in the
  overlay. connections which are not superpeers are by definition leaves. a
  leaf slot is a connection to a peer which does not route overlay messages to
  other peers, and operates in a partial half duplex fashion in the overlay.

#### fixed slots

fixed slots are identified by ip address and set up during the initialization
of the manager, usually from the configuration file. the logic will always make
outgoing connection attempts to each fixed slot which is not currently
connected. if we receive an inbound connection from an endpoint whose address
portion (without port) matches a fixed slot address, we consider the fixed
slot to be connected.

#### cluster slots

cluster slots are identified by the public key and set up during the
initialization of the manager or discovered upon receipt of messages in the
overlay from trusted connections.

--------------------------------------------------------------------------------

# algorithms

## connection strategy

the _connection strategy_ applies the configuration settings to establish
desired outbound connections. it runs periodically and progresses through a
series of stages, remaining in each stage until a condition is met

### stage 1: fixed slots

this stage is invoked when the number of active fixed connections is below the
number of fixed connections specified in the configuration, and one of the
following is true:

* there are eligible fixed addresses to try
* any outbound connection attempts are in progress

each fixed address is associated with a retry timer. on a fixed connection
failure, the timer is reset so that the address is not tried for some amount
of time, which increases according to a scheduled sequence up to some maximum
which is currently set to approximately one hour between retries. a fixed
address is considered eligible if we are not currently connected or attempting
the address, and its retry timer has expired.

the peerfinder makes its best effort to become fully connected to the fixed
addresses specified in the configuration file before moving on to establish
outgoing connections to foreign peers. this security feature helps rippled
establish itself with a trusted set of peers first before accepting untrusted
data from the network.

### stage 2: livecache

the livecache is invoked when stage 1 is not active, autoconnect is enabled,
and the number of active outbound connections is below the number desired. the
stage remains active while:

* the livecache has addresses to try
* any outbound connection attempts are in progress

peerfinder makes its best effort to exhaust addresses in the livecache before
moving on to the bootcache, because livecache addresses are highly likely
to be connectible (since they are known to have been online within the last
minute), and highly likely to have an open slot for an incoming connection
(because peers only advertise themselves in the livecache when they have
open slots).

### stage 3: bootcache

the bootcache is invoked when stage 1 and stage 2 are not active, autoconnect
is enabled, and the number of active outbound connections is below the number
desired. the stage remains active while:

* there are addresses in the cache that have not been tried recently.

entries in the bootcache are ranked, with highly connectible addresses preferred
over others. connection attempts to bootcache addresses are very likely to
succeed but unlikely to produce an active connection since the peers likely do
not have open slots. before the remote peer closes the connection it will send
a handful of addresses from its livecache to help the new peer coming online
obtain connections.

--------------------------------------------------------------------------------

# references

much of the work in peerfinder was inspired by earlier work in gnutella:

[revised gnutella ping pong scheme](http://rfc-gnutella.sourceforge.net/src/pong-caching.html)<br>
_by christopher rohrs and vincent falco_

[gnutella 0.6 protocol:](http://rfc-gnutella.sourceforge.net/src/rfc-0_6-draft.html) sections:
* 2.2.2   ping (0x00)
* 2.2.3   pong (0x01)
* 2.2.4   use of ping and pong messages
* 2.2.4.1   a simple pong caching scheme
* 2.2.4.2   other pong caching schemes

[overlay_network]: http://en.wikipedia.org/wiki/overlay_network
