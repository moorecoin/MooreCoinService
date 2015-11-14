
# ledger process #

## introduction ##

## life cycle ##

every server always has an open ledger. all received new transactions are
applied to the open ledger. the open ledger can't close until we reach
a consensus on the previous ledger, and either: there is at least one
transaction in the open ledger, or the ledger's close time has been reached.

when the open ledger is closed the transactions in the open ledger become
the initial proposal. validators will send the proposal (non-validators will
simply not send the proposal). the open ledger contains the set of transactions
the server thinks should go into the next ledger.

once the ledger is closed, servers avalanche to consensus on the candidate
transaction set and the close time. when consensus is reached, servers build
a new last closed ledger by starting with the previous last closed ledger and
applying the consensus transaction set. in the normal case, servers will all
agree on both the last closed ledger and the consensus transaction set. the
goal is to give the network the highest chances of arriving at the same
conclusion on all servers.

at all times during the consensus process the open ledger remains open with the
same transaction set, and has new transactions applied. it will likely have
transactions that are also in the new last closed ledger. valid transactions
received during the consensus process will only be in the open ledger.

validators now publish validations of the new last closed ledger. servers now
build a new open ledger from the last closed ledger. first, all transactions
that were candidates in the previous consensus round but didn't make it into
the consensus set are applied. next, transactions in the current open ledger
are applied. this covers transactions received during the previous consensus
round. this is a "rebase": now that we know the real history, the current open
ledger is rebased against the last closed ledger.

the purpose of the open ledger is as follows:
- forms the basis of the initial proposal during consensus
- used to decide if we can reject the transaction without relaying it

## byzantine failures ##

byzantine failures are resolved as follows. if there is a supermajority ledger,
then a minority of validators will discover that the consensus round is
proceeding on a different ledger than they thought. these validators will
become desynced, and switch to a strategy of trying to acquire the consensus
ledger.

if there is no majority ledger, then starting on the next consensus round there
will not be a consensus on the last closed ledger. another avalanche process
is started.

## validators ##

the only meaningful difference between a validator and a 'regular' server is
that the validator sends its proposals and validations to the network.

---

# the ledger stream #

## ledger priorities ##

there are two ledgers that are the most important for a rippled server to have:

 - the consensus ledger and
 - the last validated ledger.

if we need either of those two ledgers they are fetched with the highest
priority.  also, when they arrive, they replace their earlier counterparts
(if they exist).

the `ledgermaster` object tracks
 - the last published ledger,
 - the last validated ledger, and
 - ledger history.
so the `ledgermaster` is at the center of fetching historical ledger data.

in specific, the `ledgermaster::doadvance()` method triggers the code that
fetches historical data and controls the state machine for ledger acquisition.

the server tries to publish an on-going stream of consecutive ledgers to its
clients.  after the server has started and caught up with network
activity, say when ledger 500 is being settled, then the server puts its best
effort into publishing validated ledger 500 followed by validated ledger 501
and then 502.  this effort continues until the server is shut down.

but loading or network connectivity may sometimes interfere with that ledger
stream.  so suppose the server publishes validated ledger 600 and then
receives validated ledger 603.  then the server wants to back fill its ledger
history with ledgers 601 and 602.

the server prioritizes keeping up with current ledgers.  but if it is caught
up on the current ledger, and there are no higher priority demands on the
server, then it will attempt to back fill its historical ledgers.  it fills
in the historical ledger data first by attempting to retrieve it from the
local database.  if the local database does not have all of the necessary data
then the server requests the remaining information from network peers.

suppose the server is missing multiple historical ledgers.  take the previous
example where we have ledgers 603 and 600, but we're missing 601 and 602.  in
that case the server requests information for ledger 602 first, before
back-filling ledger 601.  we want to expand the contiguous range of
most-recent ledgers that the server has locally.  there's also a limit to
how much historical ledger data is useful.  so if we're on ledger 603, but
we're missing ledger 4 we may not bother asking for ledger 4.

## assembling a ledger ##

when data for a ledger arrives from a peer, it may take a while before the
server can apply that data.  so when ledger data arrives we schedule a job
thread to apply that data.  if more data arrives before the job starts we add
that data to the job.  we defer requesting more ledger data until all of the
data we have for that ledger has been processed.  once all of that data is
processed we can intelligently request only the additional data that we need
to fill in the ledger.  this reduces network traffic and minimizes the load
on peers supplying the data.

if we receive data for a ledger that is not currently under construction,
we don't just throw the data away.  in particular the accountstatenodes
may be useful, since they can be re-used across ledgers.  this data is
stashed in memory (not the database) where the acquire process can find
it.

peers deliver ledger data in the order in which the data can be validated.
data arrives in the following order:

 1. the hash of the ledger header
 2. the ledger header
 3. the root nodes of the transaction tree and state tree
 4. the lower (non-root) nodes of the state tree
 5. the lower (non-root) nodes of the transaction tree

