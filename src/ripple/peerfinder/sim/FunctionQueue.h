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

#ifndef ripple_peerfinder_sim_functionqueue_h_included
#define ripple_peerfinder_sim_functionqueue_h_included

namespace ripple {
namespace peerfinder {
namespace sim {

/** maintains a queue of functors that can be called later. */
class functionqueue
{
private:
    class basicwork
    {
    public:
        virtual ~basicwork ()
            { }
        virtual void operator() () = 0;
    };

    template <typename function>
    class work : public basicwork
    {
    public:
        explicit work (function f)
            : m_f (f)
            { }
        void operator() ()
            { (m_f)(); }
    private:
        function m_f;
    };

    std::list <std::unique_ptr <basicwork>> m_work;

public:
    /** returns `true` if there is no remaining work */
    bool empty ()
        { return m_work.empty(); }

    /** queue a function.
        function must be callable with this signature:
            void (void)
    */
    template <typename function>
    void post (function f)
        { m_work.emplace_back (new work <function> (f)); }

    /** run all pending functions.
        the functions will be invoked in the order they were queued.
    */
    void run ()
    {
        while (! m_work.empty ())
        {
            (*m_work.front())();
            m_work.pop_front();
        }
    }
};

}
}
}

#endif
