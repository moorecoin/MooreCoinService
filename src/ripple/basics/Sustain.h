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

#ifndef ripple_basics_sustain_h_included
#define ripple_basics_sustain_h_included

#include <string>

namespace ripple {

// "sustain" is a system for a buddy process that monitors the main process
// and relaunches it on a fault.
//
// vfalco todo rename this and put it in a class.
// vfalco todo reimplement cross-platform using beast::process and its ilk
//
extern bool havesustain ();
extern std::string stopsustain ();
extern std::string dosustain (std::string logfile);

} // ripple

#endif