inner-most nodes are supplied before outer nodes.  this allows the
requesting server to hook things up (and validate) in the order in which
data arrives.

if this process fails, then a server can also ask for ledger data by hash,
rather than by asking for specific nodes in a ledger.  asking for information
by hash is less efficient, but it allows a peer to return the information
even if the information is not assembled into a tree.  all the peer needs is
the raw data.

## which peer to ask ##

peers go though state transitions as the network goes through its state
transitions.  peer's provide their state to their directly connected peers.
by monitoring the state of each connected peer a server can tell which of
its peers has the information that it needs.

therefore if a server suffers a byzantine failure the server can tell which
of its peers did not suffer that same failure.  so the server knows which
peer(s) to ask for the missing information.

peers also report their contiguous range of ledgers.  this is another way that
a server can determine which peer to ask for a particular ledger or piece of
a ledger.

there are also indirect peer queries.  if there have been timeouts while
acquiring ledger data then a server may issue indirect queries.  in that
case the server receiving the indirect query passes the query along to any
of its peers that may have the requested data.  this is important if the
network has a byzantine failure.  if also helps protect the validation
network.  a validator may need to get a peer set from one of the other
validators, and indirect queries improve the likelihood of success with
that.

## kinds of fetch packs ##

a fetchpack is the way that peers send partial ledger data to other peers
so the receiving peer can reconstruct a ledger.

a 'normal' fetchpack is a bucket of nodes indexed by hash.  the server
building the fetchpack puts information into the fetchpack that the
destination server is likely to need.  normally they contain all of the
missing nodes needed to fill in a ledger.

a 'compact' fetchpack, on the other hand, contains only leaf nodes, no
inner nodes.  because there are no inner nodes, the ledger information that
it contains cannot be validated as the ledger is assembled.  we have to,
initially, take the accuracy of the fetchpack for granted and assemble the
ledger.  once the entire ledger is assembled the entire ledger can be
validated.  but if the ledger does not validate then there's nothing to be
done but throw the entire fetchpack away; there's no way to save a portion
of the fetchpack.

the fetchpacks just described could be termed 'reverse fetchpacks.'  they
only provide historical data.  there may be a use for what could be called a
'forward fetchpack.'  a forward fetchpack would contain the information that
is needed to build a new ledger out of the preceding ledger.

a forward compact fetchpack would need to contain:
 - the header for the new ledger,
 - the leaf nodes of the transaction tree (if there is one),
 - the index of deleted nodes in the state tree,
 - the index and data for new nodes in the state tree, and
 - the index and new data of modified nodes in the state tree.

---

# definitions #

## open ledger ##

the open ledger is the ledger that the server applies all new incoming
transactions to.

## last validated ledger ##

the most recent ledger that the server is certain will always remain part
of the permanent, public history.

## last closed ledger ##

the most recent ledger that the server believes the network reached consensus
on. different servers can arrive at a different conclusion about the last
closed ledger. this is a consequence of byzantanine failure. the purpose of
validations is to resolve the differences between servers and come to a common
conclusion about which last closed ledger is authoritative.

## consensus ##

a distributed agreement protocol. ripple uses the consensus process to solve
the problem of double-spending.

## validation ##

a signed statement indicating that it built a particular ledger as a result
of the consensus process.

## proposal ##

a signed statement of which transactions it believes should be included in
the next consensus ledger.

## ledger header ##

the "ledger header" is the chunk of data that hashes to the
ledger's hash. it contains the sequence number, parent hash,
hash of the previous ledger, hash of the root node of the
state tree, and so on.

## ledger base ##

the term "ledger base" refers to a particular type of query
and response used in the ledger fetch process that includes
the ledger header but may also contain other information
such as the root node of the state tree.

---

# ledger structures #

## account root ##

**account:** a 160-bit account id.

**balance:** balance in the account.

**flags:** ???

**ledgerentrytype:** "accountroot"

**ownercount:** the number of items the account owns that are charged to the
account.  offers are charged to the account.  trust lines may be charged to
the account (but not necessarily).  the ownercount determines the reserve on
the account.

**previoustxnid:** 256-bit index of the previous transaction on this account.

**previoustxnlgrseq:** ledger number sequence number of the previous
transaction on this account.

**sequence:** must be a value of 1 for the account to process a valid
transaction.  the value initially matches the sequence number of the state
tree of the account that signed the transaction.  the process of executing
the transaction increments the sequence number.  this is how ripple prevents
a transaction from executing more than once.

**index:** 256-bit hash of this accountroot.


## trust line ##

the trust line acts as an edge connecting two accounts: the accounts
represented by the highnode and the lownode.  which account is "high" and
"low" is determined by the values of the two 160-bit account ids.  the
account with the smaller 160-bit id is always the low account.  this
ordering makes the hash of a trust line between accounts a and b have the
same value as a trust line between accounts b and a.

