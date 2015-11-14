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

#ifndef ripple_nodestore_factory_h_included
#define ripple_nodestore_factory_h_included

#include <ripple/nodestore/backend.h>
#include <ripple/nodestore/scheduler.h>
#include <beast/utility/journal.h>

namespace ripple {
namespace nodestore {

/** base class for backend factories. */
class factory
{
public:
    virtual
    ~factory() = default;

    /** retrieve the name of this factory. */
    virtual
    std::string
    getname() const = 0;

    /** create an instance of this factory's backend.

        @param keybytes the fixed number of bytes per key.
        @param keyvalues a set of key/value configuration pairs.
        @param scheduler the scheduler to use for running tasks.
        @return a pointer to the backend object.
    */
    virtual
    std::unique_ptr <backend>
    createinstance (size_t keybytes, parameters const& parameters,
        scheduler& scheduler, beast::journal journal) = 0;
};

}
}

#endif
