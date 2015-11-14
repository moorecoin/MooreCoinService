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
#include <ripple/basics/seconds_clock.h>
#include <ripple/resource/manager.h>
#include <beast/threads/thread.h>
#include <beast/cxx14/memory.h> // <memory>
    
namespace ripple {
namespace resource {

class managerimp
    : public manager
    , public beast::thread
{
private:
    beast::journal m_journal;
    logic m_logic;
public:
    managerimp (beast::insight::collector::ptr const& collector,
        beast::journal journal)
        : thread ("resource::manager")
        , m_journal (journal)
        , m_logic (collector, get_seconds_clock (), journal)
    {
        startthread ();
    }

    ~managerimp ()
    {
        stopthread ();
    }

    consumer newinboundendpoint (beast::ip::endpoint const& address)
    {
        return m_logic.newinboundendpoint (address);
    }

    consumer newoutboundendpoint (beast::ip::endpoint const& address)
    {
        return m_logic.newoutboundendpoint (address);
    }

    consumer newadminendpoint (std::string const& name)
    {
        return m_logic.newadminendpoint (name);
    }

    gossip exportconsumers ()
    {
        return m_logic.exportconsumers();
    }

    void importconsumers (std::string const& origin, gossip const& gossip)
    {
        m_logic.importconsumers (origin, gossip);
    }

    //--------------------------------------------------------------------------

    json::value getjson ()
    {
        return m_logic.getjson ();
    }

    json::value getjson (int threshold)
    {
        return m_logic.getjson (threshold);
    }

    //--------------------------------------------------------------------------

    void onwrite (beast::propertystream::map& map)
    {
        m_logic.onwrite (map);
    }

    //--------------------------------------------------------------------------

    void run ()
    {
        do
        {
            m_logic.periodicactivity();
            wait (1000);
        }
        while (! threadshouldexit ());
    }
};

//------------------------------------------------------------------------------

manager::manager ()
    : beast::propertystream::source ("resource")
{
}

manager::~manager ()
{
}

//------------------------------------------------------------------------------

std::unique_ptr <manager> make_manager (
    beast::insight::collector::ptr const& collector,
        beast::journal journal)
{
    return std::make_unique <managerimp> (collector, journal);
}

}
}
