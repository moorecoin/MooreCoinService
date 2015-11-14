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

#ifndef beast_core_atexithook_h_included
#define beast_core_atexithook_h_included

#include <beast/intrusive/list.h>

namespace beast {

/** hook for performing activity on program exit.

    these hooks execute when objects with static storage duration are
    destroyed. the hooks are called in the reverse order that they were
    created.

    to use, derive your class from atexithook and implement onexit.
    alternatively, add atexitmemberhook as a data member of your class and
    then provide your own onexit function with this signature:

    @code

    void onexit ()

    @endcode

    @see atexitmemberhook
*/
/** @{ */
class atexithook
{
protected:
    atexithook ();
    virtual ~atexithook ();

protected:
    /** called at program exit. */
    virtual void onexit () = 0;

private:
    class manager;

    class item : public list <item>::node
    {
    public:
        explicit item (atexithook* hook);
        atexithook* hook ();

    private:
        atexithook* m_hook;
    };

    item m_item;
};

/** helper for utilizing the atexithook as a data member.
*/
template <class object>
class atexitmemberhook : public atexithook
{
public:
    explicit atexitmemberhook (object* owner) : m_owner (owner)
    {
    }

private:
    void onexit ()
    {
        m_owner->onexit ();
    }

    object* m_owner;
};
/** @} */

} // beast

#endif
