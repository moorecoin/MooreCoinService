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

#ifndef ripple_pathrequest_h
#define ripple_pathrequest_h

#include <ripple/app/paths/ripplelinecache.h>
#include <ripple/json/json_value.h>
#include <ripple/net/infosub.h>

namespace ripple {

// a pathfinding request submitted by a client
// the request issuer must maintain a strong pointer

class ripplelinecache;
class pathrequests;

// return values from parsejson <0 = invalid, >0 = valid
#define pfr_pj_invalid              -1
#define pfr_pj_nochange             0
#define pfr_pj_change               1

class pathrequest :
    public std::enable_shared_from_this <pathrequest>,
    public countedobject <pathrequest>
{
public:
    static char const* getcountedobjectname () { return "pathrequest"; }

    typedef std::weak_ptr<pathrequest>    wptr;
    typedef std::shared_ptr<pathrequest>  pointer;
    typedef const pointer&                  ref;
    typedef const wptr&                     wref;

public:
    // vfalco todo break the cyclic dependency on infosub
    pathrequest (
        std::shared_ptr <infosub> const& subscriber,
        int id,
        pathrequests&,
        beast::journal journal);

    ~pathrequest ();

    bool        isvalid ();
    bool        isnew ();
    bool        needsupdate (bool newonly, ledgerindex index);
    void        updatecomplete ();
    json::value getstatus ();

    json::value docreate (
        const std::shared_ptr<ledger>&,
        const ripplelinecache::pointer&,
        json::value const&,
        bool&);
    json::value doclose (json::value const&);
    json::value dostatus (json::value const&);

    // update jvstatus
    json::value doupdate (const std::shared_ptr<ripplelinecache>&, bool fast);
    infosub::pointer getsubscriber ();

private:
    bool isvalid (ripplelinecache::ref crcache);
    void setvalid ();
    void resetlevel (int level);
    int parsejson (json::value const&, bool complete);

    beast::journal m_journal;

    typedef ripplerecursivemutex locktype;
    typedef std::lock_guard <locktype> scopedlocktype;
    locktype mlock;

    pathrequests& mowner;

    std::weak_ptr<infosub> wpsubscriber; // who this request came from

    json::value jvid;
    json::value jvstatus;                   // last result

    // client request parameters
    rippleaddress rasrcaccount;
    rippleaddress radstaccount;
    stamount sadstamount;

    std::set<issue> scisourcecurrencies;
    std::map<issue, stpathset> mcontext;

    bool bvalid;

    locktype mindexlock;
    ledgerindex mlastindex;
    bool minprogress;

    int ilastlevel;
    bool blastsuccess;

    int iidentifier;

    boost::posix_time::ptime ptcreated;
    boost::posix_time::ptime ptquickreply;
    boost::posix_time::ptime ptfullreply;
};

} // ripple

#endif
