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
#include <ripple/app/node/sqlitefactory.h>
#include <ripple/app/data/databasecon.h>
#include <ripple/app/data/sqlitedatabase.h>
#include <ripple/core/config.h>
#include <type_traits>
#include <beast/cxx14/memory.h> // <memory>

namespace ripple {

// vfalco note ledgerindex in committedobjects is obsolete

static const char* s_nodestoredbinit [] =
{
    "pragma synchronous=normal;",
    "pragma journal_mode=wal;",
    "pragma journal_size_limit=1582080;",

#if (ulong_max > uint_max) && !defined (no_sqlite_mmap)
    "pragma mmap_size=171798691840;",
#endif

    "begin transaction;",

    "create table committedobjects (                \
        hash        character(64) primary key,      \
        objtype     char(1) not null,               \
        ledgerindex bigint unsigned,                \
        object      blob                            \
    );",

    "end transaction;"
};

//------------------------------------------------------------------------------

class sqlitebackend : public nodestore::backend
{
public:
    explicit sqlitebackend (std::string const& path, int hashnode_cache_size)
        : m_name (path)
        , m_db (new databasecon(setup_databasecon (getconfig()), path,
            s_nodestoredbinit, std::extent<decltype(s_nodestoredbinit)>::value))
    {
        std::string s ("pragma cache_size=-");
        s += std::to_string (hashnode_cache_size);
        m_db->getdb()->executesql (s);
    }

    ~sqlitebackend()
    {
    }

    std::string getname()
    {
        return m_name;
    }

    void
    close() override
    {
        // vfalco how do we do this?
        assert(false);
    }

    //--------------------------------------------------------------------------

    nodestore::status fetch (void const* key, nodeobject::ptr* pobject)
    {
        nodestore::status result = nodestore::ok;

        pobject->reset ();

        {
            auto sl (m_db->lock());

            uint256 const hash (uint256::fromvoid (key));

            static sqlitestatement pst (m_db->getdb()->getsqlitedb(),
                "select objtype,object from committedobjects where hash = ?;");

            pst.bind (1, to_string (hash));

            if (pst.isrow (pst.step()))
            {
                // vfalco note this is unfortunately needed,
                //             the databasecon creates the blob?
                blob data (pst.getblob(1));
                *pobject = nodeobject::createobject (
                    gettypefromstring (pst.peekstring (0)),
                    std::move(data),
                    hash);
            }
            else
            {
                result = nodestore::notfound;
            }

            pst.reset();
        }

        return result;
    }

    void store (nodeobject::ref object)
    {
        nodestore::batch batch;

        batch.push_back (object);

        storebatch (batch);
    }

    void storebatch (nodestore::batch const& batch)
    {
        auto sl (m_db->lock());

        static sqlitestatement pstb (m_db->getdb()->getsqlitedb(), "begin transaction;");
        static sqlitestatement pste (m_db->getdb()->getsqlitedb(), "end transaction;");
        static sqlitestatement pst (m_db->getdb()->getsqlitedb(),
            "insert or ignore into committedobjects "
                "(hash,objtype,object) values (?, ?, ?, ?);");

        pstb.step();
        pstb.reset();

        for (nodeobject::ptr const& object : batch)
        {
            dobind (pst, object);

            pst.step();
            pst.reset();
        }

        pste.step();
        pste.reset();
    }

    void for_each (std::function <void(nodeobject::ptr)> f)
    {
        // no lock needed as per the for_each() api

        uint256 hash;

        static sqlitestatement pst(m_db->getdb()->getsqlitedb(),
            "select objtype,object,hash from committedobjects;");

        while (pst.isrow (pst.step()))
        {
            hash.sethexexact(pst.getstring(3));

            // vfalco note this is unfortunately needed,
            //             the databasecon creates the blob?
            blob data (pst.getblob (1));
            nodeobject::ptr const object (nodeobject::createobject (
                gettypefromstring (pst.peekstring (0)),
                std::move(data),
                hash));

            f (object);
        }

        pst.reset ();
    }

    int getwriteload ()
    {
        return 0;
    }

    void setdeletepath() override {}

    //--------------------------------------------------------------------------

    void dobind (sqlitestatement& statement, nodeobject::ref object)
    {
        char const* type;
        switch (object->gettype())
        {
            case hotledger:             type = "l"; break;
            case hottransaction:        type = "t"; break;
            case hotaccount_node:       type = "a"; break;
            case hottransaction_node:   type = "n"; break;
            default:                    type = "u";
        }

        statement.bind(1, to_string (object->gethash()));
        statement.bind(2, type);
        statement.bindstatic(3, object->getdata());
    }

    nodeobjecttype gettypefromstring (std::string const& s)
    {
        nodeobjecttype type = hotunknown;

        if (!s.empty ())
        {
            switch (s [0])
            {
                case 'l': type = hotledger; break;
                case 't': type = hottransaction; break;
                case 'a': type = hotaccount_node; break;
                case 'n': type = hottransaction_node; break;
            }
        }
        return type;
    }

    void
    verify() override
    {
    }

private:
    std::string const m_name;
    std::unique_ptr <databasecon> m_db;
};

//------------------------------------------------------------------------------

class sqlitefactory : public nodestore::factory
{
public:
    sqlitefactory() = default;

    std::string
    getname () const
    {
        return "sqlite";
    }

    std::unique_ptr <nodestore::backend> createinstance (
        size_t, nodestore::parameters const& keyvalues,
            nodestore::scheduler&, beast::journal)
    {
        return std::make_unique <sqlitebackend> (
            keyvalues ["path"].tostdstring (),
                getconfig ().getsize(sihashnodedbcache) * 1024);
    }
};

static sqlitefactory sqlitefactory;

}
