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
#include <ripple/validators/manager.h>
#include <ripple/validators/make_manager.h>
#include <ripple/validators/impl/connectionimp.h>
#include <ripple/validators/impl/logic.h>
#include <ripple/validators/impl/storesqdb.h>
#include <beast/asio/placeholders.h>
#include <beast/asio/waitable_executor.h>
#include <boost/asio/basic_waitable_timer.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/strand.hpp>
#include <beast/cxx14/memory.h> // <memory>

/** chosenvalidators (formerly known as unl)

    motivation:

    to protect the integrity of the shared ledger data structure, validators
    independently sign ledgerhash objects with their ripplepublickey. these
    signed validations are propagated through the peer to peer network so
    that other nodes may inspect them. every peer and client on the network
    gains confidence in a ledger and its associated chain of previous ledgers
    by maintaining a suitably sized list of validator public keys that it
    trusts.

    the most important factors in choosing validators for a chosenvalidators
    list (the name we will use to designate such a list) are the following:

        - that different validators are not controlled by one entity
        - that each validator participates in a majority of ledgers
        - that a validator does not sign ledgers which fail consensus

    this module maintains chosenvalidators list. the list is built from a set
    of independent source objects, which may come from the configuration file,
    a separate file, a url from some trusted domain, or from the network itself.

    in order that rippled administrators may publish their chosenvalidators
    list at a url on a trusted domain that they own, this module compiles
    statistics on ledgers signed by validators and stores them in a database.
    from this database reports and alerts may be generated so that up-to-date
    information about the health of the set of chosenvalidators is always
    availabile.

    in addition to the automated statistics provided by the module, it is
    expected that organizations and meta-organizations will form from
    stakeholders such as gateways who publish their own lists and provide
    "best practices" to further refine the quality of validators placed into
    chosenvalidators list.


    ----------------------------------------------------------------------------

    unorganized notes:

    david:
      maybe oc should have a url that you can query to get the latest list of uri's
      for oc-approved organzations that publish lists of validators. the server and
      client can ship with that master trust url and also the list of uri's at the
      time it's released, in case for some reason it can't pull from oc. that would
      make the default installation safe even against major changes in the
      organizations that publish validator lists.

      the difference is that if an organization that provides lists of validators
      goes rogue, administrators don't have to act.

    todo:
      write up from end-user perspective on the deployment and administration
      of this feature, on the wiki. "draft" or "propose" to mark it as provisional.
      template: https://ripple.com/wiki/federation_protocol
      - what to do if you're a publisher of validatorlist
      - what to do if you're a rippled administrator
      - overview of how chosenvalidators works

    goals:
      make default configuration of rippled secure.
        * ship with trustedurilist
        * also have a preset rankedvalidators
      eliminate administrative burden of maintaining
      produce the chosenvalidators list.
      allow quantitative analysis of network health.

    what determines that a validator is good?
      - are they present (i.e. sending validations)
      - are they on the consensus ledger
      - what percentage of consensus rounds do they participate in
      - are they stalling consensus
        * measurements of constructive/destructive behavior is
          calculated in units of percentage of ledgers for which
          the behavior is measured.

    what we want from the unique node list:
      - some number of trusted roots (known by domain)
        probably organizations whose job is to provide a list of validators
      - we imagine the irga for example would establish some group whose job is to
        maintain a list of validators. there would be a public list of criteria
        that they would use to vet the validator. things like:
        * not anonymous
        * registered business
        * physical location
        * agree not to cease operations without notice / arbitrarily
        * responsive to complaints
      - identifiable jurisdiction
        * homogeneity in the jurisdiction is a business risk
        * if all validators are in the same jurisdiction this is a business risk
      - opencoin sets criteria for the organizations
      - rippled will ship with a list of trusted root "certificates"
        in other words this is a list of trusted domains from which the software
          can contact each trusted root and retrieve a list of "good" validators
          and then do something with that information
      - all the validation information would be public, including the broadcast
        messages.
      - the goal is to easily identify bad actors and assess network health
        * malicious intent
        * or, just hardware problems (faulty drive or memory)


*/

#include <ripple/core/jobqueue.h>
#include <memory>

