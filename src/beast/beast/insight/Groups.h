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

#ifndef beast_insight_groups_h_included
#define beast_insight_groups_h_included

#include <beast/insight/collector.h>
#include <beast/insight/group.h>

#include <memory>
#include <string>

namespace beast {
namespace insight {

/** a container for managing a set of metric groups. */
class groups
{
public:
    virtual ~groups() = 0;

    /** find or create a new collector with a given name. */
    /** @{ */
    virtual
    group::ptr const&
    get (std::string const& name) = 0;

    group::ptr const&
    operator[] (std::string const& name)
    {
        return get (name);
    }
    /** @} */
};

/** create a group container that uses the specified collector. */
std::unique_ptr <groups> make_groups (collector::ptr const& collector);

}
}

#endif
