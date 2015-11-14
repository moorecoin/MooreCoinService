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

#ifndef beast_intrusive_list_h_included
#define beast_intrusive_list_h_included

#include <beast/config.h>

#include <iterator>
#include <type_traits>

namespace beast {

template <typename, typename>
class list;

namespace detail {

/** copy `const` attribute from t to u if present. */
/** @{ */
template <typename t, typename u>
struct copyconst
{
    typedef typename std::remove_const <u>::type type;
};

template <typename t, typename u>
struct copyconst <t const, u>
{
    typedef typename std::remove_const <u>::type const type;
};
/** @} */

// this is the intrusive portion of the doubly linked list.
// one derivation per list that the object may appear on
// concurrently is required.
//
template <typename t, typename tag>
class listnode
{
private:
    typedef t value_type;

    friend class list<t, tag>;

    template <typename>
    friend class listiterator;

    listnode* m_next;
    listnode* m_prev;
};

//------------------------------------------------------------------------------

template <typename n>
class listiterator : public std::iterator <
    std::bidirectional_iterator_tag, std::size_t>
{
public:
    typedef typename detail::copyconst <
        n, typename n::value_type>::type value_type;
    typedef value_type* pointer;
    typedef value_type& reference;
    typedef std::size_t size_type;

    listiterator (n* node = nullptr) noexcept
        : m_node (node)
    {
    }

    template <typename m>
    listiterator (listiterator <m> const& other) noexcept
        : m_node (other.m_node)
    {
    }

    template <typename m>
    bool operator== (listiterator <m> const& other) const noexcept
    {
        return m_node == other.m_node;
    }

    template <typename m>
    bool operator!= (listiterator <m> const& other) const noexcept
    {
        return ! ((*this) == other);
    }

    reference operator* () const noexcept
    {
        return dereference ();
    }

    pointer operator-> () const noexcept
    {
        return &dereference ();
    }

    listiterator& operator++ () noexcept
    {
        increment ();
        return *this;
    }

    listiterator operator++ (int) noexcept
    {
        listiterator result (*this);
        increment ();
        return result;
    }

    listiterator& operator-- () noexcept
    {
        decrement ();
        return *this;
    }

    listiterator operator-- (int) noexcept
    {
        listiterator result (*this);
        decrement ();
        return result;
    }

private:
    reference dereference () const noexcept
    {
        return static_cast <reference> (*m_node);
    }

    void increment () noexcept
    {
        m_node = m_node->m_next;
    }

    void decrement () noexcept
    {
        m_node = m_node->m_prev;
    }

    n* m_node;
};

}

/** intrusive doubly linked list.
  
    this intrusive list is a container similar in operation to std::list in the
    standard template library (stl). like all @ref intrusive containers, list
    requires you to first derive your class from list<>::node:
  
    @code
  
    struct object : list <object>::node
    {
        explicit object (int value) : m_value (value)
        {
        }
  
        int m_value;
    };
  
    @endcode
  
    now we define the list, and add a couple of items.
  
    @code
  
    list <object> list;
  
    list.push_back (* (new object (1)));
    list.push_back (* (new object (2)));
  
    @endcode
  
    for compatibility with the standard containers, push_back() expects a
    reference to the object. unlike the standard container, however, push_back()
    places the actual object in the list and not a copy-constructed duplicate.
  
    iterating over the list follows the same idiom as the stl:
  
    @code
  
    for (list <object>::iterator iter = list.begin(); iter != list.end; ++iter)
        std::cout << iter->m_value;
  
    @endcode
  
    you can even use boost_foreach, or range based for loops:
  
    @code
  
    boost_foreach (object& object, list)  // boost only
        std::cout << object.m_value;
  
    for (object& object : list)           // c++11 only
        std::cout << object.m_value;
  
    @endcode
  
    because list is mostly stl compliant, it can be passed into stl algorithms:
    e.g. `std::for_each()` or `std::find_first_of()`.
  
    in general, objects placed into a list should be dynamically allocated
    although this cannot be enforced at compile time. since the caller provides
    the storage for the object, the caller is also responsible for deleting the
    object. an object still exists after being removed from a list, until the
    caller deletes it. this means an element can be moved from one list to
    another with practically no overhead.
  
    unlike the standard containers, an object may only exist in one list at a
    time, unless special preparations are made. the tag template parameter is
    used to distinguish between different list types for the same object,
    allowing the object to exist in more than one list simultaneously.
  
    for example, consider an actor system where a global list of actors is
    maintained, so that they can each be periodically receive processing
    time. we wish to also maintain a list of the subset of actors that require
    a domain-dependent update. to achieve this, we declare two tags, the
    associated list types, and the list element thusly:
  
    @code
  
    struct actor;         // forward declaration required
  
    struct processtag { };
    struct updatetag { };
  
    typedef list <actor, processtag> processlist;
    typedef list <actor, updatetag> updatelist;
  
    // derive from both node types so we can be in each list at once.
    //
    struct actor : processlist::node, updatelist::node
    {
        bool process ();    // returns true if we need an update
        void update ();
    };
  
    @endcode
  
    @tparam t the base type of element which the list will store
                    pointers to.
  
    @tparam tag an optional unique type name used to distinguish lists and nodes,
                when the object can exist in multiple lists simultaneously.
  
    @ingroup beast_core intrusive
*/
template <typename t, typename tag = void>
class list
{
public:
    typedef typename detail::listnode <t, tag> node;

