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

#include <beast/utility/leakchecked.h>

namespace beast {

namespace detail
{

class leakcheckedbase::leakcounterbase::singleton
{
public:
    void push_back (leakcounterbase* counter)
    {
        m_list.push_front (counter);
    }

    void checkforleaks ()
    {
        for (;;)
        {
            leakcounterbase* const counter = m_list.pop_front ();

            if (!counter)
                break;

            counter->checkforleaks ();
        }
    }

    static singleton& getinstance ()
    {
        static singleton instance;

        return instance;
    }

private:
    friend class leakcheckedbase;

    lockfreestack <leakcounterbase> m_list;
};

//------------------------------------------------------------------------------

leakcheckedbase::leakcounterbase::leakcounterbase ()
    : m_count (0)
{
    singleton::getinstance ().push_back (this);
}

void leakcheckedbase::leakcounterbase::checkforleaks ()
{
    // if there's a runtime error from this line, it means there's
    // an order of destruction problem between different translation units!
    //
    this->checkpurevirtual ();

    int const count = m_count.load ();

    if (count > 0)
    {
        outputdebugstring ("leaked objects: " + std::to_string (count) + 
            " instances of " + getclassname ());
    }
}

//------------------------------------------------------------------------------

void leakcheckedbase::reportdanglingpointer (char const* objectname)
{
    outputdebugstring (std::string ("dangling pointer deletion: ") + objectname);
    bassertfalse;
}

//------------------------------------------------------------------------------

void leakcheckedbase::checkforleaks ()
{
    leakcounterbase::singleton::getinstance ().checkforleaks ();
}

}

}
