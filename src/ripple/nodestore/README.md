# nodestore

## introduction

a `nodeobject` is a simple object that the ledger uses to store entries. it is 
comprised of a type, a hash, a ledger index and a blob. it can be uniquely 
identified by the hash, which is a 256 bit hash of the blob. the blob is a 
variable length block of serialized data. the type identifies what the blob 
contains. the fields are as follows: 
    
* `mtype`

 an enumeration that determines what the blob holds. there are four 
 different types of objects stored. 

 * **ledger**
    
   a ledger header.

 * **transaction**
      
   a signed transaction.
      
 * **account node**
        
   a node in a ledger's account state tree.
        
 * **transaction node**
        
   a node in a ledger's transaction tree.
    
* `mhash`

 a 256-bit hash of the blob.

* `mledgerindex`
      
 an unsigned integer that uniquely identifies the ledger in which this 
   nodeobject appears.

* `mdata`
      
 a blob containing the payload. stored in the following format.
 
|byte   |                     |                          |
|:------|:--------------------|:-------------------------|
|0...3  |ledger index         |32-bit big endian integer |
|4...7  |reserved ledger index|32-bit big endian integer |
|8      |type                 |nodeobjecttype enumeration|
|9...end|data                 |body of the object data   |
---    
the `nodestore` provides an interface that stores, in a persistent database, a 
collection of nodeobjects that rippled uses as its primary representation of 
ledger entries. all ledger entries are stored as nodeobjects and as such, need 
to be persisted between launches. if a nodeobject is accessed and is not in 
memory, it will be retrieved from the database.

## backend

the `nodestore` implementation provides the `backend` abstract interface, 
which lets different key/value databases to be chosen at run-time. this allows 
experimentation with different engines. improvements in the performance of the 
nodestore are a constant area of research. the database can be specified in 
the configuration file [node_db] section as follows.

one or more lines of key / value pairs

example:
```
type=rocksdb
path=rocksdb
compression=1
```
choices for 'type' (not case-sensitive)
   
* **hyperleveldb**
  
 an improved version of leveldb (preferred).

* **leveldb**

 google's leveldb database (deprecated).

* **none**

 use no backend.

* **rocksdb**

 facebook's rocksdb database, builds on leveldb. 

* **sqlite**

 use sqlite.

'path' speficies where the backend will store its data files.

choices for 'compression'

* **0** off

* **1** on (default)