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

#ifndef beast_nudb_bucket_h_included
#define beast_nudb_bucket_h_included

#include <beast/nudb/common.h>
#include <beast/nudb/detail/bulkio.h>
#include <beast/nudb/detail/field.h>
#include <beast/nudb/detail/format.h>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace beast {
namespace nudb {
namespace detail {

// bucket calculations:

// returns bucket index given hash, buckets, and modulus
//
inline
std::size_t
bucket_index (std::size_t h,
    std::size_t buckets, std::size_t modulus)
{
    std::size_t n = h % modulus;
    if (n >= buckets)
        n -= modulus / 2;
    return n;
}

//------------------------------------------------------------------------------

// tag for constructing empty buckets
struct empty_t { };
static empty_t empty;

// allows inspection and manipulation of bucket blobs in memory
template <class = void>
class bucket_t
{
private:
    std::size_t block_size_;    // size of a key file block
    std::size_t size_;          // current key count
    std::size_t spill_;         // offset of next spill record or 0
    std::uint8_t* p_;           // pointer to the bucket blob

public:
    struct value_type
    {
        std::size_t offset;
        std::size_t size;
        std::size_t hash;
    };

    bucket_t (bucket_t const&) = default;
    bucket_t& operator= (bucket_t const&) = default;

    bucket_t (std::size_t block_size, void* p);

    bucket_t (std::size_t block_size, void* p, empty_t);

    std::size_t
    block_size() const
    {
        return block_size_;
    }

    std::size_t
    compact_size() const
    {
        return detail::bucket_size(size_);
    }

    bool
    empty() const
    {
        return size_ == 0;
    }

    bool
    full() const
    {
        return size_ >=
            detail::bucket_capacity(block_size_);
    }

    std::size_t
    size() const
    {
        return size_;
    }

    // returns offset of next spill record or 0
    //
    std::size_t
    spill() const
    {
        return spill_;
    }

    // set offset of next spill record
    //
    void
    spill (std::size_t offset);

    // clear contents of the bucket
    //
    void
    clear();

    // returns the record for a key
    // entry without bounds checking.
    //
    value_type const
    at (std::size_t i) const;

    value_type const
    operator[] (std::size_t i) const
    {
        return at(i);
    }

    // returns index of entry with prefix
    // equal to or greater than the given prefix.
    //
    std::size_t
    lower_bound (std::size_t h) const;

    void
    insert (std::size_t offset,
        std::size_t size, std::size_t h);

    // erase an element by index
    //
    void
    erase (std::size_t i);

    // read a full bucket from the
    // file at the specified offset.
    //
    template <class file>
    void
    read (file& f, std::size_t offset);

    // read a compact bucket
    //
    template <class file>
    void
    read (bulk_reader<file>& r);

    // write a compact bucket to the stream.
    // this only writes entries that are not empty.
    //
    void
    write (ostream& os) const;

