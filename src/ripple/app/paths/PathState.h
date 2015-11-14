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

#ifndef ripple_pathstate_h
#define ripple_pathstate_h

#include <ripple/app/ledger/ledgerentryset.h>
#include <ripple/app/paths/node.h>
#include <ripple/app/paths/types.h>

namespace ripple {

// holds a single path state under incremental application.
class pathstate : public countedobject <pathstate>
{
  public:
    typedef std::vector<uint256> offerindexlist;
    typedef std::shared_ptr<pathstate> ptr;
    typedef std::vector<ptr> list;

    pathstate (stamount const& sasend, stamount const& sasendmax)
        : mindex (0)
        , uquality (0)
        , sainreq (sasendmax)
        , saoutreq (sasend)
    {
    }

    explicit pathstate (const pathstate& pssrc) = default;

    void reset(stamount const& in, stamount const& out);

    ter expandpath (
        ledgerentryset const&   lessource,
        stpath const&           spsourcepath,
        account const&          ureceiverid,
        account const&          usenderid
    );

    path::node::list& nodes() { return nodes_; }

    stamount const& inpass() const { return sainpass; }
    stamount const& outpass() const { return saoutpass; }
    stamount const& outreq() const { return saoutreq; }

    stamount const& inact() const { return sainact; }
    stamount const& outact() const { return saoutact; }
    stamount const& inreq() const { return sainreq; }

    void setinpass(stamount const& sa)
    {
        sainpass = sa;
    }

    void setoutpass(stamount const& sa)
    {
        saoutpass = sa;
    }

    accountissuetonodeindex const& forward() { return umforward; }
    accountissuetonodeindex const& reverse() { return umreverse; }

    void insertreverse (accountissue const& ai, path::nodeindex i)
    {
        umreverse.insert({ai, i});
    }

    json::value getjson () const;

    static char const* getcountedobjectname () { return "pathstate"; }
    offerindexlist& unfundedoffers() { return unfundedoffers_; }

    void setstatus(ter status) { terstatus = status; }
    ter status() const { return terstatus; }

    std::uint64_t quality() const { return uquality; }
    void setquality (std::uint64_t q) { uquality = q; }

    bool allliquidityconsumed() const { return allliquidityconsumed_; }
    void consumeallliquidity () { allliquidityconsumed_ = true; }

    void setindex (int i) { mindex  = i; }
    int index() const { return mindex; }

    ter checknoripple (account const& destinationaccountid,
                       account const& sourceaccountid);
    void checkfreeze ();

    static bool lesspriority (pathstate const& lhs, pathstate const& rhs);

    ledgerentryset& ledgerentries() { return lesentries; }

    bool isdry() const
    {
        return !(sainpass && saoutpass);
    }

  private:
    ter checknoripple (
        account const&, account const&, account const&, currency const&);

    /** clear path structures, and clear each node. */
    void clear();

    ter terstatus;
    path::node::list nodes_;

    // when processing, don't want to complicate directory walking with
    // deletion.  offers that became unfunded or were completely consumed go
    // here and are deleted at the end.
    offerindexlist unfundedoffers_;

    // first time scanning foward, as part of path construction, a funding
    // source was mentioned for accounts. source may only be used there.
    accountissuetonodeindex umforward;

    // first time working in reverse a funding source was used.
    // source may only be used there if not mentioned by an account.
    accountissuetonodeindex umreverse;

    ledgerentryset lesentries;

    int                         mindex;    // index/rank amoung siblings.
    std::uint64_t               uquality;  // 0 = no quality/liquity left.

    stamount const&             sainreq;   // --> max amount to spend by sender.
    stamount                    sainact;   // --> amount spent by sender so far.
    stamount                    sainpass;  // <-- amount spent by sender.

    stamount const&             saoutreq;  // --> amount to send.
    stamount                    saoutact;  // --> amount actually sent so far.
    stamount                    saoutpass; // <-- amount actually sent.

    // if true, all liquidity on this path has been consumed.
    bool allliquidityconsumed_ = false;

    ter pushnode (
        int const itype,
        account const& account,
        currency const& currency,
        account const& issuer);

    ter pushimpliednodes (
        account const& account,
        currency const& currency,
        account const& issuer);
};

} // ripple

#endif
