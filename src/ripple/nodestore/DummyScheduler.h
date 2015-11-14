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

#ifndef ripple_nodestore_dummyscheduler_h_included
#define ripple_nodestore_dummyscheduler_h_included

#include <ripple/nodestore/scheduler.h>

namespace ripple {
namespace nodestore {

/** simple nodestore scheduler that just peforms the tasks synchronously. */
class dummyscheduler : public scheduler
{
public:
    dummyscheduler ();
    ~dummyscheduler ();
    void scheduletask (task& task);
    void scheduledtasksstopped ();
    void onfetch (fetchreport const& report) override;
    void onbatchwrite (batchwritereport const& report) override;
};

}
}

#endif
