//------------------------------------------------------------------------------
/*
    this file is part of beast: https://github.com/vinniefalco/beast
    copyright 2014, vinnie falco <vinnie.falco@gmail.com>

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

#ifndef beast_nudb_store_h_included
#define beast_nudb_store_h_included

#include <beast/nudb/common.h>
#include <beast/nudb/recover.h>
#include <beast/nudb/detail/bucket.h>
#include <beast/nudb/detail/buffer.h>
#include <beast/nudb/detail/bulkio.h>
#include <beast/nudb/detail/cache.h>
#include <beast/nudb/detail/format.h>
#include <beast/nudb/detail/gentex.h>
#include <beast/nudb/detail/pool.h>
#include <boost/thread/lock_types.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <limits>
#include <beast/cxx14/memory.h> // <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

#if doxygen
#include <beast/nudb/readme.md>
#endif

namespace beast {
namespace nudb {

/*

    todo
    
    - fingerprint / checksum on log records

    - size field at end of data records
        allows walking backwards

    - timestamp every so often on data records
        allows knowing the age of the data

*/

/** a simple key/value database
    @tparam hasher the hash function to use on key
    @tparam codec the codec to apply to value data
    @tparam file the type of file object to use.
*/
template <class hasher, class codec, class file>
class store
{
public:
    using hash_type = hasher;
    using codec_type = codec;
    using file_type = file;

private:
    // requires 64-bit integers or better
    static_assert(sizeof(std::size_t)>=8, "");

    enum
    {
        // size of bulk writes
        bulk_write_size     = 16 * 1024 * 1024,

        // size of bulk reads during recover
        recover_read_size   = 16 * 1024 * 1024
    };

    using clock_type =
        std::chrono::steady_clock;

    using shared_lock_type =
        boost::shared_lock<boost::shared_mutex>;

    using unique_lock_type =
        boost::unique_lock<boost::shared_mutex>;

    struct state
    {
        file df;
        file kf;
        file lf;
        path_type dp;
        path_type kp;
        path_type lp;
        detail::pool p0;
        detail::pool p1;
        detail::cache c0;
        detail::cache c1;
        codec const codec;
        detail::key_file_header const kh;

        // pool commit high water mark
        std::size_t pool_thresh = 0;

        state (state const&) = delete;
        state& operator= (state const&) = delete;

        state (file&& df_, file&& kf_, file&& lf_,
            path_type const& dp_, path_type const& kp_,
                path_type const& lp_,
                    detail::key_file_header const& kh_,
                        std::size_t arena_alloc_size);
    };

    bool open_ = false;

    // vfalco unfortunately boost::optional doesn't support
    //        move construction so we use unique_ptr instead.
    std::unique_ptr <state> s_;     // state of an open database

    std::size_t frac_;              // accumulates load
    std::size_t thresh_;            // split threshold
    std::size_t buckets_;           // number of buckets
    std::size_t modulus_;           // hash modulus

    std::mutex u_;                  // serializes insert()
    detail::gentex g_;
    boost::shared_mutex m_;
    std::thread thread_;
    std::condition_variable_any cond_;

    // these allow insert to block, preventing the pool
    // from exceeding a limit. currently the limit is
    // baked in, and can only be reached during sustained
    // insertions, such as while importing.
    std::size_t commit_limit_ = 1ul * 1024 * 1024 * 1024;
    std::condition_variable_any cond_limit_;

    std::atomic<bool> epb_;         // `true` when ep_ set
    std::exception_ptr ep_;

public:
    store() = default;
    store (store const&) = delete;
    store& operator= (store const&) = delete;

    /** destroy the database.

        files are closed, memory is freed, and data that has not been
        committed is discarded. to ensure that all inserted data is
        written, it is necessary to call close() before destroying the
        store.

        this function catches all exceptions thrown by callees, so it
        will be necessary to call close() before destroying the store
        if callers want to catch exceptions.

        throws:
            none
    */
    ~store();

