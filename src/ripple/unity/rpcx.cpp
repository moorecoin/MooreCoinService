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

// this has to be included early to prevent an obscure msvc compile error
#include <boost/asio/deadline_timer.hpp>

#include <ripple/protocol/jsonfields.h>

#include <ripple/rpc/rpchandler.h>

#include <ripple/rpc/impl/coroutine.cpp>
#include <ripple/rpc/impl/jsonobject.cpp>
#include <ripple/rpc/impl/jsonwriter.cpp>
#include <ripple/rpc/impl/writejson.cpp>
#include <ripple/rpc/impl/manager.cpp>
#include <ripple/rpc/impl/rpchandler.cpp>
#include <ripple/rpc/impl/status.cpp>
#include <ripple/rpc/impl/yield.cpp>
#include <ripple/rpc/impl/utilities.cpp>

#include <ripple/rpc/handlers/handlers.h>
#include <ripple/rpc/impl/lookupledger.h>
#include <ripple/rpc/handlers/accountasset.cpp>
#include <ripple/rpc/handlers/accountcurrencies.cpp>
#include <ripple/rpc/handlers/accountdividend.cpp>
#include <ripple/rpc/handlers/accountinfo.cpp>
#include <ripple/rpc/handlers/accountlines.cpp>
#include <ripple/rpc/handlers/accountoffers.cpp>
#include <ripple/rpc/handlers/accounttx.cpp>
#include <ripple/rpc/handlers/accounttxold.cpp>
#include <ripple/rpc/handlers/accounttxswitch.cpp>
#include <ripple/rpc/handlers/ancestors.cpp>
#include <ripple/rpc/handlers/blacklist.cpp>
#include <ripple/rpc/handlers/bookoffers.cpp>
#include <ripple/rpc/handlers/candelete.cpp>
#include <ripple/rpc/handlers/connect.cpp>
#include <ripple/rpc/handlers/consensusinfo.cpp>
#include <ripple/rpc/handlers/dividendobject.cpp>
#include <ripple/rpc/handlers/feature.cpp>
#include <ripple/rpc/handlers/fetchinfo.cpp>
#include <ripple/rpc/handlers/getcounts.cpp>
#include <ripple/rpc/handlers/internal.cpp>
#include <ripple/rpc/handlers/ledger.cpp>
#include <ripple/rpc/handlers/ledgeraccept.cpp>
#include <ripple/rpc/handlers/ledgercleaner.cpp>
#include <ripple/rpc/handlers/ledgerclosed.cpp>
#include <ripple/rpc/handlers/ledgercurrent.cpp>
#include <ripple/rpc/handlers/ledgerdata.cpp>
#include <ripple/rpc/handlers/ledgerentry.cpp>
#include <ripple/rpc/handlers/ledgerheader.cpp>
#include <ripple/rpc/handlers/ledgerrequest.cpp>
#include <ripple/rpc/handlers/loglevel.cpp>
#include <ripple/rpc/handlers/logrotate.cpp>
#include <ripple/rpc/handlers/ownerinfo.cpp>
#include <ripple/rpc/handlers/pathfind.cpp>
#include <ripple/rpc/handlers/peers.cpp>
#include <ripple/rpc/handlers/ping.cpp>
#include <ripple/rpc/handlers/print.cpp>
#include <ripple/rpc/handlers/random.cpp>
#include <ripple/rpc/handlers/ripplepathfind.cpp>
#include <ripple/rpc/handlers/sms.cpp>
#include <ripple/rpc/handlers/serverinfo.cpp>
#include <ripple/rpc/handlers/serverstate.cpp>
#include <ripple/rpc/handlers/sign.cpp>
#include <ripple/rpc/handlers/stop.cpp>
#include <ripple/rpc/handlers/submit.cpp>
#include <ripple/rpc/handlers/subscribe.cpp>
#include <ripple/rpc/handlers/transactionentry.cpp>
#include <ripple/rpc/handlers/tx.cpp>
#include <ripple/rpc/handlers/txhistory.cpp>
#include <ripple/rpc/handlers/unladd.cpp>
#include <ripple/rpc/handlers/unldelete.cpp>
#include <ripple/rpc/handlers/unllist.cpp>
#include <ripple/rpc/handlers/unlload.cpp>
#include <ripple/rpc/handlers/unlnetwork.cpp>
#include <ripple/rpc/handlers/unlreset.cpp>
#include <ripple/rpc/handlers/unlscore.cpp>
#include <ripple/rpc/handlers/unsubscribe.cpp>
#include <ripple/rpc/handlers/validationcreate.cpp>
#include <ripple/rpc/handlers/validationseed.cpp>
#include <ripple/rpc/handlers/walletaccounts.cpp>
#include <ripple/rpc/handlers/walletpropose.cpp>
#include <ripple/rpc/handlers/walletseed.cpp>

#include <ripple/rpc/impl/accountfromstring.cpp>
#include <ripple/rpc/impl/accounts.cpp>
#include <ripple/rpc/impl/getmastergenerator.cpp>
#include <ripple/rpc/impl/handler.cpp>
#include <ripple/rpc/impl/legacypathfind.cpp>
#include <ripple/rpc/impl/lookupledger.cpp>
#include <ripple/rpc/impl/parseaccountids.cpp>
#include <ripple/rpc/impl/transactionsign.cpp>

#include <ripple/rpc/tests/coroutine.test.cpp>
#include <ripple/rpc/tests/jsonobject.test.cpp>
#include <ripple/rpc/tests/jsonrpc.test.cpp>
#include <ripple/rpc/tests/jsonwriter.test.cpp>
#include <ripple/rpc/tests/status.test.cpp>
#include <ripple/rpc/tests/writejson.test.cpp>
#include <ripple/rpc/tests/yield.test.cpp>
