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

#ifndef beast_container_aged_ordered_container_h_included
#define beast_container_aged_ordered_container_h_included

#include <beast/container/detail/aged_container_iterator.h>
#include <beast/container/detail/aged_associative_container.h>
#include <beast/container/aged_container.h>
#include <beast/chrono/abstract_clock.h>
#include <beast/utility/empty_base_optimization.h>
#include <boost/intrusive/list.hpp>
#include <boost/intrusive/set.hpp>
#include <boost/version.hpp>
#include <beast/cxx14/algorithm.h> // <algorithm>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <beast/cxx14/type_traits.h> // <type_traits>
#include <utility>

namespace beast {
namespace detail {

// traits templates used to discern reverse_iterators, which are disallowed
// for mutating operations.
template <class it>
struct is_boost_reverse_iterator
    : std::false_type
{};

#if boost_version >= 105800
template <class it>
struct is_boost_reverse_iterator<boost::intrusive::reverse_iterator<it>>
    : std::true_type
{};
#else
template <class it>
struct is_boost_reverse_iterator<boost::intrusive::detail::reverse_iterator<it>>
    : std::true_type
{};
#endif

/** associative container where each element is also indexed by time.

    this container mirrors the interface of the standard library ordered
    associative containers, with the addition that each element is associated
    with a `when` `time_point` which is obtained from the value of the clock's
    `now`. the function `touch` updates the time for an element to the current
    time as reported by the clock.

    an extra set of iterator types and member functions are provided in the
    `chronological` memberspace that allow traversal in temporal or reverse
    temporal order. this container is useful as a building block for caches
    whose items expire after a certain amount of time. the chronological
    iterators allow for fully customizable expiration strategies.

    @see aged_set, aged_multiset, aged_map, aged_multimap
*/
template <
    bool ismulti,
    bool ismap,
    class key,
    class t,
    class clock = std::chrono::steady_clock,
    class compare = std::less <key>,
    class allocator = std::allocator <
        typename std::conditional <ismap,
            std::pair <key const, t>,
                key>::type>
>
class aged_ordered_container
{
public:
    using clock_type = abstract_clock<clock>;
    using time_point = typename clock_type::time_point;
    using duration = typename clock_type::duration;
    using key_type = key;
    using mapped_type = t;
    using value_type = typename std::conditional <
        ismap, std::pair <key const, t>, key>::type;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    // introspection (for unit tests)
    using is_unordered = std::false_type;
    using is_multi = std::integral_constant <bool, ismulti>;
    using is_map = std::integral_constant <bool, ismap>;

private:
    static key const& extract (value_type const& value)
    {
        return aged_associative_container_extract_t <ismap> () (value);
    }

    // vfalco todo hoist to remove template argument dependencies
    struct element
        : boost::intrusive::set_base_hook <
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
            typedef typename aged_ordered_container::value_type value_type;
            typedef typename aged_ordered_container::time_point time_point;
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

    // vfalco todo this should only be enabled for maps.
    class pair_value_compare
        : private empty_base_optimization <compare>
        , public std::binary_function <value_type, value_type, bool>
    {
    public:
        bool operator() (value_type const& lhs, value_type const& rhs) const
        {
            return this->member() (lhs.first, rhs.first);
        }

        pair_value_compare ()
        {
        }

        pair_value_compare (pair_value_compare const& other)
            : empty_base_optimization <compare> (other)
        {
        }

    private:
        friend aged_ordered_container;

        pair_value_compare (compare const& compare)
            : empty_base_optimization <compare> (compare)
        {
        }
    };

    // compares value_type against element, used in insert_check
    // vfalco todo hoist to remove template argument dependencies
    class keyvaluecompare
        : private empty_base_optimization <compare>
        , public std::binary_function <key, element, bool>
    {
    public:
        keyvaluecompare ()
        {
        }

        keyvaluecompare (compare const& compare)
            : empty_base_optimization <compare> (compare)
        {
        }

        // vfalco note we might want only to enable these overloads
        //                if compare has is_transparent
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

        compare& compare()
        {
            return empty_base_optimization <compare>::member();
        }

        compare const& compare() const
        {
            return empty_base_optimization <compare>::member();
        }
    };

    typedef typename boost::intrusive::make_list <element,
        boost::intrusive::constant_time_size <false>
            >::type list_type;

    typedef typename std::conditional <
        ismulti,
        typename boost::intrusive::make_multiset <element,
            boost::intrusive::constant_time_size <true>
                >::type,
        typename boost::intrusive::make_set <element,
            boost::intrusive::constant_time_size <true>
                >::type
        >::type cont_type;

    typedef typename std::allocator_traits <
        allocator>::template rebind_alloc <element> elementallocator;

    using elementallocatortraits = std::allocator_traits <elementallocator>;

