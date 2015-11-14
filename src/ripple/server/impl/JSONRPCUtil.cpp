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
#include <ripple/basics/log.h>
#include <ripple/server/impl/jsonrpcutil.h>
#include <ripple/protocol/jsonfields.h>
#include <ripple/protocol/buildinfo.h>
#include <ripple/protocol/systemparameters.h>
#include <ripple/json/to_string.h>
#include <boost/algorithm/string.hpp>

namespace ripple {

unsigned int const gmaxhttpheadersize = 0x02000000;

std::string gethttpheadertimestamp ()
{
    // checkme this is probably called often enough that optimizing it makes
    //         sense. there's no point in doing all this work if this function
    //         gets called multiple times a second.
    char buffer[96];
    time_t now;
    time (&now);
    struct tm* now_gmt = gmtime (&now);
    strftime (buffer, sizeof (buffer),
        "date: %a, %d %b %y %h:%m:%s +0000\r\n",
        now_gmt);
    return std::string (buffer);
}

void httpreply (int nstatus, std::string const& content, rpc::output output)
{
    if (shouldlog (lstrace, rpc))
    {
        writelog (lstrace, rpc) << "http reply " << nstatus << " " << content;
    }

    if (nstatus == 401)
    {
        output ("http/1.0 401 authorization required\r\n");
        output (gethttpheadertimestamp ());

        // checkme this returns a different version than the replies below. is
        //         this by design or an accident or should it be using
        //         buildinfo::getfullversionstring () as well?
        output ("server: " + systemname () + "-json-rpc/v1");
        output ("\r\n");

        // be careful in modifying this! if you change the contents you must
        // update the content-length header as well to indicate the correct
        // size of the data.
        output ("www-authenticate: basic realm=\"jsonrpc\"\r\n"
                    "content-type: text/html\r\n"
                    "content-length: 296\r\n"
                    "\r\n"
                    "<!doctype html public \"-//w3c//dtd html 4.01 "
                    "transitional//en\"\r\n"
                    "\"http://www.w3.org/tr/1999/rec-html401-19991224/loose.dtd"
                    "\">\r\n"
                    "<html>\r\n"
                    "<head>\r\n"
                    "<title>error</title>\r\n"
                    "<meta http-equiv='content-type' "
                    "content='text/html; charset=iso-8859-1'>\r\n"
                    "</head>\r\n"
                    "<body><h1>401 unauthorized.</h1></body>\r\n");

        return;
    }

    switch (nstatus)
    {
    case 200: output ("http/1.1 200 ok\r\n"); break;
    case 400: output ("http/1.1 400 bad request\r\n"); break;
    case 403: output ("http/1.1 403 forbidden\r\n"); break;
    case 404: output ("http/1.1 404 not found\r\n"); break;
    case 500: output ("http/1.1 500 internal server error\r\n"); break;
    }

    output (gethttpheadertimestamp ());

    output ("connection: keep-alive\r\n"
            "content-length: ");

    // vfalco todo determine if/when this header should be added
    //if (getconfig ().rpc_allow_remote)
    //    output ("access-control-allow-origin: *\r\n");

    output (std::to_string(content.size () + 2));
    output ("\r\n"
            "content-type: application/json; charset=utf-8\r\n");

    output ("server: " + systemname () + "-json-rpc/");
    output (buildinfo::getfullversionstring ());
    output ("\r\n"
            "\r\n");
    output (content);
    output ("\r\n");
}

} // ripple
