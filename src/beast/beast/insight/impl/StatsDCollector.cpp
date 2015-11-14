//------------------------------------------------------------------------------
/*
    this file is part of beast: https://github.com/vinniefalco/beast
    copyright 2013, vinnie falco <vinnie.falco@gmail.com>

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

#include <beast/asio/ipaddressconversion.h>
#include <beast/asio/placeholders.h>
#include <beast/intrusive/list.h>
#include <beast/threads/shareddata.h>
#include <boost/asio/ip/tcp.hpp>
#include <boost/optional.hpp>
#include <cassert>
#include <climits>
#include <deque>
#include <functional>
#include <set>
#include <sstream>
#include <thread>

#ifndef beast_statsdcollector_tracing_enabled
#define beast_statsdcollector_tracing_enabled 0
#endif

namespace beast {
namespace insight {

namespace detail {

class statsdcollectorimp;

//------------------------------------------------------------------------------

class statsdmetricbase : public list <statsdmetricbase>::node
{
public:
    virtual void do_process () = 0;
};

//------------------------------------------------------------------------------

class statsdhookimpl
    : public hookimpl
    , public statsdmetricbase
{
public:
    statsdhookimpl (
        handlertype const& handler,
            std::shared_ptr <statsdcollectorimp> const& impl);

    ~statsdhookimpl ();

    void do_process ();

private:
    statsdhookimpl& operator= (statsdhookimpl const&);

    std::shared_ptr <statsdcollectorimp> m_impl;
    handlertype m_handler;
};

//------------------------------------------------------------------------------

class statsdcounterimpl
    : public counterimpl
    , public statsdmetricbase
{
public:
    statsdcounterimpl (std::string const& name,
        std::shared_ptr <statsdcollectorimp> const& impl);

    ~statsdcounterimpl ();

    void increment (counterimpl::value_type amount);

    void flush ();
    void do_increment (counterimpl::value_type amount);
    void do_process ();

private:
    statsdcounterimpl& operator= (statsdcounterimpl const&);

    std::shared_ptr <statsdcollectorimp> m_impl;
    std::string m_name;
    counterimpl::value_type m_value;
    bool m_dirty;
};

//------------------------------------------------------------------------------

class statsdeventimpl
    : public eventimpl
{
public:
    statsdeventimpl (std::string const& name,
        std::shared_ptr <statsdcollectorimp> const& impl);

    ~statsdeventimpl ();

    void notify (eventimpl::value_type const& alue);

    void do_notify (eventimpl::value_type const& value);
    void do_process ();

private:
    statsdeventimpl& operator= (statsdeventimpl const&);

    std::shared_ptr <statsdcollectorimp> m_impl;
    std::string m_name;
};

//------------------------------------------------------------------------------

class statsdgaugeimpl
    : public gaugeimpl
    , public statsdmetricbase
{
public:
    statsdgaugeimpl (std::string const& name,
        std::shared_ptr <statsdcollectorimp> const& impl);

    ~statsdgaugeimpl ();

    void set (gaugeimpl::value_type value);
    void increment (gaugeimpl::difference_type amount);

    void flush ();
    void do_set (gaugeimpl::value_type value);
    void do_increment (gaugeimpl::difference_type amount);
    void do_process ();

private:
    statsdgaugeimpl& operator= (statsdgaugeimpl const&);

    std::shared_ptr <statsdcollectorimp> m_impl;
    std::string m_name;
    gaugeimpl::value_type m_last_value;
    gaugeimpl::value_type m_value;
    bool m_dirty;
};

//------------------------------------------------------------------------------

class statsdmeterimpl
    : public meterimpl
    , public statsdmetricbase
{
public:
    explicit statsdmeterimpl (std::string const& name,
        std::shared_ptr <statsdcollectorimp> const& impl);

    ~statsdmeterimpl ();

    void increment (meterimpl::value_type amount);

    void flush ();
    void do_increment (meterimpl::value_type amount);
    void do_process ();

private:
    statsdmeterimpl& operator= (statsdmeterimpl const&);

    std::shared_ptr <statsdcollectorimp> m_impl;
    std::string m_name;
    meterimpl::value_type m_value;
    bool m_dirty;
};

//------------------------------------------------------------------------------

class statsdcollectorimp
    : public statsdcollector
    , public std::enable_shared_from_this <statsdcollectorimp>
{
private:
    enum
    {
        //max_packet_size = 484
        max_packet_size = 1472
    };

    struct statetype
    {
        list <statsdmetricbase> metrics;
    };

    typedef shareddata <statetype> state;

    journal m_journal;
    ip::endpoint m_address;
    std::string m_prefix;
    boost::asio::io_service m_io_service;
    boost::optional <boost::asio::io_service::work> m_work;
    boost::asio::io_service::strand m_strand;
    boost::asio::deadline_timer m_timer;
    boost::asio::ip::udp::socket m_socket;
    std::deque <std::string> m_data;
    state m_state;

    // must come last for order of init
    std::thread m_thread;

    static boost::asio::ip::udp::endpoint to_endpoint (
        ip::endpoint const &address)
    {
        if (address.is_v4 ())
        {
            return boost::asio::ip::udp::endpoint (
                boost::asio::ip::address_v4 (
                    address.to_v4().value), address.port ());
        }

        // vfalco todo ipv6 support
        bassertfalse;
        return boost::asio::ip::udp::endpoint (
            boost::asio::ip::address_v6 (), 0);
    }

public:
    statsdcollectorimp (
        ip::endpoint const& address,
        std::string const& prefix,
        journal journal)
        : m_journal (journal)
        , m_address (address)
        , m_prefix (prefix)
        , m_work (std::ref (m_io_service))
        , m_strand (m_io_service)
        , m_timer (m_io_service)
        , m_socket (m_io_service)
        , m_thread (&statsdcollectorimp::run, this)
    {
    }

    ~statsdcollectorimp ()
    {
        boost::system::error_code ec;
        m_timer.cancel (ec);

        m_work = boost::none;
        m_thread.join ();
    }

    hook make_hook (hookimpl::handlertype const& handler)
    {
        return hook (std::make_shared <detail::statsdhookimpl> (
            handler, shared_from_this ()));
    }

    counter make_counter (std::string const& name)
    {
        return counter (std::make_shared <detail::statsdcounterimpl> (
            name, shared_from_this ()));
    }

    event make_event (std::string const& name)
    {
        return event (std::make_shared <detail::statsdeventimpl> (
            name, shared_from_this ()));
    }

    gauge make_gauge (std::string const& name)
    {
        return gauge (std::make_shared <detail::statsdgaugeimpl> (
            name, shared_from_this ()));
    }

    meter make_meter (std::string const& name)
    {
        return meter (std::make_shared <detail::statsdmeterimpl> (
            name, shared_from_this ()));
    }

    //--------------------------------------------------------------------------

    void add (statsdmetricbase& metric)
    {
        state::access state (m_state);
        state->metrics.push_back (metric);
    }

    void remove (statsdmetricbase& metric)
    {
        state::access state (m_state);
        state->metrics.erase (state->metrics.iterator_to (metric));
    }

    //--------------------------------------------------------------------------

    boost::asio::io_service& get_io_service ()
    {
        return m_io_service;
    }

    std::string const& prefix () const
    {
        return m_prefix;
    }

    void do_post_buffer (std::string const& buffer)
    {
        m_data.emplace_back (buffer);
    }

    void post_buffer (std::string&& buffer)
    {
        m_io_service.dispatch (m_strand.wrap (std::bind (
            &statsdcollectorimp::do_post_buffer, this,
                std::move (buffer))));
    }

    void on_send (boost::system::error_code ec, std::size_t)
    {
        if (ec == boost::asio::error::operation_aborted)
            return;

        if (ec)
        {
            m_journal.error <<
                "async_send failed: " << ec.message();
            return;
        }
    }

    void log (std::vector <boost::asio::const_buffer> const& buffers)
    {
        (void)buffers;
#if beast_statsdcollector_tracing_enabled
        std::stringstream ss;
        for (auto const& buffer : buffers)
        {
            std::string const s (boost::asio::buffer_cast <char const*> (buffer),
                boost::asio::buffer_size (buffer));
            ss << s;
        }
        //m_journal.trace << std::endl << ss.str ();
        outputdebugstring (ss.str ());
#endif
    }

    // send what we have
    void send_buffers ()
    {
        // break up the array of strings into blocks
        // that each fit into one udp packet.
        //
        boost::system::error_code ec;
        std::vector <boost::asio::const_buffer> buffers;
        buffers.reserve (m_data.size ());
        std::size_t size (0);
        for (auto const& s : m_data)
        {
            std::size_t const length (s.size ());
            assert (! s.empty ());
            if (! buffers.empty () && (size + length) > max_packet_size)
            {
#if beast_statsdcollector_tracing_enabled
                log (buffers);
#endif
                m_socket.async_send (buffers, std::bind (
                    &statsdcollectorimp::on_send, this,
                        beast::asio::placeholders::error,
                            beast::asio::placeholders::bytes_transferred));
                buffers.clear ();
                size = 0;
            }

            buffers.emplace_back (&s[0], length);
            size += length;
        }

        if (! buffers.empty ())
        {
#if beast_statsdcollector_tracing_enabled
            log (buffers);
#endif
            m_socket.async_send (buffers, std::bind (
                &statsdcollectorimp::on_send, this,
                    beast::asio::placeholders::error,
                        beast::asio::placeholders::bytes_transferred));
        }
        m_data.clear ();
    }

    void set_timer ()
    {
        m_timer.expires_from_now (boost::posix_time::seconds (1));
        m_timer.async_wait (std::bind (
            &statsdcollectorimp::on_timer, this,
                beast::asio::placeholders::error));
    }

    void on_timer (boost::system::error_code ec)
    {
        if (ec == boost::asio::error::operation_aborted)
            return;

        if (ec)
        {
            m_journal.error <<
                "on_timer failed: " << ec.message();
            return;
        }

        state::access state (m_state);

        for (list <statsdmetricbase>::iterator iter (state->metrics.begin());
            iter != state->metrics.end(); ++iter)
            iter->do_process();

        send_buffers ();

        set_timer ();
    }

    void run ()
    {
        boost::system::error_code ec;

        if (m_socket.connect (to_endpoint (m_address), ec))
        {
            m_journal.error <<
                "connect failed: " << ec.message();
            return;
        }

        set_timer ();

        m_io_service.run ();

        m_socket.shutdown (
            boost::asio::ip::udp::socket::shutdown_send, ec);

        m_socket.close ();

        m_io_service.poll ();
    }
};

//------------------------------------------------------------------------------

statsdhookimpl::statsdhookimpl (handlertype const& handler,
    std::shared_ptr <statsdcollectorimp> const& impl)
    : m_impl (impl)
    , m_handler (handler)
{
    m_impl->add (*this);
}

statsdhookimpl::~statsdhookimpl ()
{
    m_impl->remove (*this);
}

void statsdhookimpl::do_process ()
{
    m_handler ();
}

//------------------------------------------------------------------------------

statsdcounterimpl::statsdcounterimpl (std::string const& name,
    std::shared_ptr <statsdcollectorimp> const& impl)
    : m_impl (impl)
    , m_name (name)
    , m_value (0)
    , m_dirty (false)
{
    m_impl->add (*this);
}

statsdcounterimpl::~statsdcounterimpl ()
{
    m_impl->remove (*this);
}

void statsdcounterimpl::increment (counterimpl::value_type amount)
{
    m_impl->get_io_service().dispatch (std::bind (
        &statsdcounterimpl::do_increment,
            std::static_pointer_cast <statsdcounterimpl> (
                shared_from_this ()), amount));
}

void statsdcounterimpl::flush ()
{
    if (m_dirty)
    {
        m_dirty = false;
        std::stringstream ss;
        ss <<
            m_impl->prefix() << "." <<
            m_name << ":" <<
            m_value << "|c" <<
            "\n";
        m_value = 0;
        m_impl->post_buffer (ss.str ());
    }
}

void statsdcounterimpl::do_increment (counterimpl::value_type amount)
{
    m_value += amount;
    m_dirty = true;
}

void statsdcounterimpl::do_process ()
{
    flush ();
}

//------------------------------------------------------------------------------

statsdeventimpl::statsdeventimpl (std::string const& name,
    std::shared_ptr <statsdcollectorimp> const& impl)
    : m_impl (impl)
    , m_name (name)
{
}

statsdeventimpl::~statsdeventimpl ()
{
}

void statsdeventimpl::notify (eventimpl::value_type const& value)
{
    m_impl->get_io_service().dispatch (std::bind (
        &statsdeventimpl::do_notify,
            std::static_pointer_cast <statsdeventimpl> (
                shared_from_this ()), value));
}

void statsdeventimpl::do_notify (eventimpl::value_type const& value)
{
    std::stringstream ss;
    ss <<
        m_impl->prefix() << "." <<
        m_name << ":" <<
        value.count() << "|ms" <<
        "\n";
    m_impl->post_buffer (ss.str ());
}

//------------------------------------------------------------------------------

statsdgaugeimpl::statsdgaugeimpl (std::string const& name,
    std::shared_ptr <statsdcollectorimp> const& impl)
    : m_impl (impl)
    , m_name (name)
    , m_last_value (0)
    , m_value (0)
    , m_dirty (false)
{
    m_impl->add (*this);
}

statsdgaugeimpl::~statsdgaugeimpl ()
{
    m_impl->remove (*this);
}

void statsdgaugeimpl::set (gaugeimpl::value_type value)
{
    m_impl->get_io_service().dispatch (std::bind (
        &statsdgaugeimpl::do_set,
            std::static_pointer_cast <statsdgaugeimpl> (
                shared_from_this ()), value));
}

void statsdgaugeimpl::increment (gaugeimpl::difference_type amount)
{
    m_impl->get_io_service().dispatch (std::bind (
        &statsdgaugeimpl::do_increment,
            std::static_pointer_cast <statsdgaugeimpl> (
                shared_from_this ()), amount));
}

void statsdgaugeimpl::flush ()
{
    if (m_dirty)
    {
        m_dirty = false;
        std::stringstream ss;
        ss <<
            m_impl->prefix() << "." <<
            m_name << ":" <<
            m_value << "|c" <<
            "\n";
        m_impl->post_buffer (ss.str ());
    }
}

void statsdgaugeimpl::do_set (gaugeimpl::value_type value)
{
    m_value = value;
    
    if (m_value != m_last_value)
    {
        m_last_value = m_value;
        m_dirty = true;
    }
}

void statsdgaugeimpl::do_increment (gaugeimpl::difference_type amount)
{
    gaugeimpl::value_type value (m_value);

    if (amount > 0)
    {
        gaugeimpl::value_type const d (
            static_cast <gaugeimpl::value_type> (amount));
        value +=
            (d >= std::numeric_limits <gaugeimpl::value_type>::max() - m_value)
            ? std::numeric_limits <gaugeimpl::value_type>::max() - m_value
            : d;
    }
    else if (amount < 0)
    {
        gaugeimpl::value_type const d (
            static_cast <gaugeimpl::value_type> (-amount));
        value = (d >= value) ? 0 : value - d; 
    }

    do_set (value);
}

void statsdgaugeimpl::do_process ()
{
    flush ();
}

//------------------------------------------------------------------------------

statsdmeterimpl::statsdmeterimpl (std::string const& name,
    std::shared_ptr <statsdcollectorimp> const& impl)
    : m_impl (impl)
    , m_name (name)
    , m_value (0)
    , m_dirty (false)
{
    m_impl->add (*this);
}

statsdmeterimpl::~statsdmeterimpl ()
{
    m_impl->remove (*this);
}

void statsdmeterimpl::increment (meterimpl::value_type amount)
{
    m_impl->get_io_service().dispatch (std::bind (
        &statsdmeterimpl::do_increment,
            std::static_pointer_cast <statsdmeterimpl> (
                shared_from_this ()), amount));
}

void statsdmeterimpl::flush ()
{
    if (m_dirty)
    {
        m_dirty = false;
        std::stringstream ss;
        ss <<
            m_impl->prefix() << "." <<
            m_name << ":" <<
            m_value << "|m" <<
            "\n";
        m_value = 0;
        m_impl->post_buffer (ss.str ());
    }
}

void statsdmeterimpl::do_increment (meterimpl::value_type amount)
{
    m_value += amount;
    m_dirty = true;
}

void statsdmeterimpl::do_process ()
{
    flush ();
}

}

//------------------------------------------------------------------------------

std::shared_ptr <statsdcollector> statsdcollector::new (
    ip::endpoint const& address, std::string const& prefix, journal journal)
{
    return std::make_shared <detail::statsdcollectorimp> (
        address, prefix, journal);
}

}
}
