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

#ifndef beast_nudb_verify_h_included
#define beast_nudb_verify_h_included

#include <beast/nudb/common.h>
#include <beast/nudb/file.h>
#include <beast/nudb/detail/bucket.h>
#include <beast/nudb/detail/bulkio.h>
#include <beast/nudb/detail/format.h>
#include <algorithm>
#include <cstddef>
#include <string>

namespace beast {
namespace nudb {

/** reports database information during verify mode. */
struct verify_info
{
    // configured
    std::size_t version = 0;            // api version
    std::size_t uid = 0;                // uid
    std::size_t appnum = 0;             // appnum
    std::size_t key_size = 0;           // size of a key in bytes
    std::size_t salt = 0;               // salt
    std::size_t pepper = 0;             // pepper
    std::size_t block_size = 0;         // block size in bytes
    float load_factor = 0;              // target bucket fill fraction

    // calculated
    std::size_t capacity = 0;           // max keys per bucket
    std::size_t buckets = 0;            // number of buckets
    std::size_t bucket_size = 0;        // size of bucket in bytes

    // measured
    std::size_t key_file_size = 0;      // key file size in bytes
    std::size_t dat_file_size = 0;      // data file size in bytes
    std::size_t key_count = 0;          // keys in buckets and active spills
    std::size_t value_count = 0;        // count of values in the data file
    std::size_t value_bytes = 0;        // sum of value bytes in the data file
    std::size_t spill_count = 0;        // used number of spill records
    std::size_t spill_count_tot = 0;    // number of spill records in data file
    std::size_t spill_bytes = 0;        // used byte of spill records
    std::size_t spill_bytes_tot = 0;    // sum of spill record bytes in data file

    // performance
    float avg_fetch = 0;                // average reads per fetch (excluding value)
    float waste = 0;                    // fraction of data file bytes wasted (0..100)
    float overhead = 0;                 // percent of extra bytes per byte of value
    float actual_load = 0;              // actual bucket fill fraction

    // number of buckets having n spills
    std::array<std::size_t, 10> hist;

