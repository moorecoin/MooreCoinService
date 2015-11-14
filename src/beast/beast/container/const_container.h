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

#ifndef beast_container_const_container_h_included
#define beast_container_const_container_h_included

namespace beast {

/** adapter to constrain a container interface.
    the interface allows for limited read only operations. derived classes
    provide additional behavior.
*/
template <class container>
class const_container
{
private:
    typedef container cont_type;

    cont_type m_cont;

protected:
    cont_type& cont()
    {
        return m_cont;
    }

    cont_type const& cont() const
    {
        return m_cont;
    }

public:
    typedef typename cont_type::value_type value_type;
    typedef typename cont_type::size_type size_type;
    typedef typename cont_type::difference_type difference_type;
    typedef typename cont_type::const_iterator iterator;
    typedef typename cont_type::const_iterator const_iterator;

    /** returns `true` if the container is empty. */
    bool
    empty() const
    {
        return m_cont.empty();
    }

    /** returns the number of items in the container. */
    size_type
    size() const
    {
        return m_cont.size();
    }

    /** returns forward iterators for traversal. */
    /** @{ */
    const_iterator
    begin() const
    {
        return m_cont.cbegin();
    }

    const_iterator
    cbegin() const
    {
        return m_cont.cbegin();
    }

    const_iterator
    end() const
    {
        return m_cont.cend();
    }

    const_iterator
    cend() const
    {
        return m_cont.cend();
    }
    /** @} */
};

} // beast

#endif
