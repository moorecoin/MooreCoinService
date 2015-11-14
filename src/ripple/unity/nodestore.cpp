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

#include <beast/nudb/nudb.cpp>

#include <ripple/nodestore/backend/hyperdbfactory.cpp>
#include <ripple/nodestore/backend/leveldbfactory.cpp>
#include <ripple/nodestore/backend/memoryfactory.cpp>
#include <ripple/nodestore/backend/nudbfactory.cpp>
#include <ripple/nodestore/backend/nullfactory.cpp>
#include <ripple/nodestore/backend/rocksdbfactory.cpp>
#include <ripple/nodestore/backend/rocksdbquickfactory.cpp>

#include <ripple/nodestore/impl/batchwriter.cpp>
#include <ripple/nodestore/impl/databaseimp.h>
#include <ripple/nodestore/impl/databaserotatingimp.cpp>
#include <ripple/nodestore/impl/dummyscheduler.cpp>
#include <ripple/nodestore/impl/decodedblob.cpp>
#include <ripple/nodestore/impl/encodedblob.cpp>
#include <ripple/nodestore/impl/managerimp.cpp>
#include <ripple/nodestore/impl/nodeobject.cpp>

#include <ripple/nodestore/tests/backend.test.cpp>
#include <ripple/nodestore/tests/basics.test.cpp>
#include <ripple/nodestore/tests/database.test.cpp>
#include <ripple/nodestore/tests/import_test.cpp>
#include <ripple/nodestore/tests/timing.test.cpp>

