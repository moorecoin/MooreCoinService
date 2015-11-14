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

#include <ripple/protocol/impl/buildinfo.cpp>
#include <ripple/protocol/impl/byteorder.cpp>
#include <ripple/protocol/impl/errorcodes.cpp>
#include <ripple/protocol/impl/hashprefix.cpp>
#include <ripple/protocol/impl/indexes.cpp>
#include <ripple/protocol/impl/ledgerformats.cpp>
#include <ripple/protocol/impl/rippleaddress.cpp>
#include <ripple/protocol/impl/serializer.cpp>
#include <ripple/protocol/impl/sfield.cpp>
#include <ripple/protocol/impl/sotemplate.cpp>
#include <ripple/protocol/impl/ter.cpp>
#include <ripple/protocol/impl/txformats.cpp>
#include <ripple/protocol/impl/uinttypes.cpp>

#include <ripple/protocol/impl/staccount.cpp>
#include <ripple/protocol/impl/starray.cpp>
#include <ripple/protocol/impl/stamount.cpp>
#include <ripple/protocol/impl/stbase.cpp>
#include <ripple/protocol/impl/stblob.cpp>
#include <ripple/protocol/impl/stinteger.cpp>
#include <ripple/protocol/impl/stledgerentry.cpp>
#include <ripple/protocol/impl/stobject.cpp>
#include <ripple/protocol/impl/stparsedjson.cpp>
#include <ripple/protocol/impl/stpathset.cpp>
#include <ripple/protocol/impl/sttx.cpp>
#include <ripple/protocol/impl/stvalidation.cpp>
#include <ripple/protocol/impl/stvector256.cpp>

#include <ripple/protocol/tests/buildinfo.test.cpp>
#include <ripple/protocol/tests/issue.test.cpp>
#include <ripple/protocol/tests/rippleaddress.test.cpp>
#include <ripple/protocol/tests/serializer.test.cpp>
#include <ripple/protocol/tests/stamount.test.cpp>
#include <ripple/protocol/tests/stobject.test.cpp>
#include <ripple/protocol/tests/sttx.test.cpp>

#if doxygen
#include <ripple/protocol/readme.md>
#endif
