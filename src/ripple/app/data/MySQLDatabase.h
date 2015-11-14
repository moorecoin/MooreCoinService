#ifndef ripple_mysqldatabase_h_included
#define ripple_mysqldatabase_h_included

#include <mysql.h>
#include <boost/thread/tss.hpp>
#include <beast/module/core/text/stringpairarray.h>
#include <beast/utility/leakchecked.h>
#include <ripple/app/data/databasecon.h>

namespace ripple {

class mysqlstatement;
class mysqlthreadspecificdata;
class mysqldatabase
    : public database
    , private beast::leakchecked <mysqldatabase>
{
public:
    explicit mysqldatabase (char const* host, std::uint32_t port, char const* username, char const* password, char const* database, bool asyncbatch);
    ~mysqldatabase ();

    void connect (){};
    void disconnect (){};

    // returns true if the query went ok
    bool executesql (const char* sql, bool fail_okay);
    bool executesqlbatch();
    
    bool batchstart() override;
    bool batchcommit() override;

    // tells you how many rows were changed by an update or insert
    std::uint64_t getnumrowsaffected ();

    // returns false if there are no results
    bool startiterrows (bool finalize);
    void enditerrows ();

    // call this after you executesql
    // will return false if there are no more rows
    bool getnextrow (bool finalize);
    
    bool begintransaction() override;
    bool endtransaction() override;

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
private:
    bool getcolnumber (const char* colname, int* retindex);
    mysqlstatement *getstatement();
    
    std::uint32_t mport;
    std::string musername;
    std::string mpassword;
    std::string mdatabase;
    bool masyncbatch;

    boost::thread_specific_ptr<mysqlstatement> mstmt;
    
    std::list<std::string> msqlqueue;
    bool mthreadbatch = false;
    std::mutex mthreadbatchlock;
};
    
class mysqlstatement
{
public:
    mysqlstatement(char const* host, std::uint32_t port, char const* username, char const* password, char const* database);
    ~mysqlstatement();

    mysql *mconnection;
    std::list<std::string> msqlqueue;
    bool minbatch;
    bool mmorerows;
    std::vector <std::string> mcolnametable;
    mysql_res *mresult;
    mysql_row mcurrow;
};
    
class mysqldatabasecon
    : public databasecon
{
public:
    mysqldatabasecon (beast::stringpairarray& mysqlparams, const char* initstring[], int countinit);
};

} // ripple

#endif