    // write a bucket to the file at the specified offset.
    // the full block_size() bytes are written.
    //
    template <class file>
    void
    write (file& f, std::size_t offset) const;

private:
    // update size and spill in the blob
    void
    update();
};

//------------------------------------------------------------------------------

template <class _>
bucket_t<_>::bucket_t (
        std::size_t block_size, void* p)
    : block_size_ (block_size)
    , p_ (reinterpret_cast<std::uint8_t*>(p))
{
    // bucket record
    istream is(p_, block_size);
    detail::read<uint16_t>(is, size_);  // count
    detail::read<uint48_t>(is, spill_); // spill
}

template <class _>
bucket_t<_>::bucket_t (
        std::size_t block_size, void* p, empty_t)
    : block_size_ (block_size)
    , size_ (0)
    , spill_ (0)
    , p_ (reinterpret_cast<std::uint8_t*>(p))
{
    clear();
}

template <class _>
void
bucket_t<_>::spill (std::size_t offset)
{
    spill_ = offset;
    update();
}

template <class _>
void
bucket_t<_>::clear()
{
    size_ = 0;
    spill_ = 0;
    std::memset(p_, 0, block_size_);
}

template <class _>
auto
bucket_t<_>::at (std::size_t i) const ->
    value_type const
{
    value_type result;
    // bucket entry
    std::size_t const w =
        field<uint48_t>::size +         // offset
        field<uint48_t>::size +         // size
        field<hash_t>::size;            // prefix
    // bucket record
    detail::istream is(p_ +
        field<std::uint16_t>::size +    // count
        field<uint48_t>::size +         // spill
        i * w, w);
    // bucket entry
    detail::read<uint48_t>(
        is, result.offset);             // offset
    detail::read<uint48_t>(
        is, result.size);               // size
    detail::read<hash_t>(
        is, result.hash);               // hash
    return result;
}

template <class _>
std::size_t
bucket_t<_>::lower_bound (
    std::size_t h) const
{
    // bucket entry
    auto const w =
        field<uint48_t>::size +         // offset
        field<uint48_t>::size +         // size
        field<hash_t>::size;            // hash
    // bucket record
    auto const p = p_ +
        field<std::uint16_t>::size +    // count
        field<uint48_t>::size +         // spill
        // bucket entry
        field<uint48_t>::size +         // offset
        field<uint48_t>::size;          // size
    std::size_t step;
    std::size_t first = 0;
    std::size_t count = size_;
    while (count > 0)
    {
        step = count / 2;
        auto const i = first + step;
        std::size_t h1;
        readp<hash_t>(p + i * w, h1);
        if (h1 < h)
        {
            first = i + 1;
            count -= step + 1;
        }
        else
        {
            count = step;
        }
    }
    return first;
}

template <class _>
void
bucket_t<_>::insert (std::size_t offset,
    std::size_t size, std::size_t h)
{
    std::size_t i = lower_bound(h);
    // bucket record
    auto const p = p_ +
        field<
            std::uint16_t>::size +  // count
        field<uint48_t>::size;      // spill
    // bucket entry
    std::size_t const w =
        field<uint48_t>::size +     // offset
        field<uint48_t>::size +     // size
        field<hash_t>::size;        // hash
    std::memmove (
        p + (i + 1)  * w,
        p +  i       * w,
        (size_ - i) * w);
    size_++;
    update();
    // bucket entry
    ostream os (p + i * w, w);
    detail::write<uint48_t>(
        os, offset);                // offset
    detail::write<uint48_t>(
        os, size);                  // size
    detail::write<hash_t>(
        os, h);                     // prefix
}

template <class _>
void
bucket_t<_>::erase (std::size_t i)
{
    // bucket record
    auto const p = p_ +
        field<
            std::uint16_t>::size +  // count
        field<uint48_t>::size;      // spill
    auto const w =
        field<uint48_t>::size +     // offset
        field<uint48_t>::size +     // size
        field<hash_t>::size;        // hash
    --size_;
    if (i < size_)
        std::memmove(
            p +  i      * w,
            p + (i + 1) * w,
            (size_ - i) * w);
    std::memset(p + size_ * w, 0, w);
    update();
}

template <class _>
template <class file>
void
bucket_t<_>::read (file& f, std::size_t offset)
{
    auto const cap = bucket_capacity (
        block_size_);
    // excludes padding to block size
    f.read (offset, p_, bucket_size(cap));
    istream is(p_, block_size_);
    detail::read<
        std::uint16_t>(is, size_);     // count
    detail::read<
        uint48_t>(is, spill_);          // spill
    if (size_ > cap)
        throw store_corrupt_error(
            "bad bucket size");
}

template <class _>
template <class file>
void
bucket_t<_>::read (bulk_reader<file>& r)
{
    // bucket record (compact)
    auto is = r.prepare(
        detail::field<std::uint16_t>::size +
        detail::field<uint48_t>::size);
    detail::read<
        std::uint16_t>(is, size_);  // count
    detail::read<uint48_t>(
        is, spill_);                // spill
    update();
    // excludes empty bucket entries
    auto const w = size_ * (
        field<uint48_t>::size +     // offset
        field<uint48_t>::size +     // size
        field<hash_t>::size);       // hash
    is = r.prepare (w);
    std::memcpy(p_ +
        field<
            std::uint16_t>::size +  // count
        field<uint48_t>::size,      // spill
        is.data(w), w);             // entries
}

template <class _>
void
bucket_t<_>::write (ostream& os) const
{
    // does not pad up to the block size. this
    // is called to write to the data file.
    auto const size = compact_size();
    // bucket record
    std::memcpy (os.data(size), p_, size);
}

template <class _>
template <class file>
void
bucket_t<_>::write (file& f, std::size_t offset) const
{
    // includes zero pad up to the block
    // size, to make the key file size always
    // a multiple of the block size.
    auto const size = compact_size();
    std::memset (p_ + size, 0,
        block_size_ - size);
    // bucket record
    f.write (offset, p_, block_size_);
}

template <class _>
void
bucket_t<_>::update()
{
    // bucket record
    ostream os(p_, block_size_);
    detail::write<
        std::uint16_t>(os, size_);  // count
    detail::write<
        uint48_t>(os, spill_);      // spill
}

using bucket = bucket_t<>;

//  spill bucket if full.
//  the bucket is cleared after it spills.
//
template <class file>
void
maybe_spill(bucket& b, bulk_writer<file>& w)
{
    if (b.full())
    {
        // spill record
        auto const offset = w.offset();
        auto os = w.prepare(
            field<uint48_t>::size + // zero
            field<uint16_t>::size + // size
            b.compact_size());
        write <uint48_t> (os, 0);   // zero
        write <std::uint16_t> (
            os, b.compact_size());  // size
        auto const spill =
            offset + os.size();
        b.write (os);               // bucket
        // update bucket
        b.clear();
        b.spill (spill);
    }
}

} // detail
} // nudb
} // beast

#endif
