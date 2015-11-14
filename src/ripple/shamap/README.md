# shamap introduction #

july 2014

the shamap is a merkle tree (http://en.wikipedia.org/wiki/merkle_tree).
the shamap is also a radix tree of radix 16
(http://en.wikipedia.org/wiki/radix_tree).

*we need some kind of sensible summary of the shamap here.*

a given shamap always stores only one of three kinds of data:

 * transactions with metadata
 * transactions without metadata, or
 * account states.

so all of the leaf nodes of a particular shamap will always have a uniform
type.  the inner nodes carry no data other than the hash of the nodes
beneath them.


## shamap types ##

there are two different ways of building and using a shamap:

 1. a mutable shamap and
 2. an immutable shamap

the distinction here is not of the classic c++ immutable-means-unchanging
sense.  an immutable shamap contains *nodes* that are immutable.  also,
once a node has been located in an immutable shamap, that node is
guaranteed to persist in that shamap for the lifetime of the shamap.

so, somewhat counter-intuitively, an immutable shamap may grow as new nodes
are introduced.  but an immutable shamap will never get smaller (until it
entirely evaporates when it is destroyed).  nodes, once introduced to the
immutable shamap, also never change their location in memory.  so nodes in
an immutable shamap can be handled using raw pointers (if you're careful).

one consequence of this design is that an immutable shamap can never be
"trimmed".  there is no way to identify unnecessary nodes in an immutable
shamap that could be removed.  once a node has been brought into the
in-memory shamap, that node stays in memory for the life of the shamap.

most shamaps are immutable, in the sense that they don't modify or remove
their contained nodes.

an example where a mutable shamap is required is when we want to apply
transactions to the last closed ledger.  to do so we'd make a mutable
snapshot of the state tree and then start applying transactions to it.
because the snapshot is mutable, changes to nodes in the snapshot will not
affect nodes in other shamaps.

an example using a immutable ledger would be when there's an open ledger
and some piece of code wishes to query the state of the ledger.  in this
case we don't wish to change the state of the shamap, so we'd use an
immutable snapshot.


## shamap creation ##

a shamap is usually not created from vacuum.  once an initial shamap is
constructed, later shamaps are usually created by calling
snapshot(bool ismutable) on the original shamap().  the returned shamap
has the expected characteristics (mutable or immutable) based on the passed
in flag.

it is cheaper to make an immutable snapshot of a shamap than to make a mutable
snapshot.  if the shamap snapshot is mutable then any of the nodes that might
be modified must be copied before they are placed in the mutable map.


## shamap thread safety ##

shamaps can be thread safe, depending on how they are used.  the shamap
uses a syncunorderedmap for its storage.  the syncunorderedmap has three
thread-safe methods:

 * size(),
 * canonicalize(), and
 * retrieve()

as long as the shamap uses only those three interfaces to its storage
(the mtnbyid variable [which stands for tree node by id]) the shamap is
thread safe.


## walking a shamap ##

*we need a good description of why someone would walk a shamap and*
*how it works in the code*


## late-arriving nodes ##

as we noted earlier, shamaps (even immutable ones) may grow.  if a shamap
is searching for a node and runs into an empty spot in the tree, then the
shamap looks to see if the node exists but has not yet been made part of
the map.  this operation is performed in the `shamap::fetchnodeexternalnt()`
method.  the *nt* is this case stands for 'no throw'.

the `fetchnodeexternalnt()` method goes through three phases:

 1. by calling `getcache()` we attempt to locate the missing node in the
    treenodecache.  the treenodecache is a cache of immutable
    shamaptreenodes that are shared across all shamaps.

    any shamaptreenode that is immutable has a sequence number of zero.
    when a mutable shamap is created then its shamaptreenodes are given
    non-zero sequence numbers.  so the `assert (ret->getseq() == 0)`
    simply confirms that the treenodecache indeed gave us an immutable node.

 2. if the node is not in the treenodecache, we attempt to locate the node
    in the historic data stored by the data base.  the call to
    to `fetch(hash)` does that work for us.

 3. finally, if mledgerseq is non-zero and we did't locate the node in the
    historic data, then we call a missingnodehandler.

    the non-zero mledgerseq indicates that the shamap is a complete map that
    belongs to a historic ledger with the given (non-zero) sequence number.
    so, if all expected data is always present, the missingnodehandler should
    never be executed.

    and, since we now know that this shamap does not fully represent
    the data from that ledger, we set the shamap's sequence number to zero.

if phase 1 returned a node, then we already know that the node is immutable.
however, if either phase 2 executes successfully, then we need to turn the
returned node into an immutable node.  that's handled by the call to
`make_shared<shamaptreenode>` inside the try block.  that code is inside
a try block because the `fetchnodeexternalnt` method promises not to throw.
in case the constructor called by `make_shared` throws we don't want to
break our promise.


## canonicalize ##

the calls to `canonicalize()` make sure that if the resulting node is already
in the shamap, then we return the node that's already present -- we never
replace a pre-existing node.  by using `canonicalize()` we manage a thread
race condition where two different threads might both recognize the lack of a
shamaptreenode at the same time.  if they both attempt to insert the node
then `canonicalize` makes sure that the first node in wins and the slower
thread receives back a pointer to the node inserted by the faster thread.

there's a problem with the current shamap design that `canonicalize()`
accommodates.  two different trees can have the exact same node (the same
hash value) with two different ids.  if the treenodecache returns a node
with the same hash but a different id, then we assume that the id of the
passed-in node is 'better' than the older id in the treenodecache.  so we
construct a new shamaptreenode by copying the one we found in the
treenodecache, but we give the new node the new id.  then we replace the
shamaptreenode in the treenodecache with this newly constructed node.

the treenodecache is not subject to the rule that any node must be
resident forever.  so it's okay to replace the old node with the new node.

the `shamap::getcache()` method exhibits the same behavior.


## shamap improvements ##

here's a simple one: the shamaptreenode::maccessseq member is currently not
used and could be removed.

here's a more important change.  the tree structure is currently embedded
in the shamaptreenodes themselves.  it doesn't have to be that way, and
that should be fixed.

when we navigate the tree (say, like `shamap::walkto()`) we currently
ask each node for information that we could determine locally.  we know
the depth because we know how many nodes we have traversed.  we know the
id that we need because that's how we're steering.  so we don't need to
store the id in the node.  the next refactor should remove all calls to
`shamaptreenode::getid()`.

then we can remove the nodeid member from shamaptreenode.

then we can change the shamap::mtnbtid  member to be mtnbyhash.

an additional possible refactor would be to have a base type, shamaptreenode,
and derive from that innernode and leafnode types.  that would remove
some storage (the array of 16 hashes) from the leafnodes.  that refactor
would also have the effect of simplifying methods like `isleaf()` and
`hasitem()`.

