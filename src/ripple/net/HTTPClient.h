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

#ifndef ripple_net_basics_httpclient_h_included
#define ripple_net_basics_httpclient_h_included

#include <boost/asio/io_service.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>

namespace ripple {

/** provides an asynchronous http client implementation with optional ssl.
*/
class httpclient
{
public:
    enum
    {
        maxclientheaderbytes = 32 * 1024
    };

    static void initializesslcontext ();

    static void get (
        bool bssl,
        boost::asio::io_service& io_service,
        std::deque <std::string> deqsites,
        const unsigned short port,
        std::string const& strpath,
        std::size_t responsemax,
        boost::posix_time::time_duration timeout,
        std::function <bool (const boost::system::error_code& ecresult, int istatus, std::string const& strdata)> complete);

    static void get (
        bool bssl,
        boost::asio::io_service& io_service,
        std::string strsite,
        const unsigned short port,
        std::string const& strpath,
        std::size_t responsemax,
        boost::posix_time::time_duration timeout,
        std::function <bool (const boost::system::error_code& ecresult, int istatus, std::string const& strdata)> complete);

    static void request (
        bool bssl,
        boost::asio::io_service& io_service,
        std::string strsite,
        const unsigned short port,
        std::function <void (boost::asio::streambuf& sb, std::string const& strhost)> build,
        std::size_t responsemax,
        boost::posix_time::time_duration timeout,
        std::function <bool (const boost::system::error_code& ecresult, int istatus, std::string const& strdata)> complete);

    enum
    {
        smstimeoutseconds = 30
    };

    static void sendsms (boost::asio::io_service& io_service, std::string const& strtext);
};

} // ripple

#endif
