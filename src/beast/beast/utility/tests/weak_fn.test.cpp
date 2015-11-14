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

#include <beast/weak_fn.h>
#include <beast/unit_test/suite.h>

namespace beast {

class weak_fn_test : public beast::unit_test::suite
{
public:
    struct t
    {
        bool& called_;

        explicit
        t (bool& called)
            : called_(called)
        {
        }

        void
        fv()
        {
            called_ = true;
        }

        void
        fi(int i)
        {
            called_ = true;
        }

        void
        fis(int, std::string)
        {
            called_ = true;
        }

        int
        fri()
        {
            called_ = true;
            return 2;
        }
    };

    void
    run()
    {
        {
            bool called = false;
            auto const p = std::make_shared<t>(called);
            std::bind(weak_fn(&t::fv, p))();
            expect(called);
        }

        {
            bool called = false;
            auto p = std::make_shared<t>(called);
            auto call = std::bind(weak_fn(&t::fv, p));
            p.reset();
            call();
            expect(! called);
        }

        {
            bool called = false;
            auto p = std::make_shared<t>(called);
            std::bind(weak_fn(&t::fi, p), 1)();
            expect(called);
        }
    
        {
            bool called = false;
            auto p = std::make_shared<t>(called);
            std::bind(weak_fn(&t::fi, p),
                std::placeholders::_1)(1);
            expect(called);
        }

        {
            bool called = false;
            auto p = std::make_shared<t>(called);
            std::bind(weak_fn(&t::fis, p),
                1, std::placeholders::_1)("foo");
            expect(called);
        }

        {
            bool called = false;
            auto p = std::make_shared<t>(called);
            try
            {
                auto call = std::bind(weak_fn(&t::fis, p, throw_if_invalid<>()),
                    1, std::placeholders::_1);
                p.reset();
                call("foo");
                fail();
            }
            catch(std::bad_weak_ptr const&)
            {
                expect(! called);
            }
        }

        {
            bool called = false;
            auto p = std::make_shared<t>(called);
            expect(std::bind(weak_fn(&t::fri, p))() == 2);
            expect(called);
        }

        {
            bool called = false;
            auto p = std::make_shared<t>(called);
            auto call = std::bind(weak_fn(&t::fv, p,
                [&called]()
                {
                    called = true;
                }));
            p.reset();
            call();
            expect(called);
        }
    }
};

beast_define_testsuite(weak_fn,asio,beast);

}
