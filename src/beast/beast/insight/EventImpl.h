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

#ifndef beast_insight_eventimpl_h_included
#define beast_insight_eventimpl_h_included

#include <beast/insight/baseimpl.h>

namespace beast {
namespace insight {

class event;

class eventimpl
    : public std::enable_shared_from_this <eventimpl>
    , public baseimpl
{
public:
    typedef std::chrono::milliseconds value_type;

    virtual ~eventimpl () = 0;
    virtual void notify (value_type const& value) = 0;
};

}
}

#endif
