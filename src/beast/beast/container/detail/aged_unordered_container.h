//------------------------------------------------------------------------------
/*
    this file is part of beast: https://github.com/vinniefalco/beast
    copyright 2013, vinnie falco <vinnie.falco@gmail.com>

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

#ifndef beast_container_aged_unordered_container_h_included
#define beast_container_aged_unordered_container_h_included

#include <beast/container/detail/aged_container_iterator.h>
#include <beast/container/detail/aged_associative_container.h>
#include <beast/container/aged_container.h>
#include <beast/chrono/abstract_clock.h>
#include <beast/utility/empty_base_optimization.h>
#include <boost/intrusive/list.hpp>
#include <boost/intrusive/unordered_set.hpp>
#include <beast/cxx14/algorithm.h> // <algorithm>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <beast/cxx14/type_traits.h> // <type_traits>
#include <utility>

/*

todo

- add constructor variations that take a bucket count

- review for noexcept and exception guarantees

- call the safe version of is_permutation that takes 4 iterators

*/

#ifndef beast_no_cxx14_is_permutation
#define beast_no_cxx14_is_permutation 1
#endif

namespace beast {
namespace detail {

/** associative container where each element is also indexed by time.

    this container mirrors the interface of the standard library unordered
    associative containers, with the addition that each element is associated
    with a `when` `time_point` which is obtained from the value of the clock's
    `now`. the function `touch` updates the time for an element to the current
    time as reported by the clock.

    an extra set of iterator types and member functions are provided in the
    `chronological` memberspace that allow traversal in temporal or reverse
    temporal order. this container is useful as a building block for caches
    whose items expire after a certain amount of time. the chronological
    iterators allow for fully customizable expiration strategies.

    @see aged_unordered_set, aged_unordered_multiset
    @see aged_unordered_map, aged_unordered_multimap
*/
template <
    bool ismulti,
    bool ismap,
    class key,
    class t,
    class clock = std::chrono::steady_clock,
    class hash = std::hash <key>,
    class keyequal = std::equal_to <key>,
    class allocator = std::allocator <
        typename std::conditional <ismap,
            std::pair <key const, t>,
                key>::type>
>
class aged_unordered_container
{
public:
    using clock_type = abstract_clock<clock>;
    using time_point = typename clock_type::time_point;
    using duration = typename clock_type::duration;
    using key_type = key;
    using mapped_type = t;
    using value_type = typename std::conditional <ismap,
        std::pair <key const, t>, key>::type;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    // introspection (for unit tests)
    using is_unordered = std::true_type;
    using is_multi = std::integral_constant <bool, ismulti>;
    using is_map = std::integral_constant <bool, ismap>;

private:
    static key const& extract (value_type const& value)
    {
        return aged_associative_container_extract_t <ismap> () (value);
    }

    // vfalco todo hoist to remove template argument dependencies
    struct element
        : boost::intrusive::unordered_set_base_hook <
            boost::intrusive::link_mode <
                boost::intrusive::normal_link>
            >
        , boost::intrusive::list_base_hook <
            boost::intrusive::link_mode <
                boost::intrusive::normal_link>
            >
    {
        // stash types here so the iterator doesn't
        // need to see the container declaration.
        struct stashed
        {
            typedef typename aged_unordered_container::value_type value_type;
            typedef typename aged_unordered_container::time_point time_point;
        };

        element (
            time_point const& when_,
            value_type const& value_)
            : value (value_)
            , when (when_)
        {
        }

        element (
            time_point const& when_,
            value_type&& value_)
            : value (std::move (value_))
            , when (when_)
        {
        }

        template <
            class... args,
            class = typename std::enable_if <
                std::is_constructible <value_type,
                    args...>::value>::type
        >
        element (time_point const& when_, args&&... args)
            : value (std::forward <args> (args)...)
            , when (when_)
        {
        }

        value_type value;
        time_point when;
    };

    // vfalco todo hoist to remove template argument dependencies
    class valuehash
        : private empty_base_optimization <hash>
        , public std::unary_function <element, std::size_t>
    {
    public:
        valuehash ()
        {
        }

        valuehash (hash const& hash)
            : empty_base_optimization <hash> (hash)
        {
        }

        std::size_t operator() (element const& e) const
        {
            return this->member() (extract (e.value));
        }

        hash& hash_function()
        {
            return this->member();
        }

        hash const& hash_function() const
        {
            return this->member();
        }
    };

    // compares value_type against element, used in find/insert_check
    // vfalco todo hoist to remove template argument dependencies
    class keyvalueequal
        : private empty_base_optimization <keyequal>
        , public std::binary_function <key, element, bool>
    {
    public:
        keyvalueequal ()
        {
        }

        keyvalueequal (keyequal const& keyequal)
            : empty_base_optimization <keyequal> (keyequal)
        {
        }

        // vfalco note we might want only to enable these overloads
        //                if keyequal has is_transparent
#if 0
        template <class k>
        bool operator() (k const& k, element const& e) const
        {
            return this->member() (k, extract (e.value));
        }

        template <class k>
        bool operator() (element const& e, k const& k) const
        {
            return this->member() (extract (e.value), k);
        }
#endif

        bool operator() (key const& k, element const& e) const
        {
            return this->member() (k, extract (e.value));
        }

        bool operator() (element const& e, key const& k) const
        {
            return this->member() (extract (e.value), k);
        }

        bool operator() (element const& lhs, element const& rhs) const
        {
            return this->member() (extract (lhs.value), extract (rhs.value));
        }

        keyequal& key_eq()
        {
            return this->member();
        }

        keyequal const& key_eq() const
        {
            return this->member();
        }
    };

    typedef typename boost::intrusive::make_list <element,
        boost::intrusive::constant_time_size <false>
            >::type list_type;

    typedef typename std::conditional <
        ismulti,
        typename boost::intrusive::make_unordered_multiset <element,
            boost::intrusive::constant_time_size <true>,
            boost::intrusive::hash <valuehash>,
            boost::intrusive::equal <keyvalueequal>,
            boost::intrusive::cache_begin <true>
                    >::type,
        typename boost::intrusive::make_unordered_set <element,
            boost::intrusive::constant_time_size <true>,
            boost::intrusive::hash <valuehash>,
            boost::intrusive::equal <keyvalueequal>,
            boost::intrusive::cache_begin <true>
                    >::type
        >::type cont_type;

    typedef typename cont_type::bucket_type bucket_type;
    typedef typename cont_type::bucket_traits bucket_traits;

    typedef typename std::allocator_traits <
        allocator>::template rebind_alloc <element> elementallocator;

    using elementallocatortraits = std::allocator_traits <elementallocator>;

    typedef typename std::allocator_traits <
        allocator>::template rebind_alloc <element> bucketallocator;

    using bucketallocatortraits = std::allocator_traits <bucketallocator>;

