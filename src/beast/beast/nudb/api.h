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

#ifndef beast_nudb_api_h_included
#define beast_nudb_api_h_included

#include <beast/nudb/create.h>
#include <beast/nudb/store.h>
#include <beast/nudb/recover.h>
#include <beast/nudb/verify.h>
#include <beast/nudb/visit.h>
#include <cstdint>

namespace beast {
namespace nudb {

// convenience for consolidating template arguments
//
template <
    class hasher,
    class codec,
    class file = native_file,
    std::size_t buffersize = 16 * 1024 * 1024
>
struct api
{
    using hash_type = hasher;
    using codec_type = codec;
    using file_type = file;
    using store = nudb::store<hasher, codec, file>;

    static std::size_t const buffer_size = buffersize;

    template <class... args>
    static
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
        return nudb::create<hasher, codec, file>(
            dat_path, key_path, log_path,
                appnum, salt, key_size, block_size,
                    load_factor, args...);
    }               

    template <class... args>
    static
    bool
    recover (
        path_type const& dat_path,
        path_type const& key_path,
        path_type const& log_path,
        args&&... args)
    {
        return nudb::recover<hasher, codec, file>(
            dat_path, key_path, log_path, buffersize,
                args...);
    }

    static
    verify_info
    verify (
        path_type const& dat_path,
        path_type const& key_path)
    {
        return nudb::verify<hasher>(
            dat_path, key_path, buffersize);
    }
    
    template <class function>
    static
    bool
    visit(
        path_type const& path,
        function&& f)
    {
        return nudb::visit<codec>(
            path, buffersize, f);
    }
};

} // nudb
} // beast

#endif
