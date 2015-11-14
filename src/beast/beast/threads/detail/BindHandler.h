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

#ifndef beast_threads_bindhandler_h_included
#define beast_threads_bindhandler_h_included

namespace beast {
namespace detail {

/** overloaded function that re-binds arguments to a handler. */
/** @{ */
template <class handler, class p1>
class bindhandler1
{
private:
    handler handler;
    p1 p1;

public:
    bindhandler1 (handler const& handler_, p1 const& p1_)
        : handler (handler_)
        , p1 (p1_)
        { }

    bindhandler1 (handler& handler_, p1 const& p1_)
        : handler (beast_move_cast(handler)(handler_))
        , p1 (p1_)
        { }

    void operator()()
    {
        handler (
            static_cast <p1 const&> (p1)
            );
    }
    
    void operator()() const
    {
        handler (p1);
    }
};

template <class handler, class p1>
bindhandler1 <handler, p1> bindhandler (handler handler, p1 const& p1)
{
    return bindhandler1 <handler, p1> (handler, p1);
}

//------------------------------------------------------------------------------

template <class handler, class p1, class p2>
class bindhandler2
{
private:
    handler handler;
    p1 p1; p2 p2;

public:
    bindhandler2 (handler const& handler_,
        p1 const& p1_, p2 const& p2_)
        : handler (handler_)
        , p1 (p1_), p2 (p2_)
        { }

    bindhandler2 (handler& handler_,
        p1 const& p1_, p2 const& p2_)
        : handler (beast_move_cast(handler)(handler_))
        , p1 (p1_), p2 (p2_)
        { }

    void operator()()
    {
        handler (
            static_cast <p1 const&> (p1), static_cast <p2 const&> (p2));
    }

    void operator()() const
        { handler (p1, p2); }
};

template <class handler, class p1, class p2>
bindhandler2 <handler, p1, p2> bindhandler (handler handler,
    p1 const& p1, p2 const& p2)
{
    return bindhandler2 <handler, p1, p2> (
        handler, p1, p2);
}

//------------------------------------------------------------------------------

template <class handler, class p1, class p2, class p3>
class bindhandler3
{
private:
    handler handler;
    p1 p1; p2 p2; p3 p3;

public:
    bindhandler3 (handler const& handler_,
        p1 const& p1_, p2 const& p2_, p3 const& p3_)
        : handler (handler_)
        , p1 (p1_), p2 (p2_), p3 (p3_)
        { }

    bindhandler3 (handler& handler_,
        p1 const& p1_, p2 const& p2_, p3 const& p3_)
        : handler (beast_move_cast(handler)(handler_))
        , p1 (p1_), p2 (p2_), p3 (p3_)
        { }

    void operator()()
    {
        handler (
            static_cast <p1 const&> (p1), static_cast <p2 const&> (p2), static_cast <p3 const&> (p3));
    }

    void operator()() const
        { handler (p1, p2, p3); }
};

template <class handler, class p1, class p2, class p3>
bindhandler3 <handler, p1, p2, p3> bindhandler (handler handler,
    p1 const& p1, p2 const& p2, p3 const& p3)
{
    return bindhandler3 <handler, p1, p2, p3> (
        handler, p1, p2, p3);
}

//------------------------------------------------------------------------------

template <class handler, class p1, class p2, class p3, class p4>
class bindhandler4
{
private:
    handler handler;
    p1 p1; p2 p2; p3 p3; p4 p4;

public:
    bindhandler4 (handler const& handler_,
        p1 const& p1_, p2 const& p2_, p3 const& p3_, p4 const& p4_)
        : handler (handler_)
        , p1 (p1_), p2 (p2_), p3 (p3_), p4 (p4_)
        { }

    bindhandler4 (handler& handler_,
        p1 const& p1_, p2 const& p2_, p3 const& p3_, p4 const& p4_)
        : handler (beast_move_cast(handler)(handler_))
        , p1 (p1_), p2 (p2_), p3 (p3_), p4 (p4_)
        { }