    class config_t
        : private valuehash
        , private keyvalueequal
        , private empty_base_optimization <elementallocator>
    {
    public:
        explicit config_t (
            clock_type& clock_)
            : clock (clock_)
        {
        }

        config_t (
            clock_type& clock_,
            hash const& hash)
            : valuehash (hash)
            , clock (clock_)
        {
        }

        config_t (
            clock_type& clock_,
            keyequal const& keyequal)
            : keyvalueequal (keyequal)
            , clock (clock_)
        {
        }

        config_t (
            clock_type& clock_,
            allocator const& alloc_)
            : empty_base_optimization <elementallocator> (alloc_)
            , clock (clock_)
        {
        }

        config_t (
            clock_type& clock_,
            hash const& hash,
            keyequal const& keyequal)
            : valuehash (hash)
            , keyvalueequal (keyequal)
            , clock (clock_)
        {
        }

        config_t (
            clock_type& clock_,
            hash const& hash,
            allocator const& alloc_)
            : valuehash (hash)
            , empty_base_optimization <elementallocator> (alloc_)
            , clock (clock_)
        {
        }

        config_t (
            clock_type& clock_,
            keyequal const& keyequal,
            allocator const& alloc_)
            : keyvalueequal (keyequal)
            , empty_base_optimization <elementallocator> (alloc_)
            , clock (clock_)
        {
        }

        config_t (
            clock_type& clock_,
            hash const& hash,
            keyequal const& keyequal,
            allocator const& alloc_)
            : valuehash (hash)
            , keyvalueequal (keyequal)
            , empty_base_optimization <elementallocator> (alloc_)
            , clock (clock_)
        {
        }

        config_t (config_t const& other)
            : valuehash (other.hash_function())
            , keyvalueequal (other.key_eq())
            , empty_base_optimization <elementallocator> (
                elementallocatortraits::
                    select_on_container_copy_construction (
                        other.alloc()))
            , clock (other.clock)
        {
        }

        config_t (config_t const& other, allocator const& alloc)
            : valuehash (other.hash_function())
            , keyvalueequal (other.key_eq())
            , empty_base_optimization <elementallocator> (alloc)
            , clock (other.clock)
        {
        }

        config_t (config_t&& other)
            : valuehash (std::move (other.hash_function()))
            , keyvalueequal (std::move (other.key_eq()))
            , empty_base_optimization <elementallocator> (
                std::move (other.alloc()))
            , clock (other.clock)
        {
        }

        config_t (config_t&& other, allocator const& alloc)
            : valuehash (std::move (other.hash_function()))
            , keyvalueequal (std::move (other.key_eq()))
            , empty_base_optimization <elementallocator> (alloc)
            , clock (other.clock)
        {
        }

        config_t& operator= (config_t const& other)
        {
            hash_function() = other.hash_function();
            key_eq() = other.key_eq();
            alloc() = other.alloc();
            clock = other.clock;
            return *this;
        }

        config_t& operator= (config_t&& other)
        {
            hash_function() = std::move (other.hash_function());
            key_eq() = std::move (other.key_eq());
            alloc() = std::move (other.alloc());
            clock = other.clock;
            return *this;
        }

        valuehash& value_hash()
        {
            return *this;
        }

        valuehash const& value_hash() const
        {
            return *this;
        }

        hash& hash_function()
        {
            return valuehash::hash_function();
        }

        hash const& hash_function() const
        {
            return valuehash::hash_function();
        }

        keyvalueequal& key_value_equal()
        {
            return *this;
        }

        keyvalueequal const& key_value_equal() const
        {
            return *this;
        }

        keyequal& key_eq()
        {
            return key_value_equal().key_eq();
        }

        keyequal const& key_eq() const
        {
            return key_value_equal().key_eq();
        }

        elementallocator& alloc()
        {
            return empty_base_optimization <
                elementallocator>::member();
        }

        elementallocator const& alloc() const
        {
            return empty_base_optimization <
                elementallocator>::member();
        }

        std::reference_wrapper <clock_type> clock;
    };

    class buckets
    {
    public:
        typedef std::vector <
            bucket_type,
            typename std::allocator_traits <allocator>::
                template rebind_alloc <bucket_type>> vec_type;

        buckets ()
            : m_max_load_factor (1.f)
            , m_vec ()
        {
            m_vec.resize (
                cont_type::suggested_upper_bucket_count (0));
        }

        buckets (allocator const& alloc)
            : m_max_load_factor (1.f)
            , m_vec (alloc)
        {
            m_vec.resize (
                cont_type::suggested_upper_bucket_count (0));
        }

        operator bucket_traits()
        {
            return bucket_traits (&m_vec[0], m_vec.size());
        }

        void clear()
        {
            m_vec.clear();
        }

        size_type max_bucket_count() const
        {
            return m_vec.max_size();
        }

        float& max_load_factor()
        {
            return m_max_load_factor;
        }

        float const& max_load_factor() const
        {
            return m_max_load_factor;
        }

        // count is the number of buckets
        template <class container>
        void rehash (size_type count, container& c)
        {
            size_type const size (m_vec.size());
            if (count == size)
                return;
            if (count > m_vec.capacity())
            {
                // need two vectors otherwise we
                // will destroy non-empty buckets.
                vec_type vec (m_vec.get_allocator());
                std::swap (m_vec, vec);
                m_vec.resize (count);
                c.rehash (bucket_traits (
                    &m_vec[0], m_vec.size()));
                return;
            }
            // rehash in place.
            if (count > size)
            {
                // this should not reallocate since
                // we checked capacity earlier.
                m_vec.resize (count);
                c.rehash (bucket_traits (
                    &m_vec[0], count));
                return;
            }
            // resize must happen after rehash otherwise
            // we might destroy non-empty buckets.
            c.rehash (bucket_traits (
                &m_vec[0], count));
            m_vec.resize (count);
        }

        // resize the buckets to accomodate at least n items.
        template <class container>
        void resize (size_type n, container& c)
        {
            size_type const suggested (
                cont_type::suggested_upper_bucket_count (n));
            rehash (suggested, c);
        }

    private:
        float m_max_load_factor;
        vec_type m_vec;
    };

    template <class... args>
    element* new_element (args&&... args)
    {
        struct deleter
        {
            std::reference_wrapper <elementallocator> a_;
            deleter (elementallocator& a)
                : a_(a)
            {
            }

            void
            operator()(element* p)
            {
                elementallocatortraits::deallocate (a_.get(), p, 1);
            }
        };

        std::unique_ptr <element, deleter> p (elementallocatortraits::allocate (
            m_config.alloc(), 1), deleter(m_config.alloc()));
        elementallocatortraits::construct (m_config.alloc(),
            p.get(), clock().now(), std::forward <args> (args)...);
        return p.release();
    }

    void delete_element (element const* p)
    {
        elementallocatortraits::destroy (m_config.alloc(), p);
        elementallocatortraits::deallocate (
            m_config.alloc(), const_cast<element*>(p), 1);
    }

    void unlink_and_delete_element (element const* p)
    {
        chronological.list.erase (
            chronological.list.iterator_to (*p));
        m_cont.erase (m_cont.iterator_to (*p));
        delete_element (p);
    }

public:
    typedef hash hasher;
    typedef keyequal key_equal;
    typedef allocator allocator_type;
    typedef value_type& reference;
    typedef value_type const& const_reference;
    typedef typename std::allocator_traits <
        allocator>::pointer pointer;
    typedef typename std::allocator_traits <
        allocator>::const_pointer const_pointer;

    // a set (that is, !ismap) iterator is aways const because the elements
    // of a set are immutable.
    typedef detail::aged_container_iterator <!ismap,
        typename cont_type::iterator> iterator;
    typedef detail::aged_container_iterator <true,
        typename cont_type::iterator> const_iterator;

    typedef detail::aged_container_iterator <!ismap,
        typename cont_type::local_iterator> local_iterator;
    typedef detail::aged_container_iterator <true,
        typename cont_type::local_iterator> const_local_iterator;

    //--------------------------------------------------------------------------
    //
    // chronological ordered iterators
    //
    // "memberspace"
    // http://accu.org/index.php/journals/1527
    //
    //--------------------------------------------------------------------------

    class chronological_t
    {
    public:
        // a set (that is, !ismap) iterator is aways const because the elements
        // of a set are immutable.
        typedef detail::aged_container_iterator <!ismap,
            typename list_type::iterator> iterator;
        typedef detail::aged_container_iterator <true,
            typename list_type::iterator> const_iterator;
        typedef detail::aged_container_iterator <!ismap,
            typename list_type::reverse_iterator> reverse_iterator;
        typedef detail::aged_container_iterator <true,
            typename list_type::reverse_iterator> const_reverse_iterator;

        iterator begin ()
         {
            return iterator (list.begin());
        }

        const_iterator begin () const
        {
            return const_iterator (list.begin ());
        }

        const_iterator cbegin() const
        {
            return const_iterator (list.begin ());
        }

        iterator end ()
        {
            return iterator (list.end ());
        }

        const_iterator end () const
        {
            return const_iterator (list.end ());
        }

        const_iterator cend () const
        {
            return const_iterator (list.end ());
        }

        reverse_iterator rbegin ()
        {
            return reverse_iterator (list.rbegin());
        }

        const_reverse_iterator rbegin () const
        {
            return const_reverse_iterator (list.rbegin ());
        }

