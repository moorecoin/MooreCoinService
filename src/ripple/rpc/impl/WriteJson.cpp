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
#include <ripple/rpc/impl/writejson.h>
#include <ripple/rpc/impl/jsonwriter.h>

namespace ripple {
namespace rpc {

namespace {

void writejson (json::value const& value, writer& writer)
{
    switch (value.type())
    {
    case json::nullvalue:
    {
        writer.output (nullptr);
        break;
    }

    case json::intvalue:
    {
        writer.output (value.asint());
        break;
    }

    case json::uintvalue:
    {
        writer.output (value.asuint());
        break;
    }

    case json::realvalue:
    {
        writer.output (value.asdouble());
        break;
    }

    case json::stringvalue:
    {
        writer.output (value.asstring());
        break;
    }

    case json::booleanvalue:
    {
        writer.output (value.asbool());
        break;
    }

    case json::arrayvalue:
    {
        writer.startroot (writer::array);
        for (auto const& i: value)
        {
            writer.rawappend();
            writejson (i, writer);
        }
        writer.finish();
        break;
    }

    case json::objectvalue:
    {
        writer.startroot (writer::object);
        auto members = value.getmembernames ();
        for (auto const& tag: members)
        {
            writer.rawset (tag);
            writejson (value[tag], writer);
        }
        writer.finish();
        break;
    }
    } // switch
}

} // namespace

void writejson (json::value const& value, output const& out)
{
    writer writer (out);
    writejson (value, writer);
}

std::string jsonasstring (json::value const& value)
{
    std::string s;
    writer writer (stringoutput (s));
    writejson (value, writer);
    return s;
}

} // rpc
} // ripple