    void operator()()
    {
        handler (
            static_cast <p1 const&> (p1), static_cast <p2 const&> (p2), static_cast <p3 const&> (p3), 
            static_cast <p4 const&> (p4)
            );
    }

    void operator()() const
        { handler (p1, p2, p3, p4); }
};

template <class handler, class p1, class p2, class p3, class p4>
bindhandler4 <handler, p1, p2, p3, p4> bindhandler (handler handler,
    p1 const& p1, p2 const& p2, p3 const& p3, p4 const& p4)
{
    return bindhandler4 <handler, p1, p2, p3, p4> (
        handler, p1, p2, p3, p4);
}

//------------------------------------------------------------------------------

template <class handler, class p1, class p2, class p3, class p4, class p5>
class bindhandler5
{
private:
    handler handler;
    p1 p1; p2 p2; p3 p3; p4 p4; p5 p5;

public:
    bindhandler5 (handler const& handler_,
        p1 const& p1_, p2 const& p2_, p3 const& p3_, p4 const& p4_, p5 const& p5_)
        : handler (handler_)
        , p1 (p1_), p2 (p2_), p3 (p3_), p4 (p4_), p5 (p5_)
        { }

    bindhandler5 (handler& handler_,
        p1 const& p1_, p2 const& p2_, p3 const& p3_, p4 const& p4_, p5 const& p5_)
        : handler (beast_move_cast(handler)(handler_))
        , p1 (p1_), p2 (p2_), p3 (p3_), p4 (p4_), p5 (p5_)
        { }

    void operator()()
    {
        handler (
            static_cast <p1 const&> (p1), static_cast <p2 const&> (p2), static_cast <p3 const&> (p3), 
            static_cast <p4 const&> (p4), static_cast <p5 const&> (p5)
            );
    }

    void operator()() const
        { handler (p1, p2, p3, p4, p5); }
};

template <class handler, class p1, class p2, class p3, class p4, class p5>
bindhandler5 <handler, p1, p2, p3, p4, p5> bindhandler (handler handler,
    p1 const& p1, p2 const& p2, p3 const& p3, p4 const& p4, p5 const& p5)
{
    return bindhandler5 <handler, p1, p2, p3, p4, p5> (
        handler, p1, p2, p3, p4, p5);
}

//------------------------------------------------------------------------------

template <class handler, class p1, class p2, class p3, class p4, class p5, class p6>
class bindhandler6
{
private:
    handler handler;
    p1 p1; p2 p2; p3 p3; p4 p4; p5 p5; p6 p6;

public:
    bindhandler6 (handler const& handler_,
        p1 const& p1_, p2 const& p2_, p3 const& p3_, p4 const& p4_, p5 const& p5_, p6 const& p6_)
        : handler (handler_)
        , p1 (p1_), p2 (p2_), p3 (p3_), p4 (p4_), p5 (p5_), p6 (p6_)
        { }

    bindhandler6 (handler& handler_,
        p1 const& p1_, p2 const& p2_, p3 const& p3_, p4 const& p4_, p5 const& p5_, p6 const& p6_)
        : handler (beast_move_cast(handler)(handler_))
        , p1 (p1_), p2 (p2_), p3 (p3_), p4 (p4_), p5 (p5_), p6 (p6_)
        { }

    void operator()()
    {
        handler (
            static_cast <p1 const&> (p1), static_cast <p2 const&> (p2), static_cast <p3 const&> (p3), 
            static_cast <p4 const&> (p4), static_cast <p5 const&> (p5), static_cast <p6 const&> (p6)
            );
    }

    void operator()() const
        { handler (p1, p2, p3, p4, p5, p6); }
};

template <class handler, class p1, class p2, class p3, class p4, class p5, class p6>
bindhandler6 <handler, p1, p2, p3, p4, p5, p6> bindhandler (handler handler,
    p1 const& p1, p2 const& p2, p3 const& p3, p4 const& p4, p5 const& p5, p6 const& p6)
{
    return bindhandler6 <handler, p1, p2, p3, p4, p5, p6> (
        handler, p1, p2, p3, p4, p5, p6);
}

/** @} */

}
}

#endif
