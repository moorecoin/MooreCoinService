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

#if 0

namespace ripple {
namespace peerfinder {
namespace sim {

class link;
class message;
class network;
class node;

// maybe this should be std::set
typedef std::list <link> links;

//------------------------------------------------------------------------------

class network
{
public:
    typedef std::list <node> peers;

    typedef hash_map <ip::endpoint, boost::reference_wrapper <node>> table;

    explicit network (params const& params,
        journal journal = journal());

    ~network ();

    params const& params() const { return m_params; }
    void prepare ();
    journal journal () const;
    int next_node_id ();
    clock_type::time_point now ();
    peers& nodes();
    peers const& nodes() const;
    node* find (ip::endpoint const& address);
    void step ();

    template <typename function>
    void post (function f)
        { m_queue.post (f); }

private:
    params m_params;
    journal m_journal;
    int m_next_node_id;
    manual_clock <std::chrono::seconds> m_clock;
    peers m_nodes;
    table m_table;
    functionqueue m_queue;
};

//------------------------------------------------------------------------------

class node;

// represents a link between two peers.
// the link holds the messages the local node will receive.
//
class link
{
public:
    typedef std::vector <message> messages;

    link (
        node& local_node,
        slotimp::ptr const& slot,
        ip::endpoint const& local_endpoint,
        node& remote_node,
        ip::endpoint const& remote_endpoint,
        bool inbound)
        : m_local_node (&local_node)
        , m_slot (slot)
        , m_local_endpoint (local_endpoint)
        , m_remote_node (&remote_node)
        , m_remote_endpoint (remote_endpoint)
        , m_inbound (inbound)
        , m_closed (false)
    {
    }

    // indicates that the remote closed their end
    bool closed () const   { return m_closed; }

    bool inbound ()  const { return m_inbound; }
    bool outbound () const { return ! m_inbound; }

    ip::endpoint const& remote_endpoint() const { return m_remote_endpoint; }
    ip::endpoint const& local_endpoint()  const { return m_local_endpoint; }

    slotimp::ptr const& slot   () const { return m_slot; }
    node&       remote_node ()       { return *m_remote_node; }
    node const& remote_node () const { return *m_remote_node; }
    node&       local_node  ()       { return *m_local_node; }
    node const& local_node  () const { return *m_local_node; }

    void post (message const& m)
    {
        m_pending.push_back (m);
    }

    bool pending () const
    {
        return m_pending.size() > 0;
    }

    void close ()
    {
        m_closed = true;
    }

    void pre_step ()
    {
        std::swap (m_current, m_pending);
    }

