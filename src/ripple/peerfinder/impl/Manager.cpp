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
#include <ripple/peerfinder/manager.h>
#include <ripple/peerfinder/impl/checker.h>
#include <ripple/peerfinder/impl/logic.h>
#include <ripple/peerfinder/impl/sourcestrings.h>
#include <ripple/peerfinder/impl/storesqdb.h>
#include <boost/asio/io_service.hpp>
#include <boost/optional.hpp>
#include <beast/cxx14/memory.h> // <memory>
#include <thread>

namespace ripple {
namespace peerfinder {

class managerimp
    : public manager
    , public beast::leakchecked <managerimp>
{
public:
    boost::asio::io_service &io_service_;
    boost::optional <boost::asio::io_service::work> work_;
    beast::file m_databasefile;
    clock_type& m_clock;
    beast::journal m_journal;
    storesqdb m_store;
    checker<boost::asio::ip::tcp> checker_;
    logic <decltype(checker_)> m_logic;

    //--------------------------------------------------------------------------

    managerimp (
        stoppable& stoppable,
        boost::asio::io_service& io_service,
        beast::file const& pathtodbfileordirectory,
        clock_type& clock,
        beast::journal journal)
        : manager (stoppable)
        , io_service_(io_service)
        , work_(boost::in_place(std::ref(io_service_)))
        , m_databasefile (pathtodbfileordirectory)
        , m_clock (clock)
        , m_journal (journal)
        , m_store (journal)
        , checker_ (io_service_)
        , m_logic (clock, m_store, checker_, journal)
    {
        if (m_databasefile.isdirectory ())
            m_databasefile = m_databasefile.getchildfile("peerfinder.sqlite");
    }

    ~managerimp()
    {
        close();
    }

    void
    close()
    {
        if (work_)
        {
            work_ = boost::none;
            checker_.stop();
            m_logic.stop();
        }
    }

    //--------------------------------------------------------------------------
    //
    // peerfinder
    //
    //--------------------------------------------------------------------------

    void setconfig (config const& config) override
    {
        m_logic.config (config);
    }

    config
    config() override
    {
        return m_logic.config();
    }

    void addfixedpeer (std::string const& name,
        std::vector <beast::ip::endpoint> const& addresses) override
    {
        m_logic.addfixedpeer (name, addresses);
    }

    void
    addfallbackstrings (std::string const& name,
        std::vector <std::string> const& strings) override
    {
        m_logic.addstaticsource (sourcestrings::new (name, strings));
    }

    void addfallbackurl (std::string const& name,
        std::string const& url)
    {
        // vfalco todo this needs to be implemented
    }

    //--------------------------------------------------------------------------

    slot::ptr
    new_inbound_slot (
        beast::ip::endpoint const& local_endpoint,
            beast::ip::endpoint const& remote_endpoint) override
    {
        return m_logic.new_inbound_slot (local_endpoint, remote_endpoint);
    }

    slot::ptr
    new_outbound_slot (beast::ip::endpoint const& remote_endpoint) override
    {
        return m_logic.new_outbound_slot (remote_endpoint);
    }

    void
    on_endpoints (slot::ptr const& slot,
        endpoints const& endpoints)  override
    {
        slotimp::ptr impl (std::dynamic_pointer_cast <slotimp> (slot));
        m_logic.on_endpoints (impl, endpoints);
    }

    void
    on_legacy_endpoints (ipaddresses const& addresses)  override
    {
        m_logic.on_legacy_endpoints (addresses);
    }

    void
    on_closed (slot::ptr const& slot)  override
    {
        slotimp::ptr impl (std::dynamic_pointer_cast <slotimp> (slot));
        m_logic.on_closed (impl);
    }

    void
    onredirects (boost::asio::ip::tcp::endpoint const& remote_address,
        std::vector<boost::asio::ip::tcp::endpoint> const& eps) override
    {
        m_logic.onredirects(eps.begin(), eps.end(), remote_address);
    }

    //--------------------------------------------------------------------------

    bool
    onconnected (slot::ptr const& slot,
        beast::ip::endpoint const& local_endpoint) override
    {
        slotimp::ptr impl (std::dynamic_pointer_cast <slotimp> (slot));
        return m_logic.onconnected (impl, local_endpoint);
    }

    result
    activate (slot::ptr const& slot,
        ripplepublickey const& key, bool cluster) override
    {
        slotimp::ptr impl (std::dynamic_pointer_cast <slotimp> (slot));
        return m_logic.activate (impl, key, cluster);
    }

    std::vector <endpoint>
    redirect (slot::ptr const& slot) override
    {
        slotimp::ptr impl (std::dynamic_pointer_cast <slotimp> (slot));
        return m_logic.redirect (impl);
    }

    std::vector <beast::ip::endpoint>
    autoconnect() override
    {
        return m_logic.autoconnect();
    }

    void
    once_per_second() override
    {
        m_logic.once_per_second();
    }

    std::vector<std::pair<slot::ptr, std::vector<endpoint>>>
    buildendpointsforpeers() override
    {
        return m_logic.buildendpointsforpeers();
    }

    //--------------------------------------------------------------------------
    //
    // stoppable
    //
    //--------------------------------------------------------------------------

    void
    onprepare ()
    {
        beast::error error (m_store.open (m_databasefile));
        if (error)
            m_journal.fatal <<
                "failed to open '" << m_databasefile.getfullpathname() << "'";
        if (! error)
            m_logic.load ();
    }

    void
    onstart()
    {
    }

    void onstop ()
    {
        close();
        stopped();
    }

    //--------------------------------------------------------------------------
    //
    // propertystream
    //
    //--------------------------------------------------------------------------

    void onwrite (beast::propertystream::map& map)
    {
        m_logic.onwrite (map);
    }
};

//------------------------------------------------------------------------------

manager::manager (stoppable& parent)
    : stoppable ("peerfinder", parent)
    , beast::propertystream::source ("peerfinder")
{
}

std::unique_ptr<manager>
make_manager (beast::stoppable& parent, boost::asio::io_service& io_service,
    beast::file const& databasefile, clock_type& clock, beast::journal journal)
{
    return std::make_unique<managerimp> (
        parent, io_service, databasefile, clock, journal);
}

}
}