namespace ripple {

/** executor which dispatches to jobqueue threads at a given jobtype. */
class job_executor
{
private:
    struct impl
    {
        impl (jobqueue& ex_, jobtype type_, std::string const& name_)
            : ex(ex_), type(type_), name(name_)
        {
        }

        jobqueue& ex;
        jobtype type;
        std::string name;
    };

    std::shared_ptr<impl> impl_;

public:
    job_executor (jobtype type, std::string const& name,
            jobqueue& ex)
        : impl_(std::make_shared<impl>(ex, type, name))
    {
    }

    template <class handler>
    void
    post (handler&& handler)
    {
        impl_->ex.addjob(impl_->type, impl_->name,
            std::forward<handler>(handler));
    }

    template <class handler>
    void
    dispatch (handler&& handler)
    {
        impl_->ex.addjob(impl_->type, impl_->name,
            std::forward<handler>(handler));
    }

    template <class handler>
    void
    defer (handler&& handler)
    {
        impl_->ex.addjob(impl_->type, impl_->name,
            std::forward<handler>(handler));
    }
};

//------------------------------------------------------------------------------

namespace validators {

// template <class executor>
class managerimp
    : public manager
    , public beast::stoppable
{
public:
    boost::asio::io_service& io_service_;
    boost::asio::io_service::strand strand_;
    beast::asio::waitable_executor exec_;
    boost::asio::basic_waitable_timer<
        std::chrono::steady_clock> timer_;
    beast::journal journal_;
    beast::file dbfile_;
    storesqdb store_;
    logic logic_;

    managerimp (stoppable& parent, boost::asio::io_service& io_service,
        beast::file const& pathtodbfileordirectory, beast::journal journal)
        : stoppable ("validators::manager", parent)
        , io_service_(io_service)
        , strand_(io_service_)
        , timer_(io_service_)
        , journal_ (journal)
        , dbfile_ (pathtodbfileordirectory)
        , store_ (journal_)
        , logic_ (store_, journal_)
    {
        if (dbfile_.isdirectory ())
            dbfile_ = dbfile_.getchildfile("validators.sqlite");

    }

    ~managerimp()
    {
    }

    //--------------------------------------------------------------------------
    //
    // manager
    //
    //--------------------------------------------------------------------------

    std::unique_ptr<connection>
    newconnection (int id) override
    {
        return std::make_unique<connectionimp>(
            id, logic_, get_seconds_clock());
    }

    void
    onledgerclosed (ledgerindex index,
        ledgerhash const& hash, ledgerhash const& parent) override
    {
        logic_.onledgerclosed (index, hash, parent);
    }

    //--------------------------------------------------------------------------
    //
    // stoppable
    //
    //--------------------------------------------------------------------------

    void onprepare()
    {
        init();
    }

    void onstart()
    {
    }

    void onstop()
    {
        boost::system::error_code ec;
        timer_.cancel(ec);

        logic_.stop();

        exec_.async_wait([this]() { stopped(); });
    }

    //--------------------------------------------------------------------------
    //
    // propertystream
    //
    //--------------------------------------------------------------------------

    void onwrite (beast::propertystream::map& map)
    {
    }

    //--------------------------------------------------------------------------
    //
    // managerimp
    //
    //--------------------------------------------------------------------------

    void init()
    {
        beast::error error (store_.open (dbfile_));

        if (! error)
        {
            logic_.load ();
        }
    }

    void
    ontimer (boost::system::error_code ec)
    {
        if (ec)
        {
            if (ec != boost::asio::error::operation_aborted)
                journal_.error <<
                    "ontimer: " << ec.message();
            return;
        }

        logic_.ontimer();

        timer_.expires_from_now(std::chrono::seconds(1), ec);
        timer_.async_wait(strand_.wrap(exec_.wrap(
            std::bind(&managerimp::ontimer, this,
                beast::asio::placeholders::error))));
    }
};

//------------------------------------------------------------------------------

manager::manager ()
    : beast::propertystream::source ("validators")
{
}

std::unique_ptr<manager>
make_manager(beast::stoppable& parent,
    boost::asio::io_service& io_service,
        beast::file const& pathtodbfileordirectory,
            beast::journal journal)
{
    return std::make_unique<managerimp> (parent,
        io_service, pathtodbfileordirectory, journal);
}

}
}
