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
#include <ripple/app/main/collectormanager.h>

namespace ripple {

class collectormanagerimp
    : public collectormanager
{
public:
    beast::journal m_journal;
    beast::insight::collector::ptr m_collector;
    std::unique_ptr <beast::insight::groups> m_groups;

    collectormanagerimp (beast::stringpairarray const& params,
        beast::journal journal)
        : m_journal (journal)
    {
        std::string const& server (params ["server"].tostdstring());

        if (server == "statsd")
        {
            beast::ip::endpoint const address (beast::ip::endpoint::from_string (
                params ["address"].tostdstring ()));
            std::string const& prefix (params ["prefix"].tostdstring ());

            m_collector = beast::insight::statsdcollector::new (address, prefix, journal);
        }
        else
        {
            m_collector = beast::insight::nullcollector::new ();
        }

        m_groups = beast::insight::make_groups (m_collector);
    }

    ~collectormanagerimp ()
    {
    }

    beast::insight::collector::ptr const& collector ()
    {
        return m_collector;
    }

    beast::insight::group::ptr const& group (std::string const& name)
    {
        return m_groups->get (name);
    }
};

//------------------------------------------------------------------------------

collectormanager::~collectormanager ()
{
}

collectormanager* collectormanager::new (beast::stringpairarray const& params,
    beast::journal journal)
{
    return new collectormanagerimp (params, journal);
}

}
