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
#include <ripple/nodestore/impl/codec.h>
#include <ripple/nodestore/impl/decodedblob.h>
#include <ripple/nodestore/impl/encodedblob.h>
#include <beast/nudb.h>
#include <beast/nudb/detail/varint.h>
#include <beast/nudb/identity_codec.h>
#include <beast/nudb/visit.h>
#include <beast/hash/xxhasher.h>
#include <boost/filesystem.hpp>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <exception>
#include <memory>

namespace ripple {
namespace nodestore {

class nudbbackend
    : public backend
{
public:
    enum
    {
        // this needs to be tuned for the
        // distribution of data sizes.
        arena_alloc_size = 16 * 1024 * 1024,

        currenttype = 1
    };

    using api = beast::nudb::api<
        beast::xxhasher, nodeobject_codec>;

    beast::journal journal_;
    size_t const keybytes_;
    std::string const name_;
    api::store db_;
    std::atomic <bool> deletepath_;
    scheduler& scheduler_;

    nudbbackend (int keybytes, parameters const& keyvalues,
        scheduler& scheduler, beast::journal journal)
        : journal_ (journal)
        , keybytes_ (keybytes)
        , name_ (keyvalues ["path"].tostdstring ())
        , deletepath_(false)
        , scheduler_ (scheduler)
    {
        if (name_.empty())
            throw std::runtime_error (
                "nodestore: missing path in nudb backend");
        auto const folder = boost::filesystem::path (name_);
        boost::filesystem::create_directories (folder);
        auto const dp = (folder / "nudb.dat").string();
        auto const kp = (folder / "nudb.key").string ();
        auto const lp = (folder / "nudb.log").string ();
        using beast::nudb::make_salt;
        api::create (dp, kp, lp,
            currenttype, make_salt(), keybytes,
                beast::nudb::block_size(kp),
            0.50);
        try
        {
            if (! db_.open (dp, kp, lp,
                    arena_alloc_size))
                throw std::runtime_error(
                    "nodestore: open failed");
            if (db_.appnum() != currenttype)
                throw std::runtime_error(
                    "nodestore: unknown appnum");
        }
        catch (std::exception const& e)
        {
            // log and terminate?
            std::cerr << e.what();
            std::terminate();
        }
    }

    ~nudbbackend ()
    {
        close();
    }

    std::string
    getname()
    {
        return name_;
    }

    void
    close() override
    {
        if (db_.is_open())
        {
            db_.close();
            if (deletepath_)
            {
                boost::filesystem::remove_all (name_);
            }
        }
    }

    status
    fetch (void const* key, nodeobject::ptr* pno)
    {
        status status;
        pno->reset();
        if (! db_.fetch (key,
            [key, pno, &status](void const* data, std::size_t size)
            {
                decodedblob decoded (key, data, size);
                if (! decoded.wasok ())
                {
                    status = datacorrupt;
                    return;
                }
                *pno = decoded.createobject();
                status = ok;
            }))
        {
            return notfound;
        }
        return status;
    }

    void
    do_insert (std::shared_ptr <nodeobject> const& no)
    {
        encodedblob e;
        e.prepare (no);
        db_.insert (e.getkey(),
            e.getdata(), e.getsize());
    }

    void
    store (std::shared_ptr <nodeobject> const& no) override
    {
        batchwritereport report;
        report.writecount = 1;
        auto const start =
            std::chrono::steady_clock::now();
        do_insert (no);
        report.elapsed = std::chrono::duration_cast <
            std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start);
        scheduler_.onbatchwrite (report);
    }

    void
    storebatch (batch const& batch) override
    {
        batchwritereport report;
        encodedblob encoded;
        report.writecount = batch.size();
        auto const start =
            std::chrono::steady_clock::now();
        for (auto const& e : batch)
            do_insert (e);
        report.elapsed = std::chrono::duration_cast <
            std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start);
        scheduler_.onbatchwrite (report);
    }

    void
    for_each (std::function <void(nodeobject::ptr)> f)
    {
        auto const dp = db_.dat_path();
        auto const kp = db_.key_path();
        auto const lp = db_.log_path();
        //auto const appnum = db_.appnum();
        db_.close();
        api::visit (dp,
            [&](
                void const* key, std::size_t key_bytes,
                void const* data, std::size_t size)
            {
                decodedblob decoded (key, data, size);
                if (! decoded.wasok ())
                    return false;
                f (decoded.createobject());
                return true;
            });
        db_.open (dp, kp, lp,
            arena_alloc_size);
    }

    int
    getwriteload ()
    {
        return 0;
    }

    void
    setdeletepath() override
    {
        deletepath_ = true;
    }

    void
    verify() override
    {
        auto const dp = db_.dat_path();
        auto const kp = db_.key_path();
        auto const lp = db_.log_path();
        db_.close();
        api::verify (dp, kp);
        db_.open (dp, kp, lp,
            arena_alloc_size);
    }
};

//------------------------------------------------------------------------------

class nudbfactory : public factory
{
public:
    nudbfactory()
    {
        manager::instance().insert(*this);
    }

    ~nudbfactory()
    {
        manager::instance().erase(*this);
    }

    std::string
    getname() const
    {
        return "nudb";
    }

    std::unique_ptr <backend>
    createinstance (
        size_t keybytes,
        parameters const& keyvalues,
        scheduler& scheduler,
        beast::journal journal)
    {
        return std::make_unique <nudbbackend> (
            keybytes, keyvalues, scheduler, journal);
    }
};

static nudbfactory nudbfactory;

}
}
