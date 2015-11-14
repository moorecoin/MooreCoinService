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

#include <beastconfig.h>
#include <ripple/app/ledger/ledgerproposal.h>
#include <ripple/core/config.h>
#include <ripple/protocol/hashprefix.h>
#include <ripple/protocol/serializer.h>

namespace ripple {

ledgerproposal::ledgerproposal (uint256 const& plgr, std::uint32_t seq,
                                uint256 const& tx, std::uint32_t closetime,
                                rippleaddress const& napeerpublic, uint256 const& suppression) :
    mpreviousledger (plgr), mcurrenthash (tx), msuppression (suppression), mclosetime (closetime),
    mproposeseq (seq), mpublickey (napeerpublic)
{
    // xxx validate key.
    // if (!mkey->setpubkey(pubkey))
    // throw std::runtime_error("invalid public key in proposal");

    mpeerid         = mpublickey.getnodeid ();
    mtime           = boost::posix_time::second_clock::universal_time ();
}

ledgerproposal::ledgerproposal (rippleaddress const& napub, rippleaddress const& napriv,
                                uint256 const& prevlgr, uint256 const& position,
                                std::uint32_t closetime) :
    mpreviousledger (prevlgr), mcurrenthash (position), mclosetime (closetime), mproposeseq (0),
    mpublickey (napub), mprivatekey (napriv)
{
    mpeerid      = mpublickey.getnodeid ();
    mtime        = boost::posix_time::second_clock::universal_time ();
}

ledgerproposal::ledgerproposal (uint256 const& prevlgr, uint256 const& position,
                                std::uint32_t closetime) :
    mpreviousledger (prevlgr), mcurrenthash (position), mclosetime (closetime), mproposeseq (0)
{
    mtime       = boost::posix_time::second_clock::universal_time ();
}

uint256 ledgerproposal::getsigninghash () const
{
    serializer s ((32 + 32 + 32 + 256 + 256) / 8);

    s.add32 (hashprefix::proposal);
    s.add32 (mproposeseq);
    s.add32 (mclosetime);
    s.add256 (mpreviousledger);
    s.add256 (mcurrenthash);

    return s.getsha512half ();
}

// compute a unique identifier for this signed proposal
uint256 ledgerproposal::computesuppressionid (
    uint256 const& proposehash,
    uint256 const& previousledger,
    std::uint32_t proposeseq,
    std::uint32_t closetime,
    blob const& pubkey,
    blob const& signature)
{

    serializer s (512);
    s.add256 (proposehash);
    s.add256 (previousledger);
    s.add32 (proposeseq);
    s.add32 (closetime);
    s.addvl (pubkey);
    s.addvl (signature);

    return s.getsha512half ();
}

bool ledgerproposal::checksign (std::string const& signature, uint256 const& signinghash)
{
    return mpublickey.verifynodepublic (signinghash, signature, ecdsa::not_strict);
}

bool ledgerproposal::changeposition (uint256 const& newposition, std::uint32_t closetime)
{
    if (mproposeseq == seqleave)
        return false;

    mcurrenthash    = newposition;
    mclosetime      = closetime;
    mtime           = boost::posix_time::second_clock::universal_time ();
    ++mproposeseq;
    return true;
}

void ledgerproposal::bowout ()
{
    mtime           = boost::posix_time::second_clock::universal_time ();
    mproposeseq     = seqleave;
}

blob ledgerproposal::sign (void)
{
    blob ret;

    mprivatekey.signnodeprivate (getsigninghash (), ret);
    // xxx if this can fail, find out sooner.
    // if (!mprivatekey.signnodeprivate(getsigninghash(), ret))
    //  throw std::runtime_error("unable to sign proposal");

    msuppression = computesuppressionid (mcurrenthash, mpreviousledger, mproposeseq,
        mclosetime, mpublickey.getnodepublic (), ret);

    return ret;
}

json::value ledgerproposal::getjson () const
{
    json::value ret = json::objectvalue;
    ret["previous_ledger"] = to_string (mpreviousledger);

    if (mproposeseq != seqleave)
    {
        ret["transaction_hash"] = to_string (mcurrenthash);
        ret["propose_seq"] = mproposeseq;
    }

    ret["close_time"] = mclosetime;

    if (mpublickey.isvalid ())
        ret["peer_id"] = mpublickey.humannodepublic ();

    return ret;
}

} // ripple
