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

#ifndef ripple_nodestore_databaserotatingimp_h_included
#define ripple_nodestore_databaserotatingimp_h_included

#include <ripple/nodestore/impl/databaseimp.h>
#include <ripple/nodestore/databaserotating.h>

namespace ripple {
namespace nodestore {

class databaserotatingimp
    : public databaseimp
    , public databaserotating
{
private:
    std::shared_ptr <backend> writablebackend_;
    std::shared_ptr <backend> archivebackend_;
    mutable std::mutex rotatemutex_;

    struct backends {
        std::shared_ptr <backend> const& writablebackend;
        std::shared_ptr <backend> const& archivebackend;
    };

    backends getbackends() const
    {
        std::lock_guard <std::mutex> lock (rotatemutex_);
        return backends {writablebackend_, archivebackend_};
    }

public:
    databaserotatingimp (std::string const& name,
                 scheduler& scheduler,
                 int readthreads,
                 std::shared_ptr <backend> writablebackend,
                 std::shared_ptr <backend> archivebackend,
                 std::unique_ptr <backend> fastbackend,
                 beast::journal journal)
            : databaseimp (name, scheduler, readthreads,
                    std::unique_ptr <backend>(), std::move (fastbackend),
                    journal)
            , writablebackend_ (writablebackend)
            , archivebackend_ (archivebackend)
    {}

    std::shared_ptr <backend> const& getwritablebackend() const override
    {
        std::lock_guard <std::mutex> lock (rotatemutex_);
        return writablebackend_;
    }

    std::shared_ptr <backend> const& getarchivebackend() const override
    {
        std::lock_guard <std::mutex> lock (rotatemutex_);
        return archivebackend_;
    }

    std::shared_ptr <backend> rotatebackends (
            std::shared_ptr <backend> const& newbackend) override;
    std::mutex& peekmutex() const override
    {
        return rotatemutex_;
    }

    std::string getname() const override
    {
        return getwritablebackend()->getname();
    }

    void
    close() override
    {
        // vfalco todo how do we close everything?
        assert(false);
    }

    std::int32_t getwriteload() const override
    {
        return getwritablebackend()->getwriteload();
    }

    void for_each (std::function <void(nodeobject::ptr)> f) override
    {
        backends b = getbackends();
        b.archivebackend->for_each (f);
        b.writablebackend->for_each (f);
    }

    void import (database& source) override
    {
        importinternal (source, *getwritablebackend());
    }

    void store (nodeobjecttype type,
                blob&& data,
                uint256 const& hash) override
    {
        storeinternal (type, std::move(data), hash,
                *getwritablebackend());
    }

    nodeobject::ptr fetchnode (uint256 const& hash) override
    {
        return fetchfrom (hash);
    }

    nodeobject::ptr fetchfrom (uint256 const& hash) override;
    taggedcache <uint256, nodeobject>& getpositivecache() override
    {
        return m_cache;
    }
};

}
}

#endif
