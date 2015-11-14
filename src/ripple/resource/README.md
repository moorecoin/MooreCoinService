# resource::manager #

the resourcemanager module has these responsibilities:

- uniquely identify endpoints which impose load.
- track the load used across endpoints.
- provide an interface to share load information in a cluster.
- warn and/or disconnect endpoints for imposing load.

## description ##

to prevent monopolization of server resources or attacks on servers,
resource consumption is monitored at each endpoint. when consumption
exceeds certain thresholds, costs are imposed. costs could include charging
additional xrp for transactions, requiring a proof of work to be
performed, or simply disconnecting the endpoint.

currently, consumption endpoints include websocket connections used to
service clients, and peer connections used to create the peer to peer
overlay network implementing the ripple protcool.

the current "balance" of a consumer represents resource consumption
debt or credit. debt is accrued when bad loads are imposed. credit is
granted when good loads are imposed. when the balance crosses heuristic
thresholds, costs are increased on the endpoint. the balance is
represented as a unitless relative quantity. this balance is currently
held by the entry struct in the impl/entry.h file.

costs associated with specific transactions are defined in the
impl/fees files.

although rpc connections consume resources, they are transient and
cannot be rate limited. it is advised not to expose rpc interfaces
to the general public.

## consumer types ##

consumers are placed into three classifications (as identified by the
resource::kind enumeration):

 - inbound,
 - outbound, and
 - admin

 each caller determines for itself the classification of the consumer it is
 creating.

## resource loading ##

it is expected that a client will impose a higher load on the server
when it first connects: the client may need to catch up on transactions
it has missed, or get trust lines, or transfer fees.  the manager must
expect this initial peak load, but not allow that high load to continue
because over the long term that would unduly stress the server.

if a client places a sustained high load on the server, that client
is initially given a warning message.  if that high load continues
the manager may tell the heavily loaded server to drop the connection
entirely and not allow re-connection for some amount of time.

each load is monitored by capturing peaks and then decaying those peak
values over time: this is implemented by the decayingsample class.

## gossip ##

each server in a cluster creates a list of ip addresses of end points
that are imposing a significant load.  this list is called gossip, which
is passed to other nodes in that cluster.  gossip helps individual
servers in the cluster identify ip addreses that might be unduly loading
the entire cluster.  again the recourse of the individual servers is to
drop connections to those ip addresses that occur commonly in the gossip.

## access ##

in rippled, the application holds a unique instance of resource::manager,
which may be retrieved by calling the method
`application::getresourcemanager()`.