**balance:**
 - **currency:** string identifying a valid currency, e.g., "btc".
 - **issuer:** there is no issuer, really, this entry is "noaccount".
 - **value:**

**flags:** ???

**highlimit:**
 - **currency:** same as for balance.
 - **issuer:** a 160-bit account id.
 - **value:** the largest amount this issuer will accept of the currency.

**highnode:** a deletion hint.

**ledgerentrytype:** "ripplestate".

**lowlimit:**
 - **currency:** same as for balance.
 - **issuer:** a 160-bit account id.
 - **value:** the largest amount of the currency this issuer will accept.

**lownode:** a deletion hint

**previoustxnid:** 256-bit hash of the previous transaction on this account.

**previoustxnlgrseq:** ledger number sequence number of the previous
transaction on this account.

**index:** 256-bit hash of this ripplestate.


## ledger hashes ##

**flags:** ???

**hashes:** a list of the hashes of the previous 256 ledgers.

**lastledgersequence:**

**ledgerentrytype:** "ledgerhashes".

**index:** 256-bit hash of this ledgerhashes.


## owner directory ##

lists all of the offers and trust lines that are associated with an account.

**flags:** ???

**indexes:** a list of hashes of items owned by the account.

**ledgerentrytype:** "directorynode".

**owner:** 160-bit id of the owner account.

**rootindex:**

**index:** a hash of the owner account.


## book directory ##

lists one or more offers that have the same quality.

if a pair of currency and issuer fields are all zeros, then that pair is
dealing in xrp.

the code, at the moment, does not recognize that the currency and issuer
fields are currencies and issuers.  so those values are presented in hex,
rather than as accounts and currencies.  that's a bug and should be fixed
at some point.

**exchangerate:** a 64-bit value.  the first 8-bits is the exponent and the
remaining bits are the mantissa.  the format is such that a bigger 64-bit
value always represents a higher exchange rate.

each type can compute its own hash.  the hash of a book directory contains,
as its lowest 64 bits, the exchange rate.  this means that if there are
multiple *almost* identical book directories, but with different exchange
rates, then these book directories will sit together in the ledger.  the best
exchange rate will be the first in the sequence of book directories.

**flags:** ???

**indexes:** 256-bit hashes of offers that match the exchange rate and
currencies described by this bookdirectory.

**ledgerentrytype:** "directorynode".

**rootindex:** always identical to the index.

**takergetscurrency:** type of currency taker receives.

**takergetsissuer:** issuer of the getscurrency.

**takerpayscurrency:** type of currency taker pays.

**takerpaysissuer:** issuer of the payscurrency.

**index:** a 256-bit hash computed using the takergetscurrency, takergetsissuer,
takerpayscurrency, and takerpaysissuer in the top 192 bits.  the lower 64-bits
are occupied by the exchange rate.

---

# the ledger cleaner #

## overview ##

the ledger cleaner checks and, if necessary, repairs the sqlite ledger and
transaction databases.  it can also check for pieces of a ledger that should
be in the node back end but are missing.  if it detects this case, it
triggers a fetch of the ledger.  the ledger cleaner only operates by manual
request. it is never started automatically.

## operations ##

the ledger cleaner can operate on a single ledger or a range of ledgers. it
always validates the ledger chain itself, ensuring that the sqlite database
contains a consistent chain of ledgers from the last validated ledger as far
back as the database goes.

if requested, it can additionally repair the sqlite entries for transactions
in each checked ledger.  this was primarily intended to repair incorrect
entries created by a bug (since fixed) that could cause transasctions from a
ledger other than the fully-validated ledger to appear in the sqlite
databases in addition to the transactions from the correct ledger.

if requested, it can additionally check the ledger for missing entries
in the account state and transaction trees.

to prevent the ledger cleaner from saturating the available i/o bandwidth
and excessively polluting caches with ancient information, the ledger
cleaner paces itself and does not attempt to get its work done quickly.

## commands ##

the ledger cleaner can be controlled and monitored with the **ledger_cleaner**
rpc command. with no parameters, this command reports on the status of the
ledger cleaner. this includes the range of ledgers it has been asked to process,
the checks it is doing, and the number of errors it has found.

the ledger cleaner can be started, stopped, or have its behavior changed by
the following rpc parameters:

**stop**: stops the ledger cleaner gracefully

**ledger**: asks the ledger cleaner to clean a specific ledger, by sequence number

**min_ledger**, **max_ledger**: sets or changes the range of ledgers cleaned

**full**: requests all operations to be done on the specified ledger(s)

**fix_txns**: a boolean indicating whether to replace the sqlite transaction
entries unconditionally

**check_nodes**: a boolean indicating whether to check the specified
ledger(s) for missing nodes in the back end node store

---

# references #

