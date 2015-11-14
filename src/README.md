# src

some of these directories come from entire outside repositories brought in
using git-subtree. this means that the source files are inserted directly
into the rippled repository. they can be edited and committed just as if they
were normal files.

however, if you create a commit that contains files both from a
subtree, and from the ripple source tree please use care when designing
the commit message, since it will appear in the subtree's individual
repository when the changes are pushed back to the upstream.

when submitting pull request, make sure that any commits which include
files from subtrees are isolated - i.e. do not mix files from subtrees
and ripple in the same commit. this way, the commit message will make
sense. we don't want to see "fix pathfinding bug with xrp" appearing
in the leveldb or beast commit log, for example.

about git-subtree:

https://github.com/apenwarr/git-subtree <br>
http://blogs.atlassian.com/2013/05/alternatives-to-git-submodule-git-subtree/ <br>

<table align=left><tr>
<th>dir</th>
<th>what</th>
</tr><tr>
<td>beast</td>
<td>beast, the amazing cross-platform library.<br>
    git@github.com:vinniefalco/beast.git
</td>
</tr></table>

## ./beast

beast, the amazing cross-platform library.

repository <br>
```
git@github.com:vinniefalco/beast.git
```
branch
```
master
```

## hyperleveldb

ripple's fork of hyperleveldb

repository <br>
```
git@github.com:ripple/hyperleveldb.git
```
branch
```
ripple-fork
```

## leveldb

ripple's fork of leveldb.

repository <br>
```
git@github.com:ripple/leveldb.git
```
branch
```
ripple-fork
```

## lightningdb (a.k.a. mdb)

ripple's fork of mdb, a fast memory-mapped key value database system.

repository <br>
```
git@github.com:ripple/lightningdb.git
```
branch
```
ripple-fork
```

## websocket

ripple's fork of websocketpp has some incompatible changes and ripple specific includes.

repository
```
git@github.com:ripple/websocketpp.git
```
branch
```
ripple-fork
```

## protobuf

ripple's fork of protobuf. we've changed some names in order to support the
unity-style of build (a single .cpp added to the project, instead of
linking to a separately built static library).

repository
```
git@github.com:ripple/protobuf.git
```
branch
```
master
```

**note** linux builds use the protobuf installed in /usr/lib. this will be
fixed in a future revision.

## sqlite

not technically a subtree but included here because it is a direct
copy of the official sqlite distributions available here:

http://sqlite.org/download.html
