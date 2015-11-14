# fee voting

the ripple payment protocol enforces a fee schedule expressed in units of the
native currency, xrp. fees for transactions are paid directly from the account
owner. there are also reserve requirements for each item that occupies storage
in the ledger. the reserve fee schedule contains both a per-account reserve,
and a per-owned-item reserve. the items an account may own include active
offers, trust lines, and tickets.

validators may vote to increase fees if they feel that the network is charging
too little. they may also vote to decrease fees if the fees are too costly
relative to the value the network provides. one common case where a validator
may want to change fees is when the value of the native currency xrp fluctuates
relative to other currencies.

the fee voting mechanism takes place every 256 ledgers ("voting ledgers"). in
a voting ledger, each validator takes a position on what they think the fees
should be. the consensus process converges on the majority position, and in
subsequent ledgers a new fee schedule is enacted.

## consensus

the ripple consensus algorithm allows distributed participants to arrive at
the same answer for yes/no questions. the canonical case for consensus is
whether or not a particular transaction is included in the ledger. fees
present a more difficult challenge, since the decision on the new fee is not
a yes or no question.

to convert validators' positions on fees into a yes or no question that can
be converged in the consensus process, the following algorithm is used:

- in the ledger before a voting ledger, validators send proposals which also
  include the values they think the network should use for the new fee schedule.

- in the voting ledger, validators examine the proposals from other validators
  and choose a new fee schedule which moves the fees in a direction closer to
  the validator's ideal fee schedule and is also likely to be accepted. a fee
  amount is likely to be accepted if a majority of validators agree on the
  number.

- each validator injects a "pseudo transaction" into their proposed ledger
  which sets the fees to the chosen schedule.

- the consensus process is applied to these fee-setting transactions as normal.
  each transaction is either included in the ledger or not. in most cases, one
  fee setting transaction will make it in while the others are rejected. in
  some rare cases more than one fee setting transaction will make it in. the
  last one to be applied will take effect. this is harmless since a majority
  of validators still agreed on it.

- after the voting ledger has been validated, future pseudo transactions
  before the next voting ledger are rejected as fee setting transactions may
  only appear in voting ledgers.

## configuration

a validating instance of rippled uses information in the configuration file
to determine how it wants to vote on the fee schedule. it is the responsibility
of the administrator to set these values.

---

# amendment

an amendment is a new or proposed change to a ledger rule. ledger rules affect 
transaction processing and consensus; peers must use the same set of rules for 
consensus to succeed, otherwise different instances of rippled will get 
different results. amendments can be almost anything but they must be accepted 
by a network majority through a consensus process before they are utilized. an 
amendment must receive at least an 80% approval rate from validating nodes for 
a period of two weeks before being accepted. the following example outlines the 
process of an amendment from its conception to approval and usage. 

*  a community member makes proposes to change transaction processing in some 
  way. the proposal is discussed amongst the community and receives its support 
  creating a community or human consensus. 

*  some members contribute their time and work to develop the amendment.

*  a pull request is created and the new code is folded into a rippled build 
  and made available for use.

*  the consensus process begins with the validating nodes.

*  if the amendment holds an 80% majority for a two week period, nodes will begin 
  including the transaction to enable it in their initial sets.

nodes may veto amendments they consider undesirable by never announcing their 
support for those amendments. just a few nodes vetoing an amendment will normally 
keep it from being accepted. nodes could also vote yes on an amendments even 
before it obtains a super-majority. this might make sense for a critical bug fix.

---

# shamapstore: online delete

optional online deletion happens through the shamapstore. records are deleted
from disk based on ledger sequence number. these records reside in the
key-value database  as well as in the sqlite ledger and transaction databases.
without online deletion storage usage grows without bounds. it can only
be pruned by stopping, manually deleting data, and restarting the server.
online deletion requires less operator intervention to manage the server.

the main mechanism to delete data from the key-value database is to keep two
databases open at all times. one database has all writes directed to it. the
other database has recent archival data from just prior to that from the current
writable database.
upon rotation, the archival database is deleted. the writable database becomes
archival, and a brand new database becomes writable. to ensure that no
necessary data for transaction processing is lost, a variety of steps occur,
including copying the contents of an entire ledger's account state map,
clearing caches, and copying the contents of (freshening) other caches.

deleting from sqlite involves more straight-forward sql delete queries from
the respective tables, with a rudimentary back-off algorithm to do portions
of the deletions at a time. this back-off is in place so that the database
lock is not held excessively. the sqlite database is not configured to
delete on-disk storage, so it will grow over time. however, with online delete
enabled, it grows at a very small rate compared with the key-value store.

the online delete routine aborts its current activities if it fails periodic
server health checks. this minimizes impact of i/o and locking of critical
objects. if interrupted, the routine will start again at the next validated
ledger close. likewise, the routine will continue in a similar fashion if the
server restarts.

configuration:

* in the [node_db] configuration section, an optional online_delete parameter is
set. if not set or if set to 0, online delete is disabled. otherwise, the
setting defines number of ledgers between deletion cycles.
* another optional parameter in [node_db] is that for advisory_delete. it is
disabled by default. if set to non-zero, requires an rpc call to activate the
deletion routine.
* online_delete must not be greater than the [ledger_history] parameter.
* [fetch_depth] will be silently set to equal the online_delete setting if
online_delete is greater than fetch_depth.
* in the [node_db] section, there is a performance tuning option, delete_batch,
which sets the maximum size in ledgers for each sql delete query.
