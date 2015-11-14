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

#ifndef ripple_sqlitedatabase_h_included
#define ripple_sqlitedatabase_h_included

#include <ripple/app/data/database.h>
#include <ripple/basics/blob.h>
#include <ripple/core/jobqueue.h>
#include <beast/module/sqlite/sqlite.h>
#include <beast/threads/thread.h>
#include <mutex>

namespace ripple {

class sqlitedatabase
    : public database
    , private beast::thread
    , private beast::leakchecked <sqlitedatabase>
{
public:
    explicit sqlitedatabase (char const* host);
    ~sqlitedatabase ();

    void connect ();
    void disconnect ();

    // returns true if the query went ok
    bool executesql (const char* sql, bool fail_okay);

    // tells you how many rows were changed by an update or insert
    int getnumrowsaffected ();

    // returns false if there are no results
    bool startiterrows (bool finalize);
    void enditerrows ();

    // call this after you executesql
    // will return false if there are no more rows
    bool getnextrow (bool finalize);

    virtual bool begintransaction() override;
    virtual bool endtransaction() override;
    bool hasfield(const std::string &table, const std::string &field) override;

    bool getnull (int colindex);
    char* getstr (int colindex, std::string& retstr);
    std::int32_t getint (int colindex);
    float getfloat (int colindex);
    bool getbool (int colindex);
    // returns amount stored in buf
    int getbinary (int colindex, unsigned char* buf, int maxsize);
    blob getbinary (int colindex);
    std::uint64_t getbigint (int colindex);

    sqlite3* peekconnection ()
    {
        return mconnection;
    }
    sqlite3*    getauxconnection ();
    virtual bool setupcheckpointing (jobqueue*);
    virtual sqlitedatabase* getsqlitedb ()
    {
        return this;
    }

    void dohook (const char* db, int walsize);

    int getkbuseddb ();
    int getkbusedall ();

private:
    void run ();
    void runwal ();

    typedef std::mutex locktype;
    typedef std::lock_guard <locktype> scopedlocktype;
    locktype m_walmutex;

    sqlite3* mconnection;
    // vfalco todo why do we need an "aux" connection? should just use a second sqlitedatabase object.
    sqlite3* mauxconnection;
    sqlite3_stmt* mcurrentstmt;
    bool mmorerows;

    jobqueue*               mwalq;
    bool                    walrunning;
};

//------------------------------------------------------------------------------

class sqlitestatement
{
private:
    sqlitestatement (const sqlitestatement&);               // no implementation
    sqlitestatement& operator= (const sqlitestatement&);    // no implementation

protected:
    sqlite3_stmt* statement;

public:
    // vfalco todo this is quite a convoluted interface. a mysterious "aux" connection?
    //             why not just have two sqlitedatabase objects?
    //
    sqlitestatement (sqlitedatabase* db, const char* statement, bool aux = false);
    sqlitestatement (sqlitedatabase* db, std::string const& statement, bool aux = false);
    ~sqlitestatement ();

    sqlite3_stmt* peekstatement ();

    // positions start at 1
    int bind (int position, const void* data, int length);
    int bindstatic (int position, const void* data, int length);
    int bindstatic (int position, blob const& value);

    int bind (int position, std::string const& value);
    int bindstatic (int position, std::string const& value);

    int bind (int position, std::uint32_t value);
    int bind (int position);

    // columns start at 0
    int size (int column);

    const void* peekblob (int column);
    blob getblob (int column);

    std::string getstring (int column);
    const char* peekstring (int column);
    std::uint32_t getuint32 (int column);
    std::int64_t getint64 (int column);

    int step ();
    int reset ();

    // translate return values of step and reset
    bool isok (int);
    bool isdone (int);
    bool isrow (int);
    bool iserror (int);
    std::string geterror (int);
};

} // ripple

#endif
