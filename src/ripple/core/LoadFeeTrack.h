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

#ifndef ripple_loadfeetrack_h_included
#define ripple_loadfeetrack_h_included

#include <ripple/json/json_value.h>
#include <beast/utility/journal.h>
#include <cstdint>

namespace ripple {

/** manages the current fee schedule.

    the "base" fee is the cost to send a reference transaction under no load,
    expressed in millionths of one xrp.

    the "load" fee is how much the local server currently charges to send a
    reference transaction. this fee fluctuates based on the load of the
    server.
*/
// vfalco todo rename "load" to "current".
class loadfeetrack
{
public:
    /** create a new tracker.
    */
    static loadfeetrack* new (beast::journal journal);

    virtual ~loadfeetrack () { }

    // scale from fee units to millionths of a ripple
    virtual std::uint64_t scalefeebase (std::uint64_t fee, std::uint64_t basefee,
                                        std::uint32_t referencefeeunits) = 0;

    // scale using load as well as base rate
    virtual std::uint64_t scalefeeload (std::uint64_t fee, std::uint64_t basefee,
                                        std::uint32_t referencefeeunits,
                                        bool badmin) = 0;

    virtual void setremotefee (std::uint32_t) = 0;

    virtual std::uint32_t getremotefee () = 0;
    virtual std::uint32_t getlocalfee () = 0;
    virtual std::uint32_t getclusterfee () = 0;

    virtual std::uint32_t getloadbase () = 0;
    virtual std::uint32_t getloadfactor () = 0;

    virtual json::value getjson (std::uint64_t basefee, std::uint32_t referencefeeunits) = 0;

    virtual void setclusterfee (std::uint32_t) = 0;
    virtual bool raiselocalfee () = 0;
    virtual bool lowerlocalfee () = 0;
    virtual bool isloadedlocal () = 0;
    virtual bool isloadedcluster () = 0;
};

} // ripple

#endif
