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

#ifndef ripple_transactionmeta_h
#define ripple_transactionmeta_h

#include <ripple/protocol/stledgerentry.h>
#include <ripple/protocol/starray.h>
#include <ripple/protocol/ter.h>
#include <boost/optional.hpp>

namespace ripple {

class transactionmetaset : beast::leakchecked <transactionmetaset>
{
public:
    typedef std::shared_ptr<transactionmetaset> pointer;
    typedef const pointer& ref;

public:
    transactionmetaset ()
        : mledger (0)
        , mindex (static_cast<std::uint32_t> (-1))
        , mresult (255)
    {
    }

    transactionmetaset (uint256 const& txid, std::uint32_t ledger, std::uint32_t index)
        : mtransactionid (txid)
        , mledger (ledger)
        , mindex (static_cast<std::uint32_t> (-1))
        , mresult (255)
    {
    }

    transactionmetaset (uint256 const& txid, std::uint32_t ledger, blob const&);

    void init (uint256 const& transactionid, std::uint32_t ledger);
    void clear ()
    {
        mnodes.clear ();
    }
    void swap (transactionmetaset&);

    uint256 const& gettxid ()
    {
        return mtransactionid;
    }
    std::uint32_t getlgrseq ()
    {
        return mledger;
    }
    int getresult () const
    {
        return mresult;
    }
    ter getresultter () const
    {
        return static_cast<ter> (mresult);
    }
    std::uint32_t getindex () const
    {
        return mindex;
    }

    bool isnodeaffected (uint256 const& ) const;
    void setaffectednode (uint256 const& , sfield::ref type, std::uint16_t nodetype);
    stobject& getaffectednode (sle::ref node, sfield::ref type); // create if needed
    stobject& getaffectednode (uint256 const& );
    const stobject& peekaffectednode (uint256 const& ) const;
    std::vector<rippleaddress> getaffectedaccounts ();


    json::value getjson (int p) const
    {
        return getasobject ().getjson (p);
    }
    void addraw (serializer&, ter, std::uint32_t index);

    stobject getasobject () const;
    starray& getnodes ()
    {
        return (mnodes);
    }

    void setdeliveredamount (stamount const& delivered)
    {
        mdelivered.reset (delivered);
    }
    
    void setfeesharetakers(starray const& feesharetakers)
    {
        mfeesharetakers.reset(feesharetakers);
    }

    stamount getdeliveredamount () const
    {
        assert (hasdeliveredamount ());
        return *mdelivered;
    }
    
    starray getfeesharetakers () const
    {
        assert (hasfeesharetakers ());
        return *mfeesharetakers;
    }

    bool hasdeliveredamount () const
    {
        return static_cast <bool> (mdelivered);
    }
    
    bool hasfeesharetakers() const
    {
        return static_cast <bool> (mfeesharetakers);
    }

    static bool thread (stobject& node, uint256 const& prevtxid, std::uint32_t prevlgrid);

private:
    uint256       mtransactionid;
    std::uint32_t mledger;
    std::uint32_t mindex;
    int           mresult;

    boost::optional <stamount> mdelivered;

    starray mnodes;
    boost::optional <starray> mfeesharetakers;
};

} // ripple

#endif
