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

#ifndef ripple_handler_handlers
#define ripple_handler_handlers

namespace ripple {

json::value doaccountasset          (rpc::context&);
json::value doaccountcurrencies     (rpc::context&);
json::value doaccountinfo           (rpc::context&);
json::value doaccountlines          (rpc::context&);
json::value doaccountoffers         (rpc::context&);
json::value doaccounttx             (rpc::context&);
json::value doaccounttxold          (rpc::context&);
json::value doaccounttxswitch       (rpc::context&);
json::value doblacklist             (rpc::context&);
json::value dobookoffers            (rpc::context&);
json::value docandelete             (rpc::context&);
json::value doconnect               (rpc::context&);
json::value doconsensusinfo         (rpc::context&);
json::value dodividendobject        (rpc::context&);
json::value dofeature               (rpc::context&);
json::value dofetchinfo             (rpc::context&);
json::value dogetcounts             (rpc::context&);
json::value dointernal              (rpc::context&);
json::value doledgeraccept          (rpc::context&);
json::value doledgercleaner         (rpc::context&);
json::value doledgerclosed          (rpc::context&);
json::value doledgercurrent         (rpc::context&);
json::value doledgerdata            (rpc::context&);
json::value doledgerentry           (rpc::context&);
json::value doledgerheader          (rpc::context&);
json::value doledgerrequest         (rpc::context&);
json::value dologlevel              (rpc::context&);
json::value dologrotate             (rpc::context&);
json::value doownerinfo             (rpc::context&);
json::value dopathfind              (rpc::context&);
json::value dopeers                 (rpc::context&);
json::value doping                  (rpc::context&);
json::value doprint                 (rpc::context&);
json::value dorandom                (rpc::context&);
json::value doripplepathfind        (rpc::context&);
json::value dosms                   (rpc::context&);
json::value doserverinfo            (rpc::context&); // for humans
json::value doserverstate           (rpc::context&); // for machines
json::value dosessionclose          (rpc::context&);
json::value dosessionopen           (rpc::context&);
json::value dosign                  (rpc::context&);
json::value dostop                  (rpc::context&);
json::value dosubmit                (rpc::context&);
json::value dosubscribe             (rpc::context&);
json::value dotransactionentry      (rpc::context&);
json::value dotx                    (rpc::context&);
json::value dotxhistory             (rpc::context&);
json::value dounladd                (rpc::context&);
json::value dounldelete             (rpc::context&);
json::value dounlfetch              (rpc::context&);
json::value dounllist               (rpc::context&);
json::value dounlload               (rpc::context&);
json::value dounlnetwork            (rpc::context&);
json::value dounlreset              (rpc::context&);
json::value dounlscore              (rpc::context&);
json::value dounsubscribe           (rpc::context&);
json::value dovalidationcreate      (rpc::context&);
json::value dovalidationseed        (rpc::context&);
json::value dowalletaccounts        (rpc::context&);
json::value dowalletlock            (rpc::context&);
json::value dowalletpropose         (rpc::context&);
json::value dowalletseed            (rpc::context&);
json::value dowalletunlock          (rpc::context&);
json::value dowalletverify          (rpc::context&);

} // ripple

#endif
