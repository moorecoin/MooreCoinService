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
#include <ripple/basics/resolverasio.h>
#include <beast/asio/ipaddressconversion.h>
#include <beast/asio/placeholders.h>
#include <beast/module/asio/asyncobject.h>
#include <beast/threads/waitableevent.h>
#include <boost/asio.hpp>
#include <atomic>
#include <cassert>
#include <deque>
#include <locale>

namespace ripple {

class resolverasioimpl
    : public resolverasio
    , public beast::asio::asyncobject <resolverasioimpl>
{
public:
    typedef std::pair <std::string, std::string> hostandport;

    beast::journal m_journal;

    boost::asio::io_service& m_io_service;
    boost::asio::io_service::strand m_strand;
    boost::asio::ip::tcp::resolver m_resolver;

    beast::waitableevent m_stop_complete;
    std::atomic <bool> m_stop_called;
    std::atomic <bool> m_stopped;

    // represents a unit of work for the resolver to do
    struct work
    {
        std::vector <std::string> names;
        handlertype handler;

        template <class stringsequence>
        work (stringsequence const& names_, handlertype const& handler_)
            : handler (handler_)
        {
            names.reserve(names_.size ());

            std::reverse_copy (names_.begin (), names_.end (),
                std::back_inserter (names));
        }
    };

    std::deque <work> m_work;

    resolverasioimpl (boost::asio::io_service& io_service,
        beast::journal journal)
            : m_journal (journal)
            , m_io_service (io_service)
            , m_strand (io_service)
            , m_resolver (io_service)
            , m_stop_complete (true, true)
            , m_stop_called (false)
            , m_stopped (true)
    {

    }

    ~resolverasioimpl ()
    {
        assert (m_work.empty ());
        assert (m_stopped);
    }

    //-------------------------------------------------------------------------
    // asyncobject
    void asynchandlerscomplete()
    {
        m_stop_complete.signal ();
    }

    //--------------------------------------------------------------------------
    //
    // resolver
    //
    //--------------------------------------------------------------------------

    void start ()
    {
        assert (m_stopped == true);
        assert (m_stop_called == false);

        if (m_stopped.exchange (false) == true)
        {
            m_stop_complete.reset ();
            addreference ();
        }
    }

    void stop_async ()
    {
        if (m_stop_called.exchange (true) == false)
        {
            m_io_service.dispatch (m_strand.wrap (std::bind (
                &resolverasioimpl::do_stop,
                    this, completioncounter (this))));

            m_journal.debug << "queued a stop request";
        }
    }

    void stop ()
    {
        stop_async ();

        m_journal.debug << "waiting to stop";
        m_stop_complete.wait();
        m_journal.debug << "stopped";
    }

    void resolve (
        std::vector <std::string> const& names,
        handlertype const& handler)
    {
        assert (m_stop_called == false);
        assert (m_stopped == true);
        assert (!names.empty());

        // todo nikb use rvalue references to construct and move
        //           reducing cost.
        m_io_service.dispatch (m_strand.wrap (std::bind (
            &resolverasioimpl::do_resolve, this,
                names, handler, completioncounter (this))));
    }

    //-------------------------------------------------------------------------
    // resolver
    void do_stop (completioncounter)
    {
        assert (m_stop_called == true);

        if (m_stopped.exchange (true) == false)
        {
            m_work.clear ();
            m_resolver.cancel ();

            removereference ();
        }
    }

    void do_finish (
        std::string name,
        boost::system::error_code const& ec,
        handlertype handler,
        boost::asio::ip::tcp::resolver::iterator iter,
        completioncounter)
    {
        if (ec == boost::asio::error::operation_aborted)
            return;

        std::vector <beast::ip::endpoint> addresses;

        // if we get an error message back, we don't return any
        // results that we may have gotten.
        if (ec == 0)
        {
            while (iter != boost::asio::ip::tcp::resolver::iterator())
            {
                addresses.push_back (beast::ipaddressconversion::from_asio (*iter));
                ++iter;
            }
        }

        handler (name, addresses);

        m_io_service.post (m_strand.wrap (std::bind (
            &resolverasioimpl::do_work, this,
                completioncounter (this))));
    }

    hostandport parsename(std::string const& str)
    {
        // attempt to find the first and last non-whitespace
        auto const find_whitespace = std::bind (
            &std::isspace <std::string::value_type>,
            std::placeholders::_1,
            std::locale ());

        auto host_first = std::find_if_not (
            str.begin (), str.end (), find_whitespace);

        auto port_last = std::find_if_not (
            str.rbegin (), str.rend(), find_whitespace).base();

        // this should only happen for all-whitespace strings
        if (host_first >= port_last)
            return std::make_pair(std::string (), std::string ());

        // attempt to find the first and last valid port separators
        auto const find_port_separator = [](char const c) -> bool
        {
            if (std::isspace (c))
                return true;

            if (c == ':')
                return true;

            return false;
        };

        auto host_last = std::find_if (
            host_first, port_last, find_port_separator);

        auto port_first = std::find_if_not (
            host_last, port_last, find_port_separator);

        return make_pair (
            std::string (host_first, host_last),
            std::string (port_first, port_last));
    }

    void do_work (completioncounter)
    {
        if (m_stop_called == true)
            return;

        // we don't have any work to do at this time
        if (m_work.empty ())
            return;

        std::string const name (m_work.front ().names.back());
        handlertype handler (m_work.front ().handler);

        m_work.front ().names.pop_back ();

        if (m_work.front ().names.empty ())
            m_work.pop_front();

        hostandport const hp (parsename (name));

        if (hp.first.empty ())
        {
            m_journal.error <<
                "unable to parse '" << name << "'";

            m_io_service.post (m_strand.wrap (std::bind (
                &resolverasioimpl::do_work, this,
                completioncounter (this))));

            return;
        }

        boost::asio::ip::tcp::resolver::query query (
            hp.first, hp.second);

        m_resolver.async_resolve (query, std::bind (
            &resolverasioimpl::do_finish, this, name,
                beast::asio::placeholders::error, handler,
                    beast::asio::placeholders::iterator,
                        completioncounter (this)));
    }

    void do_resolve (std::vector <std::string> const& names,
        handlertype const& handler, completioncounter)
    {
        assert (! names.empty());

        if (m_stop_called == false)
        {
            m_work.emplace_back (names, handler);

            m_journal.debug <<
                "queued new job with " << names.size() <<
                " tasks. " << m_work.size() << " jobs outstanding.";

            if (m_work.size() > 0)
            {
                m_io_service.post (m_strand.wrap (std::bind (
                    &resolverasioimpl::do_work, this,
                        completioncounter (this))));
            }
        }
    }
};

//-----------------------------------------------------------------------------

resolverasio *resolverasio::new (
    boost::asio::io_service& io_service,
    beast::journal journal)
{
    return new resolverasioimpl (io_service, journal);
}

//-----------------------------------------------------------------------------
resolver::~resolver ()
{
}

}
