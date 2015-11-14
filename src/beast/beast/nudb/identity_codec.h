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

#ifndef beast_nudb_identity_codec_h_included
#define beast_nudb_identity_codec_h_included

#include <utility>

namespace beast {
namespace nudb {

/** codec which maps input directly to output. */
class identity_codec
{
public:
    template <class... args>
    explicit
    identity_codec(args&&... args)
    {
    }

    char const*
    name() const
    {
        return "none";
    }

    template <class bufferfactory>
    std::pair<void const*, std::size_t>
    compress (void const* in,
        std::size_t in_size, bufferfactory&&) const
    {
        return std::make_pair(in, in_size);
    }

    template <class bufferfactory>
    std::pair<void const*, std::size_t>
    decompress (void const* in,
        std::size_t in_size, bufferfactory&&) const
    {
        return std::make_pair(in, in_size);
    }
};

} // nudb
} // beast

#endif
