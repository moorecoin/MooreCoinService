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

#ifndef ripple_rpc_testoutputsuite_h_included
#define ripple_rpc_testoutputsuite_h_included

#include <ripple/rpc/output.h>
#include <ripple/rpc/impl/jsonwriter.h>
#include <ripple/basics/testsuite.h>

namespace ripple {
namespace rpc {

class testoutputsuite : public testsuite
{
protected:
    std::string output_;
    std::unique_ptr <writer> writer_;

    void setup (std::string const& testname)
    {
        testcase (testname);
        output_.clear ();
        writer_ = std::make_unique <writer> (stringoutput (output_));
    }

    // test the result and report values.
    void expectresult (std::string const& expected,
                       std::string const& message = "")
    {
        writer_.reset ();

        expectequals (output_, expected, message);
    }
};

} // rpc
} // ripple

#endif
