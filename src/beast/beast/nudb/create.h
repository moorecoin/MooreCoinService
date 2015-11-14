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

#ifndef beast_nudb_create_h_included
#define beast_nudb_create_h_included

#include <beast/nudb/file.h>
#include <beast/nudb/detail/bucket.h>
#include <beast/nudb/detail/format.h>
#include <algorithm>
#include <cstring>
#include <random>
#include <stdexcept>
#include <utility>

namespace beast {
namespace nudb {

namespace detail {

template <class = void>
std::uint64_t
make_uid()
{
    std::random_device rng;
    std::mt19937_64 gen {rng()};
    std::uniform_int_distribution <std::size_t> dist;
    return dist(gen);
}

}

/** generate a random salt. */
template <class = void>
std::uint64_t
make_salt()
{
    std::random_device rng;
    std::mt19937_64 gen {rng()};
    std::uniform_int_distribution <std::size_t> dist;
    return dist(gen);
}

/** returns the best guess at the volume's block size. */
inline
std::size_t
block_size (path_type const& /*path*/)
{
    return 4096;
}

/** create a new database.
    preconditions:
        the files must not exist
    throws:

    @param args arguments passed to file constructors
    @return `false` if any file could not be created.
*/
template <
    class hasher,
    class codec,
    class file,
    class... args
>
bool
create (
    path_type const& dat_path,
    path_type const& key_path,
    path_type const& log_path,
    std::uint64_t appnum,
    std::uint64_t salt,
    std::size_t key_size,
    std::size_t block_size,
    float load_factor,
    args&&... args)
{
    using namespace detail;
    if (key_size < 1)
        throw std::domain_error(
            "invalid key size");
    if (block_size > field<std::uint16_t>::max)
        throw std::domain_error(
            "nudb: block size too large");
    if (load_factor <= 0.f)
        throw std::domain_error(
            "nudb: load factor too small");
    if (load_factor >= 1.f)
        throw std::domain_error(
            "nudb: load factor too large");
    auto const capacity =
        bucket_capacity(block_size);
    if (capacity < 1)
        throw std::domain_error(
            "nudb: block size too small");
    file df(args...);
    file kf(args...);
    file lf(args...);
    if (df.create(
        file_mode::append, dat_path))
    {
        if (kf.create (
            file_mode::append, key_path))
        {
            if (lf.create(
                    file_mode::append, log_path))
                goto success;
            file::erase (dat_path);
        }
        file::erase (key_path);
    }
    return false;
success:
    dat_file_header dh;
    dh.version = currentversion;
    dh.uid = make_uid();
    dh.appnum = appnum;
    dh.key_size = key_size;

    key_file_header kh;
    kh.version = currentversion;
    kh.uid = dh.uid;
    kh.appnum = appnum;
    kh.key_size = key_size;
    kh.salt = salt;
    kh.pepper = pepper<hasher>(salt);
    kh.block_size = block_size;
    // vfalco should it be 65536?
    //        how do we set the min?
    kh.load_factor = std::min<std::size_t>(
        65536.0 * load_factor, 65535);
    write (df, dh);
    write (kf, kh);
    buffer buf(block_size);
    std::memset(buf.get(), 0, block_size);
    bucket b (block_size, buf.get(), empty);
    b.write (kf, block_size);
    // vfalco leave log file empty?
    df.sync();
    kf.sync();
    lf.sync();
    return true;
}

} // nudb
} // beast

#endif
