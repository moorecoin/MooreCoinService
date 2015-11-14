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
#include <ripple/net/httprequest.h>
#include <beast/module/core/text/lexicalcast.h>
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <istream>
#include <string>

namespace ripple {

// logic to handle incoming http reqests

void httprequest::reset ()
{
    mheaders.clear ();
    srequestbody.clear ();
    sauthorization.clear ();
    idatasize = 0;
    bshouldclose = true;
    estate = await_request;
}

httprequest::action httprequest::requestdone (bool forceclose)
{
    if (forceclose || bshouldclose)
        return haclose_conn;

    reset ();
    return haread_line;
}

std::string httprequest::getreplyheaders (bool forceclose)
{
    if (forceclose || bshouldclose)
        return "connection: close\r\n";
    else
        return "connection: keep-alive\r\n";
}

httprequest::action httprequest::consume (boost::asio::streambuf& buf)
{
    std::string line;
    std::istream is (&buf);
    std::getline (is, line);
    boost::trim (line);

    //  writelog (lstrace, httprequest) << "httprequest line: " << line;

    if (estate == await_request)
    {
        // verb url proto
        if (line.empty ())
            return haread_line;

        srequest = line;
        bshouldclose = srequest.find ("http/1.1") == std::string::npos;

        estate = await_header;
        return haread_line;
    }

    if (estate == await_header)
    {
        // header_name: header_body
        if (line.empty ()) // empty line or bare \r
        {
            if (idatasize == 0)
            {
                // no body
                estate = do_request;
                return hado_request;
            }

            estate = getting_body;
            return haread_raw;
        }

        size_t colon = line.find (':');

        if (colon != std::string::npos)
        {
            std::string headername = line.substr (0, colon);
            boost::trim (headername);
            boost::to_lower (headername);

            std::string headervalue = line.substr (colon + 1);
            boost::trim (headervalue);

            mheaders[headername] += headervalue;

            if (headername == "connection")
            {
                boost::to_lower (headervalue);

                if ((headervalue == "keep-alive") || (headervalue == "keepalive"))
                    bshouldclose = false;

                if (headervalue == "close")
                    bshouldclose = true;
            }

            if (headername == "content-length")
                idatasize = beast::lexicalcastthrow <int> (headervalue);

            if (headername == "authorization")
                sauthorization = headervalue;
        }

        return haread_line;
    }

    assert (false);
    return haerror;
}

} // ripple
