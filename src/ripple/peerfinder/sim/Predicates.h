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

#ifndef ripple_peerfinder_sim_predicates_h_included
#define ripple_peerfinder_sim_predicates_h_included

namespace ripple {
namespace peerfinder {
namespace sim {

/** unarypredicate, returns `true` if the 'to' node on a link matches. */
/** @{ */
template <typename node>
class is_remote_node_pred
{
public:
    is_remote_node_pred (node const& node)
        : node (node)
        { }
    template <typename link>
    bool operator() (link const& l) const
        { return &node == &l.remote_node(); }
private:
    node const& node;
};

template <typename node>
is_remote_node_pred <node> is_remote_node (node const& node)
{
    return is_remote_node_pred <node> (node);
}

template <typename node>
is_remote_node_pred <node> is_remote_node (node const* node)
{
    return is_remote_node_pred <node> (*node);
}
/** @} */

//------------------------------------------------------------------------------

/** unarypredicate, `true` if the remote address matches. */
class is_remote_endpoint
{
public:
    explicit is_remote_endpoint (beast::ip::endpoint const& address)
        : m_endpoint (address)
        { }
    template <typename link>
    bool operator() (link const& link) const
    {
        return link.remote_endpoint() == m_endpoint;
    }
private:
    beast::ip::endpoint const m_endpoint;
};

}
}
}

#endif
