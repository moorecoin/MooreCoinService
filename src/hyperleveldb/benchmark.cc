// copyright (c) 2013, cornell university
// all rights reserved.
//
// redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//     * redistributions of source code must retain the above copyright notice,
//       this list of conditions and the following disclaimer.
//     * redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * neither the name of hyperleveldb nor the names of its contributors may
//       be used to endorse or promote products derived from this software
//       without specific prior written permission.
//
// this software is provided by the copyright holders and contributors "as is"
// and any express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular purpose
// are disclaimed. in no event shall the copyright owner or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or business
// interruption) however caused and on any theory of liability, whether in
// contract, strict liability, or tort (including negligence or otherwise)
// arising in any way out of the use of this software, even if advised of the
// possibility of such damage.

// c
#include <cstdlib>

// stl
#include <tr1/memory>
#include <vector>

// leveldb
#include <hyperleveldb/cache.h>
#include <hyperleveldb/db.h>
#include <hyperleveldb/filter_policy.h>

// po6
#include <po6/io/fd.h>
#include <po6/threads/thread.h>

// e
#include <e/popt.h>
#include <e/time.h>

// armnod
#include <armnod.h>

// numbers
#include <numbers.h>

static void
backup_thread(leveldb::db*,
              numbers::throughput_latency_logger* tll);

static void
worker_thread(leveldb::db*,
              numbers::throughput_latency_logger* tll,
              const armnod::argparser& k,
              const armnod::argparser& v);

static long _done = 0;
static long _number = 1000000;
static long _threads = 1;
static long _backup = 0;
static long _write_buf = 64ull * 1024ull * 1024ull;
static const char* _output = "benchmark.log";
static const char* _dir = ".";

int
main(int argc, const char* argv[])
{
    e::argparser ap;
    ap.autohelp();
    ap.arg().name('n', "number")
            .description("perform n operations against the database (default: 1000000)")
            .metavar("n")
            .as_long(&_number);
    ap.arg().name('t', "threads")
            .description("run the test with t concurrent threads (default: 1)")
            .metavar("t")
            .as_long(&_threads);
    ap.arg().name('o', "output")
            .description("output file for benchmark results (default: benchmark.log)")
            .as_string(&_output);
    ap.arg().name('d', "db-dir")
            .description("directory for leveldb storage (default: .)")
            .as_string(&_dir);
    ap.arg().name('w', "write-buffer")
            .description("write buffer size (default: 64mb)")
            .as_long(&_write_buf);
    ap.arg().name('b', "backup")
            .description("perform a live backup every n seconds (default: 0 (no backup))")
            .as_long(&_backup);
    armnod::argparser key_parser("key-");
    armnod::argparser value_parser("value-");
    ap.add("key generation:", key_parser.parser());
    ap.add("value generation:", value_parser.parser());

    if (!ap.parse(argc, argv))
    {
        return exit_failure;
    }

    leveldb::options opts;
    opts.create_if_missing = true;
    opts.write_buffer_size = write_buf;
    opts.filter_policy = leveldb::newbloomfilterpolicy(10);
    leveldb::db* db;
    leveldb::status st = leveldb::db::open(opts, _dir, &db);

    if (!st.ok())
    {
        std::cerr << "could not open leveldb: " << st.tostring() << std::endl;
        return exit_failure;
    }

    numbers::throughput_latency_logger tll;

    if (!tll.open(_output))
    {
        std::cerr << "could not open log: " << strerror(errno) << std::endl;
        return exit_failure;
    }

    typedef std::tr1::shared_ptr<po6::threads::thread> thread_ptr;
    std::vector<thread_ptr> threads;

    if (_backup > 0)
    {
        thread_ptr t(new po6::threads::thread(std::tr1::bind(backup_thread, db, &tll)));
        threads.push_back(t);
        t->start();
    }

    for (size_t i = 0; i < _threads; ++i)
    {
        thread_ptr t(new po6::threads::thread(std::tr1::bind(worker_thread, db, &tll, key_parser, value_parser)));
        threads.push_back(t);
        t->start();
    }

    for (size_t i = 0; i < threads.size(); ++i)
    {
        threads[i]->join();
    }

    std::string tmp;
    if (db->getproperty("leveldb.stats", &tmp)) std::cout << tmp << std::endl;
    delete db;

    if (!tll.close())
    {
        std::cerr << "could not close log: " << strerror(errno) << std::endl;
        return exit_failure;
    }

    return exit_success;
}

static uint64_t
get_random()
{
    po6::io::fd sysrand(open("/dev/urandom", o_rdonly));

    if (sysrand.get() < 0)
    {
        return 0xcafebabe;
    }

    uint64_t ret;

    if (sysrand.read(&ret, sizeof(ret)) != sizeof(ret))
    {
        return 0xdeadbeef;
    }

    return ret;
}

#define billion (1000ull * 1000ull * 1000ull)

void
backup_thread(leveldb::db* db,
              numbers::throughput_latency_logger* tll)
{
    uint64_t target = e::time() / billion;
    target += _backup;
    uint64_t idx = 0;
    numbers::throughput_latency_logger::thread_state ts;
    tll->initialize_thread(&ts);

    while (__sync_fetch_and_add(&_done, 0) < _number)
    {
        uint64_t now = e::time() / billion;

        if (now < target)
        {
            timespec ts;
            ts.tv_sec = 0;
            ts.tv_nsec = 250ull * 1000ull * 1000ull;
            nanosleep(&ts, null);
        }
        else
        {
            target = now + _backup;
            char buf[32];
            snprintf(buf, 32, "%05lu", idx);
            buf[31] = '\0';
            leveldb::slice name(buf);
            leveldb::status st;

            tll->start(&ts, 4);
            st = db->livebackup(name);
            tll->finish(&ts);
            assert(st.ok());
            ++idx;
        }
    }
}

void
worker_thread(leveldb::db* db,
              numbers::throughput_latency_logger* tll,
              const armnod::argparser& _k,
              const armnod::argparser& _v)
{
    armnod::generator key(armnod::argparser(_k).config());
    armnod::generator val(armnod::argparser(_v).config());
    key.seed(get_random());
    val.seed(get_random());
    numbers::throughput_latency_logger::thread_state ts;
    tll->initialize_thread(&ts);

    while (__sync_fetch_and_add(&_done, 1) < _number)
    {
        std::string k = key();
        std::string v = val();

        // issue a "get"
        std::string tmp;
        leveldb::readoptions ropts;
        tll->start(&ts, 1);
        leveldb::status rst = db->get(ropts, leveldb::slice(k.data(), k.size()), &tmp);
        tll->finish(&ts);
        assert(rst.ok() || rst.isnotfound());

        // issue a "put"
        leveldb::writeoptions wopts;
        wopts.sync = false;
        tll->start(&ts, 2);
        leveldb::status wst = db->put(wopts, leveldb::slice(k.data(), k.size()), leveldb::slice(v.data(), v.size()));
        tll->finish(&ts);
        assert(wst.ok());
    }

    tll->terminate_thread(&ts);
}
