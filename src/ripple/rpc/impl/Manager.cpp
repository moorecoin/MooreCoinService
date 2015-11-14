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
#include <ripple/rpc/manager.h>
#include <ripple/rpc/impl/doprint.h>
#include <beast/cxx14/memory.h>

namespace ripple {
namespace rpc {

class managerimp : public manager
{
public:
    typedef hash_map <std::string, handler_type> map;

    beast::journal m_journal;
    map m_map;

    managerimp (beast::journal journal)
        : m_journal (journal)
    {
    }

    void add (std::string const& method, handler_type&& handler)
    {
        m_map.emplace (std::piecewise_construct,
                       std::forward_as_tuple (method),
                       std::forward_as_tuple (std::move (handler)));
    }

    bool dispatch (request& req)
    {
        map::const_iterator const iter (m_map.find (req.method));
        if (iter == m_map.end())
            return false;
        iter->second (req);
        return true;
    }
};

//------------------------------------------------------------------------------

manager::~manager ()
{
}

std::unique_ptr <manager> make_manager (beast::journal journal)
{
    std::unique_ptr <manager> m (std::make_unique <managerimp> (journal));
    m->add <doprint> ("print");

    return m;
}

}
}
