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

#ifndef ripple_peerfinder_sim_graphalgorithms_h_included
#define ripple_peerfinder_sim_graphalgorithms_h_included

namespace ripple {
namespace peerfinder {
namespace sim {

template <typename vertex>
struct vertextraits;

/** call a function for each vertex in a connected graph.
    function will be called with this signature:
        void (vertex&, std::size_t diameter);
*/

template <typename vertex, typename function>
void breadth_first_traverse (vertex& start, function f)
{
    typedef vertextraits <vertex>  traits;
    typedef typename traits::edges edges;
    typedef typename traits::edge  edge;

    typedef std::pair <vertex*, int> probe;
    typedef std::deque <probe> work;
    typedef std::set <vertex*> visited;
    work work;
    visited visited;
    work.emplace_back (&start, 0);
    int diameter (0);
    while (! work.empty ())
    {
        probe const p (work.front());
        work.pop_front ();
        if (visited.find (p.first) != visited.end ())
            continue;
        diameter = std::max (p.second, diameter);
        visited.insert (p.first);
        for (typename edges::iterator iter (
            traits::edges (*p.first).begin());
                iter != traits::edges (*p.first).end(); ++iter)
        {
            vertex* v (traits::vertex (*iter));
            if (visited.find (v) != visited.end())
                continue;
            if (! iter->closed())
                work.emplace_back (v, p.second + 1);
        }
        f (*p.first, diameter);
    }
}


}
}
}

#endif
