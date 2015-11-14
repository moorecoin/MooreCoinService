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

#ifndef ripple_net_basics_sntpclient_h_included
#define ripple_net_basics_sntpclient_h_included

#include <beast/threads/stoppable.h>

namespace ripple {

class sntpclient : public beast::stoppable
{
protected:
    explicit sntpclient (beast::stoppable& parent);

public:
    static sntpclient* new (beast::stoppable& parent);
    virtual ~sntpclient() { }
    virtual void init (std::vector <std::string> const& servers) = 0;
    virtual void addserver (std::string const& mserver) = 0;
    virtual void queryall () = 0;
    virtual bool getoffset (int& offset) = 0;
};

} // ripple

#endif
