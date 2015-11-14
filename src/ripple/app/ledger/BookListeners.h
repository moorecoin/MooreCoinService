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

#ifndef ripple_booklisteners_h
#define ripple_booklisteners_h

#include <ripple/net/infosub.h>
#include <memory>

namespace ripple {

/** listen to public/subscribe messages from a book. */
class booklisteners
{
public:
    typedef std::shared_ptr<booklisteners> pointer;

    booklisteners () {}

    void addsubscriber (infosub::ref sub);
    void removesubscriber (std::uint64_t sub);
    void publish (json::value const& jvobj);

private:
    typedef ripplerecursivemutex locktype;
    typedef std::lock_guard <locktype> scopedlocktype;
    locktype mlock;

    hash_map<std::uint64_t, infosub::wptr> mlisteners;
};

} // ripple

#endif
