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

#ifndef ripple_app_basicapp_h_included
#define ripple_app_basicapp_h_included

#include <boost/asio/io_service.hpp>
#include <boost/optional.hpp>
#include <thread>
#include <vector>

// this is so that the io_service can outlive all the children
class basicapp
{
private:
    boost::optional<boost::asio::io_service::work> work_;
    std::vector<std::thread> threads_;
    boost::asio::io_service io_service_;

protected:
    basicapp(std::size_t numberofthreads);
    ~basicapp();

public:
    boost::asio::io_service&
    get_io_service()
    {
        return io_service_;
    }
};

#endif