        const_reverse_iterator crbegin() const
        {
            return const_reverse_iterator (list.rbegin ());
        }

        reverse_iterator rend ()
        {
            return reverse_iterator (list.rend ());
        }

        const_reverse_iterator rend () const
        {
            return const_reverse_iterator (list.rend ());
        }

        const_reverse_iterator crend () const
        {
            return const_reverse_iterator (list.rend ());
        }

        iterator iterator_to (value_type& value)
        {
            static_assert (std::is_standard_layout <element>::value,
                "must be standard layout");
            return list.iterator_to (*reinterpret_cast <element*>(
                 reinterpret_cast<uint8_t*>(&value)-((std::size_t)
                    std::addressof(((element*)0)->member))));
        }

        const_iterator iterator_to (value_type const& value) const
        {
            static_assert (std::is_standard_layout <element>::value,
                "must be standard layout");
            return list.iterator_to (*reinterpret_cast <element const*>(
                 reinterpret_cast<uint8_t const*>(&value)-((std::size_t)
                    std::addressof(((element*)0)->member))));
        }

    private:
        chronological_t ()
        {
        }

        chronological_t (chronological_t const&) = delete;
        chronological_t (chronological_t&&) = delete;

        friend class aged_unordered_container;
        list_type mutable list;
    } chronological;

    //--------------------------------------------------------------------------
    //
    // construction
    //
    //--------------------------------------------------------------------------

    explicit aged_unordered_container (clock_type& clock);

    aged_unordered_container (clock_type& clock, hash const& hash);

    aged_unordered_container (clock_type& clock,
        keyequal const& key_eq);

    aged_unordered_container (clock_type& clock,
        allocator const& alloc);

    aged_unordered_container (clock_type& clock,
        hash const& hash, keyequal const& key_eq);

    aged_unordered_container (clock_type& clock,
        hash const& hash, allocator const& alloc);

    aged_unordered_container (clock_type& clock,
        keyequal const& key_eq, allocator const& alloc);

    aged_unordered_container (
        clock_type& clock, hash const& hash, keyequal const& key_eq,
            allocator const& alloc);

    template <class inputit>
    aged_unordered_container (inputit first, inputit last,
        clock_type& clock);

    template <class inputit>
    aged_unordered_container (inputit first, inputit last,
        clock_type& clock, hash const& hash);

    template <class inputit>
    aged_unordered_container (inputit first, inputit last,
        clock_type& clock, keyequal const& key_eq);

    template <class inputit>
    aged_unordered_container (inputit first, inputit last,
        clock_type& clock, allocator const& alloc);

    template <class inputit>
    aged_unordered_container (inputit first, inputit last,
        clock_type& clock, hash const& hash, keyequal const& key_eq);

    template <class inputit>
    aged_unordered_container (inputit first, inputit last,
        clock_type& clock, hash const& hash, allocator const& alloc);

    template <class inputit>
    aged_unordered_container (inputit first, inputit last,
        clock_type& clock, keyequal const& key_eq,
            allocator const& alloc);

    template <class inputit>
    aged_unordered_container (inputit first, inputit last,
        clock_type& clock, hash const& hash, keyequal const& key_eq,
            allocator const& alloc);

    aged_unordered_container (aged_unordered_container const& other);

    aged_unordered_container (aged_unordered_container const& other,
        allocator const& alloc);

    aged_unordered_container (aged_unordered_container&& other);

    aged_unordered_container (aged_unordered_container&& other,
        allocator const& alloc);

    aged_unordered_container (std::initializer_list <value_type> init,
        clock_type& clock);

    aged_unordered_container (std::initializer_list <value_type> init,
        clock_type& clock, hash const& hash);

    aged_unordered_container (std::initializer_list <value_type> init,
        clock_type& clock, keyequal const& key_eq);

    aged_unordered_container (std::initializer_list <value_type> init,
        clock_type& clock, allocator const& alloc);

    aged_unordered_container (std::initializer_list <value_type> init,
        clock_type& clock, hash const& hash, keyequal const& key_eq);

    aged_unordered_container (std::initializer_list <value_type> init,
        clock_type& clock, hash const& hash, allocator const& alloc);

    aged_unordered_container (std::initializer_list <value_type> init,
        clock_type& clock, keyequal const& key_eq, allocator const& alloc);

    aged_unordered_container (std::initializer_list <value_type> init,
        clock_type& clock, hash const& hash, keyequal const& key_eq,
            allocator const& alloc);

    ~aged_unordered_container();

    aged_unordered_container& operator= (aged_unordered_container const& other);

    aged_unordered_container& operator= (aged_unordered_container&& other);

    aged_unordered_container& operator= (std::initializer_list <value_type> init);

    allocator_type get_allocator() const
    {
        return m_config.alloc();
    }

    clock_type& clock()
    {
        return m_config.clock;
    }

    clock_type const& clock() const
    {
        return m_config.clock;
    }

    //--------------------------------------------------------------------------
    //
    // element access (maps)
    //
    //--------------------------------------------------------------------------

    template <
        class k,
        bool maybe_multi = ismulti,
        bool maybe_map = ismap,
        class = typename std::enable_if <maybe_map && ! maybe_multi>::type>
    typename std::conditional <ismap, t, void*>::type&
    at (k const& k);

    template <
        class k,
        bool maybe_multi = ismulti,
        bool maybe_map = ismap,
        class = typename std::enable_if <maybe_map && ! maybe_multi>::type>
    typename std::conditional <ismap, t, void*>::type const&
    at (k const& k) const;

    template <
        bool maybe_multi = ismulti,
        bool maybe_map = ismap,
        class = typename std::enable_if <maybe_map && ! maybe_multi>::type>
    typename std::conditional <ismap, t, void*>::type&
    operator[] (key const& key);

    template <
        bool maybe_multi = ismulti,
        bool maybe_map = ismap,
        class = typename std::enable_if <maybe_map && ! maybe_multi>::type>
    typename std::conditional <ismap, t, void*>::type&
    operator[] (key&& key);

    //--------------------------------------------------------------------------
    //
    // iterators
    //
    //--------------------------------------------------------------------------

    iterator
    begin ()
    {
        return iterator (m_cont.begin());
    }

    const_iterator
    begin () const
    {
        return const_iterator (m_cont.begin ());
    }

    const_iterator
    cbegin() const
    {
        return const_iterator (m_cont.begin ());
    }

    iterator
    end ()
    {
        return iterator (m_cont.end ());
    }

    const_iterator
    end () const
    {
        return const_iterator (m_cont.end ());
    }

    const_iterator
    cend () const
    {
        return const_iterator (m_cont.end ());
    }

    iterator
    iterator_to (value_type& value)
    {
        static_assert (std::is_standard_layout <element>::value,
            "must be standard layout");
        return m_cont.iterator_to (*reinterpret_cast <element*>(
             reinterpret_cast<uint8_t*>(&value)-((std::size_t)
                std::addressof(((element*)0)->member))));
    }

    const_iterator
    iterator_to (value_type const& value) const
    {
        static_assert (std::is_standard_layout <element>::value,
            "must be standard layout");
        return m_cont.iterator_to (*reinterpret_cast <element const*>(
             reinterpret_cast<uint8_t const*>(&value)-((std::size_t)
                std::addressof(((element*)0)->member))));
    }

    //--------------------------------------------------------------------------
    //
    // capacity
    //
    //--------------------------------------------------------------------------

    bool empty() const noexcept
    {
        return m_cont.empty();
    }

    size_type size() const noexcept
    {
        return m_cont.size();
    }

    size_type max_size() const noexcept
    {
        return m_config.max_size();
    }

    //--------------------------------------------------------------------------
    //
    // modifiers
    //
    //--------------------------------------------------------------------------

    void clear();

    // map, set
    template <bool maybe_multi = ismulti>
    auto
    insert (value_type const& value) ->
        typename std::enable_if <! maybe_multi,
            std::pair <iterator, bool>>::type;

    // multimap, multiset
    template <bool maybe_multi = ismulti>
    auto
    insert (value_type const& value) ->
        typename std::enable_if <maybe_multi,
            iterator>::type;

