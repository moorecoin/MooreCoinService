# nudb: a key/value store for decentralized systems

the new breed of decentralized systems such as ripple or bitcoin
that use embedded key/value databases place different demands on
these database than what is traditional. nudb provides highly
optimized and concurrent atomic, durable, and isolated fetch and
insert operations to secondary storage, along with these features:

* low memory footprint
* values are immutable
* value sizes from 1 2^48 bytes (281tb)
* all keys are the same size
* performance independent of growth
* optimized for concurrent fetch
* key file can be rebuilt if needed
* inserts are atomic and consistent
* data file may be iterated, index rebuilt.
* key and data files may be on different volumes
* hardened against algorithmic complexity attacks
* header-only, nothing to build or link

three files are used. the data file holds keys and values stored
sequentially and size-prefixed. the key file holds a series of
fixed-size bucket records forming an on-disk hash table. the log file
stores bookkeeping information used to restore consistency when an
external failure occurs. in typical cases a fetch costs one i/o to
consult the key file and if the key is present, one i/o to read the
value.

## usage

callers define these parameters when a database is created:

* keysize: the size of a key in bytes
* blocksize: the physical size of a key file record
* loadfactor: the desired fraction of bucket occupancy

the ideal block size matches the sector size or block size of the
underlying physical media that holds the key file. functions are
provided to return a best estimate of this value for a particular
device, but a default of 4096 should work for typical installations.
the implementation tries to fit as many entries as possible in a key
file record, to maximize the amount of useful work performed per i/o.

the load factor is chosen to make bucket overflows unlikely without
sacrificing bucket occupancy. a value of 0.50 seems to work well with
a good hash function.

callers also provide these parameters when a database is opened:

* appnum: an application-defined integer constant
* allocsize: a significant multiple of the average data size

to improve performance, memory is recycled. nudb needs a hint about
the average size of the data being inserted. for an average data
size of 1kb (one kilobyte), allocsize of sixteen megabytes (16mb) is
sufficient. if the allocsize is too low, the memory recycler will
not make efficient use of allocated blocks.

two operations are defined, fetch and insert.

### fetch

the fetch operation retrieves a variable length value given the
key. the caller supplies a factory used to provide a buffer for storing
the value. this interface allows custom memory allocation strategies.

### insert

insert adds a key/value pair to the store. value data must contain at
least one byte. duplicate keys are disallowed. insertions are serialized.

## implementation

all insertions are buffered in memory, with inserted values becoming
immediately discoverable in subsequent or concurrent calls to fetch.
periodically, buffered data is safely committed to disk files using
a separate dedicated thread associated with the database. this commit
process takes place at least once per second, or more often during
a detected surge in insertion activity. in the commit process the
key/value pairs receive the following treatment:

an insertion is performed by appending a value record to the data file.
the value record has some header information including the size of the
data and a copy of the key; the data file is iteratable without the key
file. the value data follows the header. the data file is append-only
and immutable: once written, bytes are never changed.

initially the hash table in the key file consists of a single bucket.
after the load factor is exceeded from insertions, the hash table grows
in size by one bucket by doing a "split". the split operation is the
linear hashing algorithm as described by litwin and larson:
    
http://en.wikipedia.org/wiki/linear_hashing

when a bucket is split, each key is rehashed and either remains in the
original bucket or gets moved to the new bucket appended to the end of
the key file.

an insertion on a full bucket first triggers the "spill" algorithm:
first, a spill record is appended to the data file. the spill record
contains header information followed by the entire bucket record. then,
the bucket's size is set to zero and the offset of the spill record is
stored in the bucket. at this point the insertion may proceed normally,
since the bucket is empty. spilled buckets in the data file are always
full.

because every bucket holds the offset of the next spill record in the
data file, each bucket forms a linked list. in practice, careful
selection of capacity and load factor will keep the percentage of
buckets with one spill record to a minimum, with no bucket requiring
two spill records.

the implementation of fetch is straightforward: first the bucket in the
key file is checked, then each spill record in the linked list of
spill records is checked, until the key is found or there are no more
records. as almost all buckets have no spill records, the average
fetch requires one i/o (not including reading the value).

one complication in the scheme is when a split occurs on a bucket that
has one or more spill records. in this case, both the bucket being split
and the new bucket may overflow. this is handled by performing the
spill algorithm for each overflow that occurs. the new buckets may have
one or more spill records each, depending on the number of keys that
were originally present.

