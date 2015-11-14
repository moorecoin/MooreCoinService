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

#if beast_include_beastconfig
#include "../../beastconfig.h"
#endif

#include <beast/streams/basic_abstract_ostream.h>

#include <beast/unit_test/suite.h>

namespace beast {

class basic_abstract_ostream_test : public unit_test::suite
{
public:
    class test_stream : public basic_abstract_ostream <char>
    {
    public:
        test_stream&
        operator= (test_stream const&) = delete;

        explicit
        test_stream (unit_test::suite& suite_)
            : m_suite (suite_)
        {
        }

        void write (string_type const& s) override
        {
            m_suite.log << s;
        }

    private:
        unit_test::suite& m_suite;
    };

    void run()
    {
        test_stream ts (*this);

        ts << "hello";

        pass();
    }
};

beast_define_testsuite(basic_abstract_ostream,streams,beast);

}
