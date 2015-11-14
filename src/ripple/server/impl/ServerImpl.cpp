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
#include <ripple/server/impl/serverimpl.h>
#include <ripple/server/impl/peer.h>
#include <beast/chrono/chrono_io.h>
#include <boost/chrono/chrono_io.hpp>
#include <cassert>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <string>
#include <stdio.h>
#include <time.h>

namespace ripple {
namespace http {

serverimpl::serverimpl (handler& handler,
        boost::asio::io_service& io_service, beast::journal journal)
    : handler_ (handler)
    , journal_ (journal)
    , io_service_ (io_service)
    , strand_ (io_service_)
    , work_ (boost::in_place (std::ref(io_service)))
    , hist_{}
{
}

serverimpl::~serverimpl()
{
    close();
    {
        // block until all door objects destroyed
        std::unique_lock<std::mutex> lock(mutex_);
        while (! list_.empty())
            cond_.wait(lock);
    }
}

void
serverimpl::ports (std::vector<port> const& ports)
{
    if (closed())
        throw std::logic_error("ports() on closed http::server");
    for(auto const& _ : ports)
        if (! _.websockets())
            std::make_shared<door>(
                io_service_, *this, _)->run();
}

void
serverimpl::onwrite (beast::propertystream::map& map)
{
    std::lock_guard <std::mutex> lock (mutex_);
    map ["active"] = list_.size();
    {
        std::string s;
        for (int i = 0; i <= high_; ++i)
        {
            if (i)
                s += ", ";
            s += std::to_string (hist_[i]);
        }
        map ["hist"] = s;
    }
    {
        beast::propertystream::set set ("history", map);
        for (auto const& stat : stats_)
        {
            beast::propertystream::map item (set);

            item ["id"] = stat.id;
            item ["when"] = stat.when;

            {
                std::stringstream ss;
                ss << stat.elapsed;
                item ["elapsed"] = ss.str();
            }

            item ["requests"] = stat.requests;
            item ["bytes_in"] = stat.bytes_in;
            item ["bytes_out"] = stat.bytes_out;
            if (stat.ec)
                item ["error"] = stat.ec.message();
        }
    }
}

void
serverimpl::close()
{
    bool stopped = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (work_)
        {
            work_ = boost::none;
            // close all door objects
            if (list_.empty())
                stopped = true;
            else
                for(auto& _ :list_)
                    _.close();
        }
    }
    if (stopped)
        handler_.onstopped(*this);
}

//--------------------------------------------------------------------------

void
serverimpl::add (child& child)
{
    std::lock_guard<std::mutex> lock(mutex_);
    list_.push_back(child);
}

void
serverimpl::remove (child& child)
{
    bool stopped = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        list_.erase(list_.iterator_to(child));
        if (list_.empty())
        {
            cond_.notify_all();
            stopped = true;
        }
    }
    if (stopped)
        handler_.onstopped(*this);
}

bool
serverimpl::closed()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return ! work_;
}

void
serverimpl::report (stat&& stat)
{
    int const bucket = ceil_log2 (stat.requests);
    std::lock_guard <std::mutex> lock (mutex_);
    ++hist_[bucket];
    high_ = std::max (high_, bucket);
    if (stats_.size() >= historysize)
        stats_.pop_back();
    stats_.emplace_front (std::move(stat));
}

//--------------------------------------------------------------------------

int
serverimpl::ceil_log2 (unsigned long long x)
{
    static const unsigned long long t[6] = {
        0xffffffff00000000ull,
        0x00000000ffff0000ull,
        0x000000000000ff00ull,
        0x00000000000000f0ull,
        0x000000000000000cull,
        0x0000000000000002ull
    };

    int y = (((x & (x - 1)) == 0) ? 0 : 1);
    int j = 32;
    int i;

    for(i = 0; i < 6; i++) {
        int k = (((x & t[i]) == 0) ? 0 : j);
        y += k;
        x >>= k;
        j >>= 1;
    }

    return y;
}

//--------------------------------------------------------------------------

std::unique_ptr<server>
make_server (handler& handler,
    boost::asio::io_service& io_service, beast::journal journal)
{
    return std::make_unique<serverimpl>(handler, io_service, journal);
}

}
}
