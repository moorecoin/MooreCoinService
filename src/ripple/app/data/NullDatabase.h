#ifndef ripple_nulldatabase_h_included
#define ripple_nulldatabase_h_included

#include <beast/utility/leakchecked.h>
#include <ripple/app/data/databasecon.h>

namespace ripple {

class nulldatabase
    : public database
    , private beast::leakchecked <nulldatabase>
{
public:
    explicit nulldatabase ();
    ~nulldatabase ();

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
};
    
class nulldatabasecon
    : public databasecon
{
public:
    nulldatabasecon()
    {
        mdatabase = new nulldatabase();
    }
};

} // ripple

#endif
