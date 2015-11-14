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
#include <ripple/protocol/ter.h>

namespace ripple {

bool transresultinfo (ter code, std::string& token, std::string& text)
{
    struct txresultinfo
    {
        ter code;
        char const* token;
        char const* text;
    };

    // fixme: replace this with a function-static std::map and the lookup
    // code with std::map::find when the problem with magic statics on
    // visual studio is fixed.
    static
    txresultinfo const results[] =
    {
        { tecclaim,                 "tecclaim",                 "fee claimed. sequence used. no action."                        },
        { tecdir_full,              "tecdir_full",              "can not add entry to full directory."                          },
        { tecfailed_processing,     "tecfailed_processing",     "failed to correctly process transaction."                      },
        { tecinsuf_reserve_line,    "tecinsuf_reserve_line",    "insufficient reserve to add trust line."                       },
        { tecinsuf_reserve_offer,   "tecinsuf_reserve_offer",   "insufficient reserve to create offer."                         },
        { tecno_dst,                "tecno_dst",                "destination does not exist. send xrp to create it."            },
        { tecno_dst_insuf_xrp,      "tecno_dst_insuf_xrp",      "destination does not exist. too little xrp sent to create it." },
        { tecno_line_insuf_reserve, "tecno_line_insuf_reserve", "no such line. too little reserve to create it."                },
        { tecno_line_redundant,     "tecno_line_redundant",     "can't set non-existent line to default."                       },
        { tecpath_dry,              "tecpath_dry",              "path could not send partial amount."                           },
        { tecpath_partial,          "tecpath_partial",          "path could not send full amount."                              },
        { tecmaster_disabled,       "tecmaster_disabled",       "master key is disabled."                                       },
        { tecno_regular_key,        "tecno_regular_key",        "regular key is not set."                                       },

        { tecunfunded,              "tecunfunded",              "one of _add, _offer, or _send. deprecated."                    },
        { tecunfunded_add,          "tecunfunded_add",          "insufficient vrp balance for walletadd."                       },
        { tecunfunded_offer,        "tecunfunded_offer",        "insufficient balance to fund created offer."                   },
        { tecunfunded_payment,      "tecunfunded_payment",      "insufficient vrp balance to send."                             },
        { tecowners,                "tecowners",                "non-zero owner count."                                         },
        { tecno_issuer,             "tecno_issuer",             "issuer account does not exist."                                },
        { tecno_auth,               "tecno_auth",               "not authorized to hold asset."                                 },
        { tecno_line,               "tecno_line",               "no such line."                                                 },
        { tecinsuff_fee,            "tecinsuff_fee",            "insufficient balance to pay fee."                              },
        { tecfrozen,                "tecfrozen",                "asset is frozen."                                              },
        { tecno_target,             "tecno_target",             "target account does not exist."                                },
        { tecno_permission,         "tecno_permission",         "no permission to perform requested operation."                 },
        { tecno_entry,              "tecno_entry",              "no matching entry found."                                      },
        { tecinsufficient_reserve,  "tecinsufficient_reserve",  "insufficient reserve to complete requested operation."         },

        { tefalready,               "tefalready",               "the exact transaction was already in this ledger."             },
        { tefbad_add_auth,          "tefbad_add_auth",          "not authorized to add account."                                },
        { tefbad_auth,              "tefbad_auth",              "transaction's public key is not authorized."                   },
        { tefbad_ledger,            "tefbad_ledger",            "ledger in unexpected state."                                   },
        { tefcreated,               "tefcreated",               "can't add an already created account or asset."                         },
        { tefdst_tag_needed,        "tefdst_tag_needed",        "destination tag required."                                     },
        { tefexception,             "tefexception",             "unexpected program state."                                     },
        { teffailure,               "teffailure",               "failed to apply."                                              },
        { tefinternal,              "tefinternal",              "internal error."                                               },
        { tefmaster_disabled,       "tefmaster_disabled",       "master key is disabled."                                       },
        { tefmax_ledger,            "tefmax_ledger",            "ledger sequence too high."                                     },
        { tefno_auth_required,      "tefno_auth_required",      "auth is not required."                                         },
        { tefpast_seq,              "tefpast_seq",              "this sequence number has already past."                        },
        { tefreferee_exist,         "tefreferee_exist",         "this account has already had a referee."                       },
        { tefreference_exist,       "tefreference_exist",       "this account has already had a reference."                     },
        { tefwrong_prior,           "tefwrong_prior",           "this previous transaction does not match."                     },

        { tellocal_error,           "tellocal_error",           "local failure."                                                },
        { telbad_domain,            "telbad_domain",            "domain too long."                                              },
        { telbad_path_count,        "telbad_path_count",        "malformed: too many paths."                                    },
        { telbad_public_key,        "telbad_public_key",        "public key too long."                                          },
        { telfailed_processing,     "telfailed_processing",     "failed to correctly process transaction."                      },
        { telinsuf_fee_p,           "telinsuf_fee_p",           "fee insufficient."                                             },
        { telno_dst_partial,        "telno_dst_partial",        "partial payment to create account not allowed."                },

        { temmalformed,             "temmalformed",             "malformed transaction."                                        },
        { tembad_amount,            "tembad_amount",            "can only send positive amounts."                               },
        { tembad_auth_master,       "tembad_auth_master",       "auth for unclaimed account needs correct master key."          },
        { tembad_currency,          "tembad_currency",          "malformed: bad currency."                                      },
        { tembad_expiration,        "tembad_expiration",        "malformed: bad expiration."                                    },
        { tembad_fee,               "tembad_fee",               "invalid fee, negative or not vrp."                             },
        { tembad_issuer,            "tembad_issuer",            "malformed: bad issuer."                                        },
        { tembad_limit,             "tembad_limit",             "limits must be non-negative."                                  },
        { tembad_offer,             "tembad_offer",             "malformed: bad offer."                                         },
        { tembad_path,              "tembad_path",              "malformed: bad path."                                          },
        { tembad_path_loop,         "tembad_path_loop",         "malformed: loop in path."                                      },
        { tembad_send_xrp_limit,    "tembad_send_vrp_limit",    "malformed: limit quality is not allowed for vrp to vrp."       },
        { tembad_send_xrp_max,      "tembad_send_vrp_max",      "malformed: send max is not allowed for vrp to vrp or asset."            },
        { tembad_send_xrp_no_direct,"tembad_send_vrp_no_direct","malformed: no moorecoin direct is not allowed for vrp to vrp."     },
        { tembad_send_xrp_partial,  "tembad_send_vrp_partial",  "malformed: partial payment is not allowed for vrp to vrp or asset."     },
        { tembad_send_xrp_paths,    "tembad_send_vrp_paths",    "malformed: paths are not allowed for vrp to vrp."              },
        { tembad_sequence,          "tembad_sequence",          "malformed: sequence is not in the past."                       },
        { tembad_signature,         "tembad_signature",         "malformed: bad signature."                                     },
        { tembad_src_account,       "tembad_src_account",       "malformed: bad source account."                                },
        { tembad_transfer_rate,     "tembad_transfer_rate",     "malformed: transfer rate must be >= 1.0"                       },
        { temdst_is_src,            "temdst_is_src",            "destination may not be source."                                },
        { temdst_needed,            "temdst_needed",            "destination not specified."                                    },
        { teminvalid,               "teminvalid",               "the transaction is ill-formed."                                },
        { teminvalid_flag,          "teminvalid_flag",          "the transaction has an invalid flag."                          },
        { temredundant,             "temredundant",             "sends same currency to self."                                  },
        { temredundant_send_max,    "temredundant_send_max",    "send max is redundant."                                        },
        { temripple_empty,          "temripple_empty",          "pathset with no paths."                                        },
        { temuncertain,             "temuncertain",             "in process of determining result. never returned."             },
        { temunknown,               "temunknown",               "the transaction requires logic that is not implemented yet."   },
        { temdisabled,              "temdisabled",              "the transaction requires logic that is currently disabled."    },
        { tembad_div_type,          "tembad_div_type",          "bad dividend type"                                             },
        { tembad_release_schedule,  "tembad_release_schedule",  "malformed: bad releaseschedule"
            },

        { terretry,                 "terretry",                 "retry transaction."                                            },
        { terfunds_spent,           "terfunds_spent",           "can't set password, password set funds already spent."         },
        { terinsuf_fee_b,           "terinsuf_fee_b",           "account balance can't pay fee."                                },
        { terlast,                  "terlast",                  "process last."                                                 },
        { terno_ripple,             "terno_ripple",             "path does not permit rippling."                                },
        { terno_account,            "terno_account",            "the source account does not exist."                            },
        { terno_auth,               "terno_auth",               "not authorized to hold ious."                                  },
        { terno_line,               "terno_line",               "no such line."                                                 },
        { terpre_seq,               "terpre_seq",               "missing/inapplicable prior transaction."                       },
        { terowners,                "terowners",                "non-zero owner count."                                         },

        { tessuccess,               "tessuccess",               "the transaction was applied. only final in a validated ledger." },
    };

    for (auto const& result : results)
    {
        if (result.code == code)
        {
            token = result.token;
            text = result.text;
            return true;
        }
    }

    return false;
}

std::string transtoken (ter code)
{
    std::string token;
    std::string text;

    return transresultinfo (code, token, text) ? token : "-";
}

std::string transhuman (ter code)
{
    std::string token;
    std::string text;

    return transresultinfo (code, token, text) ? text : "-";
}

} // ripple
