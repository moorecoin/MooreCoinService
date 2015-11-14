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
#include <ripple/app/data/sqlitedatabase.h>
#include <ripple/core/jobqueue.h>
#include <ripple/basics/log.h>

namespace ripple {

sqlitestatement::sqlitestatement (sqlitedatabase* db, const char* sql, bool aux)
{
    assert (db);

    sqlite3* conn = aux ? db->getauxconnection () : db->peekconnection ();
    int j = sqlite3_prepare_v2 (conn, sql, strlen (sql) + 1, &statement, nullptr);

    if (j != sqlite_ok)
        throw j;
}

sqlitestatement::sqlitestatement (sqlitedatabase* db, std::string const& sql, bool aux)
{
    assert (db);

    sqlite3* conn = aux ? db->getauxconnection () : db->peekconnection ();
    int j = sqlite3_prepare_v2 (conn, sql.c_str (), sql.size () + 1, &statement, nullptr);

    if (j != sqlite_ok)
        throw j;
}

sqlitestatement::~sqlitestatement ()
{
    sqlite3_finalize (statement);
}

//------------------------------------------------------------------------------

sqlitedatabase::sqlitedatabase (const char* host)
    : database (host)
    , thread ("sqlitedb")
    , mwalq (nullptr)
    , walrunning (false)
{
    mdbtype = type::sqlite;
    
    startthread ();

    mconnection     = nullptr;
    mauxconnection  = nullptr;
    mcurrentstmt    = nullptr;
}

sqlitedatabase::~sqlitedatabase ()
{
    // blocks until the thread exits in an orderly fashion
    stopthread ();
}

void sqlitedatabase::connect ()
{
    int rc = sqlite3_open_v2 (mhost.c_str (), &mconnection,
                sqlite_open_readwrite | sqlite_open_create | sqlite_open_fullmutex, nullptr);

    if (rc)
    {
        writelog (lsfatal, sqlitedatabase) << "can't open " << mhost << " " << rc;
        sqlite3_close (mconnection);
        assert ((rc != sqlite_busy) && (rc != sqlite_locked));
    }
}

sqlite3* sqlitedatabase::getauxconnection ()
{
    scopedlocktype sl (m_walmutex);

    if (mauxconnection == nullptr)
    {
        int rc = sqlite3_open_v2 (mhost.c_str (), &mauxconnection,
                    sqlite_open_readwrite | sqlite_open_create | sqlite_open_fullmutex, nullptr);

        if (rc)
        {
            writelog (lsfatal, sqlitedatabase) << "can't aux open " << mhost << " " << rc;
            assert ((rc != sqlite_busy) && (rc != sqlite_locked));

            if (mauxconnection != nullptr)
            {
                sqlite3_close (mconnection);
                mauxconnection = nullptr;
            }
        }
    }

    return mauxconnection;
}

void sqlitedatabase::disconnect ()
{
    sqlite3_finalize (mcurrentstmt);
    sqlite3_close (mconnection);

    if (mauxconnection != nullptr)
        sqlite3_close (mauxconnection);
}

// returns true if the query went ok
bool sqlitedatabase::executesql (const char* sql, bool fail_ok)
{
#ifdef debug_hanging_locks
    assert (fail_ok || (mcurrentstmt == nullptr));
#endif

    sqlite3_finalize (mcurrentstmt);

    int rc = sqlite3_prepare_v2 (mconnection, sql, -1, &mcurrentstmt, nullptr);

    if (sqlite_ok != rc)
    {
        if (!fail_ok)
        {
#ifdef beast_debug
            writelog (lswarning, sqlitedatabase) << "perror:" << mhost << ": " << rc;
            writelog (lswarning, sqlitedatabase) << "statement: " << sql;
            writelog (lswarning, sqlitedatabase) << "error: " << sqlite3_errmsg (mconnection);
#endif
        }

        enditerrows ();
        return false;
    }

    rc = sqlite3_step (mcurrentstmt);

    if (rc == sqlite_row)
    {
        mmorerows = true;
    }
    else if (rc == sqlite_done)
    {
        enditerrows ();
        mmorerows = false;
    }
    else
    {
        if ((rc != sqlite_busy) && (rc != sqlite_locked))
        {
            writelog (lsfatal, sqlitedatabase) << mhost  << " returns error " << rc << ": " << sqlite3_errmsg (mconnection);
            assert (false);
        }

        mmorerows = false;

        if (!fail_ok)
        {
#ifdef beast_debug
            writelog (lswarning, sqlitedatabase) << "sql serror:" << mhost << ": " << rc;
            writelog (lswarning, sqlitedatabase) << "statement: " << sql;
            writelog (lswarning, sqlitedatabase) << "error: " << sqlite3_errmsg (mconnection);
#endif
        }

        enditerrows ();
        return false;
    }

    return true;
}

// returns false if there are no results
bool sqlitedatabase::startiterrows (bool finalize)
{
    mcolnametable.clear ();
    mcolnametable.resize (sqlite3_column_count (mcurrentstmt));

    for (unsigned n = 0; n < mcolnametable.size (); n++)
    {
        mcolnametable[n] = sqlite3_column_name (mcurrentstmt, n);
    }

    if (!mmorerows && finalize)
        enditerrows ();

    return (mmorerows);
}

void sqlitedatabase::enditerrows ()
{
    sqlite3_finalize (mcurrentstmt);
    mcurrentstmt = nullptr;
}

// call this after you executesql
// will return false if there are no more rows
bool sqlitedatabase::getnextrow (bool finalize)
{
    if (mmorerows)
    {
        int rc = sqlite3_step (mcurrentstmt);

        if (rc == sqlite_row)
            return (true);

        assert ((rc != sqlite_busy) && (rc != sqlite_locked));
        condlog ((rc != sqlite_done), lswarning, sqlitedatabase) << "rerror: " << mhost << ": " << rc;
    }

    if (finalize)
        enditerrows ();

    return false;
}
    
bool sqlitedatabase::begintransaction()
{
    return executesql("begin transaction;", false);
}

bool sqlitedatabase::endtransaction()
{
    return executesql("end transaction;", false);
}


bool sqlitedatabase::getnull (int colindex)
{
    return (sqlite_null == sqlite3_column_type (mcurrentstmt, colindex));
}

char* sqlitedatabase::getstr (int colindex, std::string& retstr)
{
    const char* text = reinterpret_cast<const char*> (sqlite3_column_text (mcurrentstmt, colindex));
    retstr = (text == nullptr) ? "" : text;
    return const_cast<char*> (retstr.c_str ());
}

std::int32_t sqlitedatabase::getint (int colindex)
{
    return (sqlite3_column_int (mcurrentstmt, colindex));
}

float sqlitedatabase::getfloat (int colindex)
{
    return (static_cast <float> (sqlite3_column_double (mcurrentstmt, colindex)));
}

bool sqlitedatabase::getbool (int colindex)
{
    return (sqlite3_column_int (mcurrentstmt, colindex) ? true : false);
}

int sqlitedatabase::getbinary (int colindex, unsigned char* buf, int maxsize)
{
    const void* blob = sqlite3_column_blob (mcurrentstmt, colindex);
    int size = sqlite3_column_bytes (mcurrentstmt, colindex);

    if (size < maxsize) maxsize = size;

    memcpy (buf, blob, maxsize);
    return (size);
}

blob sqlitedatabase::getbinary (int colindex)
{
    const unsigned char*        blob    = reinterpret_cast<const unsigned char*> (sqlite3_column_blob (mcurrentstmt, colindex));
    size_t                      isize   = sqlite3_column_bytes (mcurrentstmt, colindex);
    blob    vucresult;

    vucresult.resize (isize);
    std::copy (blob, blob + isize, vucresult.begin ());

    return vucresult;
}

std::uint64_t sqlitedatabase::getbigint (int colindex)
{
    return (sqlite3_column_int64 (mcurrentstmt, colindex));
}

int sqlitedatabase::getkbusedall ()
{
    return static_cast<int> (sqlite3_memory_used () / 1024);
}

int sqlitedatabase::getkbuseddb ()
{
    int cur = 0, hiw = 0;
    sqlite3_db_status (mconnection, sqlite_dbstatus_cache_used, &cur, &hiw, 0);
    return cur / 1024;
}

static int sqlitewalhook (void* s, sqlite3* dbcon, const char* dbname, int walsize)
{
    (reinterpret_cast<sqlitedatabase*> (s))->dohook (dbname, walsize);
    return sqlite_ok;
}

bool sqlitedatabase::setupcheckpointing (jobqueue* q)
{
    mwalq = q;
    sqlite3_wal_hook (mconnection, sqlitewalhook, this);
    return true;
}

void sqlitedatabase::dohook (const char* db, int pages)
{
    if (pages < 1000)
        return;

    {
        scopedlocktype sl (m_walmutex);

        if (walrunning)
            return;

        walrunning = true;
    }

    if (mwalq)
    {
        mwalq->addjob (jtwal, std::string ("wal:") + mhost, std::bind (&sqlitedatabase::runwal, this));
    }
    else
    {
        notify();
    }
}

void sqlitedatabase::run ()
{
    // simple thread loop runs wal every time it wakes up via
    // the call to thread::notify, unless thread::threadshouldexit returns
    // true in which case we simply break.
    //
    for (;;)
    {
        wait ();
        if (threadshouldexit())
            break;
        runwal();
    }
}

void sqlitedatabase::runwal ()
{
    int log = 0, ckpt = 0;
    int ret = sqlite3_wal_checkpoint_v2 (mconnection, nullptr, sqlite_checkpoint_passive, &log, &ckpt);

    if (ret != sqlite_ok)
    {
        writelog ((ret == sqlite_locked) ? lstrace : lswarning, sqlitedatabase) << "wal("
                << sqlite3_db_filename (mconnection, "main") << "): error " << ret;
    }
    else
        writelog (lstrace, sqlitedatabase) << "wal(" << sqlite3_db_filename (mconnection, "main") <<
                                           "): frames=" << log << ", written=" << ckpt;

    {
        scopedlocktype sl (m_walmutex);
        walrunning = false;
    }
}
    
bool sqlitedatabase::hasfield(const std::string &table, const std::string &field)
{
    std::string sql = "select sql from sqlite_master where tbl_name='" + table + "';";
    if (executesql(sql.c_str(), false))
    {
        for (bool bmore = startiterrows(true); bmore; bmore = getnextrow(true))
        {
            std::string schema;
            database::getstr ("sql", schema);
            if (schema.find (field) != std::string::npos)
            {
                return true;
            }
        }
    }
    return false;
}


sqlite3_stmt* sqlitestatement::peekstatement ()
{
    return statement;
}

int sqlitestatement::bind (int position, const void* data, int length)
{
    return sqlite3_bind_blob (statement, position, data, length, sqlite_transient);
}

int sqlitestatement::bindstatic (int position, const void* data, int length)
{
    return sqlite3_bind_blob (statement, position, data, length, sqlite_static);
}

int sqlitestatement::bindstatic (int position, blob const& value)
{
    return sqlite3_bind_blob (statement, position, &value.front (), value.size (), sqlite_static);
}

int sqlitestatement::bind (int position, std::uint32_t value)
{
    return sqlite3_bind_int64 (statement, position, static_cast<sqlite3_int64> (value));
}

int sqlitestatement::bind (int position, std::string const& value)
{
    return sqlite3_bind_text (statement, position, value.data (), value.size (), sqlite_transient);
}

int sqlitestatement::bindstatic (int position, std::string const& value)
{
    return sqlite3_bind_text (statement, position, value.data (), value.size (), sqlite_static);
}

int sqlitestatement::bind (int position)
{
    return sqlite3_bind_null (statement, position);
}

int sqlitestatement::size (int column)
{
    return sqlite3_column_bytes (statement, column);
}

const void* sqlitestatement::peekblob (int column)
{
    return sqlite3_column_blob (statement, column);
}

blob sqlitestatement::getblob (int column)
{
    int size = sqlite3_column_bytes (statement, column);
    blob ret (size);
    memcpy (& (ret.front ()), sqlite3_column_blob (statement, column), size);
    return ret;
}

std::string sqlitestatement::getstring (int column)
{
    return reinterpret_cast<const char*> (sqlite3_column_text (statement, column));
}

const char* sqlitestatement::peekstring (int column)
{
    return reinterpret_cast<const char*> (sqlite3_column_text (statement, column));
}

std::uint32_t sqlitestatement::getuint32 (int column)
{
    return static_cast<std::uint32_t> (sqlite3_column_int64 (statement, column));
}

std::int64_t sqlitestatement::getint64 (int column)
{
    return sqlite3_column_int64 (statement, column);
}

int sqlitestatement::step ()
{
    return sqlite3_step (statement);
}

int sqlitestatement::reset ()
{
    return sqlite3_reset (statement);
}

bool sqlitestatement::isok (int j)
{
    return j == sqlite_ok;
}

bool sqlitestatement::isdone (int j)
{
    return j == sqlite_done;
}

bool sqlitestatement::isrow (int j)
{
    return j == sqlite_row;
}

bool sqlitestatement::iserror (int j)
{
    switch (j)
    {
    case sqlite_ok:
    case sqlite_row:
    case sqlite_done:
        return false;

    default:
        return true;
    }
}

std::string sqlitestatement::geterror (int j)
{
    return sqlite3_errstr (j);
}

} // ripple