    /** returns `true` if the database is open. */
    bool
    is_open() const
    {
        return open_;
    }

    path_type const&
    dat_path() const
    {
        return s_->dp;
    }

    path_type const&
    key_path() const
    {
        return s_->kp;
    }

    path_type const&
    log_path() const
    {
        return s_->lp;
    }

    std::uint64_t
    appnum() const
    {
        return s_->kh.appnum;
    }

    /** close the database.

        all data is committed before closing.

        throws:
            store_error
    */
    void
    close();

    /** open a database.

        @param args arguments passed to file constructors
        @return `true` if each file could be opened
    */
    template <class... args>
    bool
    open (
        path_type const& dat_path,
        path_type const& key_path,
        path_type const& log_path,
        std::size_t arena_alloc_size,
        args&&... args);

    /** fetch a value.

        if key is found, handler will be called as:
            `(void)()(void const* data, std::size_t size)`

        where data and size represent the value. if the
        key is not found, the handler is not called.

        @return `true` if a matching key was found.
    */
    template <class handler>
    bool
    fetch (void const* key, handler&& handler);

    /** insert a value.

        returns:
            `true` if the key was inserted,
            `false` if the key already existed
    */
    bool
    insert (void const* key, void const* data,
        std::size_t bytes);

private:
    void
    rethrow()
    {
        if (epb_.load())
            std::rethrow_exception(ep_);
    }

    // fetch key in loaded bucket b or its spills.
    //
    template <class handler>
    bool
    fetch (std::size_t h, void const* key,
        detail::bucket b, handler&& handler);

    // returns `true` if the key exists
    // lock is unlocked after the first bucket processed
    //
    bool
    exists (std::size_t h, void const* key,
        shared_lock_type* lock, detail::bucket b);

    void
    split (detail::bucket& b1, detail::bucket& b2,
        detail::bucket& tmp, std::size_t n1, std::size_t n2,
            std::size_t buckets, std::size_t modulus,
                detail::bulk_writer<file>& w);

    detail::bucket
    load (std::size_t n, detail::cache& c1,
        detail::cache& c0, void* buf);

    void
    commit();

    void
    run();
};

//------------------------------------------------------------------------------

template <class hasher, class codec, class file>
store<hasher, codec, file>::state::state (
    file&& df_, file&& kf_, file&& lf_,
        path_type const& dp_, path_type const& kp_,
            path_type const& lp_,
                detail::key_file_header const& kh_,
                    std::size_t arena_alloc_size)
    : df (std::move(df_))
    , kf (std::move(kf_))
    , lf (std::move(lf_))
    , dp (dp_)
    , kp (kp_)
    , lp (lp_)
    , p0 (kh_.key_size, arena_alloc_size)
    , p1 (kh_.key_size, arena_alloc_size)
    , c0 (kh_.key_size, kh_.block_size)
    , c1 (kh_.key_size, kh_.block_size)
    , kh (kh_)
{
}

//------------------------------------------------------------------------------

template <class hasher, class codec, class file>
store<hasher, codec, file>::~store()
{
    try
    {
        close();
    }
    catch (...)
    {
        // if callers want to see the exceptions
        // they have to call close manually.
    }
}

