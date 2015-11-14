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

#ifndef ripple_app_application_h_included
#define ripple_app_application_h_included

#include <ripple/shamap/fullbelowcache.h>
#include <ripple/shamap/treenodecache.h>
#include <ripple/basics/taggedcache.h>
#include <beast/utility/propertystream.h>
#include <beast/cxx14/memory.h> // <memory>
#include <mutex>
    
namespace boost { namespace asio { class io_service; } }

namespace ripple {

namespace validators { class manager; }
namespace resource { class manager; }
namespace nodestore { class database; }
namespace rpc { class manager; }

// vfalco todo fix forward declares required for header dependency loops
class amendmenttable;
class collectormanager;
class ihashrouter;
class logs;
class loadfeetrack;
class localcredentials;
class uniquenodelist;
class jobqueue;
class inboundledgers;
class ledgermaster;
class loadmanager;
class networkops;
class orderbookdb;
class overlay;
class pathrequests;
class stledgerentry;
class transactionmaster;
class validations;

class databasecon;
class shamapstore;

using nodecache     = taggedcache <uint256, blob>;
using slecache      = taggedcache <uint256, stledgerentry>;

class application : public beast::propertystream::source
{
public:
    /* vfalco note

        the master lock protects:

        - the open ledger
        - server global state
            * what the last closed ledger is
            * state of the consensus engine

        other things
    */
    typedef std::recursive_mutex locktype;
    typedef std::unique_lock <locktype> scopedlocktype;
    typedef std::unique_ptr <scopedlocktype> scopedlock;

    virtual locktype& getmasterlock () = 0;

    scopedlock masterlock()
    {
        return std::make_unique<scopedlocktype> (getmasterlock());
    }

public:
    application ();

    virtual ~application () = default;

    virtual boost::asio::io_service& getioservice () = 0;
    virtual collectormanager&       getcollectormanager () = 0;
    virtual fullbelowcache&         getfullbelowcache () = 0;
    virtual jobqueue&               getjobqueue () = 0;
    virtual rpc::manager&           getrpcmanager () = 0;
    virtual nodecache&              gettempnodecache () = 0;
    virtual treenodecache&          gettreenodecache () = 0;
    virtual slecache&               getslecache () = 0;
    virtual validators::manager&    getvalidators () = 0;
    virtual amendmenttable&         getamendmenttable() = 0;
    virtual ihashrouter&            gethashrouter () = 0;
    virtual loadfeetrack&           getfeetrack () = 0;
    virtual loadmanager&            getloadmanager () = 0;
    virtual overlay&                overlay () = 0;
    virtual uniquenodelist&         getunl () = 0;
    virtual validations&            getvalidations () = 0;
    virtual nodestore::database&    getnodestore () = 0;
    virtual inboundledgers&         getinboundledgers () = 0;
    virtual ledgermaster&           getledgermaster () = 0;
    virtual networkops&             getops () = 0;
    virtual orderbookdb&            getorderbookdb () = 0;
    virtual transactionmaster&      getmastertransaction () = 0;
    virtual localcredentials&       getlocalcredentials () = 0;
    virtual resource::manager&      getresourcemanager () = 0;
    virtual pathrequests&           getpathrequests () = 0;
    virtual shamapstore&            getshamapstore () = 0;

    virtual databasecon& getrpcdb () = 0;
    virtual databasecon& gettxndb () = 0;
    virtual databasecon& getledgerdb () = 0;

    virtual std::chrono::milliseconds getiolatency () = 0;

    /** retrieve the "wallet database"

        it looks like this is used to store the unique node list.
    */
    // vfalco todo rename, document this
    //        note this will be replaced by class validators
    //
    virtual databasecon& getwalletdb () = 0;

    virtual bool getsystemtimeoffset (int& offset) = 0;
    virtual bool isshutdown () = 0;
    virtual bool running () = 0;
    virtual void setup () = 0;
    virtual void run () = 0;
    virtual void signalstop () = 0;
};

/** create an instance of the application object.
    as long as there are legacy calls to getapp it is not safe
    to create more than one application object at a time.
*/
std::unique_ptr <application>
make_application(logs& logs);

// vfalco deprecated
//
//        please do not write new code that calls getapp(). instead,
//        use dependency injection to construct your class with a
//        reference to the desired interface (application in this case).
//        or better yet, instead of relying on the entire application
//        object, construct with just the interfaces that you need.
//
//        when working in existing code, try to clean it up by rewriting
//        calls to getapp to use a data member instead, and inject the
//        needed interfaces in the constructor.
//
//        http://en.wikipedia.org/wiki/dependency_injection
//
extern application& getapp ();

}

#endif
