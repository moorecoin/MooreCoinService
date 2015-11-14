# rocksdblite

rocksdblite is a project focused on mobile use cases, which don't need a lot of fancy things we've built for server workloads and they are very sensitive to binary size. for that reason, we added a compile flag rocksdb_lite that comments out a lot of the nonessential code and keeps the binary lean.

some examples of the features disabled by rocksdb_lite:
* compiled-in support for ldb tool
* no backupable db
* no support for replication (which we provide in form of trasactionaliterator)
* no advanced monitoring tools
* no special-purpose memtables that are highly optimized for specific use cases

when adding a new big feature to rocksdb, please add rocksdb_lite compile guard if:
* nobody from mobile really needs your feature,
* your feature is adding a lot of weight to the binary.

don't add rocksdb_lite compile guard if:
* it would introduce a lot of code complexity. compile guards make code harder to read. it's a trade-off.
* your feature is not adding a lot of weight.

if unsure, ask. :)
