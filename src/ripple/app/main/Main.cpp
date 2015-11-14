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
#include <ripple/basics/log.h>
#include <ripple/app/main/application.h>
#include <ripple/basics/checklibraryversions.h>
#include <ripple/basics/stringutilities.h>
#include <ripple/basics/sustain.h>
#include <ripple/basics/threadname.h>
#include <ripple/core/config.h>
#include <ripple/core/configsections.h>
#include <ripple/crypto/randomnumbers.h>
#include <ripple/json/to_string.h>
#include <ripple/net/rpccall.h>
#include <ripple/resource/fees.h>
#include <ripple/rpc/rpchandler.h>
#include <ripple/server/role.h>
#include <ripple/protocol/buildinfo.h>
#include <beast/chrono/basic_seconds_clock.h>
#include <beast/unit_test.h>
#include <beast/utility/debug.h>
#include <beast/streams/debug_ostream.h>
#include <beast/module/core/system/systemstats.h>
#include <google/protobuf/stubs/common.h>
#include <boost/program_options.hpp>
#include <cstdlib>
#include <thread>

#if defined(beast_linux) || defined(beast_mac) || defined(beast_bsd)
#include <sys/resource.h>
#endif

#ifdef use_sha512_asm
#include <beast/crypto/sha512asm.h>
#endif //use_sha512_asm

namespace po = boost::program_options;