    // map, set
    template <bool maybe_multi = ismulti, bool maybe_map = ismap>
    auto
    insert (value_type&& value) ->
        typename std::enable_if <! maybe_multi && ! maybe_map,
            std::pair <iterator, bool>>::type;

    // multimap, multiset
    template <bool maybe_multi = ismulti, bool maybe_map = ismap>
    auto
    insert (value_type&& value) ->
        typename std::enable_if <maybe_multi && ! maybe_map,
            iterator>::type;

    // map, set
    template <bool maybe_multi = ismulti>
    typename std::enable_if <! maybe_multi,
        iterator>::type
    insert (const_iterator /*hint*/, value_type const& value)
    {
        // hint is ignored but we provide the interface so
        // callers may use ordered and unordered interchangeably.
        return insert (value).first;
    }

    // multimap, multiset
    template <bool maybe_multi = ismulti>
    typename std::enable_if <maybe_multi,
        iterator>::type
    insert (const_iterator /*hint*/, value_type const& value)
    {
        // vfalco todo the hint could be used to let
        //             the client order equal ranges
        return insert (value);
    }

    // map, set
    template <bool maybe_multi = ismulti>
    typename std::enable_if <! maybe_multi,
        iterator>::type
    insert (const_iterator /*hint*/, value_type&& value)
    {
        // hint is ignored but we provide the interface so
        // callers may use ordered and unordered interchangeably.
        return insert (std::move (value)).first;
    }

    // multimap, multiset
    template <bool maybe_multi = ismulti>
    typename std::enable_if <maybe_multi,
        iterator>::type
    insert (const_iterator /*hint*/, value_type&& value)
    {
        // vfalco todo the hint could be used to let
        //             the client order equal ranges
        return insert (std::move (value));
    }

    // map, multimap
    template <
        class p,
        bool maybe_map = ismap
    >
    typename std::enable_if <maybe_map &&
        std::is_constructible <value_type, p&&>::value,
        typename std::conditional <ismulti,
            iterator, std::pair <iterator, bool>
        >::type
    >::type
    insert (p&& value)
    {
        return emplace (std::forward <p> (value));
    }

    // map, multimap
    template <
        class p,
        bool maybe_map = ismap
    >
    typename std::enable_if <maybe_map &&
        std::is_constructible <value_type, p&&>::value,
        typename std::conditional <ismulti,
            iterator, std::pair <iterator, bool>
        >::type
    >::type
    insert (const_iterator hint, p&& value)
    {
        return emplace_hint (hint, std::forward <p> (value));
    }

    template <class inputit>
    void insert (inputit first, inputit last)
    {
        insert (first, last,
            typename std::iterator_traits <
                inputit>::iterator_category());
    }

    void
    insert (std::initializer_list <value_type> init)
    {
        insert (init.begin(), init.end());
    }

    // set, map
    template <bool maybe_multi = ismulti, class... args>
    auto
    emplace (args&&... args) ->
        typename std::enable_if <! maybe_multi,
            std::pair <iterator, bool>>::type;

    // multiset, multimap
    template <bool maybe_multi = ismulti, class... args>
    auto
    emplace (args&&... args) ->
        typename std::enable_if <maybe_multi,
            iterator>::type;

    // set, map
    template <bool maybe_multi = ismulti, class... args>
    auto
    emplace_hint (const_iterator /*hint*/, args&&... args) ->
        typename std::enable_if <! maybe_multi,
            std::pair <iterator, bool>>::type;

    // multiset, multimap
    template <bool maybe_multi = ismulti, class... args>
    typename std::enable_if <maybe_multi,
        iterator>::type
    emplace_hint (const_iterator /*hint*/, args&&... args)
    {
        // vfalco todo the hint could be used for multi, to let
        //             the client order equal ranges
        return emplace <maybe_multi> (
            std::forward <args> (args)...);
    }

    template <bool is_const, class iterator, class base>
    detail::aged_container_iterator <false, iterator, base>
    erase (detail::aged_container_iterator <
        is_const, iterator, base> pos);

    template <bool is_const, class iterator, class base>
    detail::aged_container_iterator <false, iterator, base>
    erase (detail::aged_container_iterator <
        is_const, iterator, base> first,
            detail::aged_container_iterator <
                is_const, iterator, base> last);

    template <class k>
    auto
    erase (k const& k) ->
        size_type;

    void
    swap (aged_unordered_container& other) noexcept;

    template <bool is_const, class iterator, class base>
    void
    touch (detail::aged_container_iterator <
        is_const, iterator, base> pos)
    {
        touch (pos, clock().now());
    }

    template <class k>
    auto
    touch (k const& k) ->
        size_type;

    //--------------------------------------------------------------------------
    //
    // lookup
    //
    //--------------------------------------------------------------------------

    // vfalco todo respect is_transparent (c++14)
    template <class k>
    size_type
    count (k const& k) const
    {
        return m_cont.count (k, std::cref (m_config.hash_function()),
            std::cref (m_config.key_value_equal()));
    }

    // vfalco todo respect is_transparent (c++14)
    template <class k>
    iterator
    find (k const& k)
    {
        return iterator (m_cont.find (k,
            std::cref (m_config.hash_function()),
                std::cref (m_config.key_value_equal())));
    }

    // vfalco todo respect is_transparent (c++14)
    template <class k>
    const_iterator
    find (k const& k) const
    {
        return const_iterator (m_cont.find (k,
            std::cref (m_config.hash_function()),
                std::cref (m_config.key_value_equal())));
    }

    // vfalco todo respect is_transparent (c++14)
    template <class k>
    std::pair <iterator, iterator>
    equal_range (k const& k)
    {
        auto const r (m_cont.equal_range (k,
            std::cref (m_config.hash_function()),
                std::cref (m_config.key_value_equal())));
        return std::make_pair (iterator (r.first),
            iterator (r.second));
    }

    // vfalco todo respect is_transparent (c++14)
    template <class k>
    std::pair <const_iterator, const_iterator>
    equal_range (k const& k) const
    {
        auto const r (m_cont.equal_range (k,
            std::cref (m_config.hash_function()),
                std::cref (m_config.key_value_equal())));
        return std::make_pair (const_iterator (r.first),
            const_iterator (r.second));
    }

    //--------------------------------------------------------------------------
    //
    // bucket interface
    //
    //--------------------------------------------------------------------------

    local_iterator begin (size_type n)
    {
        return local_iterator (m_cont.begin (n));
    }

    const_local_iterator begin (size_type n) const
    {
        return const_local_iterator (m_cont.begin (n));
    }

    const_local_iterator cbegin (size_type n) const
    {
        return const_local_iterator (m_cont.begin (n));
    }

    local_iterator end (size_type n)
    {
        return local_iterator (m_cont.end (n));
    }

    const_local_iterator end (size_type n) const
    {
        return const_local_iterator (m_cont.end (n));
    }

    const_local_iterator cend (size_type n) const
    {
        return const_local_iterator (m_cont.end (n));
    }

    size_type bucket_count() const
    {
        return m_cont.bucket_count();
    }

    size_type max_bucket_count() const
    {
        return m_buck.max_bucket_count();
    }

    size_type bucket_size (size_type n) const
    {
        return m_cont.bucket_size (n);
    }

    size_type bucket (key const& k) const
    {
        assert (bucket_count() != 0);
        return m_cont.bucket (k,
            std::cref (m_config.hash_function()));
    }

    //--------------------------------------------------------------------------
    //
    // hash policy
    //
    //--------------------------------------------------------------------------

    float load_factor() const
    {
        return size() /
            static_cast <float> (m_cont.bucket_count());
    }

    float max_load_factor() const
    {
        return m_buck.max_load_factor();
    }

    void max_load_factor (float ml)
    {
        m_buck.max_load_factor () =
            std::max (ml, m_buck.max_load_factor());
    }

    void rehash (size_type count)
    {
        count = std::max (count,
            size_type (size() / max_load_factor()));
        m_buck.rehash (count, m_cont);
    }

    void reserve (size_type count)
    {
        rehash (std::ceil (count / max_load_factor()));
    }

    //--------------------------------------------------------------------------
    //
    // observers
    //
    //--------------------------------------------------------------------------

