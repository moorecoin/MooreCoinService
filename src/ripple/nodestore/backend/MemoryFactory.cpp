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
#include <beast/utility/ci_char_traits.h>
#include <beast/cxx14/memory.h> // <memory>
#include <map>
#include <mutex>

namespace ripple {
namespace nodestore {

struct memorydb
{
    std::mutex mutex;
    bool open = false;
    std::map <uint256 const, nodeobject::ptr> table;
};

class memoryfactory : public factory
{
private:
    std::mutex mutex_;
    std::map <std::string, memorydb, beast::ci_less> map_;

public:
    memoryfactory();
    ~memoryfactory();

    std::string
    getname() const;

    std::unique_ptr <backend>
    createinstance (
        size_t keybytes,
        parameters const& keyvalues,
        scheduler& scheduler,
        beast::journal journal);

    memorydb&
    open (std::string const& path)
    {
        std::lock_guard<std::mutex> _(mutex_);
        auto const result = map_.emplace (std::piecewise_construct,
            std::make_tuple(path), std::make_tuple());
        memorydb& db = result.first->second;
        if (db.open)
            throw std::runtime_error("already open");
        return db;
    }
};

static memoryfactory memoryfactory;

//------------------------------------------------------------------------------

class memorybackend : public backend
{
private:
    using map = std::map <uint256 const, nodeobject::ptr>;

    std::string name_;
    beast::journal journal_;
    memorydb* db_;

public:
    memorybackend (size_t keybytes, parameters const& keyvalues,
        scheduler& scheduler, beast::journal journal)
        : name_ (keyvalues ["path"].tostdstring ())
        , journal_ (journal)
    {
        if (name_.empty())
            throw std::runtime_error ("missing path in memory backend");
        db_ = &memoryfactory.open(name_);
    }

    ~memorybackend ()
    {
        close();
    }

    std::string
    getname ()
    {
        return name_;
    }

    void
    close() override
    {
        db_ = nullptr;
    }

    //--------------------------------------------------------------------------

    status
    fetch (void const* key, nodeobject::ptr* pobject)
    {
        uint256 const hash (uint256::fromvoid (key));

        std::lock_guard<std::mutex> _(db_->mutex);

        map::iterator iter = db_->table.find (hash);
        if (iter == db_->table.end())
        {
            pobject->reset();
            return notfound;
        }
        *pobject = iter->second;
        return ok;
    }

    void
    store (nodeobject::ref object)
    {
        std::lock_guard<std::mutex> _(db_->mutex);
        db_->table.emplace (object->gethash(), object);
    }

    void
    storebatch (batch const& batch)
    {
        for (auto const& e : batch)
            store (e);
    }

    void
    for_each (std::function <void(nodeobject::ptr)> f)
    {
        for (auto const& e : db_->table)
            f (e.second);
    }

    int
    getwriteload()
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
};

//------------------------------------------------------------------------------

memoryfactory::memoryfactory()
{
    manager::instance().insert(*this);
}

memoryfactory::~memoryfactory()
{
    manager::instance().erase(*this);
}

std::string
memoryfactory::getname() const
{
    return "memory";
}

std::unique_ptr <backend>
memoryfactory::createinstance (
    size_t keybytes,
    parameters const& keyvalues,
    scheduler& scheduler,
    beast::journal journal)
{
    return std::make_unique <memorybackend> (
        keybytes, keyvalues, scheduler, journal);
}

}
}
