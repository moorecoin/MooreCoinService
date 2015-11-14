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

// vfalco note this looks like some facility for giving websocket
//         a way to produce logging output.
//
namespace websocketpp_02 {
namespace log {

void websocketlog (websocketpp_02::log::alevel::value v, std::string const& entry)
{
    using namespace ripple;

    if ((v == websocketpp_02::log::alevel::devel) || (v == websocketpp_02::log::alevel::debug_close))
    {
        writelog(lstrace, websocket) << entry;
    }
    else
    {
        writelog(lsdebug, websocket) << entry;
    }
}

void websocketlog (websocketpp_02::log::elevel::value v, std::string const& entry)
{
    using namespace ripple;

    logseverity s = lsdebug;

    if ((v & websocketpp_02::log::elevel::info) != 0)
        s = lsinfo;
    else if ((v & websocketpp_02::log::elevel::fatal) != 0)
        s = lsfatal;
    else if ((v & websocketpp_02::log::elevel::rerror) != 0)
        s = lserror;
    else if ((v & websocketpp_02::log::elevel::warn) != 0)
        s = lswarning;

    writelog(s, websocket) << entry;
}

}
}

// vim:ts=4
