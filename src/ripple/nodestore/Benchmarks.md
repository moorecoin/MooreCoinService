#benchmarks

```
$rippled --unittest=nodestoretiming --unittest-arg="type=rocksdb,num_objects=2000000,open_files=2000,filter_bits=12,cache_mb=256,file_size_mb=8,file_size_mult=2;type=rocksdbquick,num_objects=2000000"
2014-nov-01 21:49:02 validators:nfo validators constructed (info)
ripple.bench.nodestoretiming repeatableobject
 config run       inserts  batch insert   fetch 50/50 ordered fetch  fetch random fetch missing
      0   0        160.57        699.08         50.88         51.17         29.99         14.05
      0   1        406.70        797.47         32.53         60.18         46.63         14.94
      0   2        408.81        743.89         42.79         72.99         49.03         14.93
      1   0        111.03        151.06         28.89         53.44         31.88         18.46
      1   1         92.63        160.75         19.64         41.60         28.17         10.40
      1   2        101.31        122.83         30.66         55.65         32.69         16.15

configs:
 0: type=rocksdb,num_objects=2000000,open_files=2000,filter_bits=12,cache_mb=256,file_size_mb=8,file_size_mult=2
 1: type=rocksdbquick,num_objects=2000000
```

##discussion

rocksdbquickfactory is intended to provide a testbed for comparing potential rocksdb performance with the existing recommended configuration in rippled.cfg. through various executions and profiling some conclusions are presented below.

* if the write ahead log is enabled, insert speed soon clogs up under load. the batchwriter class intends to stop this from blocking the main threads by queuing up writes and running them in a separate thread. however, rocksdb already has separate threads dedicated to flushing the memtable to disk and the memtable is itself an in-memory queue. the result is two queues with a guarantee of durability in between. however if the memtable was used as the sole queue and the rocksdb::flush() call was manually triggered at opportune moments, possibly just after ledger close, then that would provide similar, but more predictable guarantees. it would also remove an unneeded thread and unnecessary memory usage. an alternative point of view is that because there will always be many other rippled instances running there is no need for such guarantees. the nodes will always be available from another peer.

* lookup in a block was previously using binary search. with rippled's use case it is highly unlikely that two adjacent key/values will ever be requested one after the other. therefore hash indexing of blocks makes much more sense. rocksdb has a number of options for hash indexing both memtables and blocks and these need more testing to find the best choice.

* the current database implementation has two forms of caching, so the lru cache of blocks at factory level does not make any sense. however, if the hash indexing and potentially the new [bloom filter](http://rocksdb.org/blog/1427/new-bloom-filter-format/) can provide faster lookup for non-existent keys, then potentially the caching could exist at factory level.

* multiple runs of the benchmarks can yield surprisingly different results. this can perhaps be attributed to the asynchronous nature of rocksdb's compaction process. the benchmarks are artifical and create highly unlikely write load to create the dataset to measure different read access patterns. therefore multiple runs of the benchmarks are required to get a feel for the effectiveness of the changes. this contrasts sharply with the keyvadb benchmarking were highly repeatable timings were discovered. also realistically sized datasets are required to get a correct insight. the number of 2,000,000 key/values (actually 4,000,000 after the two insert benchmarks complete) is too low to get a full picture.

* an interesting side effect of running the benchmarks in a profiler was that a clear pattern of what rocksdb does under the hood was observable. this led to the decision to trial hash indexing and also the discovery of the native crc32 instruction not being used.

* important point to note that is if this factory is tested with an existing set of sst files none of the old sst files will benefit from indexing changes until they are compacted at a future point in time.
