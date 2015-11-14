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

#ifndef ndebug
# define consistency_check(cond) bassert(cond)
#else
# define consistency_check(cond)
#endif

#include <ripple/peerfinder/impl/bootcache.cpp>
#include <ripple/peerfinder/impl/peerfinderconfig.cpp>
#include <ripple/peerfinder/impl/endpoint.cpp>
#include <ripple/peerfinder/impl/manager.cpp>
#include <ripple/peerfinder/impl/slotimp.cpp>
#include <ripple/peerfinder/impl/sourcestrings.cpp>

#include <ripple/peerfinder/sim/graphalgorithms.h>
#include <ripple/peerfinder/sim/wrappedsink.h>
#include <ripple/peerfinder/sim/predicates.h>
#include <ripple/peerfinder/sim/functionqueue.h>
#include <ripple/peerfinder/sim/message.h>
#include <ripple/peerfinder/sim/nodesnapshot.h>
#include <ripple/peerfinder/sim/params.h>
#include <ripple/peerfinder/sim/tests.cpp>

#include <ripple/peerfinder/tests/livecache.test.cpp>

#if doxygen
#include <ripple/peerfinder/readme.md>
#endif
