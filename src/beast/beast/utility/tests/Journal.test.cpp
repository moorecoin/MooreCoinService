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

#include <beast/utility/journal.h>
#include <beast/unit_test/suite.h>

namespace beast {

class journal_test : public unit_test::suite
{
public:
    class testsink : public journal::sink
    {
    private:
        int m_count;

    public:
        testsink()
            : m_count(0)
        {
        }

        int
        count() const
        {
            return m_count;
        }

        void
        reset()
        {
            m_count = 0;
        }

        void
        write (journal::severity, std::string const&)
        {
            ++m_count;
        }
    };

    void run ()
    {
        testsink sink;

        sink.severity(journal::kinfo);

        journal j(sink);
        
        j.trace << " ";
        expect(sink.count() == 0);
        j.debug << " ";
        expect(sink.count() == 0);
        j.info << " ";
        expect(sink.count() == 1);
        j.warning << " ";
        expect(sink.count() == 2);
        j.error << " ";
        expect(sink.count() == 3);
        j.fatal << " ";
        expect(sink.count() == 4);

        sink.reset();

        sink.severity(journal::kdebug);

        j.trace << " ";
        expect(sink.count() == 0);
        j.debug << " ";
        expect(sink.count() == 1);
        j.info << " ";
        expect(sink.count() == 2);
        j.warning << " ";
        expect(sink.count() == 3);
        j.error << " ";
        expect(sink.count() == 4);
        j.fatal << " ";
        expect(sink.count() == 5);
    }
};

beast_define_testsuite_manual(journal,utility,beast);

} // beast
