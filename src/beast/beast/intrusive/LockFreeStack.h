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

#ifndef beast_intrusive_lockfreestack_h_included
#define beast_intrusive_lockfreestack_h_included

#include <atomic>
#include <iterator>
#include <type_traits>

namespace beast {

//------------------------------------------------------------------------------

template <class container, bool isconst>
class lockfreestackiterator
    : public std::iterator <
        std::forward_iterator_tag,
        typename container::value_type,
        typename container::difference_type,
        typename std::conditional <isconst,
            typename container::const_pointer,
            typename container::pointer>::type,
        typename std::conditional <isconst,
            typename container::const_reference,
            typename container::reference>::type>
{
protected:
    typedef typename container::node node;
    typedef typename std::conditional <
        isconst, node const*, node*>::type nodeptr;

public:
    typedef typename container::value_type value_type;
    typedef typename std::conditional <isconst,
        typename container::const_pointer,
        typename container::pointer>::type pointer;
    typedef typename std::conditional <isconst,
        typename container::const_reference,
        typename container::reference>::type reference;

    lockfreestackiterator ()
        : m_node ()
    {
    }

    lockfreestackiterator (nodeptr node)
        : m_node (node)
    {
    }

    template <bool otherisconst>
    explicit lockfreestackiterator (lockfreestackiterator <container, otherisconst> const& other)
        : m_node (other.m_node)
    {
    }

    lockfreestackiterator& operator= (nodeptr node)
    {
        m_node = node;
        return static_cast <lockfreestackiterator&> (*this);
    }

    lockfreestackiterator& operator++ ()
    {
        m_node = m_node->m_next.load ();
        return static_cast <lockfreestackiterator&> (*this);
    }

    lockfreestackiterator operator++ (int)
    {
        lockfreestackiterator result (*this);
        m_node = m_node->m_next;
        return result;
    }

    nodeptr node() const
    {
        return m_node;
    }

    reference operator* () const
    {
        return *this->operator-> ();
    }

    pointer operator-> () const
    {
        return static_cast <pointer> (m_node);
    }

private:
    nodeptr m_node;
};

//------------------------------------------------------------------------------

template <class container, bool lhsisconst, bool rhsisconst>
bool operator== (lockfreestackiterator <container, lhsisconst> const& lhs,
                 lockfreestackiterator <container, rhsisconst> const& rhs)
{
    return lhs.node() == rhs.node();
}

template <class container, bool lhsisconst, bool rhsisconst>
bool operator!= (lockfreestackiterator <container, lhsisconst> const& lhs,
                 lockfreestackiterator <container, rhsisconst> const& rhs)
{
    return lhs.node() != rhs.node();
}

//------------------------------------------------------------------------------

/** multiple producer, multiple consumer (mpmc) intrusive stack.

    this stack is implemented using the same intrusive interface as list.
    all mutations are lock-free.

    the caller is responsible for preventing the "aba" problem:
        http://en.wikipedia.org/wiki/aba_problem

    @param tag  a type name used to distinguish lists and nodes, for
                putting objects in multiple lists. if this parameter is
                omitted, the default tag is used.
*/
template <class element, class tag = void>
class lockfreestack
{
public:
    class node
    {
    public:
        node () 
            : m_next (nullptr)
        { }

        explicit node (node* next)
            : m_next (next)
        { }

        node(node const&) = delete;
        node& operator= (node const&) = delete;

    private:
        friend class lockfreestack;
        
        template <class container, bool isconst>
        friend class lockfreestackiterator;

        std::atomic <node*> m_next;
    };

public:
    typedef element                             value_type;
    typedef element*                            pointer;
    typedef element&                            reference;
    typedef element const*                      const_pointer;
    typedef element const&                      const_reference;
    typedef std::size_t                         size_type;
    typedef std::ptrdiff_t                      difference_type;
    typedef lockfreestackiterator <
        lockfreestack <element, tag>, false>    iterator;
    typedef lockfreestackiterator <
        lockfreestack <element, tag>, true>     const_iterator;

    lockfreestack ()
        : m_end (nullptr)
        , m_head (&m_end)
    {
    }

    lockfreestack(lockfreestack const&) = delete;
    lockfreestack& operator= (lockfreestack const&) = delete;

    /** returns true if the stack is empty. */
    bool empty() const
    {
        return m_head.load () == &m_end;
    }

    /** push a node onto the stack.
        the caller is responsible for preventing the aba problem.
        this operation is lock-free.
        thread safety:
            safe to call from any thread.

        @param node the node to push.

        @return `true` if the stack was previously empty. if multiple threads
                are attempting to push, only one will receive `true`.
    */
    // vfalco note fix this, shouldn't it be a reference like intrusive list?
    bool push_front (node* node)
    {
        bool first;
        node* old_head = m_head.load (std::memory_order_relaxed);
        do
        {
            first = (old_head == &m_end);
            node->m_next = old_head;
        }
        while (!m_head.compare_exchange_strong (old_head, node,
                                                std::memory_order_release,
                                                std::memory_order_relaxed));
        return first;
    }

    /** pop an element off the stack.
        the caller is responsible for preventing the aba problem.
        this operation is lock-free.
        thread safety:
            safe to call from any thread.

        @return the element that was popped, or `nullptr` if the stack
                was empty.
    */
    element* pop_front ()
    {
        node* node = m_head.load ();
        node* new_head;
        do
        {
            if (node == &m_end)
                return nullptr;
            new_head = node->m_next.load ();
        }
        while (!m_head.compare_exchange_strong (node, new_head,
                                                std::memory_order_release,
                                                std::memory_order_relaxed));
        return static_cast <element*> (node);
    }

    /** return a forward iterator to the beginning or end of the stack.
        undefined behavior results if push_front or pop_front is called
        while an iteration is in progress.
        thread safety:
            caller is responsible for synchronization.
    */
    /** @{ */
    iterator begin ()
    {
        return iterator (m_head.load ());
    }

    iterator end ()
    {
        return iterator (&m_end);
    }
    
    const_iterator begin () const
    {
        return const_iterator (m_head.load ());
    }

    const_iterator end () const
    {
        return const_iterator (&m_end);
    }
    
    const_iterator cbegin () const
    {
        return const_iterator (m_head.load ());
    }

    const_iterator cend () const
    {
        return const_iterator (&m_end);
    }
    /** @} */

private:
    node m_end;
    std::atomic <node*> m_head;
};

}

#endif