namespace ripple {

void setupserver ()
{
#ifdef rlimit_nofile
    struct rlimit rl;
    if (getrlimit(rlimit_nofile, &rl) == 0)
    {
         if (rl.rlim_cur != rl.rlim_max)
         {
             rl.rlim_cur = rl.rlim_max;
             setrlimit(rlimit_nofile, &rl);
         }
    }
#endif

    getapp().setup ();
}

void startserver ()
{
    //
    // execute start up rpc commands.
    //
    if (getconfig ().rpc_startup.isarray ())
    {
        for (int i = 0; i != getconfig ().rpc_startup.size (); ++i)
        {
            json::value const& jvcommand    = getconfig ().rpc_startup[i];

            if (!getconfig ().quiet)
                std::cerr << "startup rpc: " << jvcommand << std::endl;

            resource::charge loadtype = resource::feereferencerpc;
            rpc::context context {
                jvcommand, loadtype, getapp().getops (), role::admin};

            json::value jvresult;
            rpc::docommand (context, jvresult);

            if (!getconfig ().quiet)
                std::cerr << "result: " << jvresult << std::endl;
        }
    }

    getapp().run ();                 // blocks till we get a stop rpc.
}

void printhelp (const po::options_description& desc)
{
    std::cerr
        << systemname () << "d [options] <command> <params>\n"
        << desc << std::endl
        << "commands: \n"
           "     account_info <account>|<seed>|<pass_phrase>|<key> [<ledger>] [strict]\n"
           "     account_lines <account> <account>|\"\" [<ledger>]\n"
           "     account_offers <account>|<account_public_key> [<ledger>]\n"
           "     account_tx accountid [ledger_min [ledger_max [limit [offset]]]] [binary] [count] [descending]\n"
           "     book_offers <taker_pays> <taker_gets> [<taker [<ledger> [<limit> [<proof> [<marker>]]]]]\n"
           "     can_delete [<ledgerid>|<ledgerhash>|now|always|never]\n"
           "     connect <ip> [<port>]\n"
           "     consensus_info\n"
           "     get_counts\n"
           "     json <method> <json>\n"
           "     ledger [<id>|current|closed|validated] [full]\n"
           "     ledger_accept\n"
           "     ledger_closed\n"
           "     ledger_current\n"
           "     ledger_request <ledger>\n"
           "     ledger_header <ledger>\n"
           "     logrotate \n"
           "     peers\n"
           "     proof_create [<difficulty>] [<secret>]\n"
           "     proof_solve <token>\n"
           "     proof_verify <token> <solution> [<difficulty>] [<secret>]\n"
           "     random\n"
           "     ripple ...\n"
           "     ripple_path_find <json> [<ledger>]\n"
           "     server_info\n"
           "     stop\n"
           "     tx <id>\n"
           "     unl_add <domain>|<public> [<comment>]\n"
           "     unl_delete <domain>|<public_key>\n"
           "     unl_list\n"
           "     unl_load\n"
           "     unl_network\n"
           "     unl_reset\n"
           "     validation_create [<seed>|<pass_phrase>|<key>]\n"
           "     validation_seed [<seed>|<pass_phrase>|<key>]\n"
           "     wallet_accounts <seed>\n"
           "     wallet_add <regular_seed> <paying_account> <master_seed> [<initial_funds>] [<account_annotation>]\n"
           "     wallet_claim <master_seed> <regular_seed> [<source_tag>] [<account_annotation>]\n"
           "     wallet_propose [<passphrase>]\n"
           "     wallet_seed [<seed>|<passphrase>|<passkey>]\n";
}

//------------------------------------------------------------------------------

static
void
setupconfigforunittests (config* config)
{
    config->nodedatabase = parsedelimitedkeyvaluestring ("type=memory|path=main");
    config->ephemeralnodedatabase = beast::stringpairarray ();
    config->importnodedatabase = beast::stringpairarray ();
}

static int runshutdowntests ()
{
    // shutdown tests can not be part of the normal unit tests in 'rununittests'
    // because it needs to create and destroy an application object.
    int const numshutdowniterations = 20;
    // give it enough time to sync and run a bit while synced.
    std::chrono::seconds const serveruptimeperiteration (4 * 60);
    for (int i = 0; i < numshutdowniterations; ++i)
    {
        std::cerr << "\n\nstarting server. iteration: " << i << "\n"
                  << std::endl;
        std::unique_ptr<application> app (make_application (deprecatedlogs()));
        auto shutdownapp = [&app](std::chrono::seconds sleeptime, int iteration)
        {
            std::this_thread::sleep_for (sleeptime);
            std::cerr << "\n\nstopping server. iteration: " << iteration << "\n"
                      << std::endl;
            app->signalstop();
        };
        std::thread shutdownthread (shutdownapp, serveruptimeperiteration, i);
        setupserver();
        startserver();
        shutdownthread.join();
    }
    return exit_success;
}

static int rununittests (std::string const& pattern,
                         std::string const& argument)
{
    // config needs to be set up before creating application
    setupconfigforunittests (&getconfig ());
    // vfalco todo remove dependence on constructing application object
    std::unique_ptr <application> app (make_application (deprecatedlogs()));
    using namespace beast::unit_test;
    beast::debug_ostream stream;
    reporter r (stream);
    r.arg(argument);
    bool const failed (r.run_each_if (
        global_suites(), match_auto (pattern)));
    if (failed)
        return exit_failure;
    return exit_success;
}

//------------------------------------------------------------------------------

int run (int argc, char** argv)
{
    // make sure that we have the right openssl and boost libraries.
    version::checklibraryversions();

#ifdef use_sha512_asm
    if (beast::systemstats::hasavx2())
    {
        init_sha512asm_avx2();
    }
    else if (beast::systemstats::hasavx())
    {
        init_sha512asm_avx();
    }
    else if (beast::systemstats::hassse4())
    {
        init_sha512asm_sse4();
    }
    else
    {
        assert(false);
    }
#endif //#ifdef use_sha512_asm

    using namespace std;

    setcallingthreadname ("main");
    int iresult = 0;
    po::variables_map vm;

    std::string importtext;
    {
        importtext += "import an existing node database (specified in the [";
        importtext += configsection::importnodedatabase ();
        importtext += "] configuration file section) into the current ";
        importtext += "node database (specified in the [";
        importtext += configsection::nodedatabase ();
        importtext += "] configuration file section).";
    }

    // vfalco todo replace boost program options with something from beast.
    //
    // set up option parsing.
    //
    po::options_description desc ("general options");
    desc.add_options ()
    ("help,h", "display this message.")
    ("conf", po::value<std::string> (), "specify the configuration file.")
    ("rpc", "perform rpc command (default).")
    ("rpc_ip", po::value <std::string> (), "specify the ip address for rpc command. format: <ip-address>[':'<port-number>]")
    ("rpc_port", po::value <int> (), "specify the port number for rpc command.")
    ("standalone,a", "run with no peers.")
    ("shutdowntest", po::value <std::string> ()->implicit_value (""), "perform shutdown tests.")
    ("unittest,u", po::value <std::string> ()->implicit_value (""), "perform unit tests.")
    ("unittest-arg", po::value <std::string> ()->implicit_value (""), "supplies argument to unit tests.")
    ("parameters", po::value< vector<string> > (), "specify comma separated parameters.")
    ("quiet,q", "reduce diagnotics.")
    ("quorum", po::value <int> (), "set the validation quorum.")
    ("verbose,v", "verbose logging.")
    ("load", "load the current ledger from the local db.")
    ("replay","replay a ledger close.")
    ("ledger", po::value<std::string> (), "load the specified ledger and start from .")
    ("ledgerfile", po::value<std::string> (), "load the specified ledger file.")
    ("start", "start from a fresh ledger.")
    ("net", "get the initial ledger from the network.")
    ("fg", "run in the foreground.")
    ("import", importtext.c_str ())
    ("version", "display the build version.")
    ;

    // interpret positional arguments as --parameters.
    po::positional_options_description p;
    p.add ("parameters", -1);

    // seed the rng early
    add_entropy ();

    if (!iresult)
    {
        // parse options, if no error.
        try
        {
            po::store (po::command_line_parser (argc, argv)
                .options (desc)               // parse options.
                .positional (p)               // remainder as --parameters.
                .run (),
                vm);
            po::notify (vm);                  // invoke option notify functions.
        }
        catch (...)
        {
            iresult = 1;
        }
    }

    if (!iresult && vm.count ("help"))
    {
        iresult = 1;
    }

    if (vm.count ("version"))
    {
        std::cout << "moorecoind version " <<
            buildinfo::getversionstring () << std::endl;
        return 0;
    }

    // use a watchdog process unless we're invoking a stand alone type of mode
    //
    if (havesustain ()
        && !iresult
        && !vm.count ("parameters")
        && !vm.count ("fg")
        && !vm.count ("standalone")
        && !vm.count ("shutdowntest")
        && !vm.count ("unittest"))
    {
        std::string logme = dosustain (getconfig ().getdebuglogfile ().string());

        if (!logme.empty ())
            std::cerr << logme;
    }

    if (vm.count ("quiet"))
    {
        deprecatedlogs().severity(beast::journal::kfatal);
    }
    else if (vm.count ("verbose"))
    {
        deprecatedlogs().severity(beast::journal::ktrace);
    }
    else
    {
        deprecatedlogs().severity(beast::journal::kinfo);
    }

    // run the unit tests if requested.
    // the unit tests will exit the application with an appropriate return code.
    //
    if (vm.count ("unittest"))
    {
        std::string argument;

        if (vm.count("unittest-arg"))
            argument = vm["unittest-arg"].as<std::string>();

        return rununittests(vm["unittest"].as<std::string>(), argument);
    }

    if (!iresult)
    {
        auto configfile = vm.count ("conf") ?
                vm["conf"].as<std::string> () : std::string();

        // config file, quiet flag.
        getconfig ().setup (configfile, bool (vm.count ("quiet")));

        if (vm.count ("standalone"))
        {
            getconfig ().run_standalone = true;
            getconfig ().ledger_history = 0;
            getconfig ().ledger_history_index = 0;
        }
    }

    if (vm.count ("start")) getconfig ().start_up = config::fresh;

    // handle a one-time import option
    //
    if (vm.count ("import"))
    {
        getconfig ().doimport = true;
    }

    if (vm.count ("ledger"))
    {
        getconfig ().start_ledger = vm["ledger"].as<std::string> ();
        if (vm.count("replay"))
            getconfig ().start_up = config::replay;
        else
            getconfig ().start_up = config::load;
    }
    else if (vm.count ("ledgerfile"))
    {
        getconfig ().start_ledger = vm["ledgerfile"].as<std::string> ();
        getconfig ().start_up = config::load_file;
    }
    else if (vm.count ("load"))
    {
        getconfig ().start_up = config::load;
    }
    else if (vm.count ("net"))
    {
        getconfig ().start_up = config::network;

        if (getconfig ().validation_quorum < 2)
            getconfig ().validation_quorum = 2;
    }

    if (iresult == 0)
    {
        // these overrides must happen after the config file is loaded.

        // override the rpc destination ip address
        //
        if (vm.count ("rpc_ip"))
        {
            // vfalco todo this is currently broken
            //getconfig ().setrpcipandoptionalport (vm ["rpc_ip"].as <std::string> ());
            //getconfig().overwrite("rpc", "ip", vm["rpc_ip"].as<std::string>());
        }

        // override the rpc destination port number
        //
        if (vm.count ("rpc_port"))
        {
            // vfalco todo this should be a short.
            // vfalco todo this is currently broken
            //getconfig ().setrpcport (vm ["rpc_port"].as <int> ());
            //getconfig().overwrite("rpc", "port", vm["rpc_port"].as<std::string>());
        }

        if (vm.count ("quorum"))
        {
            getconfig ().validation_quorum = vm["quorum"].as <int> ();

            if (getconfig ().validation_quorum < 0)
                iresult = 1;
        }
    }

    if (vm.count ("shutdowntest"))
    {
        return runshutdowntests ();
    }

    if (iresult == 0)
    {
        if (!vm.count ("parameters"))
        {
            // no arguments. run server.
            std::unique_ptr <application> app (make_application (deprecatedlogs()));
            setupserver ();
            startserver ();
        }
        else
        {
            // have a rpc command.
            setcallingthreadname ("rpc");
            std::vector<std::string> vcmd   = vm["parameters"].as<std::vector<std::string> > ();

            iresult = rpccall::fromcommandline (vcmd);
        }
    }

    if (1 == iresult && !vm.count ("quiet"))
        printhelp (desc);

    return iresult;
}

extern int run (int argc, char** argv);

} // ripple