template <class hasher, class codec, class file>
template <class... args>
bool
store<hasher, codec, file>::open (
    path_type const& dat_path,
    path_type const& key_path,
    path_type const& log_path,
    std::size_t arena_alloc_size,
    args&&... args)
{
    using namespace detail;
    if (is_open())
        throw std::logic_error("nudb: already open");
    epb_.store(false);
    recover<hasher, codec, file>(
        dat_path, key_path, log_path,
            recover_read_size,
                args...);
    file df(args...);
    file kf(args...);
    file lf(args...);
    if (! df.open (file_mode::append, dat_path))
        return false;
    if (! kf.open (file_mode::write, key_path))
        return false;
    if (! lf.create (file_mode::append, log_path))
        return false;
    dat_file_header dh;
    key_file_header kh;
    read (df, dh);
    read (kf, kh);
    verify<codec> (dh);
    verify<hasher> (kh);
    verify<hasher> (dh, kh);
    auto s = std::make_unique<state>(
        std::move(df), std::move(kf), std::move(lf),
            dat_path, key_path, log_path, kh,
                arena_alloc_size);
    thresh_ = std::max<std::size_t>(65536ul,
        kh.load_factor * kh.capacity);
    frac_ = thresh_ / 2;
    buckets_ = kh.buckets;
    modulus_ = ceil_pow2(kh.buckets);
    // vfalco todo this could be better
    if (buckets_ < 1)
        throw store_corrupt_error (
            "bad key file length");
    s_ = std::move(s);
    open_ = true;
    thread_ = std::thread(
        &store::run, this);
    return true;
}

template <class hasher, class codec, class file>
void
store<hasher, codec, file>::close()
{
    if (open_)
    {
        // set this first otherwise a
        // throw can cause another close().
        open_ = false;
        cond_.notify_all();
        thread_.join();
        rethrow();
        s_->lf.close();
        file::erase(s_->lp);
        s_.reset();
    }
}

template <class hasher, class codec, class file>
template <class handler>
bool
store<hasher, codec, file>::fetch (
    void const* key, handler&& handler)
{
    using namespace detail;
    rethrow();
    auto const h = hash<hasher>(
        key, s_->kh.key_size, s_->kh.salt);
    shared_lock_type m (m_);
    {
        auto iter = s_->p1.find(key);
        if (iter == s_->p1.end())
        {
            iter = s_->p0.find(key);
            if (iter == s_->p0.end())
                goto next;
        }
        buffer buf;
        auto const result =
            s_->codec.decompress(
                iter->first.data,
                    iter->first.size, buf);
        handler(result.first, result.second);
        return true;
    }
next:
    auto const n = bucket_index(
        h, buckets_, modulus_);
    auto const iter = s_->c1.find(n);
    if (iter != s_->c1.end())
        return fetch(h, key,
            iter->second, handler);
    // vfalco audit for concurrency
    genlock <gentex> g (g_);
    m.unlock();
    buffer buf (s_->kh.block_size);
    // vfalco constructs with garbage here
    bucket b (s_->kh.block_size,
        buf.get());
    b.read (s_->kf,
        (n + 1) * b.block_size());
    return fetch(h, key, b, handler);
}

template <class hasher, class codec, class file>
bool
store<hasher, codec, file>::insert (
    void const* key, void const* data,
        std::size_t size)
{
    using namespace detail;
    rethrow();
    buffer buf;
    // data record
    if (size > field<uint48_t>::max)
        throw std::logic_error(
            "nudb: size too large");
    auto const h = hash<hasher>(
        key, s_->kh.key_size, s_->kh.salt);
    std::lock_guard<std::mutex> u (u_);
    {
        shared_lock_type m (m_);
        if (s_->p1.find(key) != s_->p1.end())
            return false;
        if (s_->p0.find(key) != s_->p0.end())
            return false;
        auto const n = bucket_index(
            h, buckets_, modulus_);
        auto const iter = s_->c1.find(n);
        if (iter != s_->c1.end())
        {
            if (exists(h, key, &m,
                    iter->second))
                return false;
            // m is now unlocked
        }
        else
        {
            // vfalco audit for concurrency
            genlock <gentex> g (g_);
            m.unlock();
            buf.reserve(s_->kh.block_size);
            bucket b (s_->kh.block_size,
                buf.get());
            b.read (s_->kf,
                (n + 1) * s_->kh.block_size);
            if (exists(h, key, nullptr, b))
                return false;
        }
    }
    auto const result =
        s_->codec.compress(data, size, buf);
    // perform insert
    unique_lock_type m (m_);
    s_->p1.insert (h, key,
        result.first, result.second);
    // did we go over the commit limit?
    if (commit_limit_ > 0 &&
        s_->p1.data_size() >= commit_limit_)
    {
        // yes, start a new commit
        cond_.notify_all();
        // wait for pool to shrink
        cond_limit_.wait(m,
            [this]() { return
                s_->p1.data_size() <
                    commit_limit_; });
    }
    bool const notify =
        s_->p1.data_size() >= s_->pool_thresh;
    m.unlock();
    if (notify)
        cond_.notify_all();
    return true;
}

