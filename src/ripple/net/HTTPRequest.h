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

#ifndef ripple_net_basics_httprequest_h_included
#define ripple_net_basics_httprequest_h_included

#include <boost/asio/streambuf.hpp>
#include <map>
#include <string>

namespace ripple {

/** an http request we are handling from a client. */
class httprequest
{
public:
    enum action
    {
        // what the application code needs to do
        haerror         = 0,
        haread_line     = 1,
        haread_raw      = 2,
        hado_request    = 3,
        haclose_conn    = 4
    };

    httprequest () : estate (await_request), idatasize (0), bshouldclose (true)
    {
        ;
    }
    void reset ();

    std::string& peekbody ()
    {
        return srequestbody;
    }
    std::string getbody ()
    {
        return srequestbody;
    }
    std::string& peekrequest ()
    {
        return srequest;
    }
    std::string getrequest ()
    {
        return srequest;
    }
    std::string& peekauth ()
    {
        return sauthorization;
    }
    std::string getauth ()
    {
        return sauthorization;
    }

    std::map<std::string, std::string>& peekheaders ()
    {
        return mheaders;
    }
    std::string getreplyheaders (bool forceclose);

    action consume (boost::asio::streambuf&);
    action requestdone (bool forceclose); // call after reply is sent

    int getdatasize ()
    {
        return idatasize;
    }

private:
    enum state
    {
        await_request,  // we are waiting for the request line
        await_header,   // we are waiting for request headers
        getting_body,   // we are waiting for the body
        do_request,     // we are waiting for the request to complete
    };

    state estate;
    std::string srequest;           // verb url proto
    std::string srequestbody;
    std::string sauthorization;

    std::map<std::string, std::string> mheaders;

    int idatasize;
    bool bshouldclose;
};

}

#endif