    typedef t                    value_type;
    typedef value_type*          pointer;
    typedef value_type&          reference;
    typedef value_type const*    const_pointer;
    typedef value_type const&    const_reference;
    typedef std::size_t          size_type;
    typedef std::ptrdiff_t       difference_type;

    typedef detail::listiterator <node>       iterator;
    typedef detail::listiterator <node const> const_iterator;

    /** create an empty list. */
    list ()
    {
        m_head.m_prev = nullptr; // identifies the head
        m_tail.m_next = nullptr; // identifies the tail
        clear ();
    }

    list(list const&) = delete;
    list& operator= (list const&) = delete;

    /** determine if the list is empty.
        @return `true` if the list is empty.
    */
    bool empty () const noexcept
    {
        return size () == 0;
    }

    /** returns the number of elements in the list. */
    size_type size () const noexcept
    {
        return m_size;
    }

    /** obtain a reference to the first element.
        @invariant the list may not be empty.
        @return a reference to the first element.
    */
    reference front () noexcept
    {
        return element_from (m_head.m_next);
    }

    /** obtain a const reference to the first element.
        @invariant the list may not be empty.
        @return a const reference to the first element.
    */
    const_reference front () const noexcept
    {
        return element_from (m_head.m_next);
    }

    /** obtain a reference to the last element.
        @invariant the list may not be empty.
        @return a reference to the last element.
    */
    reference back () noexcept
    {
        return element_from (m_tail.m_prev);
    }

    /** obtain a const reference to the last element.
        @invariant the list may not be empty.
        @return a const reference to the last element.
    */
    const_reference back () const noexcept
    {
        return element_from (m_tail.m_prev);
    }

    /** obtain an iterator to the beginning of the list.
        @return an iterator pointing to the beginning of the list.
    */
    iterator begin () noexcept
    {
        return iterator (m_head.m_next);
    }

    /** obtain a const iterator to the beginning of the list.
        @return a const iterator pointing to the beginning of the list.
    */
    const_iterator begin () const noexcept
    {
        return const_iterator (m_head.m_next);
    }

    /** obtain a const iterator to the beginning of the list.
        @return a const iterator pointing to the beginning of the list.
    */
    const_iterator cbegin () const noexcept
    {
        return const_iterator (m_head.m_next);
    }

    /** obtain a iterator to the end of the list.
        @return an iterator pointing to the end of the list.
    */
    iterator end () noexcept
    {
        return iterator (&m_tail);
    }

    /** obtain a const iterator to the end of the list.
        @return a constiterator pointing to the end of the list.
    */
    const_iterator end () const noexcept
    {
        return const_iterator (&m_tail);
    }