template <class hasher, class codec, class file>
template <class handler>
bool
store<hasher, codec, file>::fetch (
    std::size_t h, void const* key,
        detail::bucket b, handler&& handler)
{
    using namespace detail;
    buffer buf0;
    buffer buf1;
    for(;;)
    {
        for (auto i = b.lower_bound(h);
            i < b.size(); ++i)
        {
            auto const item = b[i];
            if (item.hash != h)
                break;
            // data record
            auto const len = 
                s_->kh.key_size +       // key
                item.size;              // value
            buf0.reserve(len);
            s_->df.read(item.offset +
                field<uint48_t>::size,  // size
                    buf0.get(), len);
            if (std::memcmp(buf0.get(), key,
                s_->kh.key_size) == 0)
            {
                auto const result =
                    s_->codec.decompress(
                        buf0.get() + s_->kh.key_size,
                            item.size, buf1);
                handler(result.first, result.second);
                return true;
            }
        }
        auto const spill = b.spill();
        if (! spill)
            break;
        buf1.reserve(s_->kh.block_size);
        b = bucket(s_->kh.block_size,
            buf1.get());
        b.read(s_->df, spill);
    }
    return false;
}

template <class hasher, class codec, class file>
bool
store<hasher, codec, file>::exists (
    std::size_t h, void const* key,
        shared_lock_type* lock, detail::bucket b)
{
    using namespace detail;
    buffer buf(s_->kh.key_size +
        s_->kh.block_size);
    void* pk = buf.get();
    void* pb = buf.get() + s_->kh.key_size;
    for(;;)
    {
        for (auto i = b.lower_bound(h);
            i < b.size(); ++i)
        {
            auto const item = b[i];
            if (item.hash != h)
                break;
            // data record
            s_->df.read(item.offset +
                field<uint48_t>::size,      // size
                pk, s_->kh.key_size);       // key
            if (std::memcmp(pk, key,
                    s_->kh.key_size) == 0)
                return true;
        }
        auto spill = b.spill();
        if (lock && lock->owns_lock())
            lock->unlock();
        if (! spill)
            break;
        b = bucket(s_->kh.block_size, pb);
        b.read(s_->df, spill);
    }
    return false;
}

//  split the bucket in b1 to b2
//  b1 must be loaded
//  tmp is used as a temporary buffer
//  splits are written but not the new buckets
//
template <class hasher, class codec, class file>
void
store<hasher, codec, file>::split (detail::bucket& b1,
    detail::bucket& b2, detail::bucket& tmp,
        std::size_t n1, std::size_t n2,
            std::size_t buckets, std::size_t modulus,
                detail::bulk_writer<file>& w)
{
    using namespace detail;
    // trivial case: split empty bucket
    if (b1.empty())
        return;
    // split
    for (std::size_t i = 0; i < b1.size();)
    {
        auto const e = b1[i];
        auto const n = bucket_index(
            e.hash, buckets, modulus);
        assert(n==n1 || n==n2);
        if (n == n2)
        {
            b2.insert (e.offset, e.size, e.hash);
            b1.erase (i);
        }
        else
        {
            ++i;
        }
    }
    std::size_t spill = b1.spill();
    if (spill)
    {
        b1.spill (0);
        do
        {
            // if any part of the spill record is
            // in the write buffer then flush first
            // vfalco needs audit
            if (spill + bucket_size(s_->kh.capacity) >
                    w.offset() - w.size())
                w.flush();
            tmp.read (s_->df, spill);
            for (std::size_t i = 0; i < tmp.size(); ++i)
            {
                auto const e = tmp[i];
                auto const n = bucket_index(
                    e.hash, buckets, modulus);
                assert(n==n1 || n==n2);
                if (n == n2)
                {
                    maybe_spill(b2, w);
                    b2.insert(
                        e.offset, e.size, e.hash);
                }
                else
                {
                    maybe_spill(b1, w);
                    b1.insert(
                        e.offset, e.size, e.hash);
                }
            }
            spill = tmp.spill();
        }
        while (spill);
    }
}

