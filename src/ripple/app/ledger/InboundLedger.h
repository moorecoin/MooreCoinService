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

#ifndef ripple_inboundledger_h
#define ripple_inboundledger_h

#include <ripple/app/ledger/ledger.h>
#include <ripple/app/peers/peerset.h>
#include <ripple/basics/countedobject.h>
#include <set>

namespace ripple {

// vfalco todo rename to inboundledger
// a ledger we are trying to acquire
class inboundledger
    : public peerset
    , public std::enable_shared_from_this <inboundledger>
    , public countedobject <inboundledger>
{
public:
    static char const* getcountedobjectname () { return "inboundledger"; }

    typedef std::shared_ptr <inboundledger> pointer;
    typedef std::pair < std::weak_ptr<peer>, std::shared_ptr<protocol::tmledgerdata> > peerdatapairtype;

    // these are the reasons we might acquire a ledger
    enum fcreason
    {
        fchistory,      // acquiring past ledger
        fcgeneric,      // generic other reasons
        fcvalidation,   // validations suggest this ledger is important
        fccurrent,      // this might be the current ledger
        fcconsensus,    // we believe the consensus round requires this ledger
    };

public:
    inboundledger (uint256 const& hash, std::uint32_t seq, fcreason reason, clock_type& clock);

    ~inboundledger ();

    bool isheader () const
    {
        return mhaveheader;
    }
    bool isacctstcomplete () const
    {
        return mhavestate;
    }
    bool istranscomplete () const
    {
        return mhavetransactions;
    }
    bool isdone () const
    {
        return maborted || iscomplete () || isfailed ();
    }
    ledger::ref getledger () const
    {
        return mledger;
    }
    void abort ()
    {
        maborted = true;
    }
    std::uint32_t getseq () const
    {
        return mseq;
    }

    // vfalco todo make this the listener / observer pattern
    bool addoncomplete (std::function<void (inboundledger::pointer)>);

    void trigger (peer::ptr const&);
    bool trylocal ();
    void addpeers ();
    bool checklocal ();
    void init (scopedlocktype& collectionlock);

    bool gotdata (std::weak_ptr<peer>, std::shared_ptr<protocol::tmledgerdata>);

    typedef std::pair <protocol::tmgetobjectbyhash::objecttype, uint256> neededhash_t;

    std::vector<neededhash_t> getneededhashes ();

    // vfalco todo replace uint256 with something semanticallyh meaningful
    void filternodes (std::vector<shamapnodeid>& nodeids, std::vector<uint256>& nodehashes,
                             std::set<shamapnodeid>& recentnodes, int max, bool aggressive);

    /** return a json::objectvalue. */
    json::value getjson (int);
    void rundata ();

private:
    void done ();

    void ontimer (bool progress, scopedlocktype& peersetlock);

    void newpeer (peer::ptr const& peer)
    {
        trigger (peer);
    }

    std::weak_ptr <peerset> pmdowncast ();

    int processdata (std::shared_ptr<peer> peer, protocol::tmledgerdata& data);

    bool takeheader (std::string const& data);
    bool taketxnode (const std::list<shamapnodeid>& ids, const std::list<blob >& data,
                     shamapaddnode&);
    bool taketxrootnode (blob const& data, shamapaddnode&);

    // vfalco todo rename to receiveaccountstatenode
    //             don't use acronyms, but if we are going to use them at least
    //             capitalize them correctly.
    //
    bool takeasnode (const std::list<shamapnodeid>& ids, const std::list<blob >& data,
                     shamapaddnode&);
    bool takeasrootnode (blob const& data, shamapaddnode&);

private:
    ledger::pointer    mledger;
    bool               mhaveheader;
    bool               mhavestate;
    bool               mhavetransactions;
    bool               maborted;
    bool               msignaled;
    bool               mbyhash;
    std::uint32_t      mseq;
    fcreason           mreason;

    std::set <shamapnodeid> mrecenttxnodes;
    std::set <shamapnodeid> mrecentasnodes;


    // data we have received from peers
    peerset::locktype mreceiveddatalock;
    std::vector <peerdatapairtype> mreceiveddata;
    bool mreceivedispatched;

    std::vector <std::function <void (inboundledger::pointer)> > moncomplete;
};

} // ripple

#endif
