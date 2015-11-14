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
#include <ripple/nodestore/impl/managerimp.h>
#include <ripple/nodestore/impl/databaseimp.h>
#include <ripple/nodestore/impl/databaserotatingimp.h>
#include <ripple/basics/stringutilities.h>
#include <beast/utility/ci_char_traits.h>
#include <beast/cxx14/memory.h> // <memory>
#include <stdexcept>

namespace ripple {
namespace nodestore {

managerimp&
managerimp::instance()
{
    static beast::static_initializer<managerimp> _;
    return _.get();
}

void
managerimp::missing_backend()
{
    throw std::runtime_error (
        "your rippled.cfg is missing a [node_db] entry, "
        "please see the rippled-example.cfg file!"
        );
}

managerimp::managerimp()
{
}

managerimp::~managerimp()
{
}

std::unique_ptr <backend>
managerimp::make_backend (
    parameters const& parameters,
    scheduler& scheduler,
    beast::journal journal)
{
    std::unique_ptr <backend> backend;

    std::string const type (parameters ["type"].tostdstring ());

    if (! type.empty ())
    {
        factory* const factory (find (type));

        if (factory != nullptr)
        {
            backend = factory->createinstance (
                nodeobject::keybytes, parameters, scheduler, journal);
        }
        else
        {
            missing_backend ();
        }
    }
    else
    {
        missing_backend ();
    }

    return backend;
}

std::unique_ptr <database>
managerimp::make_database (
    std::string const& name,
    scheduler& scheduler,
    beast::journal journal,
    int readthreads,
    parameters const& backendparameters,
    parameters fastbackendparameters)
{
    std::unique_ptr <backend> backend (make_backend (
        backendparameters, scheduler, journal));

    std::unique_ptr <backend> fastbackend (
        (fastbackendparameters.size () > 0)
            ? make_backend (fastbackendparameters, scheduler, journal)
            : nullptr);

    return std::make_unique <databaseimp> (name, scheduler, readthreads,
        std::move (backend), std::move (fastbackend), journal);
}

std::unique_ptr <databaserotating>
managerimp::make_databaserotating (
        std::string const& name,
        scheduler& scheduler,
        std::int32_t readthreads,
        std::shared_ptr <backend> writablebackend,
        std::shared_ptr <backend> archivebackend,
        std::unique_ptr <backend> fastbackend,
        beast::journal journal)
{
    return std::make_unique <databaserotatingimp> (name, scheduler,
            readthreads, writablebackend, archivebackend,
            std::move (fastbackend), journal);
}

factory*
managerimp::find (std::string const& name)
{
    std::lock_guard<std::mutex> _(mutex_);
    auto const iter = std::find_if(list_.begin(), list_.end(),
        [&name](factory* other)
        {
            return beast::ci_equal(name, other->getname());
        } );
    if (iter == list_.end())
        return nullptr;
    return *iter;
}


void
managerimp::insert (factory& factory)
{
    std::lock_guard<std::mutex> _(mutex_);
    list_.push_back(&factory);
}

void
managerimp::erase (factory& factory)
{
    std::lock_guard<std::mutex> _(mutex_);
    auto const iter = std::find_if(list_.begin(), list_.end(),
        [&factory](factory* other) { return other == &factory; });
    assert(iter != list_.end());
    list_.erase(iter);
}

//------------------------------------------------------------------------------

manager&
manager::instance()
{
    return managerimp::instance();
}

//------------------------------------------------------------------------------

std::unique_ptr <backend>
make_backend (section const& config,
    scheduler& scheduler, beast::journal journal)
{
    beast::stringpairarray v;
    for (auto const& _ : config)
        v.set (_.first, _.second);
    return manager::instance().make_backend (
        v, scheduler, journal);
}

}
}
