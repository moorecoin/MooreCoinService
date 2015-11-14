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
#include <ripple/app/impl/basicapp.h>
#include <beast/threads/thread.h>

basicapp::basicapp(std::size_t numberofthreads)
{
    work_ = boost::in_place(std::ref(io_service_));
    threads_.reserve(numberofthreads);
    while(numberofthreads--)
        threads_.emplace_back(
            [this, numberofthreads](){
                beast::thread::setcurrentthreadname(
                    std::string("io_service #") +
                        std::to_string(numberofthreads));
                this->io_service_.run();
            });
}

basicapp::~basicapp()
{
    work_ = boost::none;
    for (auto& _ : threads_)
        _.join();
}