    /** obtain a const iterator to the end of the list
        @return a constiterator pointing to the end of the list.
    */
    const_iterator cend () const noexcept
    {
        return const_iterator (&m_tail);
    }

    /** clear the list.
        @note this does not free the elements.
    */
    void clear () noexcept
    {
        m_head.m_next = &m_tail;
        m_tail.m_prev = &m_head;
        m_size = 0;
    }

    /** insert an element.
        @invariant the element must not already be in the list.
        @param pos the location to insert after.
        @param element the element to insert.
        @return an iterator pointing to the newly inserted element.
    */
    iterator insert (iterator pos, t& element) noexcept
    {
        node* node = static_cast <node*> (&element);
        node->m_next = &*pos;
        node->m_prev = node->m_next->m_prev;
        node->m_next->m_prev = node;
        node->m_prev->m_next = node;
        ++m_size;
        return iterator (node);
    }

    /** insert another list into this one.
        the other list is cleared.
        @param pos the location to insert after.
        @param other the list to insert.
    */
    void insert (iterator pos, list& other) noexcept
    {
        if (!other.empty ())
        {
            node* before = &*pos;
            other.m_head.m_next->m_prev = before->m_prev;
            before->m_prev->m_next = other.m_head.m_next;
            other.m_tail.m_prev->m_next = before;
            before->m_prev = other.m_tail.m_prev;
            m_size += other.m_size;
            other.clear ();
        }
    }

    /** remove an element.
        @invariant the element must exist in the list.
        @param pos an iterator pointing to the element to remove.
        @return an iterator pointing to the next element after the one removed.
    */
    iterator erase (iterator pos) noexcept
    {
        node* node = &*pos;
        ++pos;
        node->m_next->m_prev = node->m_prev;
        node->m_prev->m_next = node->m_next;
        --m_size;
        return pos;
    }

    /** insert an element at the beginning of the list.
        @invariant the element must not exist in the list.
        @param element the element to insert.
    */
    iterator push_front (t& element) noexcept
    {
        return insert (begin (), element);
    }

    /** remove the element at the beginning of the list.
        @invariant the list must not be empty.
        @return a reference to the popped element.
    */
    t& pop_front () noexcept
    {
        t& element (front ());
        erase (begin ());
        return element;
    }

    /** append an element at the end of the list.
        @invariant the element must not exist in the list.
        @param element the element to append.
    */
    iterator push_back (t& element) noexcept
    {
        return insert (end (), element);
    }

    /** remove the element at the end of the list.
        @invariant the list must not be empty.
        @return a reference to the popped element.
    */
    t& pop_back () noexcept
    {
        t& element (back ());
        erase (--end ());
        return element;
    }

    /** swap contents with another list. */
    void swap (list& other) noexcept
    {
        list temp;
        temp.append (other);
        other.append (*this);
        append (temp);
    }

    /** insert another list at the beginning of this list.
        the other list is cleared.
        @param list the other list to insert.
    */
    iterator prepend (list& list) noexcept
    {
        return insert (begin (), list);
    }

    /** append another list at the end of this list.
        the other list is cleared.
        @param list the other list to append.
    */
    iterator append (list& list) noexcept
    {
        return insert (end (), list);
    }

    /** obtain an iterator from an element.
        @invariant the element must exist in the list.
        @param element the element to obtain an iterator for.
        @return an iterator to the element.
    */
    iterator iterator_to (t& element) const noexcept
    {
        return iterator (static_cast <node*> (&element));
    }

    /** obtain a const iterator from an element.
        @invariant the element must exist in the list.
        @param element the element to obtain an iterator for.
        @return a const iterator to the element.
    */
    const_iterator const_iterator_to (t const& element) const noexcept
    {
        return const_iterator (static_cast <node const*> (&element));
    }

private:
    reference element_from (node* node) noexcept
    {
        return *(static_cast <pointer> (node));
    }

    const_reference element_from (node const* node) const noexcept
    {
        return *(static_cast <const_pointer> (node));
    }

private:
    size_type m_size;
    node m_head;
    node m_tail;
};

}

#endif
