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
#include <ripple/nodestore/tests/base.test.h>
#include <ripple/nodestore/dummyscheduler.h>
#include <ripple/nodestore/manager.h>
#include <ripple/basics/basicconfig.h>
#include <ripple/unity/rocksdb.h>
#include <beast/module/core/diagnostic/unittestutilities.h>
#include <beast/random/xor_shift_engine.h>
#include <beast/unit_test/suite.h>
#include <beast/unit_test/thread.h>
#include <boost/algorithm/string.hpp>
#include <atomic>
#include <chrono>
#include <iterator>
#include <limits>
#include <map>
#include <random>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <beast/cxx14/type_traits.h> // <type_traits>
#include <utility>

#ifndef nodestore_timing_do_verify
#define nodestore_timing_do_verify 0
#endif

namespace ripple {
namespace nodestore {

// fill memory with random bits
template <class generator>
static
void
rngcpy (void* buffer, std::size_t bytes, generator& g)
{
    using result_type = typename generator::result_type;
    while (bytes >= sizeof(result_type))
    {
        auto const v = g();
        memcpy(buffer, &v, sizeof(v));
        buffer = reinterpret_cast<std::uint8_t*>(buffer) + sizeof(v);
        bytes -= sizeof(v);
    }

    if (bytes > 0)
    {
        auto const v = g();
        memcpy(buffer, &v, bytes);
    }
}

// instance of node factory produces a deterministic sequence
// of random nodeobjects within the given
class sequence
{
private:
    enum
    {
        minledger = 1,
        maxledger = 1000000,
        minsize = 250,
        maxsize = 1250
    };

    beast::xor_shift_engine gen_;
    std::uint8_t prefix_;
    std::uniform_int_distribution<std::uint32_t> d_type_;
    std::uniform_int_distribution<std::uint32_t> d_size_;

public:
    explicit
    sequence(std::uint8_t prefix)
        : prefix_ (prefix)
        , d_type_ (hotledger, hottransaction_node)
        , d_size_ (minsize, maxsize)
    {
    }

    // returns the n-th key
    uint256
    key (std::size_t n)
    {
        gen_.seed(n+1);
        uint256 result;
        rngcpy (&*result.begin(), result.size(), gen_);
        return result;
    }

    // returns the n-th complete nodeobject
    nodeobject::ptr
    obj (std::size_t n)
    {
        gen_.seed(n+1);
        uint256 key;
        auto const data = 
            static_cast<std::uint8_t*>(&*key.begin());
        *data = prefix_;
        rngcpy (data + 1, key.size() - 1, gen_);
        blob value(d_size_(gen_));
        rngcpy (&value[0], value.size(), gen_);
        return nodeobject::createobject (
            static_cast<nodeobjecttype>(d_type_(gen_)),
                std::move(value), key);
    }

    // returns a batch of nodeobjects starting at n
    void
    batch (std::size_t n, batch& b, std::size_t size)
    {
        b.clear();
        b.reserve (size);
        while(size--)
            b.emplace_back(obj(n++));
    }
};

//----------------------------------------------------------------------------------

class timing_test : public beast::unit_test::suite
{
public:
    enum
    {
        // percent of fetches for missing nodes
        missingnodepercent = 20
    };

    std::size_t const default_repeat = 3;
#ifndef ndebug
    std::size_t const default_items = 10000;
#else
    std::size_t const default_items = 100000; // release
#endif

    using clock_type = std::chrono::steady_clock;
    using duration_type = std::chrono::milliseconds;

    struct params
    {
        std::size_t items;
        std::size_t threads;
    };

    static
    std::string
    to_string (section const& config)
    {
        std::string s;
        for (auto iter = config.begin(); iter != config.end(); ++iter)
            s += (iter != config.begin() ? "," : "") +
                iter->first + "=" + iter->second;
        return s;
    }

    static
    std::string
    to_string (duration_type const& d)
    {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(3) <<
            (d.count() / 1000.) << "s";
        return ss.str();
    }

    static
    section
    parse (std::string s)
    {
        section section;
        std::vector <std::string> v;
        boost::split (v, s,
            boost::algorithm::is_any_of (","));
        section.append(v);
        return section;
    }

    //--------------------------------------------------------------------------

    // workaround for gcc's parameter pack expansion in lambdas
    // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=47226
    template <class body>
    class parallel_for_lambda
    {
    private:
        std::size_t const n_;
        std::atomic<std::size_t>& c_;

