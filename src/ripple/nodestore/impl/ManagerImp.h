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

#ifndef ripple_nodestore_managerimp_h_included
#define ripple_nodestore_managerimp_h_included

#include <ripple/nodestore/manager.h>
#include <mutex>
#include <vector>

namespace ripple {
namespace nodestore {

class managerimp : public manager
{
private:
    std::mutex mutex_;
    std::vector<factory*> list_;

public:
    static
    managerimp&
    instance();

    static
    void
    missing_backend();

    managerimp();

    ~managerimp();

    factory*
    find (std::string const& name);

    void
    insert (factory& factory) override;

    void
    erase (factory& factory) override;

    std::unique_ptr <backend>
    make_backend (
        parameters const& parameters,
        scheduler& scheduler,
        beast::journal journal) override;

    std::unique_ptr <database>
    make_database (
        std::string const& name,
        scheduler& scheduler,
        beast::journal journal,
        int readthreads,
        parameters const& backendparameters,
        parameters fastbackendparameters) override;

    std::unique_ptr <databaserotating>
    make_databaserotating (
        std::string const& name,
        scheduler& scheduler,
        std::int32_t readthreads,
        std::shared_ptr <backend> writablebackend,
        std::shared_ptr <backend> archivebackend,
        std::unique_ptr <backend> fastbackend,
        beast::journal journal) override;
};

}
}

#endif
