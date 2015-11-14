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
#include <ripple/app/data/databasecon.h>
#include <ripple/app/data/sqlitedatabase.h>

namespace ripple {

databasecon::databasecon (setup const& setup,
        std::string const& strname,
        const char* initstrings[],
        int initcount)
{
    auto const usetempfiles  // use temporary files or regular db files?
        = setup.standalone &&
          setup.startup != config::load &&
          setup.startup != config::load_file &&
          setup.startup != config::replay;
    boost::filesystem::path ppath = usetempfiles
        ? "" : (setup.datadir / strname);

    mdatabase = new sqlitedatabase (ppath.string ().c_str ());
    mdatabase->connect ();

    for (int i = 0; i < initcount; ++i)
        mdatabase->executesql (initstrings[i], true);
}

databasecon::~databasecon ()
{
    mdatabase->disconnect ();
    delete mdatabase;
}

databasecon::setup
setup_databasecon (config const& c)
{
    databasecon::setup setup;

    if (c.nodedatabase["online_delete"].isnotempty())
        setup.onlinedelete = c.nodedatabase["online_delete"].getintvalue();
    setup.startup = c.start_up;
    setup.standalone = c.run_standalone;
    setup.datadir = c.data_dir;

    return setup;
}

} // ripple
