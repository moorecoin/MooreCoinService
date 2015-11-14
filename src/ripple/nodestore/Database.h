//------------------------------------------------------------------------------
/*
    this file is part of rippled: https://github.com/ripple/rippled
    copyright (c) 2012, 2013 ripple labs inc.

    permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    the  software is provided "as is" and the author disclaims all warranties
    with  regard  to  this  software  including  all  implied  warranties  of
    merchantability  and  fitness. in no event shall the author be liable for
    any  special ,  direct, indirect, or consequential damages or any damages
    whatsoever  resulting  from  loss  of use, data or profits, whether in an
    action  of  contract, negligence or other tortious action, arising out of
    or in connection with the use or performance of this software.
*/
//==============================================================================

#ifndef ripple_nodestore_database_h_included
#define ripple_nodestore_database_h_included

#include <ripple/nodestore/nodeobject.h>
#include <ripple/nodestore/backend.h>
#include <ripple/basics/taggedcache.h>

namespace ripple {
namespace nodestore {

/** persistency layer for nodeobject

    a node is a ledger object which is uniquely identified by a key, which is
    the 256-bit hash of the body of the node. the payload is a variable length
    block of serialized data.

    all ledger data is stored as node objects and as such, needs to be persisted
    between launches. furthermore, since the set of node objects will in
    general be larger than the amount of available memory, purged node objects
    which are later accessed must be retrieved from the node store.

    @see nodeobject
*/
class database
{
public:
    /** destroy the node store.
        all pending operations are completed, pending writes flushed,
        and files closed before this returns.
    */
    virtual ~database() = default;

    /** retrieve the name associated with this backend.
        this is used for diagnostics and may not reflect the actual path
        or paths used by the underlying backend.
    */
    virtual std::string getname () const = 0;
    
    /** close the database.
        this allows the caller to catch exceptions.
    */
    virtual void close() = 0;

    /** fetch an object.
        if the object is known to be not in the database, isn't found in the
        database during the fetch, or failed to load correctly during the fetch,
        `nullptr` is returned.

        @note this can be called concurrently.
        @param hash the key of the object to retrieve.
        @return the object, or nullptr if it couldn't be retrieved.
    */
    virtual nodeobject::pointer fetch (uint256 const& hash) = 0;

    /** fetch an object without waiting.
        if i/o is required to determine whether or not the object is present,
        `false` is returned. otherwise, `true` is returned and `object` is set
        to refer to the object, or `nullptr` if the object is not present.
        if i/o is required, the i/o is scheduled.

        @note this can be called concurrently.
        @param hash the key of the object to retrieve
        @param object the object retrieved
        @return whether the operation completed
    */
    virtual bool asyncfetch (uint256 const& hash, nodeobject::pointer& object) = 0;

    /** wait for all currently pending async reads to complete.
    */
    virtual void waitreads () = 0;

    /** get the maximum number of async reads the node store prefers.
        @return the number of async reads preferred.
    */
    virtual int getdesiredasyncreadcount () = 0;

    /** store the object.

        the caller's blob parameter is overwritten.

        @param type the type of object.
        @param ledgerindex the ledger in which the object appears.
        @param data the payload of the object. the caller's
                    variable is overwritten.
        @param hash the 256-bit hash of the payload data.

        @return `true` if the object was stored?
    */
    virtual void store (nodeobjecttype type,
                        blob&& data,
                        uint256 const& hash) = 0;

    /** visit every object in the database
        this is usually called during import.

        @note this routine will not be called concurrently with itself
                or other methods.
        @see import
    */
    virtual void for_each(std::function <void(nodeobject::ptr)> f) = 0;

    /** import objects from another database. */
    virtual void import (database& source) = 0;

    /** retrieve the estimated number of pending write operations.
        this is used for diagnostics.
    */
    virtual std::int32_t getwriteload() const = 0;

    /** get the positive cache hits to total attempts ratio. */
    virtual float getcachehitrate () = 0;

    /** set the maximum number of entries and maximum cache age for both caches.

        @param size number of cache entries (0 = ignore)
        @param age maximum cache age in seconds
    */
    virtual void tune (int size, int age) = 0;

    /** remove expired entries from the positive and negative caches. */
    virtual void sweep () = 0;

    /** gather statistics pertaining to read and write activities.
        return the reads and writes, and total read and written bytes.
     */
    virtual std::uint32_t getstorecount () const = 0;
    virtual std::uint32_t getfetchtotalcount () const = 0;
    virtual std::uint32_t getfetchhitcount () const = 0;
    virtual std::uint32_t getstoresize () const = 0;
    virtual std::uint32_t getfetchsize () const = 0;
};

}
}

#endif
