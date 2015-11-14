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

#ifndef ripple_server_serverimpl_h_included
#define ripple_server_serverimpl_h_included

#include <ripple/basics/seconds_clock.h>
#include <ripple/server/handler.h>
#include <ripple/server/server.h>
#include <beast/intrusive/list.h>
#include <beast/threads/shareddata.h>
#include <beast/threads/thread.h>
#include <boost/asio.hpp>
#include <boost/intrusive/list.hpp>
#include <boost/optional.hpp>
#include <array>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>

namespace ripple {
namespace http {

class basicpeer;
class door;

struct stat
{
    std::size_t id;
    std::string when;
    std::chrono::seconds elapsed;
    int requests;
    std::size_t bytes_in;
    std::size_t bytes_out;
    boost::system::error_code ec;
};

class serverimpl : public server
{
public:
    class child : public boost::intrusive::list_base_hook <
        boost::intrusive::link_mode <boost::intrusive::normal_link>>
    {
    public:
        virtual void close() = 0;
    };

private:
    using list_type = boost::intrusive::make_list <child,
        boost::intrusive::constant_time_size <false>>::type;

    typedef std::chrono::system_clock clock_type;

    enum
    {
        historysize = 100
    };

    typedef std::vector <std::shared_ptr<door>> doors;

    handler& handler_;
    beast::journal journal_;
    boost::asio::io_service& io_service_;
    boost::asio::io_service::strand strand_;
    boost::optional <boost::asio::io_service::work> work_;

    std::mutex mutable mutex_;
    std::condition_variable cond_;
    list_type list_;
    std::deque <stat> stats_;
    int high_ = 0;

    std::array <std::size_t, 64> hist_;

public:
    serverimpl (handler& handler,
        boost::asio::io_service& io_service, beast::journal journal);

    ~serverimpl();

    beast::journal
    journal() override
    {
        return journal_;
    }

    void
    ports (std::vector<port> const& ports) override;

    void
    onwrite (beast::propertystream::map& map) override;

    void
    close() override;

public:
    handler&
    handler()
    {
        return handler_;
    }

    boost::asio::io_service&
    get_io_service()
    {
        return io_service_;
    }

    void
    add (child& child);

    void
    remove (child& child);

    bool
    closed();

    void
    report (stat&& stat);

private:
    static
    int
    ceil_log2 (unsigned long long x);
};


}
}

#endif