    public:
        parallel_for_lambda (std::size_t n,
                std::atomic<std::size_t>& c)
            : n_ (n)
            , c_ (c)
        {
        }

        template <class... args>
        void
        operator()(args&&... args)
        {
            body body(args...);
            for(;;)
            {
                auto const i = c_++;
                if (i >= n_)
                    break;
                body (i);
            }
        }
    };

    /*  execute parallel-for loop.

        constructs `number_of_threads` instances of `body`
        with `args...` parameters and runs them on individual threads
        with unique loop indexes in the range [0, n).
    */
    template <class body, class... args>
    void
    parallel_for (std::size_t const n,
        std::size_t number_of_threads, args const&... args)
    {
        std::atomic<std::size_t> c(0);
        std::vector<beast::unit_test::thread> t;
        t.reserve(number_of_threads);
        for (std::size_t id = 0; id < number_of_threads; ++id)
            t.emplace_back(*this,
                parallel_for_lambda<body>(n, c),
                    args...);
        for (auto& _ : t)
            _.join();
    }

    template <class body, class... args>
    void
    parallel_for_id (std::size_t const n,
        std::size_t number_of_threads, args const&... args)
    {
        std::atomic<std::size_t> c(0);
        std::vector<beast::unit_test::thread> t;
        t.reserve(number_of_threads);
        for (std::size_t id = 0; id < number_of_threads; ++id)
            t.emplace_back(*this,
                parallel_for_lambda<body>(n, c),
                    id, args...);
        for (auto& _ : t)
            _.join();
    }

    //--------------------------------------------------------------------------

    // insert only
    void
    do_insert (section const& config, params const& params)
    {
        beast::journal journal;
        dummyscheduler scheduler;
        auto backend = make_backend (config, scheduler, journal);
        expect (backend != nullptr);

        class body
        {
        private:
            suite& suite_;
            backend& backend_;
            sequence seq_;

        public:
            explicit
            body (suite& s, backend& backend)
                : suite_ (s)
                , backend_ (backend)
                , seq_(1)
            {
            }

            void
            operator()(std::size_t i)
            {
                try
                {
                    backend_.store(seq_.obj(i));
                }
                catch(std::exception const& e)
                {
                    suite_.fail(e.what());
                }
            }
        };

        try
        {
            parallel_for<body>(params.items,
                params.threads, std::ref(*this), std::ref(*backend));
        }
        catch(...)
        {
        #if nodestore_timing_do_verify
            backend->verify();
        #endif
            throw;
        }
        backend->close();
    }

    // fetch existing keys
    void
    do_fetch (section const& config, params const& params)
    {
        beast::journal journal;
        dummyscheduler scheduler;
        auto backend = make_backend (config, scheduler, journal);
        expect (backend != nullptr);

        class body
        {
        private:
            suite& suite_;
            backend& backend_;
            sequence seq1_;
            beast::xor_shift_engine gen_;
            std::uniform_int_distribution<std::size_t> dist_;

        public:
            body (std::size_t id, suite& s,
                    params const& params, backend& backend)
                : suite_(s)
                , backend_ (backend)
                , seq1_ (1)
                , gen_ (id + 1)
                , dist_ (0, params.items - 1)
            {
            }

            void
            operator()(std::size_t i)
            {
                try
                {
                    nodeobject::ptr obj;
                    nodeobject::ptr result;
                    obj = seq1_.obj(dist_(gen_));
                    backend_.fetch(obj->gethash().data(), &result);
                    suite_.expect (result && result->iscloneof(obj));
                }
                catch(std::exception const& e)
                {
                    suite_.fail(e.what());
                }
            }
        };
        try
        {
            parallel_for_id<body>(params.items, params.threads,
                std::ref(*this), std::ref(params), std::ref(*backend));
        }
        catch(...)
        {
        #if nodestore_timing_do_verify
            backend->verify();
        #endif
            throw;
        }
        backend->close();
    }