    verify_info()
    {
        hist.fill(0);
    }
};

/** verify consistency of the key and data files.
    effects:
        opens the key and data files in read-only mode.
        throws file_error if a file can't be opened.
        iterates the key and data files, throws store_corrupt_error
            on broken invariants.
*/
template <class hasher>
verify_info
verify (
    path_type const& dat_path,
    path_type const& key_path,
    std::size_t read_size)
{
    using namespace detail;
    using file = native_file;
    file df;
    file kf;
    if (! df.open (file_mode::scan, dat_path))
        throw store_corrupt_error(
            "no data file");
    if (! kf.open (file_mode::read, key_path))
        throw store_corrupt_error(
            "no key file");
    key_file_header kh;
    dat_file_header dh;
    read (df, dh);
    read (kf, kh);
    verify(dh);
    verify<hasher>(dh, kh);

    verify_info info;
    info.version = dh.version;
    info.uid = dh.uid;
    info.appnum = dh.appnum;
    info.key_size = dh.key_size;
    info.salt = kh.salt;
    info.pepper = kh.pepper;
    info.block_size = kh.block_size;
    info.load_factor = kh.load_factor / 65536.f;
    info.capacity = kh.capacity;
    info.buckets = kh.buckets;
    info.bucket_size = kh.bucket_size;
    info.key_file_size = kf.actual_size();
    info.dat_file_size = df.actual_size();

    // data record
    auto const dh_len =
        field<uint48_t>::size + // size
        kh.key_size;            // key

    std::size_t fetches = 0;

    // iterate data file
    buffer buf (kh.block_size + dh_len);
    bucket b (kh.block_size, buf.get());
    std::uint8_t* pd = buf.get() + kh.block_size;
    {
        bulk_reader<file> r(df,
            dat_file_header::size,
                df.actual_size(), read_size);
        while (! r.eof())
        {
            auto const offset = r.offset();
            // data record or spill record
            auto is = r.prepare(
                field<uint48_t>::size); // size
            std::size_t size;
            read<uint48_t>(is, size);
            if (size > 0)
            {
                // data record
                is = r.prepare(
                    kh.key_size +           // key
                    size);                  // data
                std::uint8_t const* const key =
                    is.data(kh.key_size);
                std::uint8_t const* const data =
                    is.data(size);
                (void)data;
                auto const h = hash<hasher>(
                    key, kh.key_size, kh.salt);
                // check bucket and spills
                try
                {
                    auto const n = bucket_index(
                        h, kh.buckets, kh.modulus);
                    b.read (kf, (n + 1) * kh.block_size);
                    ++fetches;
                }
                catch (file_short_read_error const&)
                {
                    throw store_corrupt_error(
                        "short bucket");
                }
                for (;;)
                {
                    for (auto i = b.lower_bound(h);
                        i < b.size(); ++i)
                    {
                        auto const item = b[i];
                        if (item.hash != h)
                            break;
                        if (item.offset == offset)
                            goto found;
                        ++fetches;
                    }
                    auto const spill = b.spill();
                    if (! spill)
                        throw store_corrupt_error(
                            "orphaned value");
                    try
                    {
                        b.read (df, spill);
                        ++fetches;
                    }
                    catch (file_short_read_error const&)
                    {
                        throw store_corrupt_error(
                            "short spill");
                    }
                }
            found:
                // update
                ++info.value_count;
                info.value_bytes += size;
            }
            else
            {
                // spill record
                is = r.prepare(
                    field<std::uint16_t>::size);
                read<std::uint16_t>(is, size);  // size
                if (size != kh.bucket_size)
                    throw store_corrupt_error(
                        "bad spill size");
                b.read(r);                      // bucket
                ++info.spill_count_tot;
                info.spill_bytes_tot +=
                    field<uint48_t>::size +     // zero
                    field<uint16_t>::size +     // size
                    b.compact_size();           // bucket
            }
        }
    }

    // iterate key file
    {
        for (std::size_t n = 0; n < kh.buckets; ++n)
        {
            std::size_t nspill = 0;
            b.read (kf, (n + 1) * kh.block_size);
            for(;;)
            {
                info.key_count += b.size();
                for (std::size_t i = 0; i < b.size(); ++i)
                {
                    auto const e = b[i];
                    try
                    {
                        df.read (e.offset, pd, dh_len);
                    }
                    catch (file_short_read_error const&)
                    {
                        throw store_corrupt_error(
                            "missing value");
                    }
                    // data record
                    istream is(pd, dh_len);
                    std::size_t size;
                    read<uint48_t>(is, size);   // size
                    void const* key =
                        is.data(kh.key_size);   // key
                    if (size != e.size)
                        throw store_corrupt_error(
                            "wrong size");
                    auto const h = hash<hasher>(key,
                        kh.key_size, kh.salt);
                    if (h != e.hash)
                        throw store_corrupt_error(
                            "wrong hash");
                }
                if (! b.spill())
                    break;
                try
                {
                    b.read (df, b.spill());
                    ++nspill;
                    ++info.spill_count;
                    info.spill_bytes +=
                        field<uint48_t>::size + // zero
                        field<uint16_t>::size + // size
                        b.compact_size();       // spillbucket
                }
                catch (file_short_read_error const&)
                {
                    throw store_corrupt_error(
                        "missing spill");
                }
            }
            if (nspill >= info.hist.size())
                nspill = info.hist.size() - 1;
            ++info.hist[nspill];
        }
    }

    float sum = 0;
    for (int i = 0; i < info.hist.size(); ++i)
        sum += info.hist[i] * (i + 1);
    //info.avg_fetch = sum / info.buckets;
    info.avg_fetch = float(fetches) / info.value_count;
    info.waste = (info.spill_bytes_tot - info.spill_bytes) /
        float(info.dat_file_size);
    info.overhead =
        float(info.key_file_size + info.dat_file_size) /
        (
            info.value_bytes +
            info.key_count *
                (info.key_size +
                // data record
                 field<uint48_t>::size) // size
                    ) - 1;
    info.actual_load = info.key_count / float(
        info.capacity * info.buckets);
    return info;
}

/** verify consistency of the key and data files.
    effects:
        opens the key and data files in read-only mode.
        throws file_error if a file can't be opened.
        iterates the key and data files, throws store_corrupt_error
            on broken invariants.
    this uses a different algorithm that depends on allocating
    a lareg buffer.
*/
template <class hasher, class progress>
verify_info
verify_fast (
    path_type const& dat_path,
    path_type const& key_path,
    std::size_t buffer_size,
    progress&& progress)
{
    using namespace detail;
    using file = native_file;
    file df;
    file kf;
    if (! df.open (file_mode::scan, dat_path))
        throw store_corrupt_error(
            "no data file");
    if (! kf.open (file_mode::read, key_path))
        throw store_corrupt_error(
            "no key file");
    key_file_header kh;
    dat_file_header dh;
    read (df, dh);
    read (kf, kh);
    verify(dh);
    verify<hasher>(dh, kh);

    verify_info info;
    info.version = dh.version;
    info.uid = dh.uid;
    info.appnum = dh.appnum;
    info.key_size = dh.key_size;
    info.salt = kh.salt;
    info.pepper = kh.pepper;
    info.block_size = kh.block_size;
    info.load_factor = kh.load_factor / 65536.f;
    info.capacity = kh.capacity;
    info.buckets = kh.buckets;
    info.bucket_size = kh.bucket_size;
    info.key_file_size = kf.actual_size();
    info.dat_file_size = df.actual_size();

    std::size_t fetches = 0;

    // counts unverified keys per bucket
    std::unique_ptr<std::uint32_t[]> nkeys(
        new std::uint32_t[kh.buckets]);

    // verify contiguous sequential sections of the
    // key file using multiple passes over the data.
    //
    auto const buckets = std::max<std::size_t>(1,
        buffer_size / kh.block_size);
    buffer buf((buckets + 1) * kh.block_size);
    bucket tmp(kh.block_size, buf.get() +
        buckets * kh.block_size);
    std::size_t const passes =
        (kh.buckets + buckets - 1) / buckets;
    auto const df_size = df.actual_size();
    std::size_t const work = passes * df_size;
    std::size_t npass = 0;
    for (std::size_t b0 = 0; b0 < kh.buckets;
        b0 += buckets)
    {
        auto const b1 = std::min(
            b0 + buckets, kh.buckets);
        // buffered range is [b0, b1)
        auto const bn = b1 - b0;
        kf.read((b0 + 1) * kh.block_size,
            buf.get(), bn * kh.block_size);
        // count keys in buckets
        for (std::size_t i = b0 ; i < b1; ++i)
        {
            bucket b(kh.block_size, buf.get() +
                (i - b0) * kh.block_size);
            nkeys[i] = b.size();
            std::size_t nspill = 0;
            auto spill = b.spill();
            while (spill != 0)
            {
                tmp.read(df, spill);
                nkeys[i] += tmp.size();
                spill = tmp.spill();
                ++nspill;
                ++info.spill_count;
                info.spill_bytes +=
                    field<uint48_t>::size + // zero
                    field<uint16_t>::size + // size
                    tmp.compact_size();     // spillbucket
            }
            if (nspill >= info.hist.size())
                nspill = info.hist.size() - 1;
            ++info.hist[nspill];
            info.key_count += nkeys[i];
        }
        // iterate data file
        bulk_reader<file> r(df,
            dat_file_header::size, df_size,
                64 * 1024 * 1024);
        while (! r.eof())
        {
            auto const offset = r.offset();
            progress(npass * df_size + offset, work);
            // data record or spill record
            auto is = r.prepare(
                field<uint48_t>::size); // size
            std::size_t size;
            read<uint48_t>(is, size);
            if (size > 0)
            {
                // data record
                is = r.prepare(
                    kh.key_size +           // key
                    size);                  // data
                std::uint8_t const* const key =
                    is.data(kh.key_size);
                std::uint8_t const* const data =
                    is.data(size);
                (void)data;
                auto const h = hash<hasher>(
                    key, kh.key_size, kh.salt);
                auto const n = bucket_index(
                    h, kh.buckets, kh.modulus);
                if (n < b0 || n >= b1)
                    continue;
                // check bucket and spills
                bucket b (kh.block_size, buf.get() +
                    (n - b0) * kh.block_size);
                ++fetches;
                for (;;)
                {
                    for (auto i = b.lower_bound(h);
                        i < b.size(); ++i)
                    {
                        auto const item = b[i];
                        if (item.hash != h)
                            break;
                        if (item.offset == offset)
                            goto found;
                        ++fetches;
                    }
                    auto const spill = b.spill();
                    if (! spill)
                        throw store_corrupt_error(
                            "orphaned value");
                    b = tmp;
                    try
                    {
                        b.read (df, spill);
                        ++fetches;
                    }
                    catch (file_short_read_error const&)
                    {
                        throw store_corrupt_error(
                            "short spill");
                    }
                }
            found:
                // update
                ++info.value_count;
                info.value_bytes += size;
                if (nkeys[n]-- == 0)
                    throw store_corrupt_error(
                        "duplicate value");
            }
            else
            {
                // spill record
                is = r.prepare(
                    field<std::uint16_t>::size);
                read<std::uint16_t>(is, size);      // size
                if (size != kh.bucket_size)
                    throw store_corrupt_error(
                        "bad spill size");
                tmp.read(r);                        // bucket
                if (b0 == 0)
                {
                    ++info.spill_count_tot;
                    info.spill_bytes_tot +=
                        field<uint48_t>::size +     // zero
                        field<uint16_t>::size +     // size
                        tmp.compact_size();         // bucket
                }
            }
        }
        ++npass;
    }

    // make sure every key in every bucket was visited
    for (std::size_t i = 0;
            i < kh.buckets; ++i)
        if (nkeys[i] != 0)
            throw store_corrupt_error(
                "orphan value");

    float sum = 0;
    for (int i = 0; i < info.hist.size(); ++i)
        sum += info.hist[i] * (i + 1);
    //info.avg_fetch = sum / info.buckets;
    info.avg_fetch = float(fetches) / info.value_count;
    info.waste = (info.spill_bytes_tot - info.spill_bytes) /
        float(info.dat_file_size);
    info.overhead =
        float(info.key_file_size + info.dat_file_size) /
        (
            info.value_bytes +
            info.key_count *
                (info.key_size +
                // data record
                 field<uint48_t>::size) // size
                    ) - 1;
    info.actual_load = info.key_count / float(
        info.capacity * info.buckets);
    return info;
}

} // nudb
} // beast

#endif