    void step ();

private:
    node* m_local_node;
    slotimp::ptr m_slot;
    ip::endpoint m_local_endpoint;
    node* m_remote_node;
    ip::endpoint m_remote_endpoint;
    bool m_inbound;
    bool m_closed;
    messages m_current;
    messages m_pending;
};

//--------------------------------------------------------------------------

class node
    : public callback
    , public store
    , public checker
{
private:
    typedef std::vector <savedbootstrapaddress> savedbootstrapaddresses;

public:
    struct config
    {
        config ()
            : canaccept (true)
        {
        }

        bool canaccept;
        ip::endpoint listening_endpoint;
        ip::endpoint well_known_endpoint;
        peerfinder::config config;
    };

    links m_links;
    std::vector <livecache::histogram> m_livecache_history;

    node (
        network& network,
        config const& config,
        clock_type& clock,
        journal journal)
        : m_network (network)
        , m_id (network.next_node_id())
        , m_config (config)
        , m_node_id (ripplepublickey::createfrominteger (m_id))
        , m_sink (prefix(), journal.sink())
        , m_journal (journal (m_sink, journal.severity()), reporting::node)
        , m_next_port (m_config.listening_endpoint.port() + 1)
        , m_logic (boost::in_place (
            std::ref (clock), std::ref (*this), std::ref (*this), std::ref (*this), m_journal))
        , m_when_expire (m_network.now() + std::chrono::seconds (1))
    {
        logic().setconfig (m_config.config);
        logic().load ();
    }

    ~node ()
    {
        // have to destroy the logic early because it calls back into us
        m_logic = boost::none;
    }

    void dump (journal::scopedstream& ss) const
    {
        ss << listening_endpoint();
        logic().dump (ss);
    }

    links& links()
    {
        return m_links;
    }

    links const& links() const
    {
        return m_links;
    }

    int id () const
    {
        return m_id;
    }

    ripplepublickey const& node_id () const
    {
        return m_node_id;
    }

    logic& logic ()
    {
        return m_logic.get();
    }

    logic const& logic () const
    {
        return m_logic.get();
    }

    ip::endpoint const& listening_endpoint () const
    {
        return m_config.listening_endpoint;
    }

    bool canaccept () const
    {
        return m_config.canaccept;
    }

    void receive (link const& c, message const& m)
    {
        logic().on_endpoints (c.slot (), m.payload());
    }

    void pre_step ()
    {
        for (links::iterator iter (links().begin());
            iter != links().end();)
        {
            links::iterator cur (iter++);
            cur->pre_step ();
        }
    }

    void step ()
    {
        for (links::iterator iter (links().begin());
            iter != links().end();)
        {
            links::iterator cur (iter++);
            //link& link (*cur);
            cur->step ();
#if 0
            if (iter->closed ())
            {
                // post notification?
                iter->local_node().logic().on_closed (
                    iter->remote_endpoint());
                iter = links().erase (iter);
            }
            else
#endif
        }

        logic().makeoutgoingconnections ();
        logic().sendendpoints ();

        if (m_network.now() >= m_when_expire)
        {
            logic().expire();
            m_when_expire = m_network.now() + std::chrono::seconds (1);
        }

        m_livecache_history.emplace_back (
            logic().state().livecache.histogram());

        logic().periodicactivity();
    }

    //----------------------------------------------------------------------
    //
    // callback
    //
    //----------------------------------------------------------------------

    void sendendpoints (ip::endpoint const& remote_endpoint,
        endpoints const& endpoints)
    {
        m_network.post (std::bind (&node::dosendendpoints, this,
            remote_endpoint, endpoints));
    }

    void connectpeers (ipaddresses const& addresses)
    {
        m_network.post (std::bind (&node::doconnectpeers, this,
            addresses));
    }

    void disconnectpeer (ip::endpoint const& remote_endpoint, bool graceful)
    {
        m_network.post (std::bind (&node::dodisconnectpeer, this,
            remote_endpoint, graceful));
    }

    void activatepeer (ip::endpoint const& remote_endpoint)
    {
        /* no underlying peer to activate */
    }

    void dosendendpoints (ip::endpoint const& remote_endpoint,
        endpoints const& endpoints)
    {
        links::iterator const iter1 (std::find_if (
            links().begin (), links().end(),
                is_remote_endpoint (remote_endpoint)));
        if (iter1 != links().end())
        {
            // drop the message if they closed their end
            if (iter1->closed ())
                return;
            node& remote_node (iter1->remote_node());
            // find their link to us
            links::iterator const iter2 (std::find_if (
                remote_node.links().begin(), remote_node.links().end(),
                    is_remote_endpoint (iter1->local_endpoint ())));
            consistency_check (iter2 != remote_node.links().end());

            //
            // vfalco note this looks wrong! shouldn't it call receive()
            //             on the link and not the peer?
            //
            message const m (endpoints);
            iter2->local_node().receive (*iter2, m);
            //iter2->post (m);
        }
    }

    void docheckaccept (node& remote_node, ip::endpoint const& remote_endpoint)
    {
        // find our link to the remote node
        links::iterator iter (std::find_if (m_links.begin (),
            m_links.end(), is_remote_endpoint (remote_endpoint)));
        // see if the logic closed the connection
        if (iter == m_links.end())
            return;
        // post notifications
        m_network.post (std::bind (&logic::on_handshake,
            &remote_node.logic(), iter->local_endpoint(), node_id(), false));
        m_network.post (std::bind (&logic::on_handshake,
            &logic(), remote_endpoint, remote_node.node_id(), false));
    }

    void doconnectpeers (ipaddresses const& addresses)
    {
        for (ipaddresses::const_iterator iter (addresses.begin());
            iter != addresses.end(); ++iter)
        {
            ip::endpoint const& remote_endpoint (*iter);
            node* const remote_node (m_network.find (remote_endpoint));
            // acquire slot
            slot::ptr const local_slot (
                m_logic->new_outbound_slot (remote_endpoint));
            if (! local_slot)
                continue;
            // see if the address is connectible
            if (remote_node == nullptr || ! remote_node->canaccept())
            {
                // firewalled or no one listening
                // post notification
                m_network.post (std::bind (&logic::on_closed,
                    &logic(), local_slot));
                continue;
            }
            ip::endpoint const local_endpoint (
                listening_endpoint().at_port (m_next_port++));
            // acquire slot
            slot::ptr const remote_slot (
                remote_node->logic().new_inbound_slot (
                    remote_endpoint, local_endpoint));
            if (! remote_slot)
                continue;
            // connection established, create links
            m_links.emplace_back (*this, local_slot, local_endpoint,
                *remote_node, remote_endpoint, false);
            remote_node->m_links.emplace_back (*remote_node, remote_slot,
                remote_endpoint, *this, local_endpoint, true);
            // post notifications
            m_network.post (std::bind (&logic::on_connected,
                &logic(), local_endpoint, remote_endpoint));
            m_network.post (std::bind (&node::docheckaccept,
                remote_node, std::ref (*this), local_endpoint));
        }
    }

    void doclosed (ip::endpoint const& remote_endpoint, bool graceful)
    {
        // find our link to them
        links::iterator const iter (std::find_if (
            m_links.begin(), m_links.end(),
                is_remote_endpoint (remote_endpoint)));
        // must be connected!
        check_invariant (iter != m_links.end());
        // must be closed!
        check_invariant (iter->closed());
        // remove our link to them
        m_links.erase (iter);
        // notify
        m_network.post (std::bind (&logic::on_closed,
            &logic(), remote_endpoint));
    }

    void dodisconnectpeer (ip::endpoint const& remote_endpoint, bool graceful)
    {
        // find our link to them
        links::iterator const iter1 (std::find_if (
            m_links.begin(), m_links.end(),
                is_remote_endpoint (remote_endpoint)));
        if (iter1 == m_links.end())
            return;
        node& remote_node (iter1->remote_node());
        ip::endpoint const local_endpoint (iter1->local_endpoint());
        // find their link to us
        links::iterator const iter2 (std::find_if (
            remote_node.links().begin(), remote_node.links().end(),
                is_remote_endpoint (local_endpoint)));
        if (iter2 != remote_node.links().end())
        {
            // notify the remote that we closed
            check_invariant (! iter2->closed());
            iter2->close();
            m_network.post (std::bind (&node::doclosed,
                &remote_node, local_endpoint, graceful));
        }
        if (! iter1->closed ())
        {
            // remove our link to them
            m_links.erase (iter1);
            // notify
            m_network.post (std::bind (&logic::on_closed,
                &logic(), remote_endpoint));
        }

        /*
        if (! graceful || ! iter2->pending ())
        {
            remote_node.links().erase (iter2);
            remote_node.logic().on_closed (local_endpoint);
        }
        */
    }

    //----------------------------------------------------------------------
    //
    // store
    //
    //----------------------------------------------------------------------

    std::vector <savedbootstrapaddress> loadbootstrapcache ()
    {
        std::vector <savedbootstrapaddress> result;
        savedbootstrapaddress item;
        item.address = m_config.well_known_endpoint;
        item.cumulativeuptime = std::chrono::seconds (0);
        item.connectionvalence = 0;
        result.push_back (item);
        return result;
    }

    void updatebootstrapcache (
        std::vector <savedbootstrapaddress> const& list)
    {
        m_bootstrap_cache = list;
    }

    //
    // checker
    //

    void cancel ()
    {
    }

    void async_connect (ip::endpoint const& address,
        asio::shared_handler <void (result)> handler)
    {
        node* const node (m_network.find (address));
        checker::result result;
        result.address = address;
        if (node != nullptr)
            result.canaccept = node->canaccept();
        else
            result.canaccept = false;
        handler (result);
    }

private:
    std::string prefix()
    {
        int const width (5);
        std::stringstream ss;
        ss << "#" << m_id << " ";
        std::string s (ss.str());
        s.insert (0, std::max (
            0, width - int(s.size())), ' ');
        return s;
    }

    network& m_network;
    int const m_id;
    config const m_config;
    ripplepublickey m_node_id;
    wrappedsink m_sink;
    journal m_journal;
    ip::port m_next_port;
    boost::optional <logic> m_logic;
    clock_type::time_point m_when_expire;
    savedbootstrapaddresses m_bootstrap_cache;
};

//------------------------------------------------------------------------------

void link::step ()
{
    for (messages::const_iterator iter (m_current.begin());
        iter != m_current.end(); ++iter)
        m_local_node->receive (*this, *iter);
    m_current.clear();
}

//------------------------------------------------------------------------------

static ip::endpoint next_endpoint (ip::endpoint address)
{
    if (address.is_v4())
    {
        do
        {
            address = ip::endpoint (ip::addressv4 (
                address.to_v4().value + 1)).at_port (address.port());
        }
        while (! is_public (address));

        return address;
    }

    bassert (address.is_v6());
    // unimplemented
    bassertfalse;
    return ip::endpoint();
}

network::network (

    params const& params,
    journal journal)
    : m_params (params)
    , m_journal (journal)
    , m_next_node_id (1)
{
}

void network::prepare ()
{
    ip::endpoint const well_known_endpoint (
        ip::endpoint::from_string ("1.0.0.1").at_port (1));
    ip::endpoint address (well_known_endpoint);

    for (int i = 0; i < params().nodes; ++i )
    {
        if (i == 0)
        {
            node::config config;
            config.canaccept = true;
            config.listening_endpoint = address;
            config.well_known_endpoint = well_known_endpoint;
            config.config.maxpeers = params().maxpeers;
            config.config.outpeers = params().outpeers;
            config.config.wantincoming = true;
            config.config.autoconnect = true;
            config.config.listeningport = address.port();
            m_nodes.emplace_back (
                *this,
                config,
                m_clock,
                m_journal);
            m_table.emplace (address, std::ref (m_nodes.back()));
            address = next_endpoint (address);
        }

        if (i != 0)
        {
            node::config config;
            config.canaccept = random::getsystemrandom().nextint (100) >=
                (m_params.firewalled * 100);
            config.listening_endpoint = address;
            config.well_known_endpoint = well_known_endpoint;
            config.config.maxpeers = params().maxpeers;
            config.config.outpeers = params().outpeers;
            config.config.wantincoming = true;
            config.config.autoconnect = true;
            config.config.listeningport = address.port();
            m_nodes.emplace_back (
                *this,
                config,
                m_clock,
                m_journal);
            m_table.emplace (address, std::ref (m_nodes.back()));
            address = next_endpoint (address);
        }
    }
}

network::~network ()
{
}

journal network::journal () const
{
    return m_journal;
}

int network::next_node_id ()
{
    return m_next_node_id++;
}

clock_type::time_point network::now ()
{
    return m_clock.now();
}

network::peers& network::nodes()
{
    return m_nodes;
}

#if 0
network::peers const& network::nodes() const
{
    return m_nodes;
}
#endif

node* network::find (ip::endpoint const& address)
{
    table::iterator iter (m_table.find (address));
    if (iter != m_table.end())
        return iter->second.get_pointer();
    return nullptr;
}

void network::step ()
{
    for (peers::iterator iter (m_nodes.begin());
        iter != m_nodes.end();)
        (iter++)->pre_step();

    for (peers::iterator iter (m_nodes.begin());
        iter != m_nodes.end();)
        (iter++)->step();

    m_queue.run ();

    // advance the manual clock so that
    // messages are broadcast at every step.
    //
    //m_clock += tuning::secondsperconnect;
    ++m_clock;
}

//------------------------------------------------------------------------------

template <>
struct vertextraits <node>
{
    typedef links edges;
    typedef link  edge;
    static edges& edges (node& node)
        { return node.links(); }
    static node* vertex (link& l)
        { return &l.remote_node(); }
};

//------------------------------------------------------------------------------

struct peerstats
{
    peerstats ()
        : inboundactive (0)
        , out_active (0)
        , inboundslotsfree (0)
        , outboundslotsfree (0)
    {
    }

