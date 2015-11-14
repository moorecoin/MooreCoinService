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

#include <unordered_map>
#include <beast/hash/uhash.h>
#include <beast/cxx14/memory.h> // <memory>

namespace beast {
namespace insight {

namespace detail {

class groupimp
    : public std::enable_shared_from_this <groupimp>
    , public group
{
public:
    typedef std::vector <std::shared_ptr <baseimpl>> items;

    std::string const m_name;
    collector::ptr m_collector;

    groupimp (std::string const& name_,
        collector::ptr const& collector)
        : m_name (name_)
        , m_collector (collector)
    {
    }

    ~groupimp ()
    {
    }

    std::string const& name () const
    {
        return m_name;
    }

    std::string make_name (std::string const& name)
    {
        return m_name + "." + name;
    }

    hook make_hook (hookimpl::handlertype const& handler)
    {
        return m_collector->make_hook (handler);
    }

    counter make_counter (std::string const& name)
    {
        return m_collector->make_counter (make_name (name));
    }

    event make_event (std::string const& name)
    {
        return m_collector->make_event (make_name (name));
    }

    gauge make_gauge (std::string const& name)
    {
        return m_collector->make_gauge (make_name (name));
    }

    meter make_meter (std::string const& name)
    {
        return m_collector->make_meter (make_name (name));
    }

private:
    groupimp& operator= (groupimp const&);
};

//------------------------------------------------------------------------------

class groupsimp : public groups
{
public:
    typedef std::unordered_map <std::string, std::shared_ptr <group>, uhash <>> items;

    collector::ptr m_collector;
    items m_items;

    groupsimp (collector::ptr const& collector)
        : m_collector (collector)
    {
    }

    ~groupsimp ()
    {
    }

    group::ptr const& get (std::string const& name)
    {
        std::pair <items::iterator, bool> result (
            m_items.emplace (name, group::ptr ()));
        group::ptr& group (result.first->second);
        if (result.second)
            group = std::make_shared <groupimp> (name, m_collector);
        return group;
    }
};

}

//------------------------------------------------------------------------------

groups::~groups ()
{
}

std::unique_ptr <groups> make_groups (collector::ptr const& collector)
{
    return std::make_unique <detail::groupsimp> (collector);
}

}
}
