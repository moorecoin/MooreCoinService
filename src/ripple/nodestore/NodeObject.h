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

#ifndef ripple_nodestore_nodeobject_h_included
#define ripple_nodestore_nodeobject_h_included

#include <ripple/basics/countedobject.h>
#include <ripple/protocol/protocol.h>

// vfalco note intentionally not in the nodestore namespace

namespace ripple {

/** the types of node objects. */
enum nodeobjecttype
{
    hotunknown = 0,
    hotledger = 1,
    hottransaction = 2,
    hotaccount_node = 3,
    hottransaction_node = 4
};

/** a simple object that the ledger uses to store entries.
    nodeobjects are comprised of a type, a hash, a ledger index and a blob.
    they can be uniquely identified by the hash, which is a sha 256 of the
    blob. the blob is a variable length block of serialized data. the type
    identifies what the blob contains.

    @note no checking is performed to make sure the hash matches the data.
    @see shamap
*/
class nodeobject : public countedobject <nodeobject>
{
public:
    static char const* getcountedobjectname () { return "nodeobject"; }

    enum
    {
        /** size of the fixed keys, in bytes.

            we use a 256-bit hash for the keys.

            @see nodeobject
        */
        keybytes = 32,
    };

    // please use this one. for a reference use ptr const&
    typedef std::shared_ptr <nodeobject> ptr;

    // these are deprecated, type names are capitalized.
    typedef std::shared_ptr <nodeobject> pointer;
    typedef pointer const& ref;

private:
    // this hack is used to make the constructor effectively private
    // except for when we use it in the call to make_shared.
    // there's no portable way to make make_shared<> a friend work.
    struct privateaccess { };
public:
    // this constructor is private, use createobject instead.
    nodeobject (nodeobjecttype type,
                blob&& data,
                uint256 const& hash,
                privateaccess);

    /** create an object from fields.

        the caller's variable is modified during this call. the
        underlying storage for the blob is taken over by the nodeobject.

        @param type the type of object.
        @param ledgerindex the ledger in which this object appears.
        @param data a buffer containing the payload. the caller's variable
                    is overwritten.
        @param hash the 256-bit hash of the payload data.
    */
    static ptr createobject (nodeobjecttype type,
                             blob&& data,
                             uint256 const& hash);

    /** retrieve the type of this object.
    */
    nodeobjecttype gettype () const;

    /** retrieve the hash metadata.
    */
    uint256 const& gethash () const;

    /** retrieve the binary data.
    */
    blob const& getdata () const;

    /** see if this object has the same data as another object.
    */
    bool iscloneof (nodeobject::ptr const& other) const;

    /** binary function that satisfies the strict-weak-ordering requirement.

        this compares the hashes of both objects and returns true if
        the first hash is considered to go before the second.

        @see std::sort
    */
    struct lessthan
    {
        inline bool operator() (nodeobject::ptr const& lhs, nodeobject::ptr const& rhs) const noexcept
        {
            return lhs->gethash () < rhs->gethash ();
        }
    };

private:
    nodeobjecttype mtype;
    uint256 mhash;
    blob mdata;
};

}

#endif
