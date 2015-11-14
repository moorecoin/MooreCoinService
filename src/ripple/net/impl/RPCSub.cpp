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
#include <ripple/net/rpcsub.h>
#include <ripple/basics/log.h>
#include <ripple/basics/stringutilities.h>
#include <ripple/json/to_string.h>
#include <ripple/net/rpccall.h>
#include <deque>

namespace ripple {

// subscription object for json-rpc
class rpcsubimp
    : public rpcsub
    , public beast::leakchecked <rpcsub>
{
public:
    rpcsubimp (infosub::source& source, boost::asio::io_service& io_service,
        jobqueue& jobqueue, std::string const& strurl, std::string const& strusername,
            std::string const& strpassword)
        : rpcsub (source)
        , m_io_service (io_service)
        , m_jobqueue (jobqueue)
        , murl (strurl)
        , mssl (false)
        , musername (strusername)
        , mpassword (strpassword)
        , msending (false)
    {
        std::string strscheme;

        if (!parseurl (strurl, strscheme, mip, mport, mpath))
        {
            throw std::runtime_error ("failed to parse url.");
        }
        else if (strscheme == "https")
        {
            mssl    = true;
        }
        else if (strscheme != "http")
        {
            throw std::runtime_error ("only http and https is supported.");
        }

        mseq    = 1;

        if (mport < 0)
            mport   = mssl ? 443 : 80;

        writelog (lsinfo, rpcsub) <<
            "rpccall::fromnetwork sub: ip=" << mip <<
            " port=" << mport <<
            " ssl= "<< (mssl ? "yes" : "no") <<
            " path='" << mpath << "'";
    }

    ~rpcsubimp ()
    {
    }

    void send (json::value const& jvobj, bool broadcast)
    {
        scopedlocktype sl (mlock);

        if (mdeque.size () >= eventqueuemax)
        {
            // drop the previous event.
            writelog (lswarning, rpcsub) << "rpccall::fromnetwork drop";
            mdeque.pop_back ();
        }

        writelog (broadcast ? lsdebug : lsinfo, rpcsub) <<
            "rpccall::fromnetwork push: " << jvobj;

        mdeque.push_back (std::make_pair (mseq++, jvobj));

        if (!msending)
        {
            // start a sending thread.
            msending    = true;

            writelog (lsinfo, rpcsub) << "rpccall::fromnetwork start";

            m_jobqueue.addjob (
                jtclient, "rpcsub::sendthread", std::bind (&rpcsubimp::sendthread, this));
        }
    }

    void setusername (std::string const& strusername)
    {
        scopedlocktype sl (mlock);

        musername = strusername;
    }

    void setpassword (std::string const& strpassword)
    {
        scopedlocktype sl (mlock);

        mpassword = strpassword;
    }

private:
    // xxx could probably create a bunch of send jobs in a single get of the lock.
    void sendthread ()
    {
        json::value jvevent;
        bool bsend;

        do
        {
            {
                // obtain the lock to manipulate the queue and change sending.
                scopedlocktype sl (mlock);

                if (mdeque.empty ())
                {
                    msending    = false;
                    bsend       = false;
                }
                else
                {
                    std::pair<int, json::value> pevent  = mdeque.front ();

                    mdeque.pop_front ();

                    jvevent     = pevent.second;
                    jvevent["seq"]  = pevent.first;

                    bsend       = true;
                }
            }

            // send outside of the lock.
            if (bsend)
            {
                // xxx might not need this in a try.
                try
                {
                    writelog (lsinfo, rpcsub) << "rpccall::fromnetwork: " << mip;

                    rpccall::fromnetwork (
                        m_io_service,
                        mip, mport,
                        musername, mpassword,
                        mpath, "event",
                        jvevent,
                        mssl);
                }
                catch (const std::exception& e)
                {
                    writelog (lsinfo, rpcsub) << "rpccall::fromnetwork exception: " << e.what ();
                }
            }
        }
        while (bsend);
    }

private:
// vfalco todo replace this macro with a language constant
    enum
    {
        eventqueuemax = 32
    };

    boost::asio::io_service& m_io_service;
    jobqueue& m_jobqueue;

    std::string             murl;
    std::string             mip;
    int                     mport;
    bool                    mssl;
    std::string             musername;
    std::string             mpassword;
    std::string             mpath;

    int                     mseq;                       // next id to allocate.

    bool                    msending;                   // sending threead is active.

    std::deque<std::pair<int, json::value> >    mdeque;
};

//------------------------------------------------------------------------------

rpcsub::rpcsub (infosub::source& source)
    : infosub (source, consumer())
{
}

rpcsub::pointer rpcsub::new (infosub::source& source,
    boost::asio::io_service& io_service, jobqueue& jobqueue,
        std::string const& strurl, std::string const& strusername,
        std::string const& strpassword)
{
    return std::make_shared <rpcsubimp> (std::ref (source),
        std::ref (io_service), std::ref (jobqueue),
            strurl, strusername, strpassword);
}

} // ripple
