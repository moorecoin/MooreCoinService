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
#include <ripple/resource/charge.h>

namespace ripple {
namespace resource {

charge const feeinvalidrequest    (  10, "malformed request"      );
charge const feerequestnoreply    (   1, "unsatisfiable request"  );
charge const feeinvalidsignature  ( 100, "invalid signature"      );
charge const feeunwanteddata      (   5, "useless data"           );
charge const feebaddata           (  20, "invalid data"           );

charge const feeinvalidrpc        (  10, "malformed rpc"          );
charge const feereferencerpc      (   2, "reference rpc"          );
charge const feeexceptionrpc      (  10, "exceptioned rpc"        );
charge const feelightrpc          (   5, "light rpc"              ); // david: check the cost
charge const feelowburdenrpc      (  20, "low rpc"                );
charge const feemediumburdenrpc   (  40, "medium rpc"             );
charge const feehighburdenrpc     ( 300, "heavy rpc"              );
charge const feepathfindupdate    ( 100, "path update"            );

charge const feenewtrustednote    (  10, "trusted note"           );
charge const feenewvalidtx        (  10, "valid tx"               );
charge const feesatisfiedrequest  (  10, "needed data"            );

charge const feerequesteddata     (  50, "costly request"         );
charge const feecheapquery        (  10, "trivial query"          );

charge const feewarning           ( 200, "received warning"       );
charge const feedrop              ( 300, "dropped"                );

}
}
