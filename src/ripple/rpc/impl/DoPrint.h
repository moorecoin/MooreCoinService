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

#ifndef ripple_rpc_print_h_included
#define ripple_rpc_print_h_included

#include <ripple/app/main/application.h>
#include <ripple/json/jsonpropertystream.h>

namespace ripple {
namespace rpc {

class doprint
{
public:
    void operator() (request& req)
    {
        jsonpropertystream stream;

        if (req.params.isobject() &&
            req.params["params"].isarray() &&
            req.params["params"][0u].isstring ())
        {
            req.app.write (stream, req.params["params"][0u].asstring());
        }
        else
        {
            req.app.write (stream);
        }

        req.result = stream.top();
    }
};

}
}

#endif
