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

#ifndef ripple_basics_stringutilities_h_included
#define ripple_basics_stringutilities_h_included

#include <ripple/basics/byteorder.h>
#include <ripple/basics/blob.h>
#include <ripple/basics/strhex.h>
#include <boost/format.hpp>
#include <beast/module/core/text/stringpairarray.h>
#include <sstream>
#include <string>

namespace ripple {

extern std::string urlencode (std::string const& strsrc);

// nikb todo remove this function - it's only used for some logging in the unl
//           code which can be trivially rewritten.
template<class iterator>
std::string strjoin (iterator first, iterator last, std::string strseperator)
{
    std::ostringstream  ossvalues;

    for (iterator start = first; first != last; first++)
    {
        ossvalues << str (boost::format ("%s%s") % (start == first ? "" : strseperator) % *first);
    }

    return ossvalues.str ();
}

// nikb todo remove the need for all these overloads. move them out of here.
inline const std::string strhex (std::string const& strsrc)
{
    return strhex (strsrc.begin (), strsrc.size ());
}

inline std::string strhex (blob const& vucdata)
{
    return strhex (vucdata.begin (), vucdata.size ());
}

inline std::string strhex (const std::uint64_t uihost)
{
    uint64_t    ubig    = htobe64 (uihost);

    return strhex ((unsigned char*) &ubig, sizeof (ubig));
}

inline static std::string sqlescape (std::string const& strsrc)
{
    static boost::format f ("x'%s'");
    return str (boost::format (f) % strhex (strsrc));
}

inline static std::string sqlescape (blob const& vecsrc)
{
    size_t size = vecsrc.size ();

    if (size == 0)
        return "x''";

    std::string j (size * 2 + 3, 0);

    unsigned char* optr = reinterpret_cast<unsigned char*> (&*j.begin ());
    const unsigned char* iptr = &vecsrc[0];

    *optr++ = 'x';
    *optr++ = '\'';

    for (int i = size; i != 0; --i)
    {
        unsigned char c = *iptr++;
        *optr++ = charhex (c >> 4);
        *optr++ = charhex (c & 15);
    }

    *optr++ = '\'';
    return j;
}

int strunhex (std::string& strdst, std::string const& strsrc);

uint64_t uintfromhex (std::string const& strsrc);

std::pair<blob, bool> strunhex (std::string const& strsrc);

blob strcopy (std::string const& strsrc);
std::string strcopy (blob const& vucsrc);

bool parseipport (std::string const& strsource, std::string& strip, int& iport);

bool parseurl (std::string const& strurl, std::string& strscheme,
               std::string& strdomain, int& iport, std::string& strpath);

/** create a parameters from a string.

    parameter strings have the format:

    <key>=<value>['|'<key>=<value>]
*/
extern beast::stringpairarray
parsedelimitedkeyvaluestring (
    beast::string s, beast::beast_wchar delimiter='|');

} // ripple

#endif
