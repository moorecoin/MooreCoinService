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

namespace beast
{

// manages the list of hooks, and calls
// whoever is in the list at exit time.
//
class atexithook::manager
{
public:
    manager ()
        : m_didstaticdestruction (false)
    {
    }

    static inline manager& get ()
    {
        return staticobject <manager>::get();
    }

    void insert (item& item)
    {
        scopedlocktype lock (m_mutex);

        // adding a new atexithook during or after the destruction
        // of objects with static storage duration has taken place?
        // surely something has gone wrong.
        //
        bassert (! m_didstaticdestruction);
        m_list.push_front (item);
    }

    void erase (item& item)
    {
        scopedlocktype lock (m_mutex);

        m_list.erase (m_list.iterator_to (item));
    }

private:
    // called at program exit when destructors for objects
    // with static storage duration are invoked.
    //
    void dostaticdestruction ()
    {
        // in theory this shouldn't be needed (?)
        scopedlocktype lock (m_mutex);

        bassert (! m_didstaticdestruction);
        m_didstaticdestruction = true;

        for (list <item>::iterator iter (m_list.begin()); iter != m_list.end();)
        {
            item& item (*iter++);
            atexithook* const hook (item.hook ());
            hook->onexit ();
        }

        // now do the leak checking
        //
        leakcheckedbase::checkforleaks ();
    }

    struct staticdestructor
    {
        ~staticdestructor ()
        {
            manager::get().dostaticdestruction();
        }
    };

    typedef criticalsection mutextype;
    typedef mutextype::scopedlocktype scopedlocktype;

    static staticdestructor s_staticdestructor;

    mutextype m_mutex;
    list <item> m_list;
    bool m_didstaticdestruction;
};

// this is an object with static storage duration.
// when it gets destroyed, we will call into the manager to
// call all of the atexithook items in the list.
//
atexithook::manager::staticdestructor atexithook::manager::s_staticdestructor;

//------------------------------------------------------------------------------

atexithook::item::item (atexithook* hook)
    : m_hook (hook)
{
}

atexithook* atexithook::item::hook ()
{
    return m_hook;
}

//------------------------------------------------------------------------------

atexithook::atexithook ()
    : m_item (this)
{
#if beast_ios
    // patrick dehne:
    //      atexithook::manager::insert crashes on ios
    //      if the storage is not accessed before it is used.
    //
    // vfalco todo figure out why and fix it cleanly if needed.
    //
    char* hack = atexithook::manager::s_list.s_storage;
#endif

    manager::get().insert (m_item);
}

atexithook::~atexithook ()
{
    manager::get().erase (m_item);
}

} // beast
