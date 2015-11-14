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

#ifndef beast_nudb_pool_h_included
#define beast_nudb_pool_h_included

#include <beast/nudb/detail/arena.h>
#include <beast/nudb/detail/bucket.h>
#include <beast/nudb/detail/format.h>
#include <cstdint>
#include <cstring>
#include <memory>
#include <map>
#include <utility>

namespace beast {
namespace nudb {
namespace detail {

// buffers key/value pairs in a map, associating
// them with a modifiable data file offset.
template <class = void>
class pool_t
{
public:
    struct value_type;
    class compare;

private:
    using map_type = std::map<
        value_type, std::size_t, compare>;

    arena arena_;
    std::size_t key_size_;
    std::size_t data_size_ = 0;
    map_type map_;

public:
    using iterator =
        typename map_type::iterator;

    pool_t (pool_t const&) = delete;
    pool_t& operator= (pool_t const&) = delete;

    explicit
    pool_t (std::size_t key_size,
        std::size_t alloc_size);

    pool_t& operator= (pool_t&& other);

    iterator
    begin()
    {
        return map_.begin();
    }

    iterator
    end()
    {
        return map_.end();
    }

    bool
    empty()
    {
        return map_.size() == 0;
    }

    // returns the number of elements in the pool
    std::size_t
    size() const
    {
        return map_.size();
    }

    // returns the sum of data sizes in the pool
    std::size_t
    data_size() const
    {
        return data_size_;
    }

    void
    clear();

    void
    shrink_to_fit();

    iterator
    find (void const* key);

    // insert a value
    // @param h the hash of the key
    void
    insert (std::size_t h, void const* key,
        void const* buffer, std::size_t size);

    template <class u>
    friend
    void
    swap (pool_t<u>& lhs, pool_t<u>& rhs);
};

template <class _>
struct pool_t<_>::value_type
{
    std::size_t hash;
    std::size_t size;
    void const* key;
    void const* data;

    value_type (value_type const&) = default;
    value_type& operator= (value_type const&) = default;

    value_type (std::size_t hash_, std::size_t size_,
        void const* key_, void const* data_)
        : hash (hash_)
        , size (size_)
        , key (key_)
        , data (data_)
    {
    }
};

template <class _>
class pool_t<_>::compare
{
private:
    std::size_t key_size_;

public:
    using result_type = bool;
    using first_argument_type = value_type;
    using second_argument_type = value_type;

    compare (compare const&) = default;
    compare& operator= (compare const&) = default;

    compare (std::size_t key_size)
        : key_size_ (key_size)
    {
    }

    bool
    operator()(value_type const& lhs,
        value_type const& rhs) const
    {
        return std::memcmp(
            lhs.key, rhs.key, key_size_) < 0;
    }
};

//------------------------------------------------------------------------------

template <class _>
pool_t<_>::pool_t (std::size_t key_size,
        std::size_t alloc_size)
    : arena_ (alloc_size)
    , key_size_ (key_size)
    , map_ (compare(key_size))
{
}

template <class _>
pool_t<_>&
pool_t<_>::operator= (pool_t&& other)
{
    arena_ = std::move(other.arena_);
    key_size_ = other.key_size_;
    data_size_ = other.data_size_;
    map_ = std::move(other.map_);
    return *this;
}

template <class _>
void
pool_t<_>::clear()
{
    arena_.clear();
    data_size_ = 0;
    map_.clear();
}

template <class _>
void
pool_t<_>::shrink_to_fit()
{
    arena_.shrink_to_fit();
}

template <class _>
auto
pool_t<_>::find (void const* key) ->
    iterator
{
    // vfalco need is_transparent here
    value_type tmp (0, 0, key, nullptr);
    auto const iter = map_.find(tmp);
    return iter;
}

template <class _>
void
pool_t<_>::insert (std::size_t h,
    void const* key, void const* data,
        std::size_t size)
{
    auto const k = arena_.alloc(key_size_);
    auto const d = arena_.alloc(size);
    std::memcpy(k, key, key_size_);
    std::memcpy(d, data, size);
    auto const result = map_.emplace(
        std::piecewise_construct,
            std::make_tuple(h, size, k, d),
                std::make_tuple(0));
    (void)result.second;
    // must not already exist!
    assert(result.second);
    data_size_ += size;
}

template <class _>
void
swap (pool_t<_>& lhs, pool_t<_>& rhs)
{
    using std::swap;
    swap(lhs.arena_, rhs.arena_);
    swap(lhs.key_size_, rhs.key_size_);
    swap(lhs.data_size_, rhs.data_size_);
    swap(lhs.map_, rhs.map_);
}

using pool = pool_t<>;

} // detail
} // nudb
} // beast

#endif
