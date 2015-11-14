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
#include <ripple/nodestore/factory.h>
#include <ripple/nodestore/manager.h>
#include <beast/cxx14/memory.h> // <memory>

namespace ripple {
namespace nodestore {

class nullbackend : public backend
{
public:
    nullbackend ()
    {
    }

    ~nullbackend ()
    {
    }

    std::string
    getname()
    {
        return std::string ();
    }

    void
    close() override
    {
    }

    status
    fetch (void const*, nodeobject::ptr*)
    {
        return notfound;
    }

    void
    store (nodeobject::ref object)
    {
    }

    void
    storebatch (batch const& batch)
    {
    }

    void
    for_each (std::function <void(nodeobject::ptr)> f)
    {
    }

    int
    getwriteload ()
    {
        return 0;
    }

    void
    setdeletepath() override
    {
    }

    void
    verify() override
    {
    }

private:
};

//------------------------------------------------------------------------------

class nullfactory : public factory
{
public:
    nullfactory()
    {
        manager::instance().insert(*this);
    }

    ~nullfactory()
    {
        manager::instance().erase(*this);
    }

    std::string
    getname () const
    {
        return "none";
    }

    std::unique_ptr <backend>
    createinstance (
        size_t,
        parameters const&,
        scheduler&, beast::journal)
    {
        return std::make_unique <nullbackend> ();
    }
};

static nullfactory nullfactory;

}
}