because the data file is immutable, a bucket's original spill records
are no longer referenced after the bucket is split. these blocks of data
in the data file are unrecoverable wasted space. correctly configured
databases can have a typical waste factor of 1%, which is acceptable.
these unused bytes can be removed by visiting each value in the value
file using an off-line process and inserting it into a new database,
then delete the old database and use the new one instead.

## recovery

to provide atomicity and consistency, a log file associated with the
database stores information used to roll back partial commits.

## iteration

each record in the data file is prefixed with a header identifying
whether it is a value record or a spill record, along with the size of
the record in bytes and a copy of the key if its a value record.
therefore, values may be iterated. a key file can be regenerated from
just the data file by iterating the values and performing the key
insertion algorithm.

## concurrency

locks are never held during disk reads and writes. fetches are fully
concurrent, while inserts are serialized. inserts prevent duplicate
keys. inserts are atomic, they either succeed immediately or fail.
after an insert, the key is immediately visible to subsequent fetches.

## formats

all integer values are stored as big endian. the uint48_t format
consists of 6 bytes.

### key file

the key file contains the header followed by one or more
fixed-length bucket records.

#### header (104 bytes)

    char[8]         type            the characters "nudb.key"
    uint16          version         holds the version number
    uint64          uid             unique id generated on creation
    uint64          appnum          application defined constant
    uint16          keysize         key size in bytes

    uint64          salt            a random seed
    uint64          pepper          the salt hashed
    uint16          blocksize       size of a file block in bytes

    uint16          loadfactor      target fraction in 65536ths

    uint8[56]       reserved        zeroes
    uint8[]         reserved        zero-pad to block size

the type identifies the file as belonging to nudb. the uid is
generated randomly when the database is created, and this value
is stored in the data and log files as well. the uid is used
to determine if files belong to the same database. salt is
generated when the database is created and helps prevent
complexity attacks; the salt is prepended to the key material
when computing a hash, or used to initialize the state of
the hash function. appnum is an application defined constant
set when the database is created. it can be used for anything,
for example to distinguish between different data formats.

pepper is computed by hashing the salt using a hash function
seeded with the salt. this is used to fingerprint the hash
function used. if a database is opened and the fingerprint
does not match the hash calculation performed using the template
argument provided when constructing the store, an exception
is thrown.

the header for the key file contains the file header followed by
the information above. the capacity is the number of keys per
bucket, and defines the size of a bucket record. the load factor
is the target fraction of bucket occupancy.

none of the information in the key file header or the data file
header may be changed after the database is created, including
the appnum.

#### bucket record (fixed-length)

    uint16              count           number of keys in this bucket
    uint48              spill           offset of the next spill record or 0
    bucketentry[]       entries         the bucket entries

#### bucket entry

    uint48              offset          offset in data file of the data
    uint48              size            the size of the value in bytes
    uint48              hash            the hash of the key

### data file

the data file contains the header followed by zero or more
variable-length value records and spill records.

#### header (92 bytes)

    char[8]             type            the characters "nudb.dat"
    uint16              version         holds the version number
    uint64              uid             unique id generated on creation
    uint64              appnum          application defined constant
    uint16              keysize         key size in bytes

    uint8[64]           reserved        zeroes

uid contains the same value as the salt in the corresponding key
file. this is placed in the data file so that key and value files
belonging to the same database can be identified.

#### data record (variable-length)

    uint48              size            size of the value in bytes
    uint8[keysize]      key             the key.
    uint8[size]         data            the value data.

#### spill record (fixed-length)

    uint48              zero            all zero, identifies a spill record
    uint16              size            bytes in spill bucket (for skipping)
    bucket              spillbucket     bucket record

### log file

the log file contains the header followed by zero or more fixed size
log records. each log record contains a snapshot of a bucket. when a
database is not closed cleanly, the recovery process applies the log
records to the key file, overwriting data that may be only partially
updated with known good information. after the log records are applied,
the data and key files are truncated to the last known good size.

#### header (62 bytes)

    char[8]             type            the characters "nudb.log"
    uint16              version         holds the version number
    uint64              uid             unique id generated on creation
    uint64              appnum          application defined constant
    uint16              keysize         key size in bytes

    uint64              salt            a random seed.
    uint64              pepper          the salt hashed
    uint16              blocksize       size of a file block in bytes

    uint64              keyfilesize     size of key file.
    uint64              datafilesize    size of data file.

#### log record

    uint64_t            index           bucket index (0-based)
    bucket              bucket          compact bucket record

compact buckets include only size entries. these are primarily
used to minimize the volume of writes to the log file.