//  effects:
//
//      returns a bucket from caches or the key file
//
//      if the bucket is found in c1, returns the
//          bucket from c1.
//      else if the bucket number is greater than buckets(),
//          throws.
//      else, if the bucket is found in c2, inserts the
//          bucket into c1 and returns the bucket from c1.
//      else, reads the bucket from the key file, inserts
//          the bucket into c0 and c1, and returns
//          the bucket from c1.
//
//  preconditions:
//      buf points to a buffer of at least block_size() bytes
//
//  postconditions:
//      c1, and c0, and the memory pointed to by buf may be modified
//
template <class hasher, class codec, class file>
detail::bucket
store<hasher, codec, file>::load (
    std::size_t n, detail::cache& c1,
        detail::cache& c0, void* buf)
{
    using namespace detail;
    auto iter = c1.find(n);
    if (iter != c1.end())
        return iter->second;
    iter = c0.find(n);
    if (iter != c0.end())
        return c1.insert (n,
            iter->second)->second;
    bucket tmp (s_->kh.block_size, buf);
    tmp.read (s_->kf, (n + 1) *
        s_->kh.block_size);
    c0.insert (n, tmp);
    return c1.insert (n, tmp)->second;
}

//  commit the memory pool to disk, then sync.
//
//  preconditions:
//
//  effects:
//
template <class hasher, class codec, class file>
void
store<hasher, codec, file>::commit()
{
    using namespace detail;
    buffer buf1 (s_->kh.block_size);
    buffer buf2 (s_->kh.block_size);
    bucket tmp (s_->kh.block_size, buf1.get());
    // empty cache put in place temporarily
    // so we can reuse the memory from s_->c1
    cache c1;
    {
        unique_lock_type m (m_);
        if (s_->p1.empty())
            return;
        if (s_->p1.data_size() >= commit_limit_)
            cond_limit_.notify_all();
        swap (s_->c1, c1);
        swap (s_->p0, s_->p1);
        s_->pool_thresh = std::max(
            s_->pool_thresh, s_->p0.data_size());
        m.unlock();
    }
    // prepare rollback information
    // log file header
    log_file_header lh;
    lh.version = currentversion;    // version
    lh.uid = s_->kh.uid;            // uid
    lh.appnum = s_->kh.appnum;      // appnum
    lh.key_size = s_->kh.key_size;  // key size
    lh.salt = s_->kh.salt;          // salt
    lh.pepper = pepper<hasher>(
        lh.salt);                   // pepper
    lh.block_size =
        s_->kh.block_size;          // block size
    lh.key_file_size =
        s_->kf.actual_size();       // key file size
    lh.dat_file_size =
        s_->df.actual_size();       // data file size
    write (s_->lf, lh);
    s_->lf.sync();
    // append data and spills to data file
    auto modulus = modulus_;
    auto buckets = buckets_;
    {
        // bulk write to avoid write amplification
        bulk_writer<file> w (s_->df,
            s_->df.actual_size(), bulk_write_size);
        // write inserted data to the data file
        for (auto& e : s_->p0)
        {
            // vfalco this could be ub since other
            // threads are reading other data members
            // of this object in memory
            e.second = w.offset();
            auto os = w.prepare (value_size(
                e.first.size, s_->kh.key_size));
            // data record
            write <uint48_t> (os,
                e.first.size);          // size
            write (os, e.first.key,
                s_->kh.key_size);       // key
            write (os, e.first.data,
                e.first.size);          // data
        }
        // do inserts, splits, and build view
        // of original and modified buckets
        for (auto const e : s_->p0)
        {
            // vfalco should this be >= or > ?
            if ((frac_ += 65536) >= thresh_)
            {
                // split
                frac_ -= thresh_;
                if (buckets == modulus)
                    modulus *= 2;
                auto const n1 = buckets - (modulus / 2);
                auto const n2 = buckets++;
                auto b1 = load (n1, c1, s_->c0, buf2.get());
                auto b2 = c1.create (n2);
                // if split spills, the writer is
                // flushed which can amplify writes.
                split (b1, b2, tmp, n1, n2,
                    buckets, modulus, w);
            }
            // insert
            auto const n = bucket_index(
                e.first.hash, buckets, modulus);
            auto b = load (n, c1, s_->c0, buf2.get());
            // this can amplify writes if it spills.
            maybe_spill(b, w);
            b.insert (e.second,
                e.first.size, e.first.hash);
        }
        w.flush();
    }
    // give readers a view of the new buckets.
    // this might be slightly better than the old
    // view since there could be fewer spills.
    {
        unique_lock_type m (m_);
        swap(c1, s_->c1);
        s_->p0.clear();
        buckets_ = buckets;
        modulus_ = modulus;
    }
    // write clean buckets to log file
    // vfalco should the bulk_writer buffer size be tunable?
    {
        bulk_writer<file> w(s_->lf,
            s_->lf.actual_size(), bulk_write_size);
        for (auto const e : s_->c0)
        {
            // log record
            auto os = w.prepare(
                field<std::uint64_t>::size +    // index
                e.second.compact_size());       // bucket
            // log record
            write<std::uint64_t>(os, e.first);  // index
            e.second.write(os);                 // bucket
        }
        s_->c0.clear();
        w.flush();
        s_->lf.sync();
    }
    // vfalco audit for concurrency
    {
        std::lock_guard<gentex> g (g_);
        // write new buckets to key file
        for (auto const e : s_->c1)
            e.second.write (s_->kf,
                (e.first + 1) * s_->kh.block_size);
    }
    // finalize the commit
    s_->df.sync();
    s_->kf.sync();
    s_->lf.trunc(0);
    s_->lf.sync();
    // cache is no longer needed, all fetches will go straight
    // to disk again. do this after the sync, otherwise readers
    // might get blocked longer due to the extra i/o.
    // vfalco is this correct?
    {
        unique_lock_type m (m_);
        s_->c1.clear();
    }
}

template <class hasher, class codec, class file>
void
store<hasher, codec, file>::run()
{
    auto const pred =
        [this]()
        {
            return
                ! open_ ||
                s_->p1.data_size() >=
                    s_->pool_thresh ||
                s_->p1.data_size() >=
                    commit_limit_;
        };
    try
    {
        while (open_)
        {
            for(;;)
            {
                using std::chrono::seconds;
                unique_lock_type m (m_);
                bool const timeout =
                    ! cond_.wait_for (m,
                        seconds(1), pred);
                if (! open_)
                    break;
                m.unlock();
                commit();
                // reclaim some memory if
                // we get a spare moment.
                if (timeout)
                {
                    m.lock();
                    s_->pool_thresh /= 2;
                    s_->p1.shrink_to_fit();
                    s_->p0.shrink_to_fit();
                    s_->c1.shrink_to_fit();
                    s_->c0.shrink_to_fit();
                    m.unlock();
                }
            }
        }
        commit();
    }
    catch(...)
    {
        ep_ = std::current_exception(); // must come first
        epb_.store(true);
    }
}

} // nudb
} // beast

#endif
