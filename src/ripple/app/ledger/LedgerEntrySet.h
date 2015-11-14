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

#ifndef ripple_ledgerentryset_h
#define ripple_ledgerentryset_h

#include <ripple/app/ledger/ledger.h>
#include <ripple/basics/countedobject.h>
#include <ripple/protocol/stledgerentry.h>

namespace ripple {

// vfalco does this belong here? is it correctly named?

enum transactionengineparams
{
    tapnone             = 0x00,

    // signature already checked
    tapno_check_sign    = 0x01,

    // transaction is running against an open ledger
    // true = failures are not forwarded, check transaction fee
    // false = debit ledger for consumed funds
    tapopen_ledger      = 0x10,

    // this is not the transaction's last pass
    // transaction can be retried, soft failures allowed
    tapretry            = 0x20,

    // transaction came from a privileged source
    tapadmin            = 0x400,
};

enum ledgerentryaction
{
    taanone,
    taacached,  // unmodified.
    taamodify,  // modifed, must have previously been taacached.
    taadelete,  // delete, must have previously been taadelete or taamodify.
    taacreate,  // newly created.
};

enum freezehandling
{
    fhignore_freeze,
    fhzero_if_frozen
};

class ledgerentrysetentry
    : public countedobject <ledgerentrysetentry>
{
public:
    static char const* getcountedobjectname () { return "ledgerentrysetentry"; }

    sle::pointer        mentry;
    ledgerentryaction   maction;
    int                 mseq;

    ledgerentrysetentry (sle::ref e, ledgerentryaction a, int s)
        : mentry (e)
        , maction (a)
        , mseq (s)
    {
    }
};

/** an les is a ledgerentryset.

    it's a view into a ledger used while a transaction is processing.
    the transaction manipulates the les rather than the ledger
    (because it's cheaper, can be checkpointed, and so on). when the
    transaction finishes, the les is committed into the ledger to make
    the modifications. the transaction metadata is built from the les too.
*/
class ledgerentryset
    : public countedobject <ledgerentryset>
{
public:
    static char const* getcountedobjectname () { return "ledgerentryset"; }

    ledgerentryset (
        ledger::ref ledger, transactionengineparams tep, bool immutable = false)
        : mledger (ledger), mparams (tep), mseq (0), mimmutable (immutable)
    {
    }

    ledgerentryset () : mparams (tapnone), mseq (0), mimmutable (false)
    {
    }

    // make a duplicate of this set.
    ledgerentryset duplicate () const;

    // swap the contents of two sets
    void swapwith (ledgerentryset&);

    void invalidate ()
    {
        mledger.reset ();
    }

    bool isvalid () const
    {
        return mledger != nullptr;
    }

    int getseq () const
    {
        return mseq;
    }

    transactionengineparams getparams () const
    {
        return mparams;
    }

    void bumpseq ()
    {
        ++mseq;
    }

    void init (ledger::ref ledger, uint256 const& transactionid,
               std::uint32_t ledgerid, transactionengineparams params);

    void clear ();

    ledger::pointer& getledger ()
    {
        return mledger;
    }

    bool enforcefreeze () const
    {
        return mledger->enforcefreeze ();
    }

    // basic entry functions
    sle::pointer getentry (uint256 const& index, ledgerentryaction&);
    ledgerentryaction hasentry (uint256 const& index) const;
    void entrycache (sle::ref);     // add this entry to the cache
    void entrycreate (sle::ref);    // this entry will be created
    void entrydelete (sle::ref);    // this entry will be deleted
    void entrymodify (sle::ref);    // this entry will be modified

    // higher-level ledger functions
    sle::pointer entrycreate (ledgerentrytype lettype, uint256 const& uindex);
    sle::pointer entrycache (ledgerentrytype lettype, uint256 const& uindex);

    // directory functions.
    ter diradd (
        std::uint64_t&                      unodedir,      // node of entry.
        uint256 const&                      urootindex,
        uint256 const&                      uledgerindex,
        std::function<void (sle::ref, bool)> fdescriber);

    ter dirdelete (
        const bool           bkeeproot,
        const std::uint64_t& unodedir,      // node item is mentioned in.
        uint256 const&       urootindex,
        uint256 const&       uledgerindex,  // item being deleted
        const bool           bstable,
        const bool           bsoft);

    bool dirfirst (uint256 const& urootindex, sle::pointer& slenode,
        unsigned int & udirentry, uint256 & uentryindex);
    bool dirnext (uint256 const& urootindex, sle::pointer& slenode,
        unsigned int & udirentry, uint256 & uentryindex);
    bool dirisempty (uint256 const& udirindex);
    ter dircount (uint256 const& udirindex, std::uint32_t & ucount);

    uint256 getnextledgerindex (uint256 const& uhash);
    uint256 getnextledgerindex (uint256 const& uhash, uint256 const& uend);

    /** @{ */
    void incrementownercount (sle::ref sleaccount);
    void incrementownercount (account const& owner);
    /** @} */

    /** @{ */
    void decrementownercount (sle::ref sleaccount);
    void decrementownercount (account const& owner);
    /** @} */

    // offer functions.
    ter offerdelete (sle::pointer);
    ter offerdelete (uint256 const& offerindex)
    {
        return offerdelete( entrycache (ltoffer, offerindex));
    }

    // balance functions.
    bool isfrozen (
        account const& account,
        currency const& currency,
        account const& issuer);

    bool isglobalfrozen (account const& issuer);

    stamount rippletransferfee (
        account const& usenderid, account const& ureceiverid,
        account const& issuer, const stamount & saamount);

    ter ripplecredit (
        account const& usenderid, account const& ureceiverid,
        const stamount & saamount, bool bcheckissuer = true);

    stamount accountholds (
        account const& account, currency const& currency,
        account const& issuer, freezehandling freezehandling);
    stamount accountfunds (
        account const& account, const stamount & sadefault, freezehandling freezehandling);
    ter accountsend (
        account const& usenderid, account const& ureceiverid,
        const stamount & saamount);

    std::tuple<stamount, bool> assetreleased(
        stamount const& amount,
        uint256 assetstateindex,
        sle::ref sleassetstate);

    ter assetrelease(
        account const& usrcaccountid,
        account const& udstaccountid,
        currency const& currency,
        sle::ref sleripplestate);

    ter trustcreate (
        const bool      bsrchigh,
        account const&  usrcaccountid,
        account const&  udstaccountid,
        uint256 const&  uindex,
        sle::ref        sleaccount,
        const bool      bauth,
        const bool      bnoripple,
        const bool      bfreeze,
        stamount const& sasrcbalance,
        stamount const& sasrclimit,
        const std::uint32_t usrcqualityin = 0,
        const std::uint32_t usrcqualityout = 0);
    ter trustdelete (
        sle::ref sleripplestate, account const& ulowaccountid,
        account const& uhighaccountid);
    
    ter sharefeewithreferee (account const& usenderid, account const& uissuerid, const stamount& saamount);
    
    ter addrefer (account const& refereeid, account const& referenceid);

    json::value getjson (int) const;
    void calcrawmeta (serializer&, ter result, std::uint32_t index);

    // iterator functions
    typedef std::map<uint256, ledgerentrysetentry>::iterator iterator;
    typedef std::map<uint256, ledgerentrysetentry>::const_iterator const_iterator;

    bool empty () const
    {
        return mentries.empty ();
    }
    const_iterator cbegin () const
    {
        return mentries.cbegin ();
    }
    const_iterator cend () const
    {
        return mentries.cend ();
    }
    const_iterator begin () const
    {
        return mentries.cbegin ();
    }
    const_iterator end () const
    {
        return mentries.cend ();
    }
    iterator begin ()
    {
        return mentries.begin ();
    }
    iterator end ()
    {
        return mentries.end ();
    }

    void setdeliveredamount (stamount const& amt)
    {
        mset.setdeliveredamount (amt);
    }

private:
    ledger::pointer mledger;
    std::map<uint256, ledgerentrysetentry>  mentries; // cannot be unordered!

    typedef hash_map<uint256, sle::pointer> nodetoledgerentry;

    transactionmetaset mset;
    transactionengineparams mparams;
    int mseq;
    bool mimmutable;

    ledgerentryset (
        ledger::ref ledger, const std::map<uint256, ledgerentrysetentry>& e,
        const transactionmetaset & s, int m) :
        mledger (ledger), mentries (e), mset (s), mparams (tapnone), mseq (m),
        mimmutable (false)
    {}

    sle::pointer getformod (
        uint256 const& node, ledger::ref ledger,
        nodetoledgerentry& newmods);

    bool threadtx (
        const rippleaddress & threadto, ledger::ref ledger,
        nodetoledgerentry& newmods);

    bool threadtx (
        sle::ref threadto, ledger::ref ledger, nodetoledgerentry& newmods);

    bool threadowners (
        sle::ref node, ledger::ref ledger, nodetoledgerentry& newmods);

    ter ripplesend (
        account const& usenderid, account const& ureceiverid,
        const stamount & saamount, stamount & saactual);
    
    stamount rippleholds (
        account const& account, currency const& currency,
        account const& issuer, freezehandling zeroiffrozen);
};

// nikb fixme: move these to the right place
std::uint32_t
rippletransferrate (ledgerentryset& ledger, account const& issuer);

std::uint32_t
rippletransferrate (ledgerentryset& ledger, account const& usenderid,
    account const& ureceiverid, account const& issuer);

} // ripple

#endif
