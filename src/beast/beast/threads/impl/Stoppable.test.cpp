//------------------------------------------------------------------------------
/*
    this file is part of beast: https://github.com/vinniefalco/beast
    copyright 2012, vinnie falco <vinnie.falco@gmail.com>

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

#include <beast/threads/stoppable.h>

namespace beast {

class stoppable_test
    : public unit_test::suite
{
/*
            r
          / | \
       /    |  \
      a     b   c
    / | \   /\  |
    d e f  g h  i
      |
      j
*/
    unsigned count = 0;

    class d
        : public stoppable
    {
        stoppable_test& test_;
    public:
        d(stoppable& parent, stoppable_test& test)
            : stoppable("d", parent)
            , test_(test)
        {}

        void onprepare() override
        {
            test_.expect(++test_.count == 9, "d::onprepare called out of order");
        }

        void onstart() override
        {
            test_.expect(--test_.count == 0, "d::onstart called out of order");
        }

        void onstop() override
        {
            test_.expect(++test_.count == 11, "d::onstop called out of order");
        }

        void onchildrenstopped() override
        {
            stoppable::stopped();
            test_.expect(--test_.count == 2, "d::onchildrenstopped called out of order");
        }
    };

    class j
        : public stoppable
    {
        stoppable_test& test_;
    public:
        j(stoppable& parent, stoppable_test& test)
            : stoppable("j", parent)
            , test_(test)
        {}

        void onprepare() override
        {
            test_.expect(++test_.count == 7, "j::onprepare called out of order");
        }

        void onstart() override
        {
            test_.expect(--test_.count == 1, "j::onstart called out of order");
        }

        void onstop() override
        {
            test_.expect(++test_.count == 10, "j::onstop called out of order");
        }

        void onchildrenstopped() override
        {
            stoppable::stopped();
            test_.expect(--test_.count == 4, "j::onchildrenstopped called out of order");
        }
    };

    class e
        : public stoppable
    {
        j j_;
        stoppable_test& test_;
    public:
        e(stoppable& parent, stoppable_test& test)
            : stoppable("e", parent)
            , j_(*this, test)
            , test_(test)
        {}

        void onprepare() override
        {
            test_.expect(++test_.count == 8, "e::onprepare called out of order");
        }

        void onstart() override
        {
            test_.expect(--test_.count == 2, "e::onstart called out of order");
        }

        void onstop() override
        {
            test_.expect(++test_.count == 9, "e::onstop called out of order");
        }

        void onchildrenstopped() override
        {
            stoppable::stopped();
            test_.expect(--test_.count == 3, "e::onchildrenstopped called out of order");
        }
    };

    class f
        : public stoppable
    {
        stoppable_test& test_;
    public:
        f(stoppable& parent, stoppable_test& test)
            : stoppable("f", parent)
            , test_(test)
        {}

        void onprepare() override
        {
            test_.expect(++test_.count == 6, "f::onprepare called out of order");
        }

        void onstart() override
        {
            test_.expect(--test_.count == 3, "f::onstart called out of order");
        }

        void onstop() override
        {
            test_.expect(++test_.count == 8, "f::onstop called out of order");
        }

        void onchildrenstopped() override
        {
            stoppable::stopped();
            test_.expect(--test_.count == 5, "f::onchildrenstopped called out of order");
        }
    };

    class a
        : public stoppable
    {
        enum {running, please_stop, have_stopped};
        d d_;
        e e_;
        f f_;
        stoppable_test& test_;
        std::atomic<int> stop_;
    public:
        a(stoppable& parent, stoppable_test& test)
            : stoppable("a", parent)
            , d_(*this, test)
            , e_(*this, test)
            , f_(*this, test)
            , test_(test)
            , stop_(running)
        {}

        void run()
        {
            while (stop_ == running)
                ;
            stop_ = have_stopped;
        }

        void onprepare() override
        {
            test_.expect(++test_.count == 10, "a::onprepare called out of order");
        }

        void onstart() override
        {
            test_.expect(--test_.count == 4, "a::onstart called out of order");
        }

        void onstop() override
        {
            test_.expect(++test_.count == 7, "a::onstop called out of order");
        }

        void onchildrenstopped() override
        {
            stop_ = please_stop;
            while (stop_ != have_stopped)
                ;
            stoppable::stopped();
            test_.expect(--test_.count == 1, "a::onchildrenstopped called out of order");
        }
    };

    class g
        : public stoppable
    {
        stoppable_test& test_;
    public:
        g(stoppable& parent, stoppable_test& test)
            : stoppable("g", parent)
            , test_(test)
        {}

        void onprepare() override
        {
            test_.expect(++test_.count == 4, "g::onprepare called out of order");
        }

        void onstart() override
        {
            test_.expect(--test_.count == 5, "g::onstart called out of order");
        }

        void onstop() override
        {
            test_.expect(++test_.count == 6, "g::onstop called out of order");
        }

        void onchildrenstopped() override
        {
            stoppable::stopped();
            test_.expect(--test_.count == 7, "g::onchildrenstopped called out of order");
        }
    };

    class h
        : public stoppable
    {
        stoppable_test& test_;
    public:
        h(stoppable& parent, stoppable_test& test)
            : stoppable("h", parent)
            , test_(test)
        {}

        void onprepare() override
        {
            test_.expect(++test_.count == 3, "h::onprepare called out of order");
        }

        void onstart() override
        {
            test_.expect(--test_.count == 6, "h::onstart called out of order");
        }

        void onstop() override
        {
            test_.expect(++test_.count == 5, "h::onstop called out of order");
        }

        void onchildrenstopped() override
        {
            stoppable::stopped();
            test_.expect(--test_.count == 8, "h::onchildrenstopped called out of order");
        }
    };

    class b
        : public stoppable
    {
        g g_;
        h h_;
        stoppable_test& test_;
    public:
        b(stoppable& parent, stoppable_test& test)
            : stoppable("b", parent)
            , g_(*this, test)
            , h_(*this, test)
            , test_(test)
        {}

        void onprepare() override
        {
            test_.expect(++test_.count == 5, "b::onprepare called out of order");
        }

        void onstart() override
        {
            test_.expect(--test_.count == 7, "b::onstart called out of order");
        }

        void onstop() override
        {
            test_.expect(++test_.count == 4, "b::onstop called out of order");
        }

        void onchildrenstopped() override
        {
            stoppable::stopped();
            test_.expect(--test_.count == 6, "b::onchildrenstopped called out of order");
        }
    };

    class i
        : public stoppable
    {
        stoppable_test& test_;
    public:
        i(stoppable& parent, stoppable_test& test)
            : stoppable("i", parent)
            , test_(test)
        {}

        void onprepare() override
        {
            test_.expect(++test_.count == 1, "i::onprepare called out of order");
        }

        void onstart() override
        {
            test_.expect(--test_.count == 8, "i::onstart called out of order");
        }

        void onstop() override
        {
            test_.expect(++test_.count == 3, "i::onstop called out of order");
        }

        void onchildrenstopped() override
        {
            stoppable::stopped();
            test_.expect(--test_.count == 10, "i::onchildrenstopped called out of order");
        }
    };

    class c
        : public stoppable
    {
        i i_;
        stoppable_test& test_;
    public:
        c(stoppable& parent, stoppable_test& test)
            : stoppable("c", parent)
            , i_(*this, test)
            , test_(test)
        {}

        void onprepare() override
        {
            test_.expect(++test_.count == 2, "c::onprepare called out of order");
        }

        void onstart() override
        {
            test_.expect(--test_.count == 9, "c::onstart called out of order");
        }

        void onstop() override
        {
            test_.expect(++test_.count == 2, "c::onstop called out of order");
        }

        void onchildrenstopped() override
        {
            stoppable::stopped();
            test_.expect(--test_.count == 9, "c::onchildrenstopped called out of order");
        }
    };

    class root
        : public rootstoppable
    {
        std::thread a_;
        b b_;
        c c_;
        stoppable_test& test_;
    public:
        root(stoppable_test& test)
            : rootstoppable("r")
            , a_(&a::run, std::make_unique<a>(*this, test))
            , b_(*this, test)
            , c_(*this, test)
            , test_(test)
        {}

        void run()
        {
            prepare();
            start();
            stop();
        }

        void onprepare() override
        {
            test_.expect(++test_.count == 11, "root::onprepare called out of order");
        }

        void onstart() override
        {
            test_.expect(--test_.count == 10, "root::onstart called out of order");
        }

        void onstop() override
        {
            test_.expect(++test_.count == 1, "root::onstop called out of order");
        }

        void onchildrenstopped() override
        {
            a_.join();
            stoppable::stopped();
            test_.expect(--test_.count == 0, "root::onchildrenstopped called out of order");
        }
    };

public:
    void run()
    {
        {
            root rt(*this);
            rt.run();
        }
        pass();
    }
};

beast_define_testsuite_manual(stoppable,beast_core,beast);

}