    template <typename peer>
    explicit peerstats (peer const& peer)
    {
        inboundactive = peer.logic().counts().inboundactive();
        out_active = peer.logic().counts().out_active();
        inboundslotsfree = peer.logic().counts().inboundslotsfree();
        outboundslotsfree = peer.logic().counts().outboundslotsfree();
    }

    peerstats& operator+= (peerstats const& rhs)
    {
        inboundactive += rhs.inboundactive;
        out_active += rhs.out_active;
        inboundslotsfree += rhs.inboundslotsfree;
        outboundslotsfree += rhs.outboundslotsfree;
        return *this;
    }

    int totalactive () const
        { return inboundactive + out_active; }

    int inboundactive;
    int out_active;
    int inboundslotsfree;
    int outboundslotsfree;
};

//------------------------------------------------------------------------------

inline peerstats operator+ (peerstats const& lhs, peerstats& rhs)
{
    peerstats result (lhs);
    result += rhs;
    return result;
}

//------------------------------------------------------------------------------

/** aggregates statistics on the connected network. */
class crawlstate
{
public:
    explicit crawlstate (std::size_t step)
        : m_step (step)
        , m_size (0)
        , m_diameter (0)
    {
    }

    std::size_t step () const
        { return m_step; }

    std::size_t size () const
        { return m_size; }