    // perform lookups of non-existent keys
    void
    do_missing (section const& config, params const& params)
    {
        beast::journal journal;
        dummyscheduler scheduler;
        auto backend = make_backend (config, scheduler, journal);
        expect (backend != nullptr);

        class body
        {
        private:
            suite& suite_;
            //params const& params_;
            backend& backend_;
            sequence seq2_;
            beast::xor_shift_engine gen_;
            std::uniform_int_distribution<std::size_t> dist_;

        public:
            body (std::size_t id, suite& s,
                    params const& params, backend& backend)
                : suite_ (s)
                //, params_ (params)
                , backend_ (backend)
                , seq2_ (2)
                , gen_ (id + 1)
                , dist_ (0, params.items - 1)
            {
            }

            void
            operator()(std::size_t i)
            {
                try
                {
                    auto const key = seq2_.key(i);
                    nodeobject::ptr result;
                    backend_.fetch(key.data(), &result);
                    suite_.expect (! result);
                }
                catch(std::exception const& e)
                {
                    suite_.fail(e.what());
                }
            }
        };

        try
        {
            parallel_for_id<body>(params.items, params.threads,
                std::ref(*this), std::ref(params), std::ref(*backend));
        }
        catch(...)
        {
        #if nodestore_timing_do_verify
            backend->verify();
        #endif
            throw;
        }
        backend->close();
    }

    // fetch with present and missing keys
    void
    do_mixed (section const& config, params const& params)
    {
        beast::journal journal;
        dummyscheduler scheduler;
        auto backend = make_backend (config, scheduler, journal);
        expect (backend != nullptr);

        class body
        {
        private:
            suite& suite_;
            //params const& params_;
            backend& backend_;
            sequence seq1_;
            sequence seq2_;
            beast::xor_shift_engine gen_;
            std::uniform_int_distribution<std::uint32_t> rand_;
            std::uniform_int_distribution<std::size_t> dist_;

        public:
            body (std::size_t id, suite& s,
                    params const& params, backend& backend)
                : suite_ (s)
                //, params_ (params)
                , backend_ (backend)
                , seq1_ (1)
                , seq2_ (2)
                , gen_ (id + 1)
                , rand_ (0, 99)
                , dist_ (0, params.items - 1)
            {
            }

            void
            operator()(std::size_t i)
            {
                try
                {
                    if (rand_(gen_) < missingnodepercent)
                    {
                        auto const key = seq2_.key(dist_(gen_));
                        nodeobject::ptr result;
                        backend_.fetch(key.data(), &result);
                        suite_.expect (! result);
                    }
                    else
                    {
                        nodeobject::ptr obj;
                        nodeobject::ptr result;
                        obj = seq1_.obj(dist_(gen_));
                        backend_.fetch(obj->gethash().data(), &result);
                        suite_.expect (result && result->iscloneof(obj));
                    }
                }
                catch(std::exception const& e)
                {
                    suite_.fail(e.what());
                }
            }
        };
        
        try
        {
            parallel_for_id<body>(params.items, params.threads,
                std::ref(*this), std::ref(params), std::ref(*backend));
        }
        catch(...)
        {
        #if nodestore_timing_do_verify
            backend->verify();
        #endif
            throw;
        }
        backend->close();
    }

    // simulate a rippled workload:
    // each thread randomly:
    //      inserts a new key
    //      fetches an old key
    //      fetches recent, possibly non existent data
    void
    do_work (section const& config, params const& params)
    {
        beast::journal journal;
        dummyscheduler scheduler;
        auto backend = make_backend (config, scheduler, journal);
        backend->setdeletepath();
        expect (backend != nullptr);

        class body
        {
        private:
            suite& suite_;
            params const& params_;
            backend& backend_;
            sequence seq1_;
            beast::xor_shift_engine gen_;
            std::uniform_int_distribution<std::uint32_t> rand_;
            std::uniform_int_distribution<std::size_t> recent_;
            std::uniform_int_distribution<std::size_t> older_;

        public:
            body (std::size_t id, suite& s,
                    params const& params, backend& backend)
                : suite_ (s)
                , params_ (params)
                , backend_ (backend)
                , seq1_ (1)
                , gen_ (id + 1)
                , rand_ (0, 99)
                , recent_ (params.items, params.items * 2 - 1)
                , older_ (0, params.items - 1)
            {
            }

            void
            operator()(std::size_t i)
            {
                try
                {
                    if (rand_(gen_) < 200)
                    {
                        // historical lookup
                        nodeobject::ptr obj;
                        nodeobject::ptr result;
                        auto const j = older_(gen_);
                        obj = seq1_.obj(j);
                        nodeobject::ptr result1;
                        backend_.fetch(obj->gethash().data(), &result);
                        suite_.expect (result != nullptr,
                            "object " + std::to_string(j) + " missing");
                        suite_.expect (result->iscloneof(obj),
                            "object " + std::to_string(j) + " not a clone");
                    }

                    char p[2];
                    p[0] = rand_(gen_) < 50 ? 0 : 1;
                    p[1] = 1 - p[0];
                    for (int q = 0; q < 2; ++q)
                    {
                        switch (p[q])
                        {
                        case 0:
                        {
                            // fetch recent
                            nodeobject::ptr obj;
                            nodeobject::ptr result;
                            auto const j = recent_(gen_);
                            obj = seq1_.obj(j);
                            backend_.fetch(obj->gethash().data(), &result);
                            suite_.expect(! result || result->iscloneof(obj));
                            break;
                        }

                        case 1:
                        {
                            // insert new
                            auto const j = i + params_.items;
                            backend_.store(seq1_.obj(j));
                            break;
                        }
                        }
                    }
                }
                catch(std::exception const& e)
                {
                    suite_.fail(e.what());
                }
            }
        };

        try
        {
            parallel_for_id<body>(params.items, params.threads,
                std::ref(*this), std::ref(params), std::ref(*backend));
        }
        catch(...)
        {
        #if nodestore_timing_do_verify
            backend->verify();
        #endif
            throw;
        }
        backend->close();
    }

