# rocksjava change log

## by 06/15/2014
### new features
* added basic java binding for rocksdb::env such that multiple rocksdb can share the same thread pool and environment.
* added restorebackupabledb

## by 05/30/2014
### internal framework improvement
* added disownnativehandle to rocksobject, which allows a rocksobject to give-up the ownership of its native handle.  this method is useful when sharing and transferring the ownership of rocksdb c++ resources.

## by 05/15/2014
### new features
* added rocksobject --- the base class of all rocksdb classes which holds some rocksdb resources in the c++ side.
* use environmental variable java_home in makefile for rocksjava
### public api changes
* renamed org.rocksdb.iterator to org.rocksdb.rocksiterator to avoid potential confliction with java built-in iterator.

## by 04/30/2014
### new features
* added java binding for multiget.
* added static method rocksdb.loadlibrary(), which loads necessary library files.
* added java bindings for 60+ rocksdb::options.
* added java binding for bloomfilter.
* added java binding for readoptions.
* added java binding for memtables.
* added java binding for sst formats.
* added java binding for rocksdb iterator which enables sequential scan operation.
* added java binding for statistics
* added java binding for backupabledb.

### db benchmark
* added filluniquerandom, readseq benchmark.
* 70+ command-line options.
* enabled bloomfilter configuration.

## by 04/15/2014
### new features
* added java binding for writeoptions.
* added java binding for writebatch, which enables batch-write.
* added java binding for rocksdb::options.
* added java binding for block cache.
* added java version db benchmark.

### db benchmark
* added readwhilewriting benchmark.

### internal framework improvement
* avoid a potential byte-array-copy between c++ and java in rocksdb.get.
* added sizeunit in org.rocksdb.util to store consts like kb and gb.

### 03/28/2014
* rocksjava project started.
* added java binding for rocksdb, which supports open, close, get and put.
