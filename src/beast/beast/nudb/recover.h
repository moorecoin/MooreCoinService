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

#ifndef beast_nudb_recover_h_included
#define beast_nudb_recover_h_included

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

/** perform recovery on a database.
    this implements the recovery algorithm by rolling back
    any partially committed data.
*/
template <
    class hasher,
    class codec,
    class file = native_file,
    class... args>
bool
recover (
    path_type const& dat_path,
    path_type const& key_path,
    path_type const& log_path,
    std::size_t read_size,
    args&&... args)
{
    using namespace detail;
    file df(args...);
    file lf(args...);
    file kf(args...);
    if (! df.open (file_mode::append, dat_path))
        return false;
    if (! kf.open (file_mode::write, key_path))
        return false;
    if (! lf.open (file_mode::append, log_path))
        return true;
    dat_file_header dh;
    key_file_header kh;
    log_file_header lh;
    try
    {
        read (kf, kh);
    }
    catch (file_short_read_error const&)
    {
        throw store_corrupt_error(
            "short key file header");
    }
    // vfalco should the number of buckets be based on the
    //        file size in the log record instead?
    verify<hasher>(kh);
    try
    {
        read (df, dh);
    }
    catch (file_short_read_error const&)
    {
        throw store_corrupt_error(
            "short data file header");
    }
    verify<hasher>(dh, kh);
    auto const lf_size = lf.actual_size();
    if (lf_size == 0)
    {
        lf.close();
        file::erase (log_path);
        return true;
    }
    try
    {
        read (lf, lh);
        verify<hasher>(kh, lh);
        auto const df_size = df.actual_size();
        buffer buf(kh.block_size);
        bucket b (kh.block_size, buf.get());
        bulk_reader<file> r(lf, log_file_header::size,
            lf_size, read_size);
        while(! r.eof())
        {
            std::size_t n;
            try
            {
                // log record
                auto is = r.prepare(field<
                    std::uint64_t>::size);
                read<std::uint64_t>(is, n); // index
                b.read(r);                  // bucket
            }
            catch (store_corrupt_error const&)
            {
                throw store_corrupt_error(
                    "corrupt log record");
            }
            catch (file_short_read_error const&)
            {
                // this means that the log file never
                // got fully synced. in which case, there
                // were no changes made to the key file.
                // so we can recover by just truncating.
                break;
            }
            if (b.spill() &&
                    b.spill() + kh.bucket_size > df_size)
                throw store_corrupt_error(
                    "bad spill in log record");
            // vfalco is this the right condition?
            if (n > kh.buckets)
                throw store_corrupt_error(
                    "bad index in log record");
            b.write (kf, (n + 1) * kh.block_size);
        }
        kf.trunc(lh.key_file_size);
        df.trunc(lh.dat_file_size);
        kf.sync();
        df.sync();
    }
    catch (file_short_read_error const&)
    {
        // key and data files should be consistent here
    }

    lf.trunc(0);
    lf.sync();
    lf.close();
    file::erase (log_path);
    return true;
}

} // nudb
} // beast

#endif
