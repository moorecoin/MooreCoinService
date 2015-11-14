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
#include <ripple/basics/stringutilities.h>
#include <ripple/app/data/database.h>

namespace ripple {

database::database (const char* host)
    : mnumcol (0)
{
    mhost   = host;
}

database::~database ()
{
}

//bool database::executesqlbatch(const std::vector<std::string>& sqlqueue)
//{
//    for (auto it = sqlqueue.begin(); it != sqlqueue.end(); ++it)
//    {
//        if (!executesql(it->c_str()))
//        {
//            return false;
//        }
//    }
//    return true;
//}

bool database::getnull (const char* colname)
{
    int index;

    if (getcolnumber (colname, &index))
    {
        return getnull (index);
    }

    return true;
}

char* database::getstr (const char* colname, std::string& retstr)
{
    int index;

    if (getcolnumber (colname, &index))
    {
        return getstr (index, retstr);
    }

    return nullptr;
}

std::int32_t database::getint (const char* colname)
{
    int index;

    if (getcolnumber (colname, &index))
    {
        return getint (index);
    }

    return 0;
}

float database::getfloat (const char* colname)
{
    int index;

    if (getcolnumber (colname, &index))
    {
        return getfloat (index);
    }

    return 0;
}

bool database::getbool (const char* colname)
{
    int index;

    if (getcolnumber (colname, &index))
    {
        return getbool (index);
    }

    return 0;
}

int database::getbinary (const char* colname, unsigned char* buf, int maxsize)
{
    int index;

    if (getcolnumber (colname, &index))
    {
        return (getbinary (index, buf, maxsize));
    }

    return (0);
}

blob database::getbinary (std::string const& strcolname)
{
    int index;

    if (getcolnumber (strcolname.c_str (), &index))
    {
        return getbinary (index);
    }

    return blob ();
}

std::string database::getstrbinary (std::string const& strcolname)
{
    // yyy could eliminate a copy if getstrbinary was a template.
    return strcopy (getbinary (strcolname.c_str ()));
}

std::uint64_t database::getbigint (const char* colname)
{
    int index;

    if (getcolnumber (colname, &index))
    {
        return getbigint (index);
    }

    return 0;
}

// returns false if can't find col
bool database::getcolnumber (const char* colname, int* retindex)
{
    for (unsigned int n = 0; n < mcolnametable.size (); n++)
    {
        if (strcmp (colname, mcolnametable[n].c_str ()) == 0)
        {
            *retindex = n;
            return (true);
        }
    }

    return false;
}

#if 0
int database::getsingledbvalueint (const char* sql)
{
    int ret;

    if ( executesql (sql) && startiterrows ()
{
    ret = getint (0);
        enditerrows ();
    }
    else
    {
        //theui->statusmsg("error with database: %s",sql);
        ret = 0;
    }
    return (ret);
}
#endif

#if 0
float database::getsingledbvaluefloat (const char* sql)
{
    float ret;

    if (executesql (sql) && startiterrows () && getnextrow ())
    {
        ret = getfloat (0);
        enditerrows ();
    }
    else
    {
        //theui->statusmsg("error with database: %s",sql);
        ret = 0;
    }

    return (ret);
}
#endif

#if 0
char* database::getsingledbvaluestr (const char* sql, std::string& retstr)
{
    char* ret;

    if (executesql (sql) && startiterrows ())
    {
        ret = getstr (0, retstr);
        enditerrows ();
    }
    else
    {
        //theui->statusmsg("error with database: %s",sql);
        ret = 0;
    }

    return (ret);
}
#endif

} // ripple
