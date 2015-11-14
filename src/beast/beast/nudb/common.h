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

#ifndef beast_nudb_common_h_included
#define beast_nudb_common_h_included

#include <stdexcept>
#include <string>

namespace beast {
namespace nudb {

// commonly used types

enum class file_mode
{
    scan,         // read sequential
    read,         // read random
    append,       // read random, write append
    write         // read random, write random
};

using path_type = std::string;

// all exceptions thrown by nudb are derived
// from std::runtime_error except for fail_error

/** thrown when a codec fails, e.g. corrupt data. */
struct codec_error : std::runtime_error
{
    template <class string>
    explicit
    codec_error (string const& s)
        : runtime_error(s)
    {
    }
};

/** base class for all errors thrown by file classes. */
struct file_error : std::runtime_error
{
    template <class string>
    explicit
    file_error (string const& s)
        : runtime_error(s)
    {
    }
};

/** thrown when file bytes read are less than requested. */
struct file_short_read_error : file_error
{
    file_short_read_error()
        : file_error (
            "nudb: short read")
    {
    }
};

/** thrown when file bytes written are less than requested. */
struct file_short_write_error : file_error
{
    file_short_write_error()
        : file_error (
            "nudb: short write")
    {
    }
};

/** thrown when end of istream reached while reading. */
struct short_read_error : std::runtime_error
{
    short_read_error()
        : std::runtime_error(
            "nudb: short read")
    {
    }
};

/** base class for all exceptions thrown by store. */
class store_error : public std::runtime_error
{
public:
    template <class string>
    explicit
    store_error (string const& s)
        : runtime_error(s)
    {
    }
};

/** thrown when corruption in a file is detected. */
class store_corrupt_error : public store_error
{
public:
    template <class string>
    explicit
    store_corrupt_error (string const& s)
        : store_error(s)
    {
    }
};

} // nudb
} // beast

#endif