    hasher const& hash_function() const
    {
        return m_config.hash_function();
    }

    key_equal const& key_eq () const
    {
        return m_config.key_eq();
    }

    //--------------------------------------------------------------------------
    //
    // comparison
    //
    //--------------------------------------------------------------------------

    // this differs from the standard in that the comparison
    // is only done on the key portion of the value type, ignoring
    // the mapped type.
    //
    template <
        bool otherismap,
        class otherkey,
        class othert,
        class otherduration,
        class otherhash,
        class otherallocator,
        bool maybe_multi = ismulti
    >
    typename std::enable_if <! maybe_multi, bool>::type
    operator== (
        aged_unordered_container <false, otherismap,
            otherkey, othert, otherduration, otherhash, keyequal,
                otherallocator> const& other) const;

    template <
        bool otherismap,
        class otherkey,
        class othert,
        class otherduration,
        class otherhash,
        class otherallocator,
        bool maybe_multi = ismulti
    >
    typename std::enable_if <maybe_multi, bool>::type
    operator== (
        aged_unordered_container <true, otherismap,
            otherkey, othert, otherduration, otherhash, keyequal,
                otherallocator> const& other) const;

    template <
        bool otherismulti,
        bool otherismap,
        class otherkey,
        class othert,
        class otherduration,
        class otherhash,
        class otherallocator
    >
    bool operator!= (
        aged_unordered_container <otherismulti, otherismap,
            otherkey, othert, otherduration, otherhash, keyequal,
                otherallocator> const& other) const
    {
        return ! (this->operator== (other));
    }

private:
    bool
    would_exceed (size_type additional) const
    {
        return size() + additional >
            bucket_count() * max_load_factor();
    }

    void
    maybe_rehash (size_type additional)
    {
        if (would_exceed (additional))
            m_buck.resize (size() + additional, m_cont);
        assert (load_factor() <= max_load_factor());
    }

    // map, set
    template <bool maybe_multi = ismulti>
    auto
    insert_unchecked (value_type const& value) ->
        typename std::enable_if <! maybe_multi,
            std::pair <iterator, bool>>::type;

    // multimap, multiset
    template <bool maybe_multi = ismulti>
    auto
    insert_unchecked (value_type const& value) ->
        typename std::enable_if <maybe_multi,
            iterator>::type;

    template <class inputit>
    void
    insert_unchecked (inputit first, inputit last)
    {
        for (; first != last; ++first)
            insert_unchecked (*first);
    }

    template <class inputit>
    void
    insert (inputit first, inputit last,
        std::input_iterator_tag)
    {
        for (; first != last; ++first)
            insert (*first);
    }

    template <class inputit>
    void
    insert (inputit first, inputit last,
        std::random_access_iterator_tag)
    {
        auto const n (std::distance (first, last));
        maybe_rehash (n);
        insert_unchecked (first, last);
    }

    template <bool is_const, class iterator, class base>
    void
    touch (detail::aged_container_iterator <
        is_const, iterator, base> pos,
            typename clock_type::time_point const& now)
    {
        auto& e (*pos.iterator());
        e.when = now;
        chronological.list.erase (chronological.list.iterator_to (e));
        chronological.list.push_back (e);
    }

    template <bool maybe_propagate = std::allocator_traits <
        allocator>::propagate_on_container_swap::value>
    typename std::enable_if <maybe_propagate>::type
    swap_data (aged_unordered_container& other) noexcept
    {
        std::swap (m_config.key_compare(), other.m_config.key_compare());
        std::swap (m_config.alloc(), other.m_config.alloc());
        std::swap (m_config.clock, other.m_config.clock);
    }

