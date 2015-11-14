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

#ifndef ripple_protocol_stledgerentry_h_included
#define ripple_protocol_stledgerentry_h_included

#include <ripple/protocol/ledgerformats.h>
#include <ripple/protocol/stobject.h>

namespace ripple {

class stledgerentry
    : public stobject
    , public countedobject <stledgerentry>
{
public:
    static char const* getcountedobjectname () { return "stledgerentry"; }

    typedef std::shared_ptr<stledgerentry>        pointer;
    typedef const std::shared_ptr<stledgerentry>& ref;

public:
    stledgerentry (const serializer & s, uint256 const& index);
    stledgerentry (serializeriterator & sit, uint256 const& index);
    stledgerentry (ledgerentrytype type, uint256 const& index);
    stledgerentry (const stobject & object, uint256 const& index);

    serializedtypeid getstype () const
    {
        return sti_ledgerentry;
    }
    std::string getfulltext () const;
    std::string gettext () const;
    json::value getjson (int options) const;

    uint256 const& getindex () const
    {
        return mindex;
    }
    void setindex (uint256 const& i)
    {
        mindex = i;
    }

    void setimmutable ()
    {
        mmutable = false;
    }
    bool ismutable ()
    {
        return mmutable;
    }
    stledgerentry::pointer getmutable () const;

    ledgerentrytype gettype () const
    {
        return mtype;
    }
    std::uint16_t getversion () const
    {
        return getfieldu16 (sfledgerentrytype);
    }
    ledgerformats::item const* getformat ()
    {
        return mformat;
    }

    bool isthreadedtype (); // is this a ledger entry that can be threaded
    bool isthreaded ();     // is this ledger entry actually threaded
    bool hasoneowner ();    // this node has one other node that owns it
    bool hastwoowners ();   // this node has two nodes that own it (like ripple balance)
    rippleaddress getowner ();
    rippleaddress getfirstowner ();
    rippleaddress getsecondowner ();
    uint256 getthreadedtransaction ();
    std::uint32_t getthreadedledger ();
    bool thread (uint256 const& txid, std::uint32_t ledgerseq, uint256 & prevtxid,
                 std::uint32_t & prevledgerid);
    std::vector<uint256> getowners ();  // nodes notified if this node is deleted

private:
    stledgerentry* duplicate () const
    {
        return new stledgerentry (*this);
    }

    /** make stobject comply with the template for this sle type
        can throw
    */
    void setsletype ();

private:
    uint256                     mindex;
    ledgerentrytype             mtype;
    ledgerformats::item const*  mformat;
    bool                        mmutable;
};

using sle = stledgerentry;

} // ripple

#endif
