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

#ifndef ripple_protocol_jsonfields_h_included
#define ripple_protocol_jsonfields_h_included

#include <ripple/json/json_value.h>

namespace ripple {
namespace jss {

// json static strings

#define jss(x) const json::staticstring x ( #x )

/* the "staticstring" field names are used instead of string literals to
   optimize the performance of accessing members of json::value objects.
*/
// vfalco note some of these are part of the json-rpc api and some aren't
//        todo move the string not part of the json-rpc api into another file
jss ( accepted );
jss ( account );
jss ( account_hash );
jss ( account_index );
jss ( accounts );
jss ( accountstate );
jss ( accounttreehash );
jss ( affected );
jss ( age );
jss ( amendment_blocked );
jss ( amount );
jss ( asks );
jss ( authorized );
jss ( balance );
jss ( base_fee );
jss ( base_fee_xrp );
jss ( bids );
jss ( binary );
jss ( build_version );
jss ( can_delete );
jss ( closed );
jss ( closed_ledger );
jss ( close_time );
jss ( close_time_estimated );
jss ( close_time_human );
jss ( close_time_offset );
jss ( close_time_resolution );
jss ( code );
jss ( command );
jss ( complete_ledgers );
jss ( consensus );
jss ( converge_time );
jss ( converge_time_s );
jss ( currency );
jss ( data );
jss ( date );
jss ( dividend_ledger );
jss ( delivered_amount );
jss ( engine_result );
jss ( engine_result_code );
jss ( engine_result_message );
jss ( error );
jss ( error_code );
jss ( error_exception );
jss ( error_message );
jss ( expand );
jss ( fee_base );
jss ( fee_ref );
jss ( fetch_pack );
jss ( flags );
jss ( freeze );
jss ( freeze_peer );
jss ( full );
jss ( hash );
jss ( hostid );
jss ( id );
jss ( issuer );
jss ( last_close );
jss ( ledger );
jss ( ledgerclosed );
jss ( ledger_current_index );
jss ( ledger_hash );
jss ( ledger_index );
jss ( ledger_index_max );
jss ( ledger_index_min );
jss ( ledger_time );
jss ( dividend_object );
jss ( limit );
jss ( limit_peer );
jss ( lines );
jss ( load );
jss ( load_base );
jss ( load_factor );
jss ( load_factor_cluster );
jss ( load_factor_local );
jss ( load_factor_net );
jss ( load_fee );
jss ( marker );
jss ( message );
jss ( meta );
jss ( metadata );
jss ( method );
jss ( missingcommand );
jss ( name );
jss ( network_ledger );
jss ( none );
jss ( no_ripple );
jss ( no_ripple_peer );
jss ( offers );
jss ( open );
jss ( owner_funds );
jss ( params );
jss ( parent_hash );
jss ( peer );
jss ( peer_authorized );
jss ( peer_index );
jss ( peers );
jss ( proposed );
jss ( proposers );
jss ( pubkey_node );
jss ( pubkey_validator );
jss ( published_ledger );
jss ( quality );
jss ( quality_in );
jss ( quality_out );
jss ( random );
jss ( raw_meta );
jss ( referee );
jss ( request );
jss ( reserve );
jss ( reserve_base );
jss ( reserve_base_xrp );
jss ( reserve_inc );
jss ( reserve_inc_xrp );
jss ( response );
jss ( result );
jss ( ripple_lines );
jss ( seq );
jss ( seqnum );
jss ( server_state );
jss ( server_status );
jss ( stand_alone );
jss ( states );
jss ( status );
jss ( success );
jss ( system_time_offset );
jss ( taker_gets );
jss ( taker_gets_funded );
jss ( taker_pays );
jss ( taker_pays_funded );
jss ( total_coins );
jss ( totalcoins );
jss ( total_coinsvbc );
jss ( totalcoinsvbc );
jss ( transaction );
jss ( transaction_hash );
jss ( transactions );
jss ( transtreehash );
jss ( tx );
jss ( tx_blob );
jss ( tx_json );
jss ( txn_count );
jss ( type );
jss ( type_hex );
jss ( validated );
jss ( validated_ledger );
jss ( validated_ledgers );
jss ( validation_quorum );
jss ( value );
jss ( waiting );
jss ( warning );

#undef jss

} // jss
} // ripple

#endif
