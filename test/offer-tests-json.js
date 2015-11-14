module.exports = {
  "partially crossed completely via bridging": {

    "pre_ledger": {"accounts": {"g1": {"balance": ["1000.0"]},
                                "g2": {"balance": ["1000.0"]},
                                "alice": {"balance": ["500000.0", "200/usd/g1"],
                                          "offers": [["100/usd/g1", "88.0"]]},
                                "bob": {"balance": ["500000.0", "500/usd/g2"],
                                        "offers": [["88.0", "100/usd/g2"]]},
                                "takerjoe": {"balance": ["500000.0", "500/usd/g1"]}}},

    "offer": ["takerjoe", "500/usd/g2", "500/usd/g1"],

    "post_ledger": {"accounts": {"takerjoe": {"balance": ["400/usd/g1", "100/usd/g2"],
                                              "offers": [["400/usd/g2", "400/usd/g1"]]}}}
  },

  "partially crossed (halt)": {
    "pre_ledger": {"accounts": {"g1": {"balance": ["1000.0"]},
                                "g2": {"balance": ["1000.0"]},
                                "alice": {"balance": ["500000.0", "200/usd/g1"],
                                          "offers": [["100/usd/g1", "88.0"], ["100/usd/g1", "88.0"]]},
                                                //----/ (2)            (3)      (5) there's no offers left. halt
                                "bob": {"balance": ["500000.0", "500/usd/g2"],
                                        "offers": [["88.0", "100/usd/g2"]]},
                                                //                  (4)
                                "takerjoe": {"balance": ["500000.0", "500/usd/g1"]}}},
                                           // (1)
    "offer": ["takerjoe", "500/usd/g2", "500/usd/g1"],

                                                // 500,000-88   200+100/usd/g1
    "post_ledger": {"accounts": {"alice": {"balance": ["499912.0", "300/usd/g1"],
                                           "offers": [["100/usd/g1", "88.0"]]},

                                  "bob": {"balance": ["500088.0", "400/usd/g2"],
                                          "offers": [/*["88.0", "100/usd/g2"]*/]},

                                 "takerjoe": {"balance": ["100/usd/g2", "400/usd/g1"],
                                              "offers": [["400/usd/g2", "400/usd/g1"]]}}}
  },

  "partially crossed completely via bridging (sell)": {

  "pre_ledger": {"accounts": {"g1": {"balance": ["1000.0"]},
                             "g2": {"balance": ["1000.0"]},
                             "alice": {"balance": ["500000.0", "200/usd/g1"],
                                       "offers": [["200/usd/g1", "176.0", "sell"]]},
                             "bob": {"balance": ["500000.0", "500/usd/g2"],
                                     "offers": [["88.0", "100/usd/g2"]]},
                             "takerjoe": {"balance": ["500000.0", "500/usd/g1"]}}},

  "offer": ["takerjoe", "500/usd/g2", "500/usd/g1", "sell"],

  "post_ledger": {"accounts": {"alice": {"balance": ["499912.0", "299.9999999999999/usd/g1"],
                                        "offers": [["100.0000000000001/usd/g1", "88.0"]]},
                              "takerjoe": {"balance": ["100/usd/g2", "400.0000000000001/usd/g1"],
                                           "offers": [["400.0000000000001/usd/g2", "400.0000000000001/usd/g1"]]}}}
  },

  "completely crossed via bridging + direct": {

    "pre_ledger": {"accounts": {"g1": {"balance": ["1000.0"]},
                                "g2": {"balance": ["1000.0"]},
                                "alice": {"balance": ["500000.0", "500/usd/g1", "500/usd/g2"],
                                          "offers": [["50/usd/g1", "50/usd/g2"],
                                                     ["49/usd/g1", "50/usd/g2"],
                                                     ["48/usd/g1", "50/usd/g2"],
                                                     ["47/usd/g1", "50/usd/g2"],
                                                     ["46/usd/g1", "50/usd/g2"],
                                                     ["45/usd/g1", "50/usd/g2"],
                                                     ["44/usd/g1", "50/usd/g2"],
                                                     ["43/usd/g1", "50/usd/g2"],
                                                     ["100/usd/g1", "88.0"]]},
                                "bob": {"balance": ["500000.0", "500/usd/g2"],
                                        "offers": [["88.0", "100/usd/g2"]]},
                                "takerjoe": {"balance": ["500000.0", "600/usd/g1"]}}},

    "offer": ["takerjoe", "500/usd/g2", "500/usd/g1"],

    "post_ledger": {"accounts": {"takerjoe": {"balance": ["500/usd/g2", "128/usd/g1"]}}}
  },

  "partially crossed via bridging + direct": {
    "pre_ledger": {"accounts": {"g1": {"balance": ["1000.0"]},
                                "g2": {"balance": ["1000.0"]},
                                "alice": {"balance": ["500000.0", "500/usd/g1", "500/usd/g2"],
                                          "offers": [["372/usd/g1", "400/usd/g2"],
                                                     ["100/usd/g1", "88.0"]]},
                                "bob": {"balance": ["500000.0", "500/usd/g2"],
                                        "offers": [["88.0", "100/usd/g2"]]},
                                "takerjoe": {"balance": ["500000.0", "600/usd/g1"]}}},

    "offer": ["takerjoe", "600/usd/g2", "600/usd/g1"],

    "post_ledger": {"accounts": {"takerjoe": {"balance": ["500/usd/g2", "128/usd/g1"],
                                              "offers": [["100/usd/g2", "100/usd/g1"]]}}}
  },

  "partially crossed via bridging + direct": {
    "pre_ledger": {"accounts": {"g1": {"balance": ["1000.0"]},
                                "g2": {"balance": ["1000.0"]},
                                "alice": {"balance": ["500000.0", "500/usd/g1", "500/usd/g2"],
                                          "offers": [["372/usd/g1", "400/usd/g2"],
                                                     ["100/usd/g1", "88.0"]]},
                                "bob": {"balance": ["500000.0", "500/usd/g2"],
                                        "offers": [["88.0", "100/usd/g2"]]},
                                "takerjoe": {"balance": ["500000.0", "600/usd/g1"]}}},

    "offer": ["takerjoe", "600/usd/g2", "600/usd/g1", "sell"],

    "post_ledger": {"accounts": {"takerjoe": {"balance": ["500/usd/g2", "128/usd/g1"],
                                              "offers": [["128/usd/g2", "128/usd/g1"]]}}}
  }
}