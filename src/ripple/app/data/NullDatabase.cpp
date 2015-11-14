#include <ripple/app/data/nulldatabase.h>

namespace ripple {
nulldatabase::nulldatabase()
    : database("null")
{
    mdbtype = type::null;
}
    
nulldatabase::~nulldatabase()
{
}
    
// returns true if the query went ok
bool nulldatabase::executesql(const char* sql, bool fail_ok)
{
    return false;
}
    
bool nulldatabase::batchstart()
{
    return false;
}

bool nulldatabase::batchcommit()
{
    return false;
}

bool nulldatabase::executesqlbatch()
{
    return false;
}

// tells you how many rows were changed by an update or insert
std::uint64_t nulldatabase::getnumrowsaffected ()
{
    return 0;
}

// returns false if there are no results
bool nulldatabase::startiterrows (bool finalize)
{
    return false;
}

void nulldatabase::enditerrows()
{
}

// call this after you executesql
// will return false if there are no more rows
bool nulldatabase::getnextrow(bool finalize)
{
    return false;
}

bool nulldatabase::begintransaction()
{
    return executesql("start transaction;", false);
}

bool nulldatabase::endtransaction()
{
    return executesql("commit;", false);
}
    
bool nulldatabase::getnull (int colindex)
{
    return false;
}

char* nulldatabase::getstr (int colindex, std::string& retstr)
{
    return null;
}

std::int32_t nulldatabase::getint (int colindex)
{
    return 0;
}

float nulldatabase::getfloat (int colindex)
{
    return 0;
}

bool nulldatabase::getbool (int colindex)
{
    return false;
}

// returns amount stored in buf
int nulldatabase::getbinary (int colindex, unsigned char* buf, int maxsize)
{
    return 0;
}

blob nulldatabase::getbinary (int colindex)
{
    blob vucresult;
    return vucresult;
}

std::uint64_t nulldatabase::getbigint (int colindex)
{
    return 0;
}
    
bool nulldatabase::hasfield(const std::string &table, const std::string &field)
{
    return true;
}

} // ripple
