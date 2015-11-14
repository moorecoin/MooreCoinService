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

#ifndef ripple_amendment_table_h
#define ripple_amendment_table_h

#include <ripple/app/book/types.h>

namespace ripple {

/** the status of all amendments requested in a given window. */
class amendmentset
{
public:
    std::uint32_t mclosetime;
    int mtrustedvalidations;                    // number of trusted validations
    hash_map<uint256, int> mvotes; // yes votes by amendment

    amendmentset (std::uint32_t ct) : mclosetime (ct), mtrustedvalidations (0)
    {
        ;
    }
    void addvoter ()
    {
        ++mtrustedvalidations;
    }
    void addvote (uint256 const& amendment)
    {
        ++mvotes[amendment];
    }
};

/** current state of an amendment.
    tells if a amendment is supported, enabled or vetoed. a vetoed amendment
    means the node will never announce its support.
*/
class amendmentstate
{
public:
    bool mvetoed;   // we don't want this amendment enabled
    bool menabled;
    bool msupported;
    bool mdefault;  // include in genesis ledger

    core::clock::time_point m_firstmajority; // first time we saw a majority (close time)
    core::clock::time_point m_lastmajority;  // most recent time we saw a majority (close time)

    std::string mfriendlyname;

    amendmentstate ()
        : mvetoed (false), menabled (false), msupported (false), mdefault (false),
          m_firstmajority (0), m_lastmajority (0)
    {
        ;
    }

    void setveto ()
    {
        mvetoed = true;
    }
    void setdefault ()
    {
        mdefault = true;
    }
    bool isdefault ()
    {
        return mdefault;
    }
    bool issupported ()
    {
        return msupported;
    }
    bool isvetoed ()
    {
        return mvetoed;
    }
    bool isenabled ()
    {
        return menabled;
    }
    std::string const& getfiendlyname ()
    {
        return mfriendlyname;
    }
    void setfriendlyname (std::string const& n)
    {
        mfriendlyname = n;
    }
};

/** the amendment table stores the list of enabled and potential amendments.
    individuals amendments are voted on by validators during the consensus
    process.
*/
class amendmenttable
{
public:
    /** create a new amendmenttable.

        @param majoritytime the number of seconds an amendment must hold a majority
                            before we're willing to vote yes on it.
        @param majorityfraction ratio, out of 256, of servers that must say
                                they want an amendment before we consider it to
                                have a majority.
        @param journal
    */

    virtual ~amendmenttable() { }

    virtual void addinitial () = 0;

    virtual amendmentstate* addknown (const char* amendmentid,
        const char* friendlyname, bool veto) = 0;
    virtual uint256 get (std::string const& name) = 0;

    virtual bool veto (uint256 const& amendment) = 0;
    virtual bool unveto (uint256 const& amendment) = 0;

    virtual bool enable (uint256 const& amendment) = 0;
    virtual bool disable (uint256 const& amendment) = 0;

    virtual bool isenabled (uint256 const& amendment) = 0;
    virtual bool issupported (uint256 const& amendment) = 0;

    virtual void setenabled (const std::vector<uint256>& amendments) = 0;
    virtual void setsupported (const std::vector<uint256>& amendments) = 0;

    virtual void reportvalidations (const amendmentset&) = 0;

    virtual json::value getjson (int) = 0;

    /** returns a json::objectvalue. */
    virtual json::value getjson (uint256 const& ) = 0;

    virtual void
    dovalidation (ledger::ref lastclosedledger, stobject& basevalidation) = 0;
    virtual void
    dovoting (ledger::ref lastclosedledger, shamap::ref initialposition) = 0;
};

std::unique_ptr<amendmenttable>
make_amendmenttable (std::chrono::seconds majoritytime, int majorityfraction,
    beast::journal journal);

} // ripple

#endif
