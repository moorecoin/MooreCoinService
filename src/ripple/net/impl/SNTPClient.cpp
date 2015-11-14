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
#include <ripple/basics/log.h>
#include <ripple/crypto/randomnumbers.h>
#include <ripple/net/sntpclient.h>
#include <beast/asio/placeholders.h>
#include <beast/threads/thread.h>
#include <boost/asio.hpp>
#include <boost/foreach.hpp>
#include <mutex>

namespace ripple {

// #define sntp_debug

static uint8_t sntpquerydata[48] =
{ 0x1b, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

// ntp query frequency - 4 minutes
#define ntp_query_frequency     (4 * 60)

// ntp minimum interval to query same servers - 3 minutes
#define ntp_min_query           (3 * 60)

// ntp sample window (should be odd)
#define ntp_sample_window       9

// ntp timestamp constant
#define ntp_unix_offset         0x83aa7e80

// ntp timestamp validity
#define ntp_timestamp_valid     ((ntp_query_frequency + ntp_min_query) * 2)

// sntp packet offsets
#define ntp_off_info            0
#define ntp_off_rootdelay       1
#define ntp_off_rootdisp        2
#define ntp_off_referenceid     3
#define ntp_off_refts_int       4
#define ntp_off_refts_frac      5
#define ntp_off_orgts_int       6
#define ntp_off_orgts_frac      7
#define ntp_off_recvts_int      8
#define ntp_off_recvts_frac     9
#define ntp_off_xmitts_int      10
#define ntp_off_xmitts_frac     11

class sntpclientimp
    : public sntpclient
    , public beast::thread
    , public beast::leakchecked <sntpclientimp>
{
public:
    class sntpquery
    {
    public:
        bool                mreceivedreply;
        time_t              mlocaltimesent;
        std::uint32_t              mquerynonce;

        sntpquery (time_t j = (time_t) - 1)   : mreceivedreply (false), mlocaltimesent (j)
        {
            ;
        }
    };

    //--------------------------------------------------------------------------

    explicit sntpclientimp (stoppable& parent)
        : sntpclient (parent)
        , thread ("sntpclient")
        , msocket (m_io_service)
        , mtimer (m_io_service)
        , mresolver (m_io_service)
        , moffset (0)
        , mlastoffsetupdate ((time_t) - 1)
        , mreceivebuffer (256)
    {
        msocket.open (boost::asio::ip::udp::v4 ());

        msocket.async_receive_from (boost::asio::buffer (mreceivebuffer, 256),
            mreceiveendpoint, std::bind (
                &sntpclientimp::receivepacket, this,
                    beast::asio::placeholders::error,
                        beast::asio::placeholders::bytes_transferred));

        mtimer.expires_from_now (boost::posix_time::seconds (ntp_query_frequency));
        mtimer.async_wait (std::bind (&sntpclientimp::timerentry, this, beast::asio::placeholders::error));
    }

    ~sntpclientimp ()
    {
        stopthread ();
    }

    //--------------------------------------------------------------------------

    void onstart ()
    {
        startthread ();
    }

    void onstop ()
    {
        // hack!
        m_io_service.stop ();
    }

    void run ()
    {
        m_io_service.run ();

        stopped ();
    }

    //--------------------------------------------------------------------------

    void init (const std::vector<std::string>& servers)
    {
        std::vector<std::string>::const_iterator it = servers.begin ();

        if (it == servers.end ())
        {
            writelog (lsinfo, sntpclient) << "sntp: no server specified";
            return;
        }

        boost_foreach (std::string const& it, servers)
        addserver (it);
        queryall ();
    }

    void addserver (std::string const& server)
    {
        scopedlocktype sl (mlock);
        mservers.push_back (std::make_pair (server, (time_t) - 1));
    }

    void queryall ()
    {
        while (doquery ())
        {
        }
    }

    bool getoffset (int& offset)
    {
        scopedlocktype sl (mlock);

        if ((mlastoffsetupdate == (time_t) - 1) || ((mlastoffsetupdate + ntp_timestamp_valid) < time (nullptr)))
            return false;

        offset = moffset;
        return true;
    }

    bool doquery ()
    {
        scopedlocktype sl (mlock);
        std::vector< std::pair<std::string, time_t> >::iterator best = mservers.end ();

        for (std::vector< std::pair<std::string, time_t> >::iterator it = mservers.begin (), end = best;
                it != end; ++it)
            if ((best == end) || (it->second == (time_t) - 1) || (it->second < best->second))
                best = it;

        if (best == mservers.end ())
        {
            writelog (lstrace, sntpclient) << "sntp: no server to query";
            return false;
        }

        time_t now = time (nullptr);

        if ((best->second != (time_t) - 1) && ((best->second + ntp_min_query) >= now))
        {
            writelog (lstrace, sntpclient) << "sntp: all servers recently queried";
            return false;
        }

        best->second = now;

        boost::asio::ip::udp::resolver::query query (boost::asio::ip::udp::v4 (), best->first, "ntp");
        mresolver.async_resolve (query,
                                 std::bind (&sntpclientimp::resolvecomplete, this,
                                              beast::asio::placeholders::error, beast::asio::placeholders::iterator));
    #ifdef sntp_debug
        writelog (lstrace, sntpclient) << "sntp: resolve pending for " << best->first;
    #endif
        return true;
    }

    void resolvecomplete (const boost::system::error_code& error, boost::asio::ip::udp::resolver::iterator it)
    {
        if (!error)
        {
            boost::asio::ip::udp::resolver::iterator sel = it;
            int i = 1;

            while (++it != boost::asio::ip::udp::resolver::iterator ())
                if ((rand () % ++i) == 0)
                    sel = it;

            if (sel != boost::asio::ip::udp::resolver::iterator ())
            {
                scopedlocktype sl (mlock);
                sntpquery& query = mqueries[*sel];
                time_t now = time (nullptr);

                if ((query.mlocaltimesent == now) || ((query.mlocaltimesent + 1) == now))
                {
                    // this can happen if the same ip address is reached through multiple names
                    writelog (lstrace, sntpclient) << "sntp: redundant query suppressed";
                    return;
                }

                query.mreceivedreply = false;
                query.mlocaltimesent = now;
                random_fill (&query.mquerynonce);
                reinterpret_cast<std::uint32_t*> (sntpquerydata)[ntp_off_xmitts_int] = static_cast<std::uint32_t> (time (nullptr)) + ntp_unix_offset;
                reinterpret_cast<std::uint32_t*> (sntpquerydata)[ntp_off_xmitts_frac] = query.mquerynonce;
                msocket.async_send_to (boost::asio::buffer (sntpquerydata, 48), *sel,
                                       std::bind (&sntpclientimp::sendcomplete, this,
                                                    beast::asio::placeholders::error, beast::asio::placeholders::bytes_transferred));
            }
        }
    }

    void receivepacket (const boost::system::error_code& error, std::size_t bytes_xferd)
    {
        if (!error)
        {
            scopedlocktype sl (mlock);
    #ifdef sntp_debug
            writelog (lstrace, sntpclient) << "sntp: packet from " << mreceiveendpoint;
    #endif
            std::map<boost::asio::ip::udp::endpoint, sntpquery>::iterator query = mqueries.find (mreceiveendpoint);

            if (query == mqueries.end ())
                writelog (lsdebug, sntpclient) << "sntp: reply from " << mreceiveendpoint << " found without matching query";
            else if (query->second.mreceivedreply)
                writelog (lsdebug, sntpclient) << "sntp: duplicate response from " << mreceiveendpoint;
            else
            {
                query->second.mreceivedreply = true;

                if (time (nullptr) > (query->second.mlocaltimesent + 1))
                    writelog (lswarning, sntpclient) << "sntp: late response from " << mreceiveendpoint;
                else if (bytes_xferd < 48)
                    writelog (lswarning, sntpclient) << "sntp: short reply from " << mreceiveendpoint
                                                     << " (" << bytes_xferd << ") " << mreceivebuffer.size ();
                else if (reinterpret_cast<std::uint32_t*> (&mreceivebuffer[0])[ntp_off_orgts_frac] != query->second.mquerynonce)
                    writelog (lswarning, sntpclient) << "sntp: reply from " << mreceiveendpoint << "had wrong nonce";
                else
                    processreply ();
            }
        }

        msocket.async_receive_from (boost::asio::buffer (mreceivebuffer, 256), mreceiveendpoint,
                                    std::bind (&sntpclientimp::receivepacket, this, beast::asio::placeholders::error,
                                            beast::asio::placeholders::bytes_transferred));
    }

    void sendcomplete (const boost::system::error_code& error, std::size_t)
    {
        condlog (error, lswarning, sntpclient) << "sntp: send error";
    }

    void processreply ()
    {
        assert (mreceivebuffer.size () >= 48);
        std::uint32_t* recvbuffer = reinterpret_cast<std::uint32_t*> (&mreceivebuffer.front ());

        unsigned info = ntohl (recvbuffer[ntp_off_info]);
        int64_t timev = ntohl (recvbuffer[ntp_off_recvts_int]);
        unsigned stratum = (info >> 16) & 0xff;

        if ((info >> 30) == 3)
        {
            writelog (lsinfo, sntpclient) << "sntp: alarm condition " << mreceiveendpoint;
            return;
        }

        if ((stratum == 0) || (stratum > 14))
        {
            writelog (lsinfo, sntpclient) << "sntp: unreasonable stratum (" << stratum << ") from " << mreceiveendpoint;
            return;
        }

        std::int64_t now = static_cast<int> (time (nullptr));
        timev -= now;
        timev -= ntp_unix_offset;

        // add offset to list, replacing oldest one if appropriate
        moffsetlist.push_back (timev);

        if (moffsetlist.size () >= ntp_sample_window)
            moffsetlist.pop_front ();

        mlastoffsetupdate = now;

        // select median time
        std::list<int> offsetlist = moffsetlist;
        offsetlist.sort ();
        int j = offsetlist.size ();
        std::list<int>::iterator it = offsetlist.begin ();

        for (int i = 0; i < (j / 2); ++i)
            ++it;

        moffset = *it;

        if ((j % 2) == 0)
            moffset = (moffset + (*--it)) / 2;

        if ((moffset == -1) || (moffset == 1)) // small corrections likely do more harm than good
            moffset = 0;

        condlog (timev || moffset, lstrace, sntpclient) << "sntp: offset is " << timev << ", new system offset is " << moffset;
    }

    void timerentry (const boost::system::error_code& error)
    {
        if (!error)
        {
            doquery ();
            mtimer.expires_from_now (boost::posix_time::seconds (ntp_query_frequency));
            mtimer.async_wait (std::bind (&sntpclientimp::timerentry, this, beast::asio::placeholders::error));
        }
    }

private:
    using locktype = std::mutex;
    using scopedlocktype = std::lock_guard <locktype>;
    locktype mlock;

    boost::asio::io_service m_io_service;
    std::map <boost::asio::ip::udp::endpoint, sntpquery> mqueries;

    boost::asio::ip::udp::socket        msocket;
    boost::asio::deadline_timer         mtimer;
    boost::asio::ip::udp::resolver      mresolver;

    std::vector< std::pair<std::string, time_t> >   mservers;

    int                                             moffset;
    time_t                                          mlastoffsetupdate;
    std::list<int>                                  moffsetlist;

    std::vector<uint8_t>                mreceivebuffer;
    boost::asio::ip::udp::endpoint      mreceiveendpoint;
};

//------------------------------------------------------------------------------

sntpclient::sntpclient (stoppable& parent)
    : stoppable ("sntpclient", parent)
{
}

//------------------------------------------------------------------------------

sntpclient* sntpclient::new (stoppable& parent)
{
    return new sntpclientimp (parent);
}

} // ripple