// must be outside the namespace for obvious reasons
//
int main (int argc, char** argv)
{
    // workaround for boost.context / boost.coroutine
    // https://svn.boost.org/trac/boost/ticket/10657
    (void)beast::time::currenttimemillis();

#if defined(__gnuc__) && !defined(__clang__)
    auto constexpr gccver = (__gnuc__ * 100 * 100) +
                            (__gnuc_minor__ * 100) +
                            __gnuc_patchlevel__;

    static_assert (gccver >= 40801,
        "gcc version 4.8.1 or later is required to compile moorecoind.");
#endif

    static_assert (boost_version >= 105500,
        "boost version 1.55 or later is required to compile moorecoind");

    //
    // these debug heap calls do nothing in release or non visual studio builds.
    //

    // checks the heap at every allocation and deallocation (slow).
    //
    //beast::debug::setalwayscheckheap (false);

    // keeps freed memory blocks and fills them with a guard value.
    //
    //beast::debug::setheapdelayedfree (false);

    // at exit, reports all memory blocks which have not been freed.
    //
#if ripple_dump_leaks_on_exit
    beast::debug::setheapreportleaks (true);
#else
    beast::debug::setheapreportleaks (false);
#endif

    atexit(&google::protobuf::shutdownprotobuflibrary);

    auto const result (ripple::run (argc, argv));

    beast::basic_seconds_clock_main_hook();

    return result;
}
