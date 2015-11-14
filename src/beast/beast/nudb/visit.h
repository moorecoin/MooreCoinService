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

#ifndef beast_nudb_visit_h_included
#define beast_nudb_visit_h_included

#include <beast/nudb/common.h>
#include <beast/nudb/file.h>
#include <beast/nudb/detail/buffer.h>
#include <beast/nudb/detail/bulkio.h>
#include <beast/nudb/detail/format.h>
#include <algorithm>
#include <cstddef>
#include <string>

namespace beast {
namespace nudb {

/** visit each key/data pair in a database file.

    function will be called with this signature:
        bool(void const* key, std::size_t key_size,
             void const* data, std::size_t size)

    if function returns false, the visit is terminated.

    @return `true` if the visit completed
    this only requires the data file.
*/
template <class codec, class function>
bool
visit(
    path_type const& path,
    std::size_t read_size,
    function&& f)
{
    using namespace detail;
    using file = native_file;
    file df;
    df.open (file_mode::scan, path);
    dat_file_header dh;
    read (df, dh);
    verify<codec> (dh);
    codec codec;
    // iterate data file
    bulk_reader<file> r(
        df, dat_file_header::size,
            df.actual_size(), read_size);
    buffer buf;
    try
    {
        while (! r.eof())
        {
            // data record or spill record
            std::size_t size;
            auto is = r.prepare(
                field<uint48_t>::size); // size
            read<uint48_t>(is, size);
            if (size > 0)
            {
                // data record
                is = r.prepare(
                    dh.key_size +           // key
                    size);                  // data
                std::uint8_t const* const key =
                    is.data(dh.key_size);
                auto const result = codec.decompress(
                    is.data(size), size, buf);
                if (! f(key, dh.key_size,
                        result.first, result.second))
                    return false;
            }
            else
            {
                // spill record
                is = r.prepare(
                    field<std::uint16_t>::size);
                read<std::uint16_t>(is, size);  // size
                r.prepare(size); // skip bucket
            }
        }
    }
    catch (file_short_read_error const&)
    {
        throw store_corrupt_error(
            "nudb: data short read");
    }

    return true;
}

} // nudb
} // beast

#endif