    int diameter () const
        { return m_diameter; }

    peerstats const& stats () const
        { return m_stats; }

    // network wide average
    double outpeers () const
    {
        if (m_size > 0)
            return double (m_stats.out_active) / m_size;
        return 0;
    }

    // histogram, shows the number of peers that have a specific number of
    // active connections. the index into the array is the number of connections,
    // and the value is the number of peers.
    //
    std::vector <int> totalactivehistogram;

    template <typename peer>
    void operator() (peer const& peer, int diameter)
    {
        ++m_size;
        peerstats const stats (peer);
        int const bucket (stats.totalactive ());
        if (totalactivehistogram.size() < bucket + 1)
            totalactivehistogram.resize (bucket + 1);
        ++totalactivehistogram [bucket];
        m_stats += stats;
        m_diameter = diameter;
    }

private:
    std::size_t m_step;
    std::size_t m_size;
    peerstats m_stats;
    int m_diameter;
};

//------------------------------------------------------------------------------

/** report the results of a network crawl. */
template <typename stream, typename crawl>
void report_crawl (stream const& stream, crawl const& c)
{
    if (! stream)
        return;
    stream
        << std::setw (6) << c.step()
        << std::setw (6) << c.size()
        << std::setw (6) << std::fixed << std::setprecision(2) << c.outpeers()
        << std::setw (6) << c.diameter()
        //<< to_string (c.totalactivehistogram)
        ;
}

template <typename stream, typename crawlsequence>
void report_crawls (stream const& stream, crawlsequence const& c)
{
    if (! stream)
        return;
    stream
        << "crawl report"
        << std::endl
        << std::setw (6) << "step"
        << std::setw (6) << "size"
        << std::setw (6) << "out"
        << std::setw (6) << "hops"
        //<< std::setw (6) << "count"
        ;
    for (typename crawlsequence::const_iterator iter (c.begin());
        iter != c.end(); ++iter)
        report_crawl (stream, *iter);
    stream << std::endl;
}

/** report a table with aggregate information on each node. */
template <typename nodesequence>
void report_nodes (nodesequence const& nodes, journal::stream const& stream)
{
    stream <<
        divider() << std::endl <<
        "nodes report" << std::endl <<
        rfield ("id") <<
        rfield ("total") <<
        rfield ("in") <<
        rfield ("out") <<
        rfield ("tries") <<
        rfield ("live") <<
        rfield ("boot")
        ;

    for (typename nodesequence::const_iterator iter (nodes.begin());
        iter != nodes.end(); ++iter)
    {
        typename nodesequence::value_type const& node (*iter);
        logic const& logic (node.logic());
        logic::state const& state (logic.state());
        stream <<
            rfield (node.id ()) <<
            rfield (state.counts.totalactive ()) <<
            rfield (state.counts.inboundactive ()) <<
            rfield (state.counts.out_active ()) <<
            rfield (state.counts.connectcount ()) <<
            rfield (state.livecache.size ()) <<
            rfield (state.bootcache.size ())
            ;
    }
}

//------------------------------------------------------------------------------

/** convert a sequence into a formatted delimited string.
    the range is [first, last)
*/
/** @{ */
template <typename inputiterator, class chart, class traits>
std::basic_string <chart, traits>
    sequence_to_string (inputiterator first, inputiterator last,
        std::basic_string <chart, traits> const& sep = ",", int width = -1)
{
    std::basic_stringstream <chart, traits> ss;
    while (first != last)
    {
        inputiterator const iter (first++);
        if (width > 0)
            ss << std::setw (width) << *iter;
        else
            ss << *iter;
        if (first != last)
            ss << sep;
    }
    return ss.str();
}

template <typename inputiterator>
std::string sequence_to_string (inputiterator first, inputiterator last,
    char const* sep, int width = -1)
{
    return sequence_to_string (first, last, std::string (sep), width);
}
/** @} */

/** report the time-evolution of a specified node. */
template <typename node, typename stream>
void report_node_timeline (node const& node, stream const& stream)
{
    typename livecache::histogram::size_type const histw (
        3 * livecache::histogram::size() - 1);
    // title
    stream <<
        divider () << std::endl <<
        "node #" << node.id() << " history" << std::endl <<
        divider ();
    // legend
    stream <<
        fpad (4) << fpad (2) <<
        fpad (2) << field ("livecache entries by hops", histw) << fpad (2)
        ;
    {
        journal::scopedstream ss (stream);
        ss <<
            rfield ("step",4) << fpad (2);
        ss << "[ ";
        for (typename livecache::histogram::size_type i (0);
            i < livecache::histogram::size(); ++i)
        {
            ss << rfield (i,2);
            if (i != livecache::histogram::size() - 1)
                ss << fpad (1);
        }
        ss << " ]";
    }

    // entries
    typedef std::vector <livecache::histogram> history;
    history const& h (node.m_livecache_history);
    std::size_t step (0);
    for (typename history::const_iterator iter (h.begin());
        iter != h.end(); ++iter)
    {
        ++step;
        livecache::histogram const& t (*iter);
        stream <<
            rfield (step,4) << fpad (2) <<
            fpad (2) <<
                field (sequence_to_string (t.begin (), t.end(), " ", 2), histw) <<
                fpad (2)
                ;
    }
}

//------------------------------------------------------------------------------

class peerfindertests : public unittest
{
public:
    void runtest ()
    {
        //debug::setalwayscheckheap (true);

        begintestcase ("network");

        params p;
        p.steps      = 200;
        p.nodes      = 1000;
        p.outpeers   = 9.5;
        p.maxpeers   = 200;
        p.firewalled = 0.80;

        network n (p, journal (journal(), reporting::network));

        // report network parameters
        if (reporting::params)
        {
            journal::stream const stream (journal().info);

            if (stream)
            {
                stream
                    << "network parameters"
                    << std::endl
                    << std::setw (6) << "steps"
                    << std::setw (6) << "nodes"
                    << std::setw (6) << "out"
                    << std::setw (6) << "max"
                    << std::setw (6) << "fire"
                    ;

                stream
                    << std::setw (6) << p.steps
                    << std::setw (6) << p.nodes
                    << std::setw (6) << std::fixed << std::setprecision (1) << p.outpeers
                    << std::setw (6) << p.maxpeers
                    << std::setw (6) << int (p.firewalled * 100)
                    ;

                stream << std::endl;
            }
        }

        //
        // run the simulation
        //
        n.prepare ();
        {
            // note that this stream is only for the crawl,
            // the network has its own journal.
            journal::stream const stream (
                journal().info, reporting::crawl);

            std::vector <crawlstate> crawls;
            if (reporting::crawl)
                crawls.reserve (p.steps);

            // iterate the network
            for (std::size_t step (0); step < p.steps; ++step)
            {
                if (reporting::crawl)
                {
                    crawls.emplace_back (step);
                    crawlstate& c (crawls.back ());
                    breadth_first_traverse <node, crawlstate&> (
                        n.nodes().front(), c);
                }
                n.journal().info <<
                    divider () << std::endl <<
                    "time " << n.now ().time_since_epoch () << std::endl <<
                    divider ()
                    ;

                n.step();
                n.journal().info << std::endl;
            }
            n.journal().info << std::endl;

            // report the crawls
            report_crawls (stream, crawls);
        }

        // run detailed nodes dump report
        if (reporting::dump_nodes)
        {
            journal::stream const stream (journal().info);
            for (network::peers::const_iterator iter (n.nodes().begin());
                iter != n.nodes().end(); ++iter)
            {
                journal::scopedstream ss (stream);
                node const& node (*iter);
                ss << std::endl <<
                    "--------------" << std::endl <<
                    "#" << node.id() <<
                    " at " << node.listening_endpoint ();
                node.logic().dump (ss);
            }
        }

        // run aggregate nodes report
        if (reporting::nodes)
        {
            journal::stream const stream (journal().info);
            report_nodes (n.nodes (), stream);
            stream << std::endl;
        }

        // run node report
        {
            journal::stream const stream (journal().info);
            report_node_timeline (n.nodes().front(), stream);
            stream << std::endl;
        }

        pass();
    }

    peerfindertests () : unittest ("peerfinder", "ripple", runmanual)
    {
    }
};

static peerfindertests peerfindertests;

}
}
}

#endif
