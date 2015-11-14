#include <mysql.h>
#include <ripple/app/data/mysqldatabase.h>
#include <ripple/app/main/application.h>
#include <ripple/core/jobqueue.h>
#include <ripple/basics/log.h>

namespace ripple {
mysqldatabase::mysqldatabase(char const* host, std::uint32_t port,
                             char const* username, char const* password,
                             char const* database, bool asyncbatch)
    : database (host),
      mport(port),
      musername(username),
      mpassword(password),
      mdatabase(database)
    , masyncbatch(asyncbatch)
{
    mdbtype = type::mysql;
}
    
mysqldatabase::~mysqldatabase()
{
}
    
// returns true if the query went ok
bool mysqldatabase::executesql(const char* sql, bool fail_ok)
{
    auto stmt = getstatement();
    if (stmt->minbatch)
    {
        stmt->msqlqueue.push_back(sql);
        return true;
    }
    
    assert(stmt->mconnection);
    enditerrows();
    int rc = mysql_query(stmt->mconnection, sql);
    if (rc != 0)
    {
        writelog (lswarning, mysqldatabase)
            << "executesql-" << sql
            << " error_info:" << mysql_error(stmt->mconnection);
        return false;
    }
    //sql has result(like `select`)
    if (mysql_field_count(stmt->mconnection) > 0)
    {
        stmt->mresult = mysql_store_result(stmt->mconnection);
        if (stmt->mresult == nullptr)
        {
            writelog (lswarning, mysqldatabase)
                << "startiterrows: " << mysql_error(stmt->mconnection);
            return false;
        }
        if (mysql_num_rows(stmt->mresult) > 0)
        {
            stmt->mmorerows = true;
        }
        else
        {
            stmt->mmorerows = false;
        }
    }
    return true;
}
    
bool mysqldatabase::batchstart()
{
    auto stmt = getstatement();
    stmt->minbatch = true;
    stmt->msqlqueue.clear();
    return true;
}

bool mysqldatabase::batchcommit()
{
    auto stmt = getstatement();
    if (!stmt->minbatch)
    {
        return false;
    }
    stmt->minbatch = false;
    if (masyncbatch)
    {
        std::unique_lock <std::mutex> lock (mthreadbatchlock);
        std::move(stmt->msqlqueue.begin(), stmt->msqlqueue.end(), std::back_inserter(msqlqueue));
        if (!mthreadbatch && !msqlqueue.empty ())
        {
            mthreadbatch = true;
            getapp().getjobqueue().addjob(jtdb_batch,
                                          "dbbatch",
                                          std::bind(&mysqldatabase::executesqlbatch, this));
        }
    }
    else
    {
        std::move(stmt->msqlqueue.begin(), stmt->msqlqueue.end(), std::back_inserter(msqlqueue));
        executesqlbatch();
    }
    return true;
}

bool mysqldatabase::executesqlbatch()
{
    std::unique_lock <std::mutex> lock (mthreadbatchlock);
    while (!msqlqueue.empty())
    {
        std::string sql = msqlqueue.front();
        msqlqueue.pop_front();
        lock.unlock();
        executesql(sql.c_str(), true);
        lock.lock();
    }
    mthreadbatch = false;
    return true;
}

// tells you how many rows were changed by an update or insert
std::uint64_t mysqldatabase::getnumrowsaffected ()
{
    return mysql_affected_rows(getstatement()->mconnection);
}

// returns false if there are no results
bool mysqldatabase::startiterrows (bool finalize)
{
    auto stmt = getstatement();
    if (!stmt->mmorerows)
    {
        enditerrows ();
        return false;
    }
    stmt->mcolnametable.clear();
    auto fieldcnt = mysql_num_fields(stmt->mresult);
    stmt->mcolnametable.resize(fieldcnt);
    
    auto fields = mysql_fetch_fields(stmt->mresult);
    for (auto i = 0; i < fieldcnt; i++)
    {
        stmt->mcolnametable[i] = fields[i].name;
    }
    
    getnextrow(0);
    
    return true;
}
    
bool mysqldatabase::getcolnumber (const char* colname, int* retindex)
{
    auto stmt = getstatement();
    for (unsigned int n = 0; n < stmt->mcolnametable.size (); n++)
    {
        if (strcmp (colname, stmt->mcolnametable[n].c_str ()) == 0)
        {
            *retindex = n;
            return (true);
        }
    }
    return false;

}

void mysqldatabase::enditerrows()
{
    auto stmt = getstatement();
    if (stmt->mresult)
    {
        mysql_free_result(stmt->mresult);
        stmt->mresult = nullptr;
    }
    stmt->mmorerows = false;
    stmt->mcolnametable.clear();
    stmt->mresult = nullptr;
    stmt->mcurrow = nullptr;
}

// call this after you executesql
// will return false if there are no more rows
bool mysqldatabase::getnextrow(bool finalize)
{
    auto stmt = getstatement();
    if (stmt->mmorerows)
    {
        stmt->mcurrow = mysql_fetch_row(stmt->mresult);
        if (stmt->mcurrow)
        {
            return stmt->mcurrow;
        }
    }
    if (finalize)
    {
        enditerrows();
    }
    return false;
}

bool mysqldatabase::begintransaction()
{
    return executesql("start transaction;", false);
}

bool mysqldatabase::endtransaction()
{
    return executesql("commit;", false);
}
    
bool mysqldatabase::getnull (int colindex)
{
    auto stmt = getstatement();
    return stmt->mcurrow[colindex] == nullptr;
}

char* mysqldatabase::getstr (int colindex, std::string& retstr)
{
    auto stmt = getstatement();
    const char* text = reinterpret_cast<const char*> (stmt->mcurrow[colindex]);
    retstr = (text == nullptr) ? "" : text;
    return const_cast<char*> (retstr.c_str ());
}

std::int32_t mysqldatabase::getint (int colindex)
{
    auto stmt = getstatement();
    return boost::lexical_cast<std::int32_t>(stmt->mcurrow[colindex]);
}

float mysqldatabase::getfloat (int colindex)
{
    auto stmt = getstatement();
    return boost::lexical_cast<float>(stmt->mcurrow[colindex]);
}

bool mysqldatabase::getbool (int colindex)
{
    auto stmt = getstatement();
    return stmt->mcurrow[colindex][0] != '0';
}

// returns amount stored in buf
int mysqldatabase::getbinary (int colindex, unsigned char* buf, int maxsize)
{
    auto stmt = getstatement();
    auto collength = mysql_fetch_lengths(stmt->mresult);
    auto copysize = collength[colindex];
    if (copysize < maxsize)
    {
        maxsize = static_cast<int>(copysize);
    }
    memcpy(buf, stmt->mcurrow[colindex], maxsize);
    return static_cast<int>(copysize);
}

blob mysqldatabase::getbinary (int colindex)
{
    auto stmt = getstatement();
    auto collength = mysql_fetch_lengths(stmt->mresult);
    const unsigned char* blob = reinterpret_cast<const unsigned char*>(stmt->mcurrow[colindex]);
    size_t isize = collength[colindex];
    blob vucresult;
    vucresult.resize(isize);
    std::copy (blob, blob + isize, vucresult.begin());
    return vucresult;
}

std::uint64_t mysqldatabase::getbigint (int colindex)
{
    auto stmt = getstatement();
    return boost::lexical_cast<std::int32_t>(stmt->mcurrow[colindex]);
}

mysqlstatement* mysqldatabase::getstatement()
{
    auto stmt = mstmt.get();
    if (!stmt)
    {
        stmt = new mysqlstatement(mhost.c_str(), mport, musername.c_str(), mpassword.c_str(), mdatabase.c_str());
        mstmt.reset(stmt);
    }
    return stmt;
}
    
bool mysqldatabase::hasfield(const std::string &table, const std::string &field)
{
    std::string sql = "show columns from `" + table + "`;";
    if (executesql(sql.c_str(), false))
    {
        for (bool bmore = startiterrows(true); bmore; bmore = getnextrow(true))
        {
            std::string schema;
            database::getstr ("field", schema);
            if (schema == field)
            {
                return true;
            }
        }
    }
    return false;
}
    
//---------------------------------------------------
mysqlstatement::mysqlstatement(char const* host, std::uint32_t port, char const* username, char const* password, char const* database)
{
    mconnection = mysql_init(nullptr);
    if (!mconnection || mysql_real_connect(mconnection, host,
                                           username, password,
                                           database, port, null, client_multi_statements) == nullptr)
    {
        writelog (lsfatal, mysqldatabase)
            << "connect fail: host-" << host
            << " port-" << port
            << " database:" << database
            << " error_info:" << mysql_error(mconnection);
        if (mconnection)
        {
            mysql_close(mconnection);
            mconnection = nullptr;
        }
        assert(false);
    }
    // set auto connect
    my_bool reconnect = 1;
    mysql_options(mconnection, mysql_opt_reconnect, &reconnect);
    
    minbatch = false;

    mmorerows = false;
    mresult = nullptr;
    mcurrow = nullptr;
}

mysqlstatement::~mysqlstatement()
{
    if (mresult)
    {
        mysql_free_result(mresult);
    }

    if (mconnection)
    {
        mysql_close(mconnection);
    }
}
    
mysqldatabasecon::mysqldatabasecon(beast::stringpairarray& params, const char* initstrings[], int initcount)
{
    assert(params[beast::string("type")] == "mysql"
           && params[beast::string("host")] != beast::string::empty
           && params[beast::string("port")] != beast::string::empty
           && params[beast::string("username")] != beast::string::empty
           && params[beast::string("password")] != beast::string::empty
           && params[beast::string("database")] != beast::string::empty);
    
    std::string host = params[beast::string("host")].tostdstring();
    int port = boost::lexical_cast<int>(params[beast::string("port")].tostdstring());
    std::string username = params[beast::string("username")].tostdstring();
    std::string password = params[beast::string("password")].tostdstring();
    std::string database = params[beast::string("database")].tostdstring();
    bool asyncbatch = getconfig().transactiondatabase[beast::string("async_batch")] == beast::string("true");
    
    mdatabase = new mysqldatabase(host.c_str(), port, username.c_str(), password.c_str(), database.c_str(), asyncbatch);
    mdatabase->connect ();
    
    for (int i = 0; i < initcount; ++i)
        mdatabase->executesql (initstrings[i], true);
}

} // ripple