    class config_t
        : private keyvaluecompare
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
            compare const& comp)
            : keyvaluecompare (comp)
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
            compare const& comp,
            allocator const& alloc_)
            : keyvaluecompare (comp)
            , empty_base_optimization <elementallocator> (alloc_)
            , clock (clock_)
        {
        }

        config_t (config_t const& other)
            : keyvaluecompare (other.key_compare())
            , empty_base_optimization <elementallocator> (
                elementallocatortraits::
                    select_on_container_copy_construction (
                        other.alloc()))
            , clock (other.clock)
        {
        }

        config_t (config_t const& other, allocator const& alloc)
            : keyvaluecompare (other.key_compare())
            , empty_base_optimization <elementallocator> (alloc)
            , clock (other.clock)
        {
        }

        config_t (config_t&& other)
            : keyvaluecompare (std::move (other.key_compare()))
            , empty_base_optimization <elementallocator> (
                std::move (other))
            , clock (other.clock)
        {
        }

        config_t (config_t&& other, allocator const& alloc)
            : keyvaluecompare (std::move (other.key_compare()))
            , empty_base_optimization <elementallocator> (alloc)
            , clock (other.clock)
        {
        }

        config_t& operator= (config_t const& other)
        {
            if (this != &other)
            {
                compare() = other.compare();
                alloc() = other.alloc();
                clock = other.clock;
            }
            return *this;
        }

        config_t& operator= (config_t&& other)
        {
            compare() = std::move (other.compare());
            alloc() = std::move (other.alloc());
            clock = other.clock;
            return *this;
        }

        compare& compare ()
        {
            return keyvaluecompare::compare();
        }

        compare const& compare () const
        {
            return keyvaluecompare::compare();
        }

        keyvaluecompare& key_compare()
        {
            return *this;
        }

        keyvaluecompare const& key_compare() const
        {
            return *this;
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
    typedef compare key_compare;
    typedef typename std::conditional <
        ismap,
        pair_value_compare,
        compare>::type value_compare;
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
        typename cont_type::reverse_iterator> reverse_iterator;
    typedef detail::aged_container_iterator <true,
        typename cont_type::reverse_iterator> const_reverse_iterator;

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

        friend class aged_ordered_container;
        list_type mutable list;
    } chronological;

    //--------------------------------------------------------------------------
    //
    // construction
    //
    //--------------------------------------------------------------------------

    explicit aged_ordered_container (clock_type& clock);

    aged_ordered_container (clock_type& clock,
        compare const& comp);

    aged_ordered_container (clock_type& clock,
        allocator const& alloc);

    aged_ordered_container (clock_type& clock,
        compare const& comp, allocator const& alloc);

    template <class inputit>
    aged_ordered_container (inputit first, inputit last, clock_type& clock);

    template <class inputit>
    aged_ordered_container (inputit first, inputit last, clock_type& clock,
        compare const& comp);

    template <class inputit>
    aged_ordered_container (inputit first, inputit last, clock_type& clock,
        allocator const& alloc);

    template <class inputit>
    aged_ordered_container (inputit first, inputit last, clock_type& clock,
        compare const& comp, allocator const& alloc);

    aged_ordered_container (aged_ordered_container const& other);

    aged_ordered_container (aged_ordered_container const& other,
        allocator const& alloc);

    aged_ordered_container (aged_ordered_container&& other);

    aged_ordered_container (aged_ordered_container&& other,
        allocator const& alloc);

    aged_ordered_container (std::initializer_list <value_type> init,
        clock_type& clock);

    aged_ordered_container (std::initializer_list <value_type> init,
        clock_type& clock, compare const& comp);

    aged_ordered_container (std::initializer_list <value_type> init,
        clock_type& clock, allocator const& alloc);

    aged_ordered_container (std::initializer_list <value_type> init,
        clock_type& clock, compare const& comp, allocator const& alloc);

    ~aged_ordered_container();

    aged_ordered_container&
    operator= (aged_ordered_container const& other);

    aged_ordered_container&
    operator= (aged_ordered_container&& other);

    aged_ordered_container&
    operator= (std::initializer_list <value_type> init);

    allocator_type
    get_allocator() const
    {
        return m_config.alloc();
    }

    clock_type&
    clock()
    {
        return m_config.clock;
    }

    clock_type const&
    clock() const
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

    reverse_iterator
    rbegin ()
    {
        return reverse_iterator (m_cont.rbegin());
    }

    const_reverse_iterator
    rbegin () const
    {
        return const_reverse_iterator (m_cont.rbegin ());
    }

    const_reverse_iterator
    crbegin() const
    {
        return const_reverse_iterator (m_cont.rbegin ());
    }

    reverse_iterator
    rend ()
    {
        return reverse_iterator (m_cont.rend ());
    }

    const_reverse_iterator
    rend () const
    {
        return const_reverse_iterator (m_cont.rend ());
    }

