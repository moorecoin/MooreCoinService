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

#ifndef ripple_app_rpc_internal_handler
#define ripple_app_rpc_internal_handler

namespace ripple {
namespace rpc {

/** to dynamically add custom or experimental rpc handlers, construct a new
 * instance of internalhandler with your own handler function. */
struct internalhandler
{
    typedef json::value (*handler_t) (const json::value&);

    internalhandler (const std::string& name, handler_t handler)
            : name_ (name),
              handler_ (handler)
    {
        nexthandler_ = internalhandler::headhandler;
        internalhandler::headhandler = this;
    }

    internalhandler* nexthandler_;
    std::string name_;
    handler_t handler_;

    static internalhandler* headhandler;
};

} // rpc
} // ripple

#endif
