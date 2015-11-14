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

#ifndef ripple_basics_uptimetimer_h_included
#define ripple_basics_uptimetimer_h_included

#include <ctime>

namespace ripple {

/** tracks program uptime.

    the timer can be switched to a manual system of updating, to reduce
    system calls. (?)
*/
// vfalco todo determine if the non-manual timing is actually needed
class uptimetimer
{
private:
    uptimetimer ();
    ~uptimetimer ();

public:
    int getelapsedseconds () const;

    void beginmanualupdates ();
    void endmanualupdates ();

    void incrementelapsedtime ();

    static uptimetimer& getinstance ();

private:
    // vfalco deprecated, use a memory barrier instead of forcing a cache line
    int volatile m_elapsedtime;

    time_t m_starttime;

    bool m_isupdatingmanually;
};

} // ripple

#endif
