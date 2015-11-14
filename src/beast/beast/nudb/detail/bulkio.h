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

#ifndef beast_nudb_bulkio_h_included
#define beast_nudb_bulkio_h_included

#include <beast/nudb/detail/buffer.h>
#include <beast/nudb/detail/stream.h>
#include <algorithm>
#include <cstddef>

namespace beast {
namespace nudb {
namespace detail {

// scans a file in sequential large reads
template <class file>
class bulk_reader
{
private:
    file& f_;
    buffer buf_;
    std::size_t last_;      // size of file
    std::size_t offset_;    // current position
    std::size_t avail_;     // bytes left to read in buf
    std::size_t used_;      // bytes consumed in buf

public:
    bulk_reader (file& f, std::size_t offset,
        std::size_t last, std::size_t buffer_size);

    std::size_t
    offset() const
    {
        return offset_ - avail_;
    }

    bool
    eof() const
    {
        return offset() >= last_;
    }

    istream
    prepare (std::size_t needed);
};

template <class file>
bulk_reader<file>::bulk_reader (file& f, std::size_t offset,
    std::size_t last, std::size_t buffer_size)
    : f_ (f)
    , last_ (last)
    , offset_ (offset)
    , avail_ (0)
    , used_ (0)
{
    buf_.reserve (buffer_size);
}


template <class file>
istream
bulk_reader<file>::prepare (std::size_t needed)
{
    if (needed > avail_)
    {
        if (offset_ + needed - avail_ > last_)
            throw file_short_read_error();
        if (needed > buf_.size())
        {
            buffer buf;
            buf.reserve (needed);
            std::memcpy (buf.get(),
                buf_.get() + used_, avail_);
            buf_ = std::move(buf);
        }
        else
        {
            std::memmove (buf_.get(),
                buf_.get() + used_, avail_);
        }

        auto const n = std::min(
            buf_.size() - avail_, last_ - offset_);
        f_.read(offset_, buf_.get() + avail_, n);
        offset_ += n;
        avail_ += n;
        used_ = 0;
    }
    istream is(buf_.get() + used_, needed);
    used_ += needed;
    avail_ -= needed;
    return is;
}

//------------------------------------------------------------------------------

// buffers file writes
// caller must call flush manually at the end
template <class file>
class bulk_writer
{
private:
    file& f_;
    buffer buf_;
    std::size_t offset_;    // current position
    std::size_t used_;      // bytes written to buf

public:
    bulk_writer (file& f, std::size_t offset,
        std::size_t buffer_size);

    ostream
    prepare (std::size_t needed);

    // returns the number of bytes buffered
    std::size_t
    size()
    {
        return used_;
    }

    // return current offset in file. this
    // is advanced with each call to prepare.
    std::size_t
    offset() const
    {
        return offset_ + used_;
    }

    // flush cannot be called from the destructor
    // since it can throw, so callers must do it manually.
    void
    flush();
};

template <class file>
bulk_writer<file>::bulk_writer (file& f,
        std::size_t offset, std::size_t buffer_size)
    : f_ (f)
    , offset_ (offset)
    , used_ (0)

{
    buf_.reserve (buffer_size);
}

template <class file>
ostream
bulk_writer<file>::prepare (std::size_t needed)
{
    if (used_ + needed > buf_.size())
        flush();
    if (needed > buf_.size())
        buf_.reserve (needed);
    ostream os (buf_.get() + used_, needed);
    used_ += needed;
    return os;
}

template <class file>
void
bulk_writer<file>::flush()
{
    if (used_)
    {
        auto const offset = offset_;
        auto const used = used_;
        offset_ += used_;
        used_ = 0;
        f_.write (offset, buf_.get(), used);
    }
}

} // detail
} // nudb
} // beast

#endif
