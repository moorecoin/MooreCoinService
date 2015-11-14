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

#ifndef ripple_nodestore_backend_h_included
#define ripple_nodestore_backend_h_included

#include <ripple/nodestore/types.h>

namespace ripple {
namespace nodestore {

/** a backend used for the nodestore.

    the nodestore uses a swappable backend so that other database systems
    can be tried. different databases may offer various features such
    as improved performance, fault tolerant or distributed storage, or
    all in-memory operation.

    a given instance of a backend is fixed to a particular key size.
*/
class backend
{
public:
    /** destroy the backend.

        all open files are closed and flushed. if there are batched writes
        or other tasks scheduled, they will be completed before this call
        returns.
    */
    virtual ~backend() = default;

    /** get the human-readable name of this backend.
        this is used for diagnostic output.
    */
    virtual std::string getname() = 0;

    /** close the backend.
        this allows the caller to catch exceptions.
    */
    virtual void close() = 0;

    /** fetch a single object.
        if the object is not found or an error is encountered, the
        result will indicate the condition.
        @note this will be called concurrently.
        @param key a pointer to the key data.
        @param pobject [out] the created object if successful.
        @return the result of the operation.
    */
    virtual status fetch (void const* key, nodeobject::ptr* pobject) = 0;

    /** store a single object.
        depending on the implementation this may happen immediately
        or deferred using a scheduled task.
        @note this will be called concurrently.
        @param object the object to store.
    */
    virtual void store (nodeobject::ptr const& object) = 0;

    /** store a group of objects.
        @note this function will not be called concurrently with
              itself or @ref store.
    */
    virtual void storebatch (batch const& batch) = 0;

    /** visit every object in the database
        this is usually called during import.
        @note this routine will not be called concurrently with itself
              or other methods.
        @see import
    */
    virtual void for_each (std::function <void (nodeobject::ptr)> f) = 0;

    /** estimate the number of write operations pending. */
    virtual int getwriteload () = 0;

    /** remove contents on disk upon destruction. */
    virtual void setdeletepath() = 0;

    /** perform consistency checks on database .*/
    virtual void verify() = 0;
};

}
}

#endif
