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

#ifndef ripple_databasecon_h
#define ripple_databasecon_h

#include <ripple/app/data/database.h>
#include <ripple/core/config.h>
#include <boost/filesystem/path.hpp>
#include <mutex>
#include <string>

namespace ripple {

class database;

// vfalco note this looks like a pointless class. figure out
//         what purpose it is really trying to serve and do it better.
class databasecon
{
public:
    struct setup
    {
        bool onlinedelete = false;
        config::startuptype startup = config::normal;
        bool standalone = false;
        boost::filesystem::path datadir;
    };

    databasecon (setup const& setup,
            std::string const& name,
            const char* initstring[],
            int countinit);
    ~databasecon ();

    database* getdb ()
    {
        return mdatabase;
    }

    typedef std::recursive_mutex mutex;

    std::unique_lock<mutex> lock ()
    {
        return std::unique_lock<mutex>(mlock);
    }

    mutex& peekmutex()
    {
        return mlock;
    }

protected:
    databasecon () {}
    database* mdatabase;

private:
    mutex  mlock;
};

//------------------------------------------------------------------------------

databasecon::setup
setup_databasecon (config const& c);

} // ripple

#endif
