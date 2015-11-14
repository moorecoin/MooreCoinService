//------------------------------------------------------------------------------
/*
    this file is part of rippled: https://github.com/ripple/rippled
    copyright 2014 ripple labs inc.

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
#include <ripple/overlay/impl/tmhello.h>
#include <beast/unit_test/suite.h>

namespace ripple {

class tmhello_test : public beast::unit_test::suite
{
private:
    template <class fwdit>
    static
    std::string
    join (fwdit first, fwdit last, char c = ',')
    {
        std::string result;
        if (first == last)
            return result;
        result = to_string(*first++);
        while(first != last)
            result += "," + to_string(*first++);
        return result;
    }

    void
    check(std::string const& s, std::string const& answer)
    {
        auto const result = parse_protocolversions(s);
        expect(join(result.begin(), result.end()) == answer);
    }

public:
    void
    test_protocolversions()
    {
        check("", "");
        check("rtxp/1.0", "1.0");
        check("rtxp/1.0, websocket/1.0", "1.0");
        check("rtxp/1.0, rtxp/1.0", "1.0");
        check("rtxp/1.0, rtxp/1.1", "1.0,1.1");
        check("rtxp/1.1, rtxp/1.0", "1.0,1.1");
    }

    void
    run()
    {
        test_protocolversions();
    }
};

beast_define_testsuite(tmhello,overlay,ripple);

}
