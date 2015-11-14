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
#include <ripple/basics/stringutilities.h>
#include <ripple/core/config.h>
#include <ripple/net/httpclient.h>
#include <ripple/websocket/autosocket/autosocket.h>
#include <beast/asio/placeholders.h>
#include <beast/module/core/memory/sharedsingleton.h>
#include <beast/module/core/text/lexicalcast.h>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/regex.hpp>

namespace ripple {

//
// fetch a web page via http or https.
//

class httpclientsslcontext
{
public:
    httpclientsslcontext ()
        : m_context (boost::asio::ssl::context::sslv23)
    {
        boost::system::error_code ec;

        if (getconfig().ssl_verify_file.empty ())
        {
            m_context.set_default_verify_paths (ec);

            if (ec && getconfig().ssl_verify_dir.empty ())
                throw std::runtime_error (boost::str (
                    boost::format ("failed to set_default_verify_paths: %s") % ec.message ()));
        }
        else
        {
            m_context.load_verify_file (getconfig().ssl_verify_file);
        }

        if (! getconfig().ssl_verify_dir.empty ())
        {
            m_context.add_verify_path (getconfig().ssl_verify_dir, ec);

            if (ec)
                throw std::runtime_error (boost::str (
                    boost::format ("failed to add verify path: %s") % ec.message ()));
        }
    }

