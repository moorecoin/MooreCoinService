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

#include <ripple/unity/hyperleveldb.h>

#if ripple_hyperleveldb_available

#include <ripple/core/config.h> // vfalco bad dependency
#include <ripple/nodestore/factory.h>
#include <ripple/nodestore/manager.h>
#include <ripple/nodestore/impl/batchwriter.h>
#include <ripple/nodestore/impl/decodedblob.h>
#include <ripple/nodestore/impl/encodedblob.h>
#include <beast/cxx14/memory.h> // <memory>
    
namespace ripple {
namespace nodestore {

class hyperdbbackend
    : public backend
    , public batchwriter::callback
{
private:
    std::atomic <bool> m_deletepath;

public:
    beast::journal m_journal;
    size_t const m_keybytes;
    scheduler& m_scheduler;
    batchwriter m_batch;
    std::string m_name;
    std::unique_ptr <hyperleveldb::filterpolicy const> m_filter_policy;
    std::unique_ptr <hyperleveldb::db> m_db;

    hyperdbbackend (size_t keybytes, parameters const& keyvalues,
        scheduler& scheduler, beast::journal journal)
        : m_deletepath (false)
        , m_journal (journal)
        , m_keybytes (keybytes)
        , m_scheduler (scheduler)
        , m_batch (*this, scheduler)
        , m_name (keyvalues ["path"].tostdstring ())
    {
        if (m_name.empty ())
            throw std::runtime_error ("missing path in leveldbfactory backend");

        hyperleveldb::options options;
        options.create_if_missing = true;

        if (keyvalues ["cache_mb"].isempty ())
        {
            options.block_cache = hyperleveldb::newlrucache (getconfig ().getsize (sihashnodedbcache) * 1024 * 1024);
        }
        else
        {
            options.block_cache = hyperleveldb::newlrucache (keyvalues["cache_mb"].getintvalue() * 1024l * 1024l);
        }

        if (keyvalues ["filter_bits"].isempty())
        {
            if (getconfig ().node_size >= 2)
                m_filter_policy.reset  (hyperleveldb::newbloomfilterpolicy (10));
        }
        else if (keyvalues ["filter_bits"].getintvalue() != 0)
        {
            m_filter_policy.reset (hyperleveldb::newbloomfilterpolicy (keyvalues ["filter_bits"].getintvalue ()));
        }
        options.filter_policy = m_filter_policy.get();

        if (! keyvalues["open_files"].isempty ())
        {
            options.max_open_files = keyvalues ["open_files"].getintvalue();
        }

        hyperleveldb::db* db = nullptr;
        hyperleveldb::status status = hyperleveldb::db::open (options, m_name, &db);
        if (!status.ok () || !db)
            throw std::runtime_error (std::string (
                "unable to open/create hyperleveldb: ") + status.tostring());

        m_db.reset (db);
    }

    ~hyperdbbackend ()
    {
        close();
    }

    std::string
    getname()
    {
        return m_name;
    }

    void
    close() override
    {
        if (m_db)
        {
            m_db.reset();
            if (m_deletepath)
            {
                boost::filesystem::path dir = m_name;
                boost::filesystem::remove_all (dir);
            }
        }
    }

    //--------------------------------------------------------------------------

    status
    fetch (void const* key, nodeobject::ptr* pobject)
    {
        pobject->reset ();

        status status (ok);

        hyperleveldb::readoptions const options;
        hyperleveldb::slice const slice (static_cast <char const*> (key), m_keybytes);

        std::string string;

        hyperleveldb::status getstatus = m_db->get (options, slice, &string);

        if (getstatus.ok ())
        {
            decodedblob decoded (key, string.data (), string.size ());

            if (decoded.wasok ())
            {
                *pobject = decoded.createobject ();
            }
            else
            {
                // decoding failed, probably corrupted!
                //
                status = datacorrupt;
            }
        }
        else
        {
            if (getstatus.iscorruption ())
            {
                status = datacorrupt;
            }
            else if (getstatus.isnotfound ())
            {
                status = notfound;
            }
            else
            {
                status = unknown;
            }
        }

        return status;
    }

    void
    store (nodeobject::ref object)
    {
        m_batch.store (object);
    }

    void
    storebatch (batch const& batch)
    {
        hyperleveldb::writebatch wb;

        encodedblob encoded;

        for (auto const& e : batch)
        {
            encoded.prepare (e);

            wb.put (
                hyperleveldb::slice (reinterpret_cast <char const*> (
                    encoded.getkey ()), m_keybytes),
                hyperleveldb::slice (reinterpret_cast <char const*> (
                    encoded.getdata ()), encoded.getsize ()));
        }

        hyperleveldb::writeoptions const options;

        auto ret = m_db->write (options, &wb);

        if (!ret.ok ())
            throw std::runtime_error ("storebatch failed: " + ret.tostring());
    }

    void
    for_each (std::function <void (nodeobject::ptr)> f)
    {
        hyperleveldb::readoptions const options;

        std::unique_ptr <hyperleveldb::iterator> it (m_db->newiterator (options));

        for (it->seektofirst (); it->valid (); it->next ())
        {
            if (it->key ().size () == m_keybytes)
            {
                decodedblob decoded (it->key ().data (),
                    it->value ().data (), it->value ().size ());

                if (decoded.wasok ())
                {
                    f (decoded.createobject ());
                }
                else
                {
                    // uh oh, corrupted data!
                    m_journal.fatal <<
                        "corrupt nodeobject #" << uint256::fromvoid (it->key ().data ());
                }
            }
            else
            {
                // vfalco note what does it mean to find an
                //             incorrectly sized key? corruption?
                m_journal.fatal <<
                    "bad key size = " << it->key ().size ();
            }
        }
    }

    int
    getwriteload ()
    {
        return m_batch.getwriteload ();
    }

    void
    setdeletepath() override
    {
        m_deletepath = true;
    }

    //--------------------------------------------------------------------------

    void
    writebatch (batch const& batch)
    {
        storebatch (batch);
    }

    void
    verify() override
    {
    }
};

//------------------------------------------------------------------------------

class hyperdbfactory : public nodestore::factory
{
public:
    hyperdbfactory()
    {
        manager::instance().insert(*this);
    }

    ~hyperdbfactory()
    {
        manager::instance().erase(*this);
    }

    std::string
    getname () const
    {
        return "hyperleveldb";
    }

    std::unique_ptr <backend>
    createinstance (
        size_t keybytes,
        parameters const& keyvalues,
        scheduler& scheduler,
        beast::journal journal)
    {
        return std::make_unique <hyperdbbackend> (
            keybytes, keyvalues, scheduler, journal);
    }
};

static hyperdbfactory hyperdbfactory;

}
}

#endif
