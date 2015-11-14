//------------------------------------------------------------------------------
/*
    this file is part of rippled: https://github.com/ripple/rippled
    copyright (c) 2012, 2013 ripple labs inc.

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

#include <beastconfig.h>
#include <ripple/basics/countedobject.h>

namespace ripple {

countedobjects& countedobjects::getinstance ()
{
    static countedobjects instance;

    return instance;
}

countedobjects::countedobjects ()
    : m_count (0)
    , m_head (nullptr)
{
}

countedobjects::~countedobjects ()
{
}

countedobjects::list countedobjects::getcounts (int minimumthreshold) const
{
    list counts;

    // when other operations are concurrent, the count
    // might be temporarily less than the actual count.
    int const count = m_count.load ();

    counts.reserve (count);

    counterbase* counter = m_head.load ();

    while (counter != nullptr)
    {
        if (counter->getcount () >= minimumthreshold)
        {
            entry entry;

            entry.first = counter->getname ();
            entry.second = counter->getcount ();

            counts.push_back (entry);
        }

        counter = counter->getnext ();
    }

    return counts;
}

//------------------------------------------------------------------------------

countedobjects::counterbase::counterbase ()
    : m_count (0)
{
    // insert ourselves at the front of the lock-free linked list

    countedobjects& instance = countedobjects::getinstance ();
    counterbase* head;

    do
    {
        head = instance.m_head.load ();
        m_next = head;
    }
    while (instance.m_head.exchange (this) != head);

    ++instance.m_count;
}

countedobjects::counterbase::~counterbase ()
{
    // vfalco note if the counters are destroyed before the singleton,
    //             undefined behavior will result if the singleton's member
    //             functions are called.
}

} // ripple