    boost::asio::ssl::context& context()
    {
        return m_context;
    }

private:
    boost::asio::ssl::context m_context;
};

//------------------------------------------------------------------------------

// vfalco note i moved the ssl_context out of the config and into this
//             singleton to eliminate the asio dependency in the headers.
//
void httpclient::initializesslcontext ()
{
    beast::sharedsingleton <httpclientsslcontext>::get();
}

//------------------------------------------------------------------------------

class httpclientimp
    : public std::enable_shared_from_this <httpclientimp>
    , public httpclient
    , beast::leakchecked <httpclientimp>
{
public:
    httpclientimp (boost::asio::io_service& io_service,
                              const unsigned short port,
                              std::size_t responsemax)
        : msocket (io_service, beast::sharedsingleton <httpclientsslcontext>::get()->context())
        , mresolver (io_service)
        , mheader (maxclientheaderbytes)
        , mport (port)
        , mresponsemax (responsemax)
        , mdeadline (io_service)
    {
        if (!getconfig ().ssl_verify)
            msocket.sslsocket ().set_verify_mode (boost::asio::ssl::verify_none);
    }

    //--------------------------------------------------------------------------

    void makeget (std::string const& strpath, boost::asio::streambuf& sb,
        std::string const& strhost)
    {
        std::ostream    osrequest (&sb);

        osrequest <<
                  "get " << strpath << " http/1.0\r\n"
                  "host: " << strhost << "\r\n"
                  "accept: */*\r\n"                       // yyy do we need this line?
                  "connection: close\r\n\r\n";
    }

    //--------------------------------------------------------------------------

    void request (
        bool bssl,
        std::deque<std::string> deqsites,
        std::function<void (boost::asio::streambuf& sb, std::string const& strhost)> build,
        boost::posix_time::time_duration timeout,
        std::function<bool (const boost::system::error_code& ecresult,
            int istatus, std::string const& strdata)> complete)
    {
        mssl        = bssl;
        mdeqsites   = deqsites;
        mbuild      = build;
        mcomplete   = complete;
        mtimeout    = timeout;

        httpsnext ();
    }

    //--------------------------------------------------------------------------

    void get (
        bool bssl,
        std::deque<std::string> deqsites,
        std::string const& strpath,
        boost::posix_time::time_duration timeout,
        std::function<bool (const boost::system::error_code& ecresult, int istatus,
            std::string const& strdata)> complete)
    {

        mcomplete   = complete;
        mtimeout    = timeout;

        request (
            bssl,
            deqsites,
            std::bind (&httpclientimp::makeget, shared_from_this (), strpath,
                       std::placeholders::_1, std::placeholders::_2),
            timeout,
            complete);
    }

    //--------------------------------------------------------------------------

    void httpsnext ()
    {
        writelog (lstrace, httpclient) << "fetch: " << mdeqsites[0];

        std::shared_ptr <boost::asio::ip::tcp::resolver::query> query (
            new boost::asio::ip::tcp::resolver::query (
                mdeqsites[0],
                beast::lexicalcast <std::string> (mport),
                boost::asio::ip::resolver_query_base::numeric_service));
        mquery  = query;

        mdeadline.expires_from_now (mtimeout, mshutdown);

        writelog (lstrace, httpclient) << "expires_from_now: " << mshutdown.message ();

        if (!mshutdown)
        {
            mdeadline.async_wait (
                std::bind (
                    &httpclientimp::handledeadline,
                    shared_from_this (),
                    beast::asio::placeholders::error));
        }

        if (!mshutdown)
        {
            writelog (lstrace, httpclient) << "resolving: " << mdeqsites[0];

            mresolver.async_resolve (*mquery,
                                     std::bind (
                                         &httpclientimp::handleresolve,
                                         shared_from_this (),
                                         beast::asio::placeholders::error,
                                         beast::asio::placeholders::iterator));
        }

        if (mshutdown)
            invokecomplete (mshutdown);
    }

    void handledeadline (const boost::system::error_code& ecresult)
    {
        if (ecresult == boost::asio::error::operation_aborted)
        {
            // timer canceled because deadline no longer needed.
            writelog (lstrace, httpclient) << "deadline cancelled.";

            // aborter is done.
        }
        else if (ecresult)
        {
            writelog (lstrace, httpclient) << "deadline error: " << mdeqsites[0] << ": " << ecresult.message ();

            // can't do anything sound.
            abort ();
        }
        else
        {
            writelog (lstrace, httpclient) << "deadline arrived.";

            // mark us as shutting down.
            // xxx use our own error code.
            mshutdown   = boost::system::error_code (boost::system::errc::bad_address, boost::system::system_category ());

            // cancel any resolving.
            mresolver.cancel ();

            // stop the transaction.
            msocket.async_shutdown (std::bind (
                                        &httpclientimp::handleshutdown,
                                        shared_from_this (),
                                        beast::asio::placeholders::error));

        }
    }

    void handleshutdown (
        const boost::system::error_code& ecresult
    )
    {
        if (ecresult)
        {
            writelog (lstrace, httpclient) << "shutdown error: " << mdeqsites[0] << ": " << ecresult.message ();
        }
    }

    void handleresolve (
        const boost::system::error_code& ecresult,
        boost::asio::ip::tcp::resolver::iterator itrendpoint
    )
    {
        if (!mshutdown)
            mshutdown   = ecresult;

        if (mshutdown)
        {
            writelog (lstrace, httpclient) << "resolve error: " << mdeqsites[0] << ": " << mshutdown.message ();

            invokecomplete (mshutdown);
        }
        else
        {
            writelog (lstrace, httpclient) << "resolve complete.";

            boost::asio::async_connect (
                msocket.lowest_layer (),
                itrendpoint,
                std::bind (
                    &httpclientimp::handleconnect,
                    shared_from_this (),
                    beast::asio::placeholders::error));
        }
    }

    void handleconnect (const boost::system::error_code& ecresult)
    {
        if (!mshutdown)
            mshutdown   = ecresult;

        if (mshutdown)
        {
            writelog (lstrace, httpclient) << "connect error: " << mshutdown.message ();
        }

        if (!mshutdown)
        {
            writelog (lstrace, httpclient) << "connected.";

            if (getconfig ().ssl_verify)
            {
                mshutdown   = msocket.verify (mdeqsites[0]);

                if (mshutdown)
                {
                    writelog (lstrace, httpclient) << "set_verify_callback: " << mdeqsites[0] << ": " << mshutdown.message ();
                }
            }
        }

        if (mshutdown)
        {
            invokecomplete (mshutdown);
        }
        else if (mssl)
        {
            msocket.async_handshake (
                autosocket::ssl_socket::client,
                std::bind (
                    &httpclientimp::handlerequest,
                    shared_from_this (),
                    beast::asio::placeholders::error));
        }
        else
        {
            handlerequest (ecresult);
        }
    }

    void handlerequest (const boost::system::error_code& ecresult)
    {
        if (!mshutdown)
            mshutdown   = ecresult;

        if (mshutdown)
        {
            writelog (lstrace, httpclient) << "handshake error:" << mshutdown.message ();

            invokecomplete (mshutdown);
        }
        else
        {
            writelog (lstrace, httpclient) << "session started.";

            mbuild (mrequest, mdeqsites[0]);

            msocket.async_write (
                mrequest,
                std::bind (&httpclientimp::handlewrite,
                             shared_from_this (),
                             beast::asio::placeholders::error,
                             beast::asio::placeholders::bytes_transferred));
        }
    }

    void handlewrite (const boost::system::error_code& ecresult, std::size_t bytes_transferred)
    {
        if (!mshutdown)
            mshutdown   = ecresult;

        if (mshutdown)
        {
            writelog (lstrace, httpclient) << "write error: " << mshutdown.message ();

            invokecomplete (mshutdown);
        }
        else
        {
            writelog (lstrace, httpclient) << "wrote.";

            msocket.async_read_until (
                mheader,
                "\r\n\r\n",
                std::bind (&httpclientimp::handleheader,
                             shared_from_this (),
                             beast::asio::placeholders::error,
                             beast::asio::placeholders::bytes_transferred));
        }
    }

    void handleheader (const boost::system::error_code& ecresult, std::size_t bytes_transferred)
    {
        std::string     strheader ((std::istreambuf_iterator<char> (&mheader)), std::istreambuf_iterator<char> ());
        writelog (lstrace, httpclient) << "header: \"" << strheader << "\"";

        static boost::regex restatus ("\\`http/1\\s+ (\\d{3}) .*\\'");          // http/1.1 200 ok
        static boost::regex resize ("\\`.*\\r\\ncontent-length:\\s+([0-9]+).*\\'");
        static boost::regex rebody ("\\`.*\\r\\n\\r\\n(.*)\\'");

        boost::smatch   smmatch;

        bool    bmatch  = boost::regex_match (strheader, smmatch, restatus);    // match status code.

        if (!bmatch)
        {
            // xxx use our own error code.
            writelog (lstrace, httpclient) << "no status code";
            invokecomplete (boost::system::error_code (boost::system::errc::bad_address, boost::system::system_category ()));
            return;
        }

        mstatus = beast::lexicalcastthrow <int> (std::string (smmatch[1]));

        if (boost::regex_match (strheader, smmatch, rebody)) // we got some body
            mbody = smmatch[1];

        if (boost::regex_match (strheader, smmatch, resize))
        {
            int size = beast::lexicalcastthrow <int> (std::string(smmatch[1]));

            if (size < mresponsemax)
                mresponsemax = size;
        }

        if (mresponsemax == 0)
        {
            // no body wanted or available
            invokecomplete (ecresult, mstatus);
        }
        else if (mbody.size () >= mresponsemax)
        {
            // we got the whole thing
            invokecomplete (ecresult, mstatus, mbody);
        }
        else
        {
            msocket.async_read (
                mresponse.prepare (mresponsemax - mbody.size ()),
                boost::asio::transfer_all (),
                std::bind (&httpclientimp::handledata,
                             shared_from_this (),
                             beast::asio::placeholders::error,
                             beast::asio::placeholders::bytes_transferred));
        }
    }

    void handledata (const boost::system::error_code& ecresult, std::size_t bytes_transferred)
    {
        if (!mshutdown)
            mshutdown   = ecresult;

        if (mshutdown && mshutdown != boost::asio::error::eof)
        {
            writelog (lstrace, httpclient) << "read error: " << mshutdown.message ();

            invokecomplete (mshutdown);
        }
        else
        {
            if (mshutdown)
            {
                writelog (lstrace, httpclient) << "complete.";
            }
            else
            {
                mresponse.commit (bytes_transferred);
                std::string strbody ((std::istreambuf_iterator<char> (&mresponse)), std::istreambuf_iterator<char> ());
                invokecomplete (ecresult, mstatus, mbody + strbody);
            }
        }
    }

    // call cancel the deadline timer and invoke the completion routine.
    void invokecomplete (const boost::system::error_code& ecresult, int istatus = 0, std::string const& strdata = "")
    {
        boost::system::error_code eccancel;

        (void) mdeadline.cancel (eccancel);

        if (eccancel)
        {
            writelog (lstrace, httpclient) << "invokecomplete: deadline cancel error: " << eccancel.message ();
        }

        writelog (lsdebug, httpclient) << "invokecomplete: deadline popping: " << mdeqsites.size ();

        if (!mdeqsites.empty ())
        {
            mdeqsites.pop_front ();
        }

        bool    bagain  = true;

        if (mdeqsites.empty () || !ecresult)
        {
            // ecresult: !0 = had an error, last entry
            //    istatus: result, if no error
            //  strdata: data, if no error
            bagain  = mcomplete && mcomplete (ecresult ? ecresult : eccancel, istatus, strdata);
        }

        if (!mdeqsites.empty () && bagain)
        {
            httpsnext ();
        }
    }

    static bool onsmsresponse (const boost::system::error_code& ecresult, int istatus, std::string const& strdata)
    {
        writelog (lsinfo, httpclient) << "sms: response:" << istatus << " :" << strdata;

        return true;
    }

private:
    typedef std::shared_ptr<httpclient> pointer;

    bool                                                        mssl;
    autosocket                                                  msocket;
    boost::asio::ip::tcp::resolver                              mresolver;
    std::shared_ptr<boost::asio::ip::tcp::resolver::query>    mquery;
    boost::asio::streambuf                                      mrequest;
    boost::asio::streambuf                                      mheader;
    boost::asio::streambuf                                      mresponse;
    std::string                                                 mbody;
    const unsigned short                                        mport;
    int                                                         mresponsemax;
    int                                                         mstatus;
    std::function<void (boost::asio::streambuf& sb, std::string const& strhost)>         mbuild;
    std::function<bool (const boost::system::error_code& ecresult, int istatus, std::string const& strdata)> mcomplete;

    boost::asio::deadline_timer                                 mdeadline;

    // if not success, we are shutting down.
    boost::system::error_code                                   mshutdown;

    std::deque<std::string>                                     mdeqsites;
    boost::posix_time::time_duration                            mtimeout;
};

//------------------------------------------------------------------------------

void httpclient::get (
    bool bssl,
    boost::asio::io_service& io_service,
    std::deque<std::string> deqsites,
    const unsigned short port,
    std::string const& strpath,
    std::size_t responsemax,
    boost::posix_time::time_duration timeout,
    std::function<bool (const boost::system::error_code& ecresult, int istatus,
        std::string const& strdata)> complete)
{
    std::shared_ptr <httpclientimp> client (
        new httpclientimp (io_service, port, responsemax));

    client->get (bssl, deqsites, strpath, timeout, complete);
}

void httpclient::get (
    bool bssl,
    boost::asio::io_service& io_service,
    std::string strsite,
    const unsigned short port,
    std::string const& strpath,
    std::size_t responsemax,
    boost::posix_time::time_duration timeout,
    std::function<bool (const boost::system::error_code& ecresult, int istatus,
        std::string const& strdata)> complete)
{
    std::deque<std::string> deqsites (1, strsite);

    std::shared_ptr <httpclientimp> client (
        new httpclientimp (io_service, port, responsemax));

    client->get (bssl, deqsites, strpath, timeout, complete);
}

void httpclient::request (
    bool bssl,
    boost::asio::io_service& io_service,
    std::string strsite,
    const unsigned short port,
    std::function<void (boost::asio::streambuf& sb, std::string const& strhost)> setrequest,
    std::size_t responsemax,
    boost::posix_time::time_duration timeout,
    std::function<bool (const boost::system::error_code& ecresult, int istatus,
        std::string const& strdata)> complete)
{
    std::deque<std::string> deqsites (1, strsite);

    std::shared_ptr <httpclientimp> client (
        new httpclientimp (io_service, port, responsemax));

    client->request (bssl, deqsites, setrequest, timeout, complete);
}

void httpclient::sendsms (boost::asio::io_service& io_service, std::string const& strtext)
{
    std::string strscheme;
    std::string strdomain;
    int         iport;
    std::string strpath;

    if (getconfig ().sms_url == "" || !parseurl (getconfig ().sms_url, strscheme, strdomain, iport, strpath))
    {
        writelog (lswarning, httpclient) << "smsrequest: bad url:" << getconfig ().sms_url;
    }
    else
    {
        bool const bssl = strscheme == "https";

        std::deque<std::string> deqsites (1, strdomain);
        std::string struri  =
            boost::str (boost::format ("%s?from=%s&to=%s&api_key=%s&api_secret=%s&text=%s")
                        % (strpath.empty () ? "/" : strpath)
                        % getconfig ().sms_from
                        % getconfig ().sms_to
                        % getconfig ().sms_key
                        % getconfig ().sms_secret
                        % urlencode (strtext));

        // writelog (lsinfo) << "sms: request:" << struri;
        writelog (lsinfo, httpclient) << "sms: request: '" << strtext << "'";

        if (iport < 0)
            iport = bssl ? 443 : 80;

        std::shared_ptr <httpclientimp> client (
            new httpclientimp (io_service, iport, maxclientheaderbytes));

        client->get (bssl, deqsites, struri, boost::posix_time::seconds (smstimeoutseconds),
                     std::bind (&httpclientimp::onsmsresponse,
                                std::placeholders::_1, std::placeholders::_2,
                                std::placeholders::_3));
    }
}

} // ripple