    template <bool maybe_propagate = std::allocator_traits <
        allocator>::propagate_on_container_swap::value>
    typename std::enable_if <! maybe_propagate>::type
    swap_data (aged_unordered_container& other) noexcept
    {
        std::swap (m_config.key_compare(), other.m_config.key_compare());
        std::swap (m_config.clock, other.m_config.clock);
    }

private:
    config_t m_config;
    buckets m_buck;
    cont_type mutable m_cont;
};

//------------------------------------------------------------------------------

template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
aged_unordered_container <ismulti, ismap, key, t, clock,
    hash, keyequal, allocator>::
aged_unordered_container (
    clock_type& clock)
    : m_config (clock)
    , m_cont (m_buck,
        std::cref (m_config.value_hash()),
            std::cref (m_config.key_value_equal()))
{
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
aged_unordered_container <ismulti, ismap, key, t, clock,
    hash, keyequal, allocator>::
aged_unordered_container (
    clock_type& clock,
    hash const& hash)
    : m_config (clock, hash)
    , m_cont (m_buck,
        std::cref (m_config.value_hash()),
            std::cref (m_config.key_value_equal()))
{
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
aged_unordered_container <ismulti, ismap, key, t, clock,
    hash, keyequal, allocator>::
aged_unordered_container (
    clock_type& clock,
    keyequal const& key_eq)
    : m_config (clock, key_eq)
    , m_cont (m_buck,
        std::cref (m_config.value_hash()),
            std::cref (m_config.key_value_equal()))
{
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
aged_unordered_container <ismulti, ismap, key, t, clock,
    hash, keyequal, allocator>::
aged_unordered_container (
    clock_type& clock,
    allocator const& alloc)
    : m_config (clock, alloc)
    , m_buck (alloc)
    , m_cont (m_buck,
        std::cref (m_config.value_hash()),
            std::cref (m_config.key_value_equal()))
{
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
aged_unordered_container <ismulti, ismap, key, t, clock,
    hash, keyequal, allocator>::
aged_unordered_container (
    clock_type& clock,
    hash const& hash,
    keyequal const& key_eq)
    : m_config (clock, hash, key_eq)
    , m_cont (m_buck,
        std::cref (m_config.value_hash()),
            std::cref (m_config.key_value_equal()))
{
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
aged_unordered_container <ismulti, ismap, key, t, clock,
    hash, keyequal, allocator>::
aged_unordered_container (
    clock_type& clock,
    hash const& hash,
    allocator const& alloc)
    : m_config (clock, hash, alloc)
    , m_buck (alloc)
    , m_cont (m_buck,
        std::cref (m_config.value_hash()),
            std::cref (m_config.key_value_equal()))
{
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
aged_unordered_container <ismulti, ismap, key, t, clock,
    hash, keyequal, allocator>::
aged_unordered_container (
    clock_type& clock,
    keyequal const& key_eq,
    allocator const& alloc)
    : m_config (clock, key_eq, alloc)
    , m_buck (alloc)
    , m_cont (m_buck,
        std::cref (m_config.value_hash()),
            std::cref (m_config.key_value_equal()))
{
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
aged_unordered_container <ismulti, ismap, key, t, clock,
    hash, keyequal, allocator>::
aged_unordered_container (
    clock_type& clock,
    hash const& hash,
    keyequal const& key_eq,
    allocator const& alloc)
    : m_config (clock, hash, key_eq, alloc)
    , m_buck (alloc)
    , m_cont (m_buck,
        std::cref (m_config.value_hash()),
            std::cref (m_config.key_value_equal()))
{
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
template <class inputit>
aged_unordered_container <ismulti, ismap, key, t, clock,
    hash, keyequal, allocator>::
aged_unordered_container (inputit first, inputit last,
    clock_type& clock)
    : m_config (clock)
    , m_cont (m_buck,
        std::cref (m_config.value_hash()),
            std::cref (m_config.key_value_equal()))
{
    insert (first, last);
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
template <class inputit>
aged_unordered_container <ismulti, ismap, key, t, clock,
    hash, keyequal, allocator>::
aged_unordered_container (inputit first, inputit last,
    clock_type& clock,
    hash const& hash)
    : m_config (clock, hash)
    , m_cont (m_buck,
        std::cref (m_config.value_hash()),
            std::cref (m_config.key_value_equal()))
{
    insert (first, last);
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
template <class inputit>
aged_unordered_container <ismulti, ismap, key, t, clock,
    hash, keyequal, allocator>::
aged_unordered_container (inputit first, inputit last,
    clock_type& clock,
    keyequal const& key_eq)
    : m_config (clock, key_eq)
    , m_cont (m_buck,
        std::cref (m_config.value_hash()),
            std::cref (m_config.key_value_equal()))
{
    insert (first, last);
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
template <class inputit>
aged_unordered_container <ismulti, ismap, key, t, clock,
    hash, keyequal, allocator>::
aged_unordered_container (inputit first, inputit last,
    clock_type& clock,
    allocator const& alloc)
    : m_config (clock, alloc)
    , m_buck (alloc)
    , m_cont (m_buck,
        std::cref (m_config.value_hash()),
            std::cref (m_config.key_value_equal()))
{
    insert (first, last);
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
template <class inputit>
aged_unordered_container <ismulti, ismap, key, t, clock,
    hash, keyequal, allocator>::
aged_unordered_container (inputit first, inputit last,
    clock_type& clock,
    hash const& hash,
    keyequal const& key_eq)
    : m_config (clock, hash, key_eq)
    , m_cont (m_buck,
        std::cref (m_config.value_hash()),
            std::cref (m_config.key_value_equal()))
{
    insert (first, last);
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
template <class inputit>
aged_unordered_container <ismulti, ismap, key, t, clock,
    hash, keyequal, allocator>::
aged_unordered_container (inputit first, inputit last,
    clock_type& clock,
    hash const& hash,
    allocator const& alloc)
    : m_config (clock, hash, alloc)
    , m_buck (alloc)
    , m_cont (m_buck,
        std::cref (m_config.value_hash()),
            std::cref (m_config.key_value_equal()))
{
    insert (first, last);
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
template <class inputit>
aged_unordered_container <ismulti, ismap, key, t, clock,
    hash, keyequal, allocator>::
aged_unordered_container (inputit first, inputit last,
    clock_type& clock,
    keyequal const& key_eq,
    allocator const& alloc)
    : m_config (clock, key_eq, alloc)
    , m_buck (alloc)
    , m_cont (m_buck,
        std::cref (m_config.value_hash()),
            std::cref (m_config.key_value_equal()))
{
    insert (first, last);
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
template <class inputit>
aged_unordered_container <ismulti, ismap, key, t, clock,
    hash, keyequal, allocator>::
aged_unordered_container (inputit first, inputit last,
    clock_type& clock,
    hash const& hash,
    keyequal const& key_eq,
    allocator const& alloc)
    : m_config (clock, hash, key_eq, alloc)
    , m_buck (alloc)
    , m_cont (m_buck,
        std::cref (m_config.value_hash()),
            std::cref (m_config.key_value_equal()))
{
    insert (first, last);
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
aged_unordered_container <ismulti, ismap, key, t, clock,
    hash, keyequal, allocator>::
aged_unordered_container (aged_unordered_container const& other)
    : m_config (other.m_config)
    , m_buck (m_config.alloc())
    , m_cont (m_buck,
        std::cref (m_config.value_hash()),
            std::cref (m_config.key_value_equal()))
{
    insert (other.cbegin(), other.cend());
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
aged_unordered_container <ismulti, ismap, key, t, clock,
    hash, keyequal, allocator>::
aged_unordered_container (aged_unordered_container const& other,
    allocator const& alloc)
    : m_config (other.m_config, alloc)
    , m_buck (alloc)
    , m_cont (m_buck,
        std::cref (m_config.value_hash()),
            std::cref (m_config.key_value_equal()))
{
    insert (other.cbegin(), other.cend());
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
aged_unordered_container <ismulti, ismap, key, t, clock,
    hash, keyequal, allocator>::
aged_unordered_container (aged_unordered_container&& other)
    : m_config (std::move (other.m_config))
    , m_buck (std::move (other.m_buck))
    , m_cont (std::move (other.m_cont))
{
    chronological.list = std::move (other.chronological.list);
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
aged_unordered_container <ismulti, ismap, key, t, clock,
    hash, keyequal, allocator>::
aged_unordered_container (aged_unordered_container&& other,
    allocator const& alloc)
    : m_config (std::move (other.m_config), alloc)
    , m_buck (alloc)
    , m_cont (m_buck,
        std::cref (m_config.value_hash()),
            std::cref (m_config.key_value_equal()))
{
    insert (other.cbegin(), other.cend());
    other.clear ();
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
aged_unordered_container <ismulti, ismap, key, t, clock,
    hash, keyequal, allocator>::
aged_unordered_container (std::initializer_list <value_type> init,
    clock_type& clock)
    : m_config (clock)
    , m_cont (m_buck,
        std::cref (m_config.value_hash()),
            std::cref (m_config.key_value_equal()))
{
    insert (init.begin(), init.end());
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
aged_unordered_container <ismulti, ismap, key, t, clock,
    hash, keyequal, allocator>::
aged_unordered_container (std::initializer_list <value_type> init,
    clock_type& clock,
    hash const& hash)
    : m_config (clock, hash)
    , m_cont (m_buck,
        std::cref (m_config.value_hash()),
            std::cref (m_config.key_value_equal()))
{
    insert (init.begin(), init.end());
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
aged_unordered_container <ismulti, ismap, key, t, clock,
    hash, keyequal, allocator>::
aged_unordered_container (std::initializer_list <value_type> init,
    clock_type& clock,
    keyequal const& key_eq)
    : m_config (clock, key_eq)
    , m_cont (m_buck,
        std::cref (m_config.value_hash()),
            std::cref (m_config.key_value_equal()))
{
    insert (init.begin(), init.end());
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
aged_unordered_container <ismulti, ismap, key, t, clock,
    hash, keyequal, allocator>::
aged_unordered_container (std::initializer_list <value_type> init,
    clock_type& clock,
    allocator const& alloc)
    : m_config (clock, alloc)
    , m_buck (alloc)
    , m_cont (m_buck,
        std::cref (m_config.value_hash()),
            std::cref (m_config.key_value_equal()))
{
    insert (init.begin(), init.end());
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
aged_unordered_container <ismulti, ismap, key, t, clock,
    hash, keyequal, allocator>::
aged_unordered_container (std::initializer_list <value_type> init,
    clock_type& clock,
    hash const& hash,
    keyequal const& key_eq)
    : m_config (clock, hash, key_eq)
    , m_cont (m_buck,
        std::cref (m_config.value_hash()),
            std::cref (m_config.key_value_equal()))
{
    insert (init.begin(), init.end());
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
aged_unordered_container <ismulti, ismap, key, t, clock,
    hash, keyequal, allocator>::
aged_unordered_container (std::initializer_list <value_type> init,
    clock_type& clock,
    hash const& hash,
    allocator const& alloc)
    : m_config (clock, hash, alloc)
    , m_buck (alloc)
    , m_cont (m_buck,
        std::cref (m_config.value_hash()),
            std::cref (m_config.key_value_equal()))
{
    insert (init.begin(), init.end());
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
aged_unordered_container <ismulti, ismap, key, t, clock,
    hash, keyequal, allocator>::
aged_unordered_container (std::initializer_list <value_type> init,
    clock_type& clock,
    keyequal const& key_eq,
    allocator const& alloc)
    : m_config (clock, key_eq, alloc)
    , m_buck (alloc)
    , m_cont (m_buck,
        std::cref (m_config.value_hash()),
            std::cref (m_config.key_value_equal()))
{
    insert (init.begin(), init.end());
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
aged_unordered_container <ismulti, ismap, key, t, clock,
    hash, keyequal, allocator>::
aged_unordered_container (std::initializer_list <value_type> init,
    clock_type& clock,
    hash const& hash,
    keyequal const& key_eq,
    allocator const& alloc)
    : m_config (clock, hash, key_eq, alloc)
    , m_buck (alloc)
    , m_cont (m_buck,
        std::cref (m_config.value_hash()),
            std::cref (m_config.key_value_equal()))
{
    insert (init.begin(), init.end());
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
aged_unordered_container <ismulti, ismap, key, t, clock,
    hash, keyequal, allocator>::
~aged_unordered_container()
{
    clear();
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
auto
aged_unordered_container <ismulti, ismap, key, t, clock,
    hash, keyequal, allocator>::
operator= (aged_unordered_container const& other)
    -> aged_unordered_container&
{
    if (this != &other)
    {
        size_type const n (other.size());
        clear();
        m_config = other.m_config;
        m_buck = buckets (m_config.alloc());
        maybe_rehash (n);
        insert_unchecked (other.begin(), other.end());
    }
    return *this;
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
auto
aged_unordered_container <ismulti, ismap, key, t, clock,
    hash, keyequal, allocator>::
operator= (aged_unordered_container&& other) ->
    aged_unordered_container&
{
    size_type const n (other.size());
    clear();
    m_config = std::move (other.m_config);
    m_buck = buckets (m_config.alloc());
    maybe_rehash (n);
    insert_unchecked (other.begin(), other.end());
    other.clear();
    return *this;
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
auto
aged_unordered_container <ismulti, ismap, key, t, clock,
    hash, keyequal, allocator>::
operator= (std::initializer_list <value_type> init) ->
    aged_unordered_container&
{
    clear ();
    insert (init);
    return *this;
}

//------------------------------------------------------------------------------

template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
template <class k, bool maybe_multi, bool maybe_map, class>
typename std::conditional <ismap, t, void*>::type&
aged_unordered_container <ismulti, ismap, key, t, clock,
    hash, keyequal, allocator>::
at (k const& k)
{
    auto const iter (m_cont.find (k,
        std::cref (m_config.hash_function()),
            std::cref (m_config.key_value_equal())));
    if (iter == m_cont.end())
        throw std::out_of_range ("key not found");
    return iter->value.second;
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
template <class k, bool maybe_multi, bool maybe_map, class>
typename std::conditional <ismap, t, void*>::type const&
aged_unordered_container <ismulti, ismap, key, t, clock,
    hash, keyequal, allocator>::
at (k const& k) const
{
    auto const iter (m_cont.find (k,
        std::cref (m_config.hash_function()),
            std::cref (m_config.key_value_equal())));
    if (iter == m_cont.end())
        throw std::out_of_range ("key not found");
    return iter->value.second;
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
template <bool maybe_multi, bool maybe_map, class>
typename std::conditional <ismap, t, void*>::type&
aged_unordered_container <ismulti, ismap, key, t, clock,
    hash, keyequal, allocator>::
operator[] (key const& key)
{
    maybe_rehash (1);
    typename cont_type::insert_commit_data d;
    auto const result (m_cont.insert_check (key,
        std::cref (m_config.hash_function()),
            std::cref (m_config.key_value_equal()), d));
    if (result.second)
    {
        element* const p (new_element (
            std::piecewise_construct,
                std::forward_as_tuple (key),
                    std::forward_as_tuple ()));
        m_cont.insert_commit (*p, d);
        chronological.list.push_back (*p);
        return p->value.second;
    }
    return result.first->value.second;
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
template <bool maybe_multi, bool maybe_map, class>
typename std::conditional <ismap, t, void*>::type&
aged_unordered_container <ismulti, ismap, key, t, clock,
    hash, keyequal, allocator>::
operator[] (key&& key)
{
    maybe_rehash (1);
    typename cont_type::insert_commit_data d;
    auto const result (m_cont.insert_check (key,
        std::cref (m_config.hash_function()),
            std::cref (m_config.key_value_equal()), d));
    if (result.second)
    {
        element* const p (new_element (
            std::piecewise_construct,
                std::forward_as_tuple (std::move (key)),
                    std::forward_as_tuple ()));
        m_cont.insert_commit (*p, d);
        chronological.list.push_back (*p);
        return p->value.second;
    }
    return result.first->value.second;
}

//------------------------------------------------------------------------------

template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
void
aged_unordered_container <ismulti, ismap, key, t, clock,
    hash, keyequal, allocator>::
clear()
{
    for (auto iter (chronological.list.begin());
        iter != chronological.list.end();)
        unlink_and_delete_element (&*iter++);
    chronological.list.clear();
    m_cont.clear();
    m_buck.clear();
}

// map, set
template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
template <bool maybe_multi>
auto
aged_unordered_container <ismulti, ismap, key, t, clock,
    hash, keyequal, allocator>::
insert (value_type const& value) ->
    typename std::enable_if <! maybe_multi,
        std::pair <iterator, bool>>::type
{
    maybe_rehash (1);
    typename cont_type::insert_commit_data d;
    auto const result (m_cont.insert_check (extract (value),
        std::cref (m_config.hash_function()),
            std::cref (m_config.key_value_equal()), d));
    if (result.second)
    {
        element* const p (new_element (value));
        auto const iter (m_cont.insert_commit (*p, d));
        chronological.list.push_back (*p);
        return std::make_pair (iterator (iter), true);
    }
    return std::make_pair (iterator (result.first), false);
}

// multimap, multiset
template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
template <bool maybe_multi>
auto
aged_unordered_container <ismulti, ismap, key, t, clock,
    hash, keyequal, allocator>::
insert (value_type const& value) ->
    typename std::enable_if <maybe_multi,
        iterator>::type
{
    maybe_rehash (1);
    element* const p (new_element (value));
    chronological.list.push_back (*p);
    auto const iter (m_cont.insert (*p));
    return iterator (iter);
}

// map, set
template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
template <bool maybe_multi, bool maybe_map>
auto
aged_unordered_container <ismulti, ismap, key, t, clock,
    hash, keyequal, allocator>::
insert (value_type&& value) ->
    typename std::enable_if <! maybe_multi && ! maybe_map,
        std::pair <iterator, bool>>::type
{
    maybe_rehash (1);
    typename cont_type::insert_commit_data d;
    auto const result (m_cont.insert_check (extract (value),
        std::cref (m_config.hash_function()),
            std::cref (m_config.key_value_equal()), d));
    if (result.second)
    {
        element* const p (new_element (std::move (value)));
        auto const iter (m_cont.insert_commit (*p, d));
        chronological.list.push_back (*p);
        return std::make_pair (iterator (iter), true);
    }
    return std::make_pair (iterator (result.first), false);
}

// multimap, multiset
template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
template <bool maybe_multi, bool maybe_map>
auto
aged_unordered_container <ismulti, ismap, key, t, clock,
    hash, keyequal, allocator>::
insert (value_type&& value) ->
    typename std::enable_if <maybe_multi && ! maybe_map,
        iterator>::type
{
    maybe_rehash (1);
    element* const p (new_element (std::move (value)));
    chronological.list.push_back (*p);
    auto const iter (m_cont.insert (*p));
    return iterator (iter);
}

#if 1 // use insert() instead of insert_check() insert_commit()
// set, map
template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
template <bool maybe_multi, class... args>
auto
aged_unordered_container <ismulti, ismap, key, t, clock,
    hash, keyequal, allocator>::
emplace (args&&... args) ->
    typename std::enable_if <! maybe_multi,
        std::pair <iterator, bool>>::type
{
    maybe_rehash (1);
    // vfalco note its unfortunate that we need to
    //             construct element here
    element* const p (new_element (std::forward <args> (args)...));
    auto const result (m_cont.insert (*p));
    if (result.second)
    {
        chronological.list.push_back (*p);
        return std::make_pair (iterator (result.first), true);
    }
    delete_element (p);
    return std::make_pair (iterator (result.first), false);
}
#else // as original, use insert_check() / insert_commit () pair.
// set, map
template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
template <bool maybe_multi, class... args>
auto
aged_unordered_container <ismulti, ismap, key, t, clock,
    hash, keyequal, allocator>::
emplace (args&&... args) ->
    typename std::enable_if <! maybe_multi,
        std::pair <iterator, bool>>::type
{
    maybe_rehash (1);
    // vfalco note its unfortunate that we need to
    //             construct element here
    element* const p (new_element (
        std::forward <args> (args)...));
    typename cont_type::insert_commit_data d;
    auto const result (m_cont.insert_check (extract (p->value),
        std::cref (m_config.hash_function()),
            std::cref (m_config.key_value_equal()), d));
    if (result.second)
    {
        auto const iter (m_cont.insert_commit (*p, d));
        chronological.list.push_back (*p);
        return std::make_pair (iterator (iter), true);
    }
    delete_element (p);
    return std::make_pair (iterator (result.first), false);
}
#endif // 0

// multiset, multimap
template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
template <bool maybe_multi, class... args>
auto
aged_unordered_container <ismulti, ismap, key, t, clock,
    hash, keyequal, allocator>::
emplace (args&&... args) ->
    typename std::enable_if <maybe_multi,
        iterator>::type
{
    maybe_rehash (1);
    element* const p (new_element (
        std::forward <args> (args)...));
    chronological.list.push_back (*p);
    auto const iter (m_cont.insert (*p));
    return iterator (iter);
}

// set, map
template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
template <bool maybe_multi, class... args>
auto
aged_unordered_container <ismulti, ismap, key, t, clock,
    hash, keyequal, allocator>::
emplace_hint (const_iterator /*hint*/, args&&... args) ->
    typename std::enable_if <! maybe_multi,
        std::pair <iterator, bool>>::type
{
    maybe_rehash (1);
    // vfalco note its unfortunate that we need to
    //             construct element here
    element* const p (new_element (
        std::forward <args> (args)...));
    typename cont_type::insert_commit_data d;
    auto const result (m_cont.insert_check (extract (p->value),
        std::cref (m_config.hash_function()),
            std::cref (m_config.key_value_equal()), d));
    if (result.second)
    {
        auto const iter (m_cont.insert_commit (*p, d));
        chronological.list.push_back (*p);
        return std::make_pair (iterator (iter), true);
    }
    delete_element (p);
    return std::make_pair (iterator (result.first), false);
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
template <bool is_const, class iterator, class base>
detail::aged_container_iterator <false, iterator, base>
aged_unordered_container <ismulti, ismap, key, t, clock,
    hash, keyequal, allocator>::
erase (detail::aged_container_iterator <
    is_const, iterator, base> pos)
{
    unlink_and_delete_element(&*((pos++).iterator()));
    return detail::aged_container_iterator <
        false, iterator, base> (pos.iterator());
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
template <bool is_const, class iterator, class base>
detail::aged_container_iterator <false, iterator, base>
aged_unordered_container <ismulti, ismap, key, t, clock,
    hash, keyequal, allocator>::
erase (detail::aged_container_iterator <
    is_const, iterator, base> first,
        detail::aged_container_iterator <
            is_const, iterator, base> last)
{
    for (; first != last;)
        unlink_and_delete_element(&*((first++).iterator()));

    return detail::aged_container_iterator <
        false, iterator, base> (first.iterator());
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
template <class k>
auto
aged_unordered_container <ismulti, ismap, key, t, clock,
    hash, keyequal, allocator>::
erase (k const& k) ->
    size_type
{
    auto iter (m_cont.find (k, std::cref (m_config.hash_function()),
        std::cref (m_config.key_value_equal())));
    if (iter == m_cont.end())
        return 0;
    size_type n (0);
    for (;;)
    {
        auto p (&*iter++);
        bool const done (
            m_config (*p, extract (iter->value)));
        unlink_and_delete_element (p);
        ++n;
        if (done)
            break;
    }
    return n;
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
void
aged_unordered_container <ismulti, ismap, key, t, clock,
    hash, keyequal, allocator>::
swap (aged_unordered_container& other) noexcept
{
    swap_data (other);
    std::swap (chronological, other.chronological);
    std::swap (m_cont, other.m_cont);
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
template <class k>
auto
aged_unordered_container <ismulti, ismap, key, t, clock,
    hash, keyequal, allocator>::
touch (k const& k) ->
    size_type
{
    auto const now (clock().now());
    size_type n (0);
    auto const range (equal_range (k));
    for (auto iter : range)
    {
        touch (iter, now);
        ++n;
    }
    return n;
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
template <
    bool otherismap,
    class otherkey,
    class othert,
    class otherduration,
    class otherhash,
    class otherallocator,
    bool maybe_multi
>
typename std::enable_if <! maybe_multi, bool>::type
aged_unordered_container <
    ismulti, ismap, key, t, clock, hash, keyequal, allocator>::
operator== (
    aged_unordered_container <false, otherismap,
        otherkey, othert, otherduration, otherhash, keyequal,
            otherallocator> const& other) const
{
    if (size() != other.size())
        return false;
    for (auto iter (cbegin()), last (cend()), olast (other.cend());
        iter != last; ++iter)
    {
        auto oiter (other.find (extract (*iter)));
        if (oiter == olast)
            return false;
    }
    return true;
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
template <
    bool otherismap,
    class otherkey,
    class othert,
    class otherduration,
    class otherhash,
    class otherallocator,
    bool maybe_multi
>
typename std::enable_if <maybe_multi, bool>::type
aged_unordered_container <
    ismulti, ismap, key, t, clock, hash, keyequal, allocator>::
operator== (
    aged_unordered_container <true, otherismap,
        otherkey, othert, otherduration, otherhash, keyequal,
            otherallocator> const& other) const
{
    if (size() != other.size())
        return false;
    typedef std::pair <const_iterator, const_iterator> eqrng;
    for (auto iter (cbegin()), last (cend()); iter != last;)
    {
        auto const& k (extract (*iter));
        auto const eq (equal_range (k));
        auto const oeq (other.equal_range (k));
#if beast_no_cxx14_is_permutation
        if (std::distance (eq.first, eq.second) !=
            std::distance (oeq.first, oeq.second) ||
            ! std::is_permutation (eq.first, eq.second, oeq.first))
            return false;
#else
        if (! std::is_permutation (eq.first,
            eq.second, oeq.first, oeq.second))
            return false;
#endif
        iter = eq.second;
    }
    return true;
}

//------------------------------------------------------------------------------

// map, set
template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
template <bool maybe_multi>
auto
aged_unordered_container <ismulti, ismap, key, t, clock,
    hash, keyequal, allocator>::
insert_unchecked (value_type const& value) ->
    typename std::enable_if <! maybe_multi,
        std::pair <iterator, bool>>::type
{
    typename cont_type::insert_commit_data d;
    auto const result (m_cont.insert_check (extract (value),
        std::cref (m_config.hash_function()),
            std::cref (m_config.key_value_equal()), d));
    if (result.second)
    {
        element* const p (new_element (value));
        auto const iter (m_cont.insert_commit (*p, d));
        chronological.list.push_back (*p);
        return std::make_pair (iterator (iter), true);
    }
    return std::make_pair (iterator (result.first), false);
}

// multimap, multiset
template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
template <bool maybe_multi>
auto
aged_unordered_container <ismulti, ismap, key, t, clock,
    hash, keyequal, allocator>::
insert_unchecked (value_type const& value) ->
    typename std::enable_if <maybe_multi,
        iterator>::type
{
    element* const p (new_element (value));
    chronological.list.push_back (*p);
    auto const iter (m_cont.insert (*p));
    return iterator (iter);
}

//------------------------------------------------------------------------------

}

//------------------------------------------------------------------------------

template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator>
struct is_aged_container <detail::aged_unordered_container <
        ismulti, ismap, key, t, clock, hash, keyequal, allocator>>
    : std::true_type
{
};

// free functions

template <bool ismulti, bool ismap, class key, class t, class clock,
    class hash, class keyequal, class allocator>
void swap (
    detail::aged_unordered_container <ismulti, ismap,
        key, t, clock, hash, keyequal, allocator>& lhs,
    detail::aged_unordered_container <ismulti, ismap,
        key, t, clock, hash, keyequal, allocator>& rhs) noexcept
{
    lhs.swap (rhs);
}

/** expire aged container items past the specified age. */
template <bool ismulti, bool ismap, class key, class t,
    class clock, class hash, class keyequal, class allocator,
        class rep, class period>
std::size_t expire (detail::aged_unordered_container <
    ismulti, ismap, key, t, clock, hash, keyequal, allocator>& c,
        std::chrono::duration <rep, period> const& age) noexcept
{
    std::size_t n (0);
    auto const expired (c.clock().now() - age);
    for (auto iter (c.chronological.cbegin());
        iter != c.chronological.cend() &&
            iter.when() <= expired;)
    {
        iter = c.erase (iter);
        ++n;
    }
    return n;
}

}

#endif
