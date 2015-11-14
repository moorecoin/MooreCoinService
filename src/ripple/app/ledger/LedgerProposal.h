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

#ifndef ripple_app_ledgerproposal_h_included
#define ripple_app_ledgerproposal_h_included

#include <ripple/basics/countedobject.h>
#include <ripple/basics/base_uint.h>
#include <ripple/json/json_value.h>
#include <ripple/protocol/rippleaddress.h>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <cstdint>
#include <string>

namespace ripple {

class ledgerproposal
    : public countedobject <ledgerproposal>
{
public:
    static char const* getcountedobjectname () { return "ledgerproposal"; }

    static const std::uint32_t seqleave = 0xffffffff; // leaving the consensus process

    typedef std::shared_ptr<ledgerproposal> pointer;
    typedef const pointer& ref;

    // proposal from peer
    ledgerproposal (uint256 const& prevlgr, std::uint32_t proposeseq, uint256 const& propose,
                    std::uint32_t closetime, const rippleaddress & napeerpublic, uint256 const& suppress);

    // our first proposal
    ledgerproposal (const rippleaddress & pubkey, const rippleaddress & privkey,
                    uint256 const& prevledger, uint256 const& position, std::uint32_t closetime);

    // an unsigned "dummy" proposal for nodes not validating
    ledgerproposal (uint256 const& prevledger, uint256 const& position, std::uint32_t closetime);

    uint256 getsigninghash () const;
    bool checksign (std::string const& signature, uint256 const& signinghash);
    bool checksign (std::string const& signature)
    {
        return checksign (signature, getsigninghash ());
    }
    bool checksign ()
    {
        return checksign (msignature, getsigninghash ());
    }

    nodeid const& getpeerid () const
    {
        return mpeerid;
    }
    uint256 const& getcurrenthash () const
    {
        return mcurrenthash;
    }
    uint256 const& getprevledger () const
    {
        return mpreviousledger;
    }
    uint256 const& getsuppressionid () const
    {
        return msuppression;
    }
    std::uint32_t getproposeseq () const
    {
        return mproposeseq;
    }
    std::uint32_t getclosetime () const
    {
        return mclosetime;
    }
    rippleaddress const& peekpublic () const
    {
        return mpublickey;
    }
    blob getpubkey () const
    {
        return mpublickey.getnodepublic ();
    }
    blob sign ();

    void setprevledger (uint256 const& prevledger)
    {
        mpreviousledger = prevledger;
    }
    void setsignature (std::string const& signature)
    {
        msignature = signature;
    }
    bool hassignature ()
    {
        return !msignature.empty ();
    }
    bool isprevledger (uint256 const& pl)
    {
        return mpreviousledger == pl;
    }
    bool isbowout ()
    {
        return mproposeseq == seqleave;
    }

    const boost::posix_time::ptime getcreatetime ()
    {
        return mtime;
    }
    bool isstale (boost::posix_time::ptime cutoff)
    {
        return mtime <= cutoff;
    }

    bool changeposition (uint256 const& newposition, std::uint32_t newclosetime);
    void bowout ();
    json::value getjson () const;

    static uint256 computesuppressionid (
        uint256 const& proposehash,
        uint256 const& previousledger,
        std::uint32_t proposeseq,
        std::uint32_t closetime,
        blob const& pubkey,
        blob const& signature);

private:
    uint256 mpreviousledger, mcurrenthash, msuppression;
    std::uint32_t mclosetime, mproposeseq;

    nodeid         mpeerid;
    rippleaddress   mpublickey;
    rippleaddress   mprivatekey;    // if ours

    std::string                 msignature; // set only if needed
    boost::posix_time::ptime    mtime;
};

} // ripple

#endif
