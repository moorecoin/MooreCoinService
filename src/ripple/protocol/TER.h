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

#ifndef ripple_protocol_ter_h
#define ripple_protocol_ter_h

#include <string>

namespace ripple {

// see https://ripple.com/wiki/transaction_errors

// vfalco todo consider renaming ter to txerr or txresult for clarity.
//
enum ter    // aka transactionengineresult
{
    // note: range is stable.  exact numbers are currently unstable.  use tokens.

    // -399 .. -300: l local error (transaction fee inadequate, exceeds local limit)
    // only valid during non-consensus processing.
    // implications:
    // - not forwarded
    // - no fee check
    tellocal_error  = -399,
    telbad_domain,
    telbad_path_count,
    telbad_public_key,
    telfailed_processing,
    telinsuf_fee_p,
    telno_dst_partial,

    // -299 .. -200: m malformed (bad signature)
    // causes:
    // - transaction corrupt.
    // implications:
    // - not applied
    // - not forwarded
    // - reject
    // - can not succeed in any imagined ledger.
    temmalformed    = -299,

    tembad_amount,
    tembad_auth_master,
    tembad_currency,
    tembad_expiration,
    tembad_fee,
    tembad_issuer,
    tembad_limit,
    tembad_offer,
    tembad_path,
    tembad_path_loop,
    tembad_send_xrp_limit,
    tembad_send_xrp_max,
    tembad_send_xrp_no_direct,
    tembad_send_xrp_partial,
    tembad_send_xrp_paths,
    tembad_sequence,
    tembad_signature,
    tembad_src_account,
    tembad_transfer_rate,
    temdst_is_src,
    temdst_needed,
    teminvalid,
    teminvalid_flag,
    temredundant,
    temredundant_send_max,
    temripple_empty,
    temdisabled,
    tembad_div_type,
    tembad_release_schedule,

    // an intermediate result used internally, should never be returned.
    temuncertain,
    temunknown,

    // -199 .. -100: f
    //    failure (sequence number previously used)
    //
    // causes:
    // - transaction cannot succeed because of ledger state.
    // - unexpected ledger state.
    // - c++ exception.
    //
    // implications:
    // - not applied
    // - not forwarded
    // - could succeed in an imagined ledger.
    teffailure      = -199,
    tefalready,
    tefbad_add_auth,
    tefbad_auth,
    tefbad_ledger,
    tefcreated,
    tefdst_tag_needed,
    tefexception,
    tefinternal,
    tefno_auth_required,    // can't set auth if auth is not required.
    tefpast_seq,
    tefwrong_prior,
    tefmaster_disabled,
    tefmax_ledger,
    tefreferee_exist,
    tefreference_exist,

    // -99 .. -1: r retry
    //   sequence too high, no funds for txn fee, originating -account
    //   non-existent
    //
    // cause:
    //   prior application of another, possibly non-existent, transaction could
    //   allow this transaction to succeed.
    //
    // implications:
    // - not applied
    // - not forwarded
    // - might succeed later
    // - hold
    // - makes hole in sequence which jams transactions.
    terretry        = -99,
    terfunds_spent,      // this is a free transaction, so don't burden network.
    terinsuf_fee_b,      // can't pay fee, therefore don't burden network.
    terno_account,       // can't pay fee, therefore don't burden network.
    terno_auth,          // not authorized to hold ious.
    terno_line,          // internal flag.
    terowners,           // can't succeed with non-zero owner count.
    terpre_seq,          // can't pay fee, no point in forwarding, so don't
                         // burden network.
    terlast,             // process after all other transactions
    terno_ripple,        // rippling not allowed

    // 0: s success (success)
    // causes:
    // - success.
    // implications:
    // - applied
    // - forwarded
    tessuccess      = 0,

    // 100 .. 159 c
    //   claim fee only (ripple transaction with no good paths, pay to
    //   non-existent account, no path)
    //
    // causes:
    // - success, but does not achieve optimal result.
    // - invalid transaction or no effect, but claim fee to use the sequence
    //   number.
    //
    // implications:
    // - applied
    // - forwarded
    //
    // only allowed as a return code of appliedtransaction when !tapretry.
    // otherwise, treated as terretry.
    //
    // do not change these numbers: they appear in ledger meta data.
    tecclaim                    = 100,
    tecpath_partial             = 101,
    tecunfunded_add             = 102,
    tecunfunded_offer           = 103,
    tecunfunded_payment         = 104,
    tecfailed_processing        = 105,
    tecdir_full                 = 121,
    tecinsuf_reserve_line       = 122,
    tecinsuf_reserve_offer      = 123,
    tecno_dst                   = 124,
    tecno_dst_insuf_xrp         = 125,
    tecno_line_insuf_reserve    = 126,
    tecno_line_redundant        = 127,
    tecpath_dry                 = 128,
    tecunfunded                 = 129,  // deprecated, old ambiguous unfunded.
    tecmaster_disabled          = 130,
    tecno_regular_key           = 131,
    tecowners                   = 132,
    tecno_issuer                = 133,
    tecno_auth                  = 134,
    tecno_line                  = 135,
    tecinsuff_fee               = 136,
    tecfrozen                   = 137,
    tecno_target                = 138,
    tecno_permission            = 139,
    tecno_entry                 = 140,
    tecinsufficient_reserve     = 141,
};

inline bool istellocal(ter x)
{
    return ((x) >= tellocal_error && (x) < temmalformed);
}

inline bool istemmalformed(ter x)
{
    return ((x) >= temmalformed && (x) < teffailure);
}

inline bool isteffailure(ter x)
{
    return ((x) >= teffailure && (x) < terretry);
}

inline bool isterretry(ter x)
{
    return ((x) >= terretry && (x) < tessuccess);
}

inline bool istessuccess(ter x)
{
    return ((x) == tessuccess);
}

inline bool istecclaim(ter x)
{
    return ((x) >= tecclaim);
}

// vfalco todo group these into a shell class along with the defines above.
extern
bool
transresultinfo (ter code, std::string& token, std::string& text);

extern
std::string
transtoken (ter code);

extern
std::string
transhuman (ter code);

} // ripple

#endif