    //--------------------------------------------------------------------------

    using test_func = void (timing_test::*)(section const&, params const&);
    using test_list = std::vector <std::pair<std::string, test_func>>;

    duration_type
    do_test (test_func f,
        section const& config, params const& params)
    {
        auto const start = clock_type::now();
        (this->*f)(config, params);
        return std::chrono::duration_cast<duration_type> (
            clock_type::now() - start);
    }

    void
    do_tests (std::size_t threads, test_list const& tests,
        std::vector<std::string> const& config_strings)
    {
        using std::setw;
        int w = 8;
        for (auto const& test : tests)
            if (w < test.first.size())
                w = test.first.size();
        log <<
            "\n" <<
            threads << " thread" << (threads > 1 ? "s" : "") << ", " <<
            default_items << " objects";
        {
            std::stringstream ss;
            ss << std::left << setw(10) << "backend" << std::right;
            for (auto const& test : tests)
                ss << " " << setw(w) << test.first;
            log << ss.str();
        }

        for (auto const& config_string : config_strings)
        {
            params params;
            params.items = default_items;
            params.threads = threads;
            for (auto i = default_repeat; i--;)
            {
                section config = parse(config_string);
                config.set ("path",
                    beast::unittestutilities::tempdirectory(
                        "test_db").getfullpathname().tostdstring());
                std::stringstream ss;
                ss << std::left << setw(10) <<
                    get(config, "type", std::string()) << std::right;
                for (auto const& test : tests)
                    ss << " " << setw(w) << to_string(
                        do_test (test.second, config, params));
                ss << "   " << to_string(config);
                log << ss.str();
            }
        }
    }

    void
    run() override
    {
        testcase ("timing", suite::abort_on_fail);

        /*  parameters:

            repeat          number of times to repeat each test
            items           number of objects to create in the database

        */
        std::string default_args =
            "type=nudb"
        #if ripple_rocksdb_available
            ";type=rocksdb,open_files=2000,filter_bits=12,cache_mb=256,"
                "file_size_mb=8,file_size_mult=2"
        #endif
        #if 0
            ";type=memory|path=nodestore"
        #endif
            ;

        test_list const tests =
            {
                 { "insert",    &timing_test::do_insert }
                ,{ "fetch",     &timing_test::do_fetch }
                ,{ "missing",   &timing_test::do_missing }
                ,{ "mixed",     &timing_test::do_mixed }
                ,{ "work",      &timing_test::do_work }
            };

        auto args = arg().empty() ? default_args : arg();
        std::vector <std::string> config_strings;
        boost::split (config_strings, args,
            boost::algorithm::is_any_of (";"));
        for (auto iter = config_strings.begin();
                iter != config_strings.end();)
            if (iter->empty())
                iter = config_strings.erase (iter);
            else
                ++iter;

        do_tests ( 1, tests, config_strings);
        do_tests ( 4, tests, config_strings);
        do_tests ( 8, tests, config_strings);
        //do_tests (16, tests, config_strings);
    }
};

beast_define_testsuite_manual(timing,nodestore,ripple);

}
}

