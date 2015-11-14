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
#include <ripple/protocol/serializer.h>
#include <beast/unit_test/suite.h>

namespace ripple {

class serializer_test : public beast::unit_test::suite
{
public:
    void run ()
    {
        serializer s1;
        s1.add32 (3);
        s1.add256 (uint256 ());

        serializer s2;
        s2.add32 (0x12345600);
        s2.addraw (s1.peekdata ());

        expect (s1.getprefixhash (0x12345600) == s2.getsha512half ());
    }
};

beast_define_testsuite(serializer,ripple_data,ripple);

} // ripple