    const_reverse_iterator
    crend () const
    {
        return const_reverse_iterator (m_cont.rend ());
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

    bool
    empty() const noexcept
    {
        return m_cont.empty();
    }

    size_type
    size() const noexcept
    {
        return m_cont.size();
    }

    size_type
    max_size() const noexcept
    {
        return m_config.max_size();
    }

    //--------------------------------------------------------------------------
    //
    // modifiers
    //
    //--------------------------------------------------------------------------

    void
    clear();

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

    // set
    template <bool maybe_multi = ismulti, bool maybe_map = ismap>
    auto
    insert (value_type&& value) ->
        typename std::enable_if <! maybe_multi && ! maybe_map,
            std::pair <iterator, bool>>::type;

    // multiset
    template <bool maybe_multi = ismulti, bool maybe_map = ismap>
    auto
    insert (value_type&& value) ->
        typename std::enable_if <maybe_multi && ! maybe_map,
            iterator>::type;

    //---

    // map, set
    template <bool maybe_multi = ismulti>
    auto
    insert (const_iterator hint, value_type const& value) ->
        typename std::enable_if <! maybe_multi,
            iterator>::type;

    // multimap, multiset
    template <bool maybe_multi = ismulti>
    typename std::enable_if <maybe_multi,
        iterator>::type
    insert (const_iterator /*hint*/, value_type const& value)
    {
        // vfalco todo figure out how to utilize 'hint'
        return insert (value);
    }

    // map, set
    template <bool maybe_multi = ismulti>
    auto
    insert (const_iterator hint, value_type&& value) ->
        typename std::enable_if <! maybe_multi,
            iterator>::type;

    // multimap, multiset
    template <bool maybe_multi = ismulti>
    typename std::enable_if <maybe_multi,
        iterator>::type
    insert (const_iterator /*hint*/, value_type&& value)
    {
        // vfalco todo figure out how to utilize 'hint'
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
                iterator,
                std::pair <iterator, bool>
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
                iterator,
                std::pair <iterator, bool>
            >::type
    >::type
    insert (const_iterator hint, p&& value)
    {
        return emplace_hint (hint, std::forward <p> (value));
    }

    template <class inputit>
    void
    insert (inputit first, inputit last)
    {
        for (; first != last; ++first)
            insert (cend(), *first);
    }

    void
    insert (std::initializer_list <value_type> init)
    {
        insert (init.begin(), init.end());
    }

    // map, set
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

    // map, set
    template <bool maybe_multi = ismulti, class... args>
    auto
    emplace_hint (const_iterator hint, args&&... args) ->
        typename std::enable_if <! maybe_multi,
            std::pair <iterator, bool>>::type;

    // multiset, multimap
    template <bool maybe_multi = ismulti, class... args>
    typename std::enable_if <maybe_multi,
        iterator>::type
    emplace_hint (const_iterator /*hint*/, args&&... args)
    {
        // vfalco todo figure out how to utilize 'hint'
        return emplace <maybe_multi> (
            std::forward <args> (args)...);
    }

    // enable_if prevents erase (reverse_iterator pos) from compiling
    template <bool is_const, class iterator, class base,
         class = std::enable_if_t<!is_boost_reverse_iterator<iterator>::value>>
    detail::aged_container_iterator <false, iterator, base>
    erase (detail::aged_container_iterator <is_const, iterator, base> pos);

    // enable_if prevents erase (reverse_iterator first, reverse_iterator last)
    // from compiling
    template <bool is_const, class iterator, class base,
        class = std::enable_if_t<!is_boost_reverse_iterator<iterator>::value>>
    detail::aged_container_iterator <false, iterator, base>
    erase (detail::aged_container_iterator <is_const, iterator, base> first,
           detail::aged_container_iterator <is_const, iterator, base> last);

    template <class k>
    auto
    erase (k const& k) ->
        size_type;

    void
    swap (aged_ordered_container& other) noexcept;

    //--------------------------------------------------------------------------

    // enable_if prevents touch (reverse_iterator pos) from compiling
    template <bool is_const, class iterator, class base,
        class = std::enable_if_t<!is_boost_reverse_iterator<iterator>::value>>
    void
    touch (detail::aged_container_iterator <is_const, iterator, base> pos)
    {
        touch (pos, clock().now());
    }

    template <class k>
    size_type
    touch (k const& k);

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
        return m_cont.count (k,
            std::cref (m_config.key_compare()));
    }

    // vfalco todo respect is_transparent (c++14)
    template <class k>
    iterator
    find (k const& k)
    {
        return iterator (m_cont.find (k,
            std::cref (m_config.key_compare())));
    }

    // vfalco todo respect is_transparent (c++14)
    template <class k>
    const_iterator
    find (k const& k) const
    {
        return const_iterator (m_cont.find (k,
            std::cref (m_config.key_compare())));
    }

    // vfalco todo respect is_transparent (c++14)
    template <class k>
    std::pair <iterator, iterator>
    equal_range (k const& k)
    {
        auto const r (m_cont.equal_range (k,
            std::cref (m_config.key_compare())));
        return std::make_pair (iterator (r.first),
            iterator (r.second));
    }

    // vfalco todo respect is_transparent (c++14)
    template <class k>
    std::pair <const_iterator, const_iterator>
    equal_range (k const& k) const
    {
        auto const r (m_cont.equal_range (k,
            std::cref (m_config.key_compare())));
        return std::make_pair (const_iterator (r.first),
            const_iterator (r.second));
    }

    // vfalco todo respect is_transparent (c++14)
    template <class k>
    iterator
    lower_bound (k const& k)
    {
        return iterator (m_cont.lower_bound (k,
            std::cref (m_config.key_compare())));
    }

    // vfalco todo respect is_transparent (c++14)
    template <class k>
    const_iterator
    lower_bound (k const& k) const
    {
        return const_iterator (m_cont.lower_bound (k,
            std::cref (m_config.key_compare())));
    }

    // vfalco todo respect is_transparent (c++14)
    template <class k>
    iterator
    upper_bound (k const& k)
    {
        return iterator (m_cont.upper_bound (k,
            std::cref (m_config.key_compare())));
    }

    // vfalco todo respect is_transparent (c++14)
    template <class k>
    const_iterator
    upper_bound (k const& k) const
    {
        return const_iterator (m_cont.upper_bound (k,
            std::cref (m_config.key_compare())));
    }

    //--------------------------------------------------------------------------
    //
    // observers
    //
    //--------------------------------------------------------------------------

    key_compare
    key_comp() const
    {
        return m_config.compare();
    }

    // vfalco todo should this return const reference for set?
    value_compare
    value_comp() const
    {
        return value_compare (m_config.compare());
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
        bool otherismulti,
        bool otherismap,
        class othert,
        class otherduration,
        class otherallocator
    >
    bool
    operator== (
        aged_ordered_container <otherismulti, otherismap,
            key, othert, otherduration, compare,
                otherallocator> const& other) const;

    template <
        bool otherismulti,
        bool otherismap,
        class othert,
        class otherduration,
        class otherallocator
    >
    bool
    operator!= (
        aged_ordered_container <otherismulti, otherismap,
            key, othert, otherduration, compare,
                otherallocator> const& other) const
    {
        return ! (this->operator== (other));
    }

    template <
        bool otherismulti,
        bool otherismap,
        class othert,
        class otherduration,
        class otherallocator
    >
    bool
    operator< (
        aged_ordered_container <otherismulti, otherismap,
            key, othert, otherduration, compare,
                otherallocator> const& other) const
    {
        value_compare const comp (value_comp ());
        return std::lexicographical_compare (
            cbegin(), cend(), other.cbegin(), other.cend(), comp);
    }

    template <
        bool otherismulti,
        bool otherismap,
        class othert,
        class otherduration,
        class otherallocator
    >
    bool
    operator<= (
        aged_ordered_container <otherismulti, otherismap,
            key, othert, otherduration, compare,
                otherallocator> const& other) const
    {
        return ! (other < *this);
    }

    template <
        bool otherismulti,
        bool otherismap,
        class othert,
        class otherduration,
        class otherallocator
    >
    bool
    operator> (
        aged_ordered_container <otherismulti, otherismap,
            key, othert, otherduration, compare,
                otherallocator> const& other) const
    {
        return other < *this;
    }

    template <
        bool otherismulti,
        bool otherismap,
        class othert,
        class otherduration,
        class otherallocator
    >
    bool
    operator>= (
        aged_ordered_container <otherismulti, otherismap,
            key, othert, otherduration, compare,
                otherallocator> const& other) const
    {
        return ! (*this < other);
    }

private:
    // enable_if prevents erase (reverse_iterator pos, now) from compiling
    template <bool is_const, class iterator, class base,
        class = std::enable_if_t<!is_boost_reverse_iterator<iterator>::value>>
    void
    touch (detail::aged_container_iterator <
        is_const, iterator, base> pos,
            typename clock_type::time_point const& now);

    template <bool maybe_propagate = std::allocator_traits <
        allocator>::propagate_on_container_swap::value>
    typename std::enable_if <maybe_propagate>::type
    swap_data (aged_ordered_container& other) noexcept;

    template <bool maybe_propagate = std::allocator_traits <
        allocator>::propagate_on_container_swap::value>
    typename std::enable_if <! maybe_propagate>::type
    swap_data (aged_ordered_container& other) noexcept;

private:
    config_t m_config;
    cont_type mutable m_cont;
};

//------------------------------------------------------------------------------

template <bool ismulti, bool ismap, class key, class t,
    class clock, class compare, class allocator>
aged_ordered_container <ismulti, ismap, key, t, clock, compare, allocator>::
aged_ordered_container (
    clock_type& clock)
    : m_config (clock)
{
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class compare, class allocator>
aged_ordered_container <ismulti, ismap, key, t, clock, compare, allocator>::
aged_ordered_container (
    clock_type& clock,
    compare const& comp)
    : m_config (clock, comp)
{
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class compare, class allocator>
aged_ordered_container <ismulti, ismap, key, t, clock, compare, allocator>::
aged_ordered_container (
    clock_type& clock,
    allocator const& alloc)
    : m_config (clock, alloc)
{
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class compare, class allocator>
aged_ordered_container <ismulti, ismap, key, t, clock, compare, allocator>::
aged_ordered_container (
    clock_type& clock,
    compare const& comp,
    allocator const& alloc)
    : m_config (clock, comp, alloc)
{
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class compare, class allocator>
template <class inputit>
aged_ordered_container <ismulti, ismap, key, t, clock, compare, allocator>::
aged_ordered_container (inputit first, inputit last,
    clock_type& clock)
    : m_config (clock)
{
    insert (first, last);
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class compare, class allocator>
template <class inputit>
aged_ordered_container <ismulti, ismap, key, t, clock, compare, allocator>::
aged_ordered_container (inputit first, inputit last,
    clock_type& clock,
    compare const& comp)
    : m_config (clock, comp)
{
    insert (first, last);
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class compare, class allocator>
template <class inputit>
aged_ordered_container <ismulti, ismap, key, t, clock, compare, allocator>::
aged_ordered_container (inputit first, inputit last,
    clock_type& clock,
    allocator const& alloc)
    : m_config (clock, alloc)
{
    insert (first, last);
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class compare, class allocator>
template <class inputit>
aged_ordered_container <ismulti, ismap, key, t, clock, compare, allocator>::
aged_ordered_container (inputit first, inputit last,
    clock_type& clock,
    compare const& comp,
    allocator const& alloc)
    : m_config (clock, comp, alloc)
{
    insert (first, last);
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class compare, class allocator>
aged_ordered_container <ismulti, ismap, key, t, clock, compare, allocator>::
aged_ordered_container (aged_ordered_container const& other)
    : m_config (other.m_config)
{
    insert (other.cbegin(), other.cend());
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class compare, class allocator>
aged_ordered_container <ismulti, ismap, key, t, clock, compare, allocator>::
aged_ordered_container (aged_ordered_container const& other,
    allocator const& alloc)
    : m_config (other.m_config, alloc)
{
    insert (other.cbegin(), other.cend());
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class compare, class allocator>
aged_ordered_container <ismulti, ismap, key, t, clock, compare, allocator>::
aged_ordered_container (aged_ordered_container&& other)
    : m_config (std::move (other.m_config))
    , m_cont (std::move (other.m_cont))
{
    chronological.list = std::move (other.chronological.list);
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class compare, class allocator>
aged_ordered_container <ismulti, ismap, key, t, clock, compare, allocator>::
aged_ordered_container (aged_ordered_container&& other,
    allocator const& alloc)
    : m_config (std::move (other.m_config), alloc)
{
    insert (other.cbegin(), other.cend());
    other.clear ();
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class compare, class allocator>
aged_ordered_container <ismulti, ismap, key, t, clock, compare, allocator>::
aged_ordered_container (std::initializer_list <value_type> init,
    clock_type& clock)
    : m_config (clock)
{
    insert (init.begin(), init.end());
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class compare, class allocator>
aged_ordered_container <ismulti, ismap, key, t, clock, compare, allocator>::
aged_ordered_container (std::initializer_list <value_type> init,
    clock_type& clock,
    compare const& comp)
    : m_config (clock, comp)
{
    insert (init.begin(), init.end());
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class compare, class allocator>
aged_ordered_container <ismulti, ismap, key, t, clock, compare, allocator>::
aged_ordered_container (std::initializer_list <value_type> init,
    clock_type& clock,
    allocator const& alloc)
    : m_config (clock, alloc)
{
    insert (init.begin(), init.end());
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class compare, class allocator>
aged_ordered_container <ismulti, ismap, key, t, clock, compare, allocator>::
aged_ordered_container (std::initializer_list <value_type> init,
    clock_type& clock,
    compare const& comp,
    allocator const& alloc)
    : m_config (clock, comp, alloc)
{
    insert (init.begin(), init.end());
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class compare, class allocator>
aged_ordered_container <ismulti, ismap, key, t, clock, compare, allocator>::
~aged_ordered_container()
{
    clear();
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class compare, class allocator>
auto
aged_ordered_container <ismulti, ismap, key, t, clock, compare, allocator>::
operator= (aged_ordered_container const& other) ->
    aged_ordered_container&
{
    if (this != &other)
    {
        clear();
        this->m_config = other.m_config;
        insert (other.begin(), other.end());
    }
    return *this;
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class compare, class allocator>
auto
aged_ordered_container <ismulti, ismap, key, t, clock, compare, allocator>::
operator= (aged_ordered_container&& other) ->
    aged_ordered_container&
{
    clear();
    this->m_config = std::move (other.m_config);
    insert (other.begin(), other.end());
    other.clear();
    return *this;
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class compare, class allocator>
auto
aged_ordered_container <ismulti, ismap, key, t, clock, compare, allocator>::
operator= (std::initializer_list <value_type> init) ->
    aged_ordered_container&
{
    clear ();
    insert (init);
    return *this;
}

//------------------------------------------------------------------------------

template <bool ismulti, bool ismap, class key, class t,
    class clock, class compare, class allocator>
template <class k, bool maybe_multi, bool maybe_map, class>
typename std::conditional <ismap, t, void*>::type&
aged_ordered_container <ismulti, ismap, key, t, clock, compare, allocator>::
at (k const& k)
{
    auto const iter (m_cont.find (k,
        std::cref (m_config.key_compare())));
    if (iter == m_cont.end())
        throw std::out_of_range ("key not found");
    return iter->value.second;
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class compare, class allocator>
template <class k, bool maybe_multi, bool maybe_map, class>
typename std::conditional <ismap, t, void*>::type const&
aged_ordered_container <ismulti, ismap, key, t, clock, compare, allocator>::
at (k const& k) const
{
    auto const iter (m_cont.find (k,
        std::cref (m_config.key_compare())));
    if (iter == m_cont.end())
        throw std::out_of_range ("key not found");
    return iter->value.second;
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class compare, class allocator>
template <bool maybe_multi, bool maybe_map, class>
typename std::conditional <ismap, t, void*>::type&
aged_ordered_container <ismulti, ismap, key, t, clock, compare, allocator>::
operator[] (key const& key)
{
    typename cont_type::insert_commit_data d;
    auto const result (m_cont.insert_check (key,
        std::cref (m_config.key_compare()), d));
    if (result.second)
    {
        element* const p (new_element (
            std::piecewise_construct, std::forward_as_tuple (key),
                std::forward_as_tuple ()));
        m_cont.insert_commit (*p, d);
        chronological.list.push_back (*p);
        return p->value.second;
    }
    return result.first->value.second;
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class compare, class allocator>
template <bool maybe_multi, bool maybe_map, class>
typename std::conditional <ismap, t, void*>::type&
aged_ordered_container <ismulti, ismap, key, t, clock, compare, allocator>::
operator[] (key&& key)
{
    typename cont_type::insert_commit_data d;
    auto const result (m_cont.insert_check (key,
        std::cref (m_config.key_compare()), d));
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
    class clock, class compare, class allocator>
void
aged_ordered_container <ismulti, ismap, key, t, clock, compare, allocator>::
clear()
{
    for (auto iter (chronological.list.begin());
        iter != chronological.list.end();)
        delete_element (&*iter++);
    chronological.list.clear();
    m_cont.clear();
}

// map, set
template <bool ismulti, bool ismap, class key, class t,
    class clock, class compare, class allocator>
template <bool maybe_multi>
auto
aged_ordered_container <ismulti, ismap, key, t, clock, compare, allocator>::
insert (value_type const& value) ->
    typename std::enable_if <! maybe_multi,
        std::pair <iterator, bool>>::type
{
    typename cont_type::insert_commit_data d;
    auto const result (m_cont.insert_check (extract (value),
        std::cref (m_config.key_compare()), d));
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
    class clock, class compare, class allocator>
template <bool maybe_multi>
auto
aged_ordered_container <ismulti, ismap, key, t, clock, compare, allocator>::
insert (value_type const& value) ->
    typename std::enable_if <maybe_multi,
        iterator>::type
{
    auto const before (m_cont.upper_bound (
        extract (value), std::cref (m_config.key_compare())));
    element* const p (new_element (value));
    chronological.list.push_back (*p);
    auto const iter (m_cont.insert_before (before, *p));
    return iterator (iter);
}

// set
template <bool ismulti, bool ismap, class key, class t,
    class clock, class compare, class allocator>
template <bool maybe_multi, bool maybe_map>
auto
aged_ordered_container <ismulti, ismap, key, t, clock, compare, allocator>::
insert (value_type&& value) ->
    typename std::enable_if <! maybe_multi && ! maybe_map,
        std::pair <iterator, bool>>::type
{
    typename cont_type::insert_commit_data d;
    auto const result (m_cont.insert_check (extract (value),
        std::cref (m_config.key_compare()), d));
    if (result.second)
    {
        element* const p (new_element (std::move (value)));
        auto const iter (m_cont.insert_commit (*p, d));
        chronological.list.push_back (*p);
        return std::make_pair (iterator (iter), true);
    }
    return std::make_pair (iterator (result.first), false);
}

// multiset
template <bool ismulti, bool ismap, class key, class t,
    class clock, class compare, class allocator>
template <bool maybe_multi, bool maybe_map>
auto
aged_ordered_container <ismulti, ismap, key, t, clock, compare, allocator>::
insert (value_type&& value) ->
    typename std::enable_if <maybe_multi && ! maybe_map,
        iterator>::type
{
    auto const before (m_cont.upper_bound (
        extract (value), std::cref (m_config.key_compare())));
    element* const p (new_element (std::move (value)));
    chronological.list.push_back (*p);
    auto const iter (m_cont.insert_before (before, *p));
    return iterator (iter);
}

//---

// map, set
template <bool ismulti, bool ismap, class key, class t,
    class clock, class compare, class allocator>
template <bool maybe_multi>
auto
aged_ordered_container <ismulti, ismap, key, t, clock, compare, allocator>::
insert (const_iterator hint, value_type const& value) ->
    typename std::enable_if <! maybe_multi,
        iterator>::type
{
    typename cont_type::insert_commit_data d;
    auto const result (m_cont.insert_check (hint.iterator(),
        extract (value), std::cref (m_config.key_compare()), d));
    if (result.second)
    {
        element* const p (new_element (value));
        auto const iter (m_cont.insert_commit (*p, d));
        chronological.list.push_back (*p);
        return iterator (iter);
    }
    return iterator (result.first);
}

// map, set
template <bool ismulti, bool ismap, class key, class t,
    class clock, class compare, class allocator>
template <bool maybe_multi>
auto
aged_ordered_container <ismulti, ismap, key, t, clock, compare, allocator>::
insert (const_iterator hint, value_type&& value) ->
    typename std::enable_if <! maybe_multi,
        iterator>::type
{
    typename cont_type::insert_commit_data d;
    auto const result (m_cont.insert_check (hint.iterator(),
        extract (value), std::cref (m_config.key_compare()), d));
    if (result.second)
    {
        element* const p (new_element (std::move (value)));
        auto const iter (m_cont.insert_commit (*p, d));
        chronological.list.push_back (*p);
        return iterator (iter);
    }
    return iterator (result.first);
}

// map, set
template <bool ismulti, bool ismap, class key, class t,
    class clock, class compare, class allocator>
template <bool maybe_multi, class... args>
auto
aged_ordered_container <ismulti, ismap, key, t, clock, compare, allocator>::
emplace (args&&... args) ->
    typename std::enable_if <! maybe_multi,
        std::pair <iterator, bool>>::type
{
    // vfalco note its unfortunate that we need to
    //             construct element here
    element* const p (new_element (
        std::forward <args> (args)...));
    typename cont_type::insert_commit_data d;
    auto const result (m_cont.insert_check (extract (p->value),
        std::cref (m_config.key_compare()), d));
    if (result.second)
    {
        auto const iter (m_cont.insert_commit (*p, d));
        chronological.list.push_back (*p);
        return std::make_pair (iterator (iter), true);
    }
    delete_element (p);
    return std::make_pair (iterator (result.first), false);
}

// multiset, multimap
template <bool ismulti, bool ismap, class key, class t,
    class clock, class compare, class allocator>
template <bool maybe_multi, class... args>
auto
aged_ordered_container <ismulti, ismap, key, t, clock, compare, allocator>::
emplace (args&&... args) ->
    typename std::enable_if <maybe_multi,
        iterator>::type
{
    element* const p (new_element (
        std::forward <args> (args)...));
    auto const before (m_cont.upper_bound (extract (p->value),
        std::cref (m_config.key_compare())));
    chronological.list.push_back (*p);
    auto const iter (m_cont.insert_before (before, *p));
    return iterator (iter);
}

// map, set
template <bool ismulti, bool ismap, class key, class t,
    class clock, class compare, class allocator>
template <bool maybe_multi, class... args>
auto
aged_ordered_container <ismulti, ismap, key, t, clock, compare, allocator>::
emplace_hint (const_iterator hint, args&&... args) ->
    typename std::enable_if <! maybe_multi,
        std::pair <iterator, bool>>::type
{
    // vfalco note its unfortunate that we need to
    //             construct element here
    element* const p (new_element (
        std::forward <args> (args)...));
    typename cont_type::insert_commit_data d;
    auto const result (m_cont.insert_check (hint.iterator(),
        extract (p->value), std::cref (m_config.key_compare()), d));
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
    class clock, class compare, class allocator>
template <bool is_const, class iterator, class base, class>
detail::aged_container_iterator <false, iterator, base>
aged_ordered_container <ismulti, ismap, key, t, clock, compare, allocator>::
erase (detail::aged_container_iterator <is_const, iterator, base> pos)
{
    unlink_and_delete_element(&*((pos++).iterator()));
    return detail::aged_container_iterator <
        false, iterator, base> (pos.iterator());
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class compare, class allocator>
template <bool is_const, class iterator, class base, class>
detail::aged_container_iterator <false, iterator, base>
aged_ordered_container <ismulti, ismap, key, t, clock, compare, allocator>::
erase (detail::aged_container_iterator <is_const, iterator, base> first,
       detail::aged_container_iterator <is_const, iterator, base> last)
{
    for (; first != last;)
        unlink_and_delete_element(&*((first++).iterator()));

    return detail::aged_container_iterator <
        false, iterator, base> (first.iterator());
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class compare, class allocator>
template <class k>
auto
aged_ordered_container <ismulti, ismap, key, t, clock, compare, allocator>::
erase (k const& k) ->
    size_type
{
    auto iter (m_cont.find (k,
        std::cref (m_config.key_compare())));
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
    class clock, class compare, class allocator>
void
aged_ordered_container <ismulti, ismap, key, t, clock, compare, allocator>::
swap (aged_ordered_container& other) noexcept
{
    swap_data (other);
    std::swap (chronological, other.chronological);
    std::swap (m_cont, other.m_cont);
}

//------------------------------------------------------------------------------

template <bool ismulti, bool ismap, class key, class t,
    class clock, class compare, class allocator>
template <class k>
auto
aged_ordered_container <ismulti, ismap, key, t, clock, compare, allocator>::
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

//------------------------------------------------------------------------------

template <bool ismulti, bool ismap, class key, class t,
    class clock, class compare, class allocator>
template <bool otherismulti, bool otherismap,
    class othert, class otherduration, class otherallocator>
bool
aged_ordered_container <ismulti, ismap, key, t, clock, compare, allocator>::
operator== (
    aged_ordered_container <otherismulti, otherismap,
        key, othert, otherduration, compare,
            otherallocator> const& other) const
{
    typedef aged_ordered_container <otherismulti, otherismap,
        key, othert, otherduration, compare,
            otherallocator> other;
    if (size() != other.size())
        return false;
    std::equal_to <void> eq;
    return std::equal (cbegin(), cend(), other.cbegin(), other.cend(),
        [&eq, &other](value_type const& lhs,
            typename other::value_type const& rhs)
        {
            return eq (extract (lhs), other.extract (rhs));
        });
}

//------------------------------------------------------------------------------

template <bool ismulti, bool ismap, class key, class t,
    class clock, class compare, class allocator>
template <bool is_const, class iterator, class base, class>
void
aged_ordered_container <ismulti, ismap, key, t, clock, compare, allocator>::
touch (detail::aged_container_iterator <
    is_const, iterator, base> pos,
        typename clock_type::time_point const& now)
{
    auto& e (*pos.iterator());
    e.when = now;
    chronological.list.erase (chronological.list.iterator_to (e));
    chronological.list.push_back (e);
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class compare, class allocator>
template <bool maybe_propagate>
typename std::enable_if <maybe_propagate>::type
aged_ordered_container <ismulti, ismap, key, t, clock, compare, allocator>::
swap_data (aged_ordered_container& other) noexcept
{
    std::swap (m_config.key_compare(), other.m_config.key_compare());
    std::swap (m_config.alloc(), other.m_config.alloc());
    std::swap (m_config.clock, other.m_config.clock);
}

template <bool ismulti, bool ismap, class key, class t,
    class clock, class compare, class allocator>
template <bool maybe_propagate>
typename std::enable_if <! maybe_propagate>::type
aged_ordered_container <ismulti, ismap, key, t, clock, compare, allocator>::
swap_data (aged_ordered_container& other) noexcept
{
    std::swap (m_config.key_compare(), other.m_config.key_compare());
    std::swap (m_config.clock, other.m_config.clock);
}

}

//------------------------------------------------------------------------------

template <bool ismulti, bool ismap, class key, class t,
    class clock, class compare, class allocator>
struct is_aged_container <detail::aged_ordered_container <
        ismulti, ismap, key, t, clock, compare, allocator>>
    : std::true_type
{
};

// free functions

template <bool ismulti, bool ismap, class key, class t,
    class clock, class compare, class allocator>
void swap (
    detail::aged_ordered_container <ismulti, ismap,
        key, t, clock, compare, allocator>& lhs,
    detail::aged_ordered_container <ismulti, ismap,
        key, t, clock, compare, allocator>& rhs) noexcept
{
    lhs.swap (rhs);
}

/** expire aged container items past the specified age. */
template <bool ismulti, bool ismap, class key, class t,
    class clock, class compare, class allocator,
        class rep, class period>
std::size_t expire (detail::aged_ordered_container <
    ismulti, ismap, key, t, clock, compare, allocator>& c,
        std::chrono::duration <rep, period> const& age)
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
