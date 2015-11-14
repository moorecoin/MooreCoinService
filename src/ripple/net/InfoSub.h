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

#ifndef ripple_net_rpc_infosub_h_included
#define ripple_net_rpc_infosub_h_included

#include <ripple/basics/countedobject.h>
#include <ripple/json/json_value.h>
#include <ripple/protocol/rippleaddress.h>
#include <ripple/resource/consumer.h>
#include <ripple/protocol/book.h>
#include <beast/threads/stoppable.h>
#include <mutex>

namespace ripple {

// operations that clients may wish to perform against the network
// master operational handler, server sequencer, network tracker

class pathrequest;

/** manages a client's subscription to data feeds.
*/
class infosub
    : public countedobject <infosub>
{
public:
    static char const* getcountedobjectname () { return "infosub"; }

    typedef std::shared_ptr<infosub>          pointer;

    // vfalco todo standardize on the names of weak / strong pointer typedefs.
    typedef std::weak_ptr<infosub>            wptr;

    typedef const std::shared_ptr<infosub>&   ref;

    typedef resource::consumer consumer;

public:
    /** abstracts the source of subscription data.
    */
    class source : public beast::stoppable
    {
    protected:
        source (char const* name, beast::stoppable& parent);

    public:
        // vfalco todo rename the 'rt' parameters to something meaningful.
        virtual void subaccount (ref isplistener,
            const hash_set<rippleaddress>& vnaaccountids,
                std::uint32_t uledgerindex, bool rt) = 0;

        virtual void unsubaccount (std::uint64_t ulistener,
            const hash_set<rippleaddress>& vnaaccountids,
                bool rt) = 0;

        // vfalco todo document the bool return value
        virtual bool subledger (ref isplistener, json::value& jvresult) = 0;
        virtual bool unsubledger (std::uint64_t ulistener) = 0;

        virtual bool subserver (ref isplistener, json::value& jvresult,
            bool admin) = 0;
        virtual bool unsubserver (std::uint64_t ulistener) = 0;

        virtual bool subbook (ref isplistener, book const&) = 0;
        virtual bool unsubbook (std::uint64_t ulistener, book const&) = 0;

        virtual bool subtransactions (ref isplistener) = 0;
        virtual bool unsubtransactions (std::uint64_t ulistener) = 0;

        virtual bool subrttransactions (ref isplistener) = 0;
        virtual bool unsubrttransactions (std::uint64_t ulistener) = 0;

        // vfalco todo remove
        //             this was added for one particular partner, it
        //             "pushes" subscription data to a particular url.
        //
        virtual pointer findrpcsub (std::string const& strurl) = 0;
        virtual pointer addrpcsub (std::string const& strurl, ref rspentry) = 0;
    };

public:
    infosub (source& source, consumer consumer);

    virtual ~infosub ();

    consumer& getconsumer();

    virtual void send (json::value const& jvobj, bool broadcast) = 0;

    // vfalco note why is this virtual?
    virtual void send (
        json::value const& jvobj, std::string const& sobj, bool broadcast);

    std::uint64_t getseq ();

    void onsendempty ();

    void insertsubaccountinfo (rippleaddress addr, std::uint32_t uledgerindex);

    void clearpathrequest ();

    void setpathrequest (const std::shared_ptr<pathrequest>& req);

    std::shared_ptr <pathrequest> const& getpathrequest ();

protected:
    typedef std::mutex locktype;
    typedef std::lock_guard <locktype> scopedlocktype;
    locktype mlock;

private:
    consumer                      m_consumer;
    source&                       m_source;
    hash_set <rippleaddress>      msubaccountinfo;
    hash_set <rippleaddress>      msubaccounttransaction;
    std::shared_ptr <pathrequest> mpathrequest;
    std::uint64_t                 mseq;
};

} // ripple

#endif
