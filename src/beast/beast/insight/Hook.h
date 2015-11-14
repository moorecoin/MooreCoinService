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

#ifndef beast_insight_hook_h_included
#define beast_insight_hook_h_included

#include <beast/insight/base.h>
#include <beast/insight/hookimpl.h>

#include <memory>

namespace beast {
namespace insight {
    
/** a reference to a handler for performing polled collection. */
class hook : public base
{
public:
    /** create a null hook.
        a null hook has no associated handler.
    */
    hook ()
        { }

    /** create a hook referencing the specified implementation.
        normally this won't be called directly. instead, call the appropriate
        factory function in the collector interface.
        @see collector.
    */
    explicit hook (std::shared_ptr <hookimpl> const& impl)
        : m_impl (impl)
        { }

    std::shared_ptr <hookimpl> const& impl () const
    {
        return m_impl;
    }

private:
    std::shared_ptr <hookimpl> m_impl;
};

}
}

#endif
