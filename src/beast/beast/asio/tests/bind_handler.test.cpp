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

// libs: boost_system

#if beast_include_beastconfig
#include <beastconfig.h>
#endif

#include <beast/unit_test/suite.h>

#include <beast/asio/bind_handler.h>

#include <functional>

namespace beast {
namespace asio {

class bind_handler_test : public unit_test::suite
{
public:
    static void foo (int)
    {
    }

    void run()
    {
        auto f (bind_handler (
            std::bind (&foo, std::placeholders::_1),
            42));
        f();
        pass();
    }
};

beast_define_testsuite(bind_handler,asio,beast);

}
}
