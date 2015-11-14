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

#ifndef ripple_nodestore_manager_h_included
#define ripple_nodestore_manager_h_included

#include <ripple/nodestore/factory.h>
#include <ripple/nodestore/databaserotating.h>
#include <ripple/basics/basicconfig.h>
#include <beast/utility/journal.h>

namespace ripple {
namespace nodestore {

/** singleton for managing nodestore factories and back ends. */
class manager
{
public:
    /** returns the instance of the manager singleton. */
    static
    manager&
    instance();

    /** add a factory. */
    virtual
    void
    insert (factory& factory) = 0;

    /** remove a factory. */
    virtual
    void
    erase (factory& factory) = 0;

    /** return a pointer to the matching factory if it exists.
        @param  name the name to match, performed case-insensitive.
        @return `nullptr` if a match was not found.
    */
    //virtual factory* find (std::string const& name) const = 0;

    /** create a backend. */
    virtual
    std::unique_ptr <backend>
    make_backend (parameters const& parameters,
        scheduler& scheduler, beast::journal journal) = 0;

    /** construct a nodestore database.

        the parameters are key value pairs passed to the backend. the
        'type' key must exist, it defines the choice of backend. most
        backends also require a 'path' field.

        some choices for 'type' are:
            hyperleveldb, leveldbfactory, sqlite, mdb

        if the fastbackendparameter is omitted or empty, no ephemeral database
        is used. if the scheduler parameter is omited or unspecified, a
        synchronous scheduler is used which performs all tasks immediately on
        the caller's thread.

        @note if the database cannot be opened or created, an exception is thrown.

        @param name a diagnostic label for the database.
        @param scheduler the scheduler to use for performing asynchronous tasks.
        @param readthreads the number of async read threads to create
        @param backendparameters the parameter string for the persistent backend.
        @param fastbackendparameters [optional] the parameter string for the ephemeral backend.

        @return the opened database.
    */
    virtual
    std::unique_ptr <database>
    make_database (std::string const& name, scheduler& scheduler,
        beast::journal journal, int readthreads,
            parameters const& backendparameters,
                parameters fastbackendparameters = parameters()) = 0;

    virtual
    std::unique_ptr <databaserotating>
    make_databaserotating (std::string const& name,
        scheduler& scheduler, std::int32_t readthreads,
            std::shared_ptr <backend> writablebackend,
                std::shared_ptr <backend> archivebackend,
                std::unique_ptr <backend> fastbackend,
                    beast::journal journal) = 0;
};

//------------------------------------------------------------------------------

/** create a backend. */
std::unique_ptr <backend>
make_backend (section const& config,
    scheduler& scheduler, beast::journal journal);

}
}

#endif
