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
#include <ripple/basics/tostring.h>
#include <beast/module/core/text/lexicalcast.h>
#include <beast/unit_test/suite.h>
#include <boost/algorithm/string.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/regex.hpp>
#include <algorithm>
#include <cstdarg>

namespace ripple {

// nikb note: this function is only used by strunhex (std::string const& strsrc)
// which results in a pointless copy from std::string into std::vector. should
// we just scrap this function altogether?
int strunhex (std::string& strdst, std::string const& strsrc)
{
    std::string tmp;

    tmp.reserve ((strsrc.size () + 1) / 2);

    auto iter = strsrc.cbegin ();

    if (strsrc.size () & 1)
    {
        int c = charunhex (*iter);

        if (c < 0)
            return -1;

        tmp.push_back(c);
        ++iter;
    }

    while (iter != strsrc.cend ())
    {
        int chigh = charunhex (*iter);
        ++iter;

        if (chigh < 0)
            return -1;

        int clow = charunhex (*iter);
        ++iter;

        if (clow < 0)
            return -1;

        tmp.push_back (static_cast<char>((chigh << 4) | clow));
    }

    strdst = std::move(tmp);

    return strdst.size ();
}

std::pair<blob, bool> strunhex (std::string const& strsrc)
{
    std::string strtmp;

    if (strunhex (strtmp, strsrc) == -1)
        return std::make_pair (blob (), false);

    return std::make_pair(strcopy (strtmp), true);
}

uint64_t uintfromhex (std::string const& strsrc)
{
    uint64_t uvalue (0);

    if (strsrc.size () > 16)
        throw std::invalid_argument("overlong 64-bit value");

    for (auto c : strsrc)
    {
        int ret = charunhex (c);

        if (ret == -1)
            throw std::invalid_argument("invalid hex digit");

        uvalue = (uvalue << 4) | ret;
    }

    return uvalue;
}

//
// misc string
//

blob strcopy (std::string const& strsrc)
{
    blob vucdst;

    vucdst.resize (strsrc.size ());

    std::copy (strsrc.begin (), strsrc.end (), vucdst.begin ());

    return vucdst;
}

std::string strcopy (blob const& vucsrc)
{
    std::string strdst;

    strdst.resize (vucsrc.size ());

    std::copy (vucsrc.begin (), vucsrc.end (), strdst.begin ());

    return strdst;

}

extern std::string urlencode (std::string const& strsrc)
{
    std::string strdst;
    int         ioutput = 0;
    int         isize   = strsrc.length ();

    strdst.resize (isize * 3);

    for (int iinput = 0; iinput < isize; iinput++)
    {
        unsigned char c = strsrc[iinput];

        if (c == ' ')
        {
            strdst[ioutput++]   = '+';
        }
        else if (isalnum (c))
        {
            strdst[ioutput++]   = c;
        }
        else
        {
            strdst[ioutput++]   = '%';
            strdst[ioutput++]   = charhex (c >> 4);
            strdst[ioutput++]   = charhex (c & 15);
        }
    }

    strdst.resize (ioutput);

    return strdst;
}

//
// ip port parsing
//
// <-- iport: "" = -1
// vfalco todo make this not require boost... and especially boost::asio
bool parseipport (std::string const& strsource, std::string& strip, int& iport)
{
    boost::smatch   smmatch;
    bool            bvalid  = false;

    static boost::regex reendpoint ("\\`\\s*(\\s+)(?:\\s+(\\d+))?\\s*\\'");

    if (boost::regex_match (strsource, smmatch, reendpoint))
    {
        boost::system::error_code   err;
        std::string                 stripraw    = smmatch[1];
        std::string                 strportraw  = smmatch[2];

        boost::asio::ip::address    addrip      = boost::asio::ip::address::from_string (stripraw, err);

        bvalid  = !err;

        if (bvalid)
        {
            strip   = addrip.to_string ();
            iport   = strportraw.empty () ? -1 : beast::lexicalcastthrow <int> (strportraw);
        }
    }

    return bvalid;
}

// todo callers should be using beast::url and beast::parse_url instead.
bool parseurl (std::string const& strurl, std::string& strscheme, std::string& strdomain, int& iport, std::string& strpath)
{
    // scheme://username:password@hostname:port/rest
    static boost::regex reurl ("(?i)\\`\\s*([[:alpha:]][-+.[:alpha:][:digit:]]*)://([^:/]+)(?::(\\d+))?(/.*)?\\s*?\\'");
    boost::smatch   smmatch;

    bool    bmatch  = boost::regex_match (strurl, smmatch, reurl);          // match status code.

    if (bmatch)
    {
        std::string strport;

        strscheme   = smmatch[1];
        strdomain   = smmatch[2];
        strport     = smmatch[3];
        strpath     = smmatch[4];

        boost::algorithm::to_lower (strscheme);

        iport   = strport.empty () ? -1 : beast::lexicalcast <int> (strport);
    }

    return bmatch;
}

beast::stringpairarray parsedelimitedkeyvaluestring (beast::string parameters,
                                                   beast::beast_wchar delimiter)
{
    beast::stringpairarray keyvalues;

    while (parameters.isnotempty ())
    {
        beast::string pair;

        {
            int const delimiterpos = parameters.indexofchar (delimiter);

            if (delimiterpos != -1)
            {
                pair = parameters.substring (0, delimiterpos);

                parameters = parameters.substring (delimiterpos + 1);
            }
            else
            {
                pair = parameters;

                parameters = beast::string::empty;
            }
        }

        int const equalpos = pair.indexofchar ('=');

        if (equalpos != -1)
        {
            beast::string const key = pair.substring (0, equalpos);
            beast::string const value = pair.substring (equalpos + 1, pair.length ());

            keyvalues.set (key, value);
        }
    }

    return keyvalues;
}

} // ripple
