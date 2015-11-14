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

#ifndef ripple_wsdoor_h_included
#define ripple_wsdoor_h_included

#include <ripple/net/infosub.h>
#include <ripple/resource/manager.h>
#include <ripple/server/port.h>
#include <beast/threads/stoppable.h>

namespace ripple {

/** handles accepting incoming websocket connections. */
class wsdoor : public beast::stoppable
{
protected:
    explicit wsdoor (stoppable& parent);

public:
    virtual ~wsdoor() = default;

    // vfalco todo add this member function to prevent races on shutdown
    //virtual void close() = 0;
};

std::unique_ptr<wsdoor>
make_wsdoor (http::port const& port, resource::manager& resourcemanager,
    infosub::source& source);

}

#endif
