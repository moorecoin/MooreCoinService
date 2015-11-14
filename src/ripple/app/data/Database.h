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

#ifndef ripple_database_h_included
#define ripple_database_h_included

#include <ripple/basics/blob.h>
#include <cstdint>
#include <string>
#include <vector>

namespace ripple {

// vfalco get rid of these macros
//
#define sql_foreach(_db, _strquery)     \
    if ((_db)->executesql(_strquery))   \
        for (bool _bmore = (_db)->startiterrows(); _bmore; _bmore = (_db)->getnextrow())

#define sql_exists(_db, _strquery)     \
    ((_db)->executesql(_strquery) && (_db)->startiterrows())

/*
    this maintains the connection to the database
*/

class sqlitedatabase;
class jobqueue;

class database
{
public:
    enum class type {
        mysql,
        sqlite,
        null,
    };
    explicit database (const char* host);

    virtual ~database ();

    virtual void connect () = 0;

    virtual void disconnect () = 0;

    // returns true if the query went ok
    virtual bool executesql (const char* sql, bool fail_okay = false) = 0;

    bool executesql (std::string strsql, bool fail_okay = false)
    {
        return executesql (strsql.c_str (), fail_okay);
    }
    
    //execute a batch of sql in one call (only sql without result can call this)
//    virtual bool executesqlbatch(const std::vector<std::string>& sqlqueue);
    virtual bool batchstart(){return true;};
    virtual bool batchcommit(){return true;};

    // returns false if there are no results
    virtual bool startiterrows (bool finalize = true) = 0;
    virtual void enditerrows () = 0;

    // call this after you executesql
    // will return false if there are no more rows
    virtual bool getnextrow (bool finalize = true) = 0;
    
    virtual bool begintransaction() = 0;
    virtual bool endtransaction() = 0;

    // get data from the current row
    bool getnull (const char* colname);
    char* getstr (const char* colname, std::string& retstr);
    std::string getstrbinary (std::string const& strcolname);
    std::int32_t getint (const char* colname);
    float getfloat (const char* colname);
    bool getbool (const char* colname);

    // returns amount stored in buf
    int getbinary (const char* colname, unsigned char* buf, int maxsize);
    blob getbinary (std::string const& strcolname);

    std::uint64_t getbigint (const char* colname);

    virtual bool getnull (int colindex) = 0;
    virtual char* getstr (int colindex, std::string& retstr) = 0;
    virtual std::int32_t getint (int colindex) = 0;
    virtual float getfloat (int colindex) = 0;
    virtual bool getbool (int colindex) = 0;
    virtual int getbinary (int colindex, unsigned char* buf, int maxsize) = 0;
    virtual std::uint64_t getbigint (int colindex) = 0;
    virtual blob getbinary (int colindex) = 0;
    virtual bool hasfield(const std::string &table, const std::string &field) = 0;
    
    const type getdbtype()
    {
        return mdbtype;
    }

    // int getsingledbvalueint(const char* sql);
    // float getsingledbvaluefloat(const char* sql);
    // char* getsingledbvaluestr(const char* sql, std::string& retstr);

    // vfalco todo make this parameter a reference instead of a pointer.
    virtual bool setupcheckpointing (jobqueue*)
    {
        return false;
    }
    virtual sqlitedatabase* getsqlitedb ()
    {
        return nullptr;
    }
    virtual int getkbusedall ()
    {
        return -1;
    }
    virtual int getkbuseddb ()
    {
        return -1;
    }

protected:
    virtual bool getcolnumber (const char* colname, int* retindex);

    int mnumcol;
    std::string mhost;
    std::vector <std::string> mcolnametable;
    type mdbtype;
};

} // ripple

#endif
