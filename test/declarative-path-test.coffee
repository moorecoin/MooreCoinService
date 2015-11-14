################################### requires ###################################

extend                     = require 'extend'
fs                         = require 'fs'
async                      = require 'async'
deep_eq                    = require 'deep-equal'

{amount
 remote
 seed
 base
 transaction
 pathfind
 sjcl
 uint160}                  = require 'ripple-lib'

testutils                  = require './testutils'
{server}                   = require './server'
{ledgerstate, testaccount} = require './ledger-state'
{test_accounts}            = require './random-test-addresses'

simple_assert              = require 'assert'

#################################### readme ####################################
"""
the tests are written in a declarative style:
  
  each case has an entry in the `path_finding_cases` object
  the key translates to a `suite(key, {...})`
  the `{...}` passed in is compiled into a setup/teardown for the `ledger` and
  into a bunch of `test` invokations for the `paths_expected`
    
    - aliases are used throughout for easier reading

      - test account addresses will be created `on the fly`
         
         no need to declare in testconfig.js
         debugged responses from the server substitute addresses for aliases

    - the fixtures are setup just once for each ledger, multiple path finding
      tests can be executed
     
    - `paths_expected` top level keys are group names
                       2nd level keys are path test declarations
                       
                       test declaration keys can be suffixed meaningfully with
                       
                          `_skip`
                          `_only`
                          
                        test declaration values can set
                        
                          debug: true
                          
                            will dump the path declaration and 
                            translated request and subsequent response

    - hops in `alternatives[*][paths][*]` can be written in shorthand
        eg.
          abc/g3|g3
            get `abc/g3` through `g3`

          abc/m1|m1
            get `abc/m1` through `m1`

          xrp|$
            get `xrp` through `$` 
                              $ signifies an order book rather than account
  
  ------------------------------------------------------------------------------
  tests can be written in the 'path-tests-json.js' file in same directory   # <--
  ------------------------------------------------------------------------------
"""
#################################### helpers ###################################

assert = simple_assert

refute = (cond, msg) -> assert(!cond, msg)
prettyj = pretty_json =  (v) -> json.stringify(v, undefined, 2)

propagater = (done) ->
  (f) ->
    ->
      return if done.aborted
      try
        f(arguments...)
      catch e
        done.aborted = true
        throw e

assert_match = (o, key_vals, message) ->
  """
  assert_match path[i], matcher,
               "alternative[#{ai}].paths[#{pi}]"
  """

  for k,v of key_vals
    assert.equal o[k], v, message

#################################### config ####################################

config = testutils.init_config()

############################### alternatives test ##############################

expand_alternative = (alt) ->
  """
  
  make explicit the currency and issuer in each hop in paths_computed
  
  """
  amt = amount.from_json(alt.source_amount)

  for path in alt.paths_computed
    prev_issuer = amt.issuer().to_json()
    prev_currency = amt.currency().to_json()

    for hop, hop_i in path
      if not hop.currency?
        hop.currency = prev_currency

      if not hop.issuer? and hop.currency != 'xrp'
        if hop.account?
          hop.issuer = hop.account
        else
          hop.issuer = prev_issuer

      if hop.type & 0x10
        prev_currency = hop.currency

      if hop.type & 0x20
        prev_issuer = hop.issuer
      else if hop.account?
        prev_issuer = hop.account

  return alt

create_shorthand = (alternatives) ->
  """
  
  convert explicit paths_computed into the format used by `paths_expected`
  these can be pasted in as the basis of tests.
  
  """
  shorthand = []

  for alt in alternatives
    short_alt = {}
    shorthand.push short_alt

    amt = amount.from_json alt.source_amount
    if amt.is_native()
      short_alt.amount = amt.to_human()
      if not (~short_alt.amount.search('.'))
        short_alt.amount = short_alt.amount + '.0'
    else
      short_alt.amount = amt.to_text_full()

    short_alt.paths    = []

    for path in alt.paths_computed
      short_path = []
      short_alt.paths.push short_path

      for node in path
        hop = node.currency
        hop = "#{hop}/#{node.issuer}" if node.issuer?
        hop = "#{hop}|#{if node.account? then node.account else "$"}"
        short_path.push hop

  return shorthand

ensure_list = (v) ->
  if array.isarray(v)
    v
  else
    [v]

test_alternatives_factory = (realias_pp, realias_text) ->
  """
  
  we are using a factory to create `test_alternatives` because it needs the 
  per ledger `realias_*` functions

  """
  hop_matcher = (decl_hop) ->
    [ci, f] = decl_hop.split('|')
    if not f?
        throw new error("no `|` in #{decl_hop}")

    [c,  i] = ci.split('/')
    is_account = if f == '$' then false else true
    matcher = currency: c
    matcher.issuer = i if i?
    matcher.account = f if is_account
    matcher

  match_path = (test, path, ai, pi) ->
    test = (hop_matcher(hop) for hop in test)
    assert.equal path.length, test.length,
                  "alternative[#{ai}] path[#{pi}] expecting #{test.length} hops"

    for matcher, i in test
      assert_match path[i], matcher,
                   "alternative[#{ai}].paths[#{pi}]"
    return

  simple_match_path = (test, path, ai, pi) ->
    """
    
      @test

        a shorthand specified path

      @path
        
        a path as returned by the server with `expand_alternative` done
        so issuer and currency are always stated.
    
    """
    test = (hop_matcher(hop) for hop in test)
    return false if not test.length == path.length

    for matcher, i in test
      for k, v of matcher
        return false if not path[i]?
        if path[i][k] != v
          return false
    true

  amounts = ->
    (amount.from_json a for a in arguments)

  amounts_text = ->
    (realias_text a.to_text_full() for a in arguments)

  check_for_no_redundant_paths = (alternatives) ->
    for alt, i in alternatives
      existing_paths = []
      for path in alt.paths_computed
        for existing in existing_paths
          assert !(deep_eq path, existing),
                 "duplicate path in alternatives[#{i}]\n"+
                 "#{realias_pp alternatives[0]}"

        existing_paths.push path
    return

  test_alternatives = (test, actual, error_context) ->
    """
    
      @test
        alternatives in shorthand format

      @actual
        alternatives as returned in a `path_find` response
        
      @error_context
      
        a function providing a string with extra context to provide to assertion
        messages

    """
    check_for_no_redundant_paths actual

    for t, ti in ensure_list(test)
      a = actual[ti]
      [t_amt, a_amt] = amounts(t.amount, a.source_amount)
      [t_amt_txt, a_amt_txt] = amounts_text(t_amt, a_amt)

      # console.log typeof t_amt

      assert t_amt.equals(a_amt),
             "expecting alternative[#{ti}].amount: "+
             "#{t_amt_txt} == #{a_amt_txt}"

      t_paths = ensure_list(t.paths)

      tn = t_paths.length
      an = a.paths_computed.length
      assert.equal tn, an, "different number of paths specified for alternative[#{ti}]"+
                            ", expected: #{prettyj t_paths}, "+
                            "actual(shorthand): #{prettyj create_shorthand actual}"+
                            "actual(verbose): #{prettyj a.paths_computed}"+
                            error_context()

      for p, i in t_paths
        matched = false

        for m in a.paths_computed
          if simple_match_path(p, m, ti, i)
            matched = true
            break

        assert matched, "can't find a match for path[#{i}]: #{prettyj p} "+
                        "amongst #{prettyj create_shorthand [a]}"+
                        error_context()
    return

################################################################################

create_path_test = (pth) ->
  return (done) ->
    self    = this
    what    = self.log_what
    ledger  = self.ledger
    test_alternatives = test_alternatives_factory ledger.pretty_json.bind(ledger),
                                                  ledger.realias_issuer


    what "#{pth.title}: #{pth.src} sending #{pth.dst}, "+
         "#{pth.send}, via #{pth.via}"

    one_message = (f) ->
      self.remote._servers[0].once 'before_send_message_for_non_mutators', f

    sent = "todo: need to patch ripple-lib"
    one_message (m) -> sent = m

    error_info = (m, more) ->
      info =
        path_expected:     pth,
        path_find_request: sent,
        path_find_updates: messages

      extend(info, more) if more?
      ledger.pretty_json(info)

    assert amount.from_json(pth.send).is_valid(),
           "#{pth.send} is not valid amount"

    _src = uint160.json_rewrite(pth.src)
    _dst = uint160.json_rewrite(pth.dst)
    _amt = amount.from_json(pth.send)

    # self.server.clear_logs() "todo: need to patch ripple-lib"
    pf = self.remote.path_find(_src, _dst, _amt, [{currency: pth.via}])

    updates  = 0
    max_seen = 0
    messages = {}

    propagates = propagater done

    pf.on "error", propagates (m) ->                                       # <--
      assert false, "fail (error): #{error_info(m)}"
      done()

    pf.on "update", propagates (m) ->                                      # <--
      # todo:hack:
      expand_alternative alt for alt in m.alternatives


      messages[if updates then "update-#{updates}" else 'initial-response'] = m
      updates++
      # console.log updates

      assert m.alternatives.length >= max_seen,
             "subsequent path_find update' should never have less " +
             "alternatives:\n#{ledger.pretty_json messages}"

      max_seen = m.alternatives.length

      # if updates == 2
      #   testutils.ledger_close(self.remote, -> )

      if updates == 2
        # "todo: need to patch ripple-lib"
        # self.log_pre(self.server.get_logs(), "server logs")

        if pth.do_send?
          do_send( (ledger.pretty_json.bind ledger), what, self.remote, pth,
                   messages['update-2'], done )

        if pth.debug
          console.log ledger.pretty_json(messages)
          console.log error_info(m)
          console.log ledger.pretty_json create_shorthand(m.alternatives)

        if pth.alternatives?
          # we realias before doing any comparisons
          alts = ledger.realias_issuer(json.stringify(m.alternatives))
          alts = json.parse(alts)
          test = pth.alternatives

          assert test.length == alts.length,
                "number of `alternatives` specified is different: "+
                "#{error_info(m)}"

          if test.length == alts.length
            test_alternatives(pth.alternatives, alts, -> error_info(m))

        if pth.n_alternatives?
          assert pth.n_alternatives ==  m.alternatives.length,
                 "fail (wrong n_alternatives): #{error_info(m)}"

        done() if not pth.do_send?

################################ suite creation ################################

skip_or_only = (title, test_or_suite) ->
  endswith = (s, suffix) ->
    ~s.indexof(suffix, s.length - suffix.length)

  if endswith title, '_only'
    test_or_suite.only
  else if endswith title, '_skip'
    test_or_suite.skip
  else
    test_or_suite

definer_factory = (group, title, path) ->
  path.title = "#{[group, title].join('.')}"
  test_func = skip_or_only path.title, test
  ->
    test_func(path.title, create_path_test(path) )

gather_path_definers = (path_expected) ->
  tests = []
  for group, subgroup of path_expected
    for title, path of subgroup
      tests.push definer_factory(group, title, path)
  tests

suite_factory = (declaration) ->
  ->
    context = null

    suitesetup (done) ->
      context = @
      @log_what = ->

      testutils.build_setup().call @, ->
        context.ledger = new ledgerstate(declaration.ledger,
                                         assert,
                                         context.remote,
                                         config)

        context.ledger.setup(context.log_what, done)

    suiteteardown (done) ->
      testutils.build_teardown().call context, done

    for definer in gather_path_definers(declaration.paths_expected)
      definer()

define_suites = (path_finding_cases) ->
  for case_name, declaration of path_finding_cases
    suite_func = skip_or_only case_name, suite
    suite_func case_name, suite_factory(declaration)

############################## path finding cases ##############################
# later we reference a0, the `unknown account`, directly embedding the full
# address.
a0 = (new testaccount('a0')).address
assert a0 == 'rbmhuvavi372aerwzwergjhljqkmmawxx'

try
  path_finding_cases = require('./path-tests-json')
catch e
  console.log e

# you need two gateways, same currency. a market maker. a source that trusts one
# gateway and holds its currency, and a destination that trusts the other.

extend path_finding_cases,
  "cny test":
    paths_expected:
      bs:
        p101: src: "src", dst: "gateway_dst", send: "10.1/cny/gateway_dst", via: "xrp", n_alternatives: 1

    ledger:
      accounts:
        src:
          balance: ["4999.999898"]
          trusts: []
          offers: []

        gateway_dst:
          balance: ["10846.168060"]
          trusts: []
          offers: []

        money_maker_1:
          balance: ["4291.430036"]
          trusts: []
          offers: []

        money_maker_2:
          balance: [
            "106839375770"
            "0.0000000003599/cny/money_maker_1"
            "137.6852546843001/cny/gateway_dst"
          ]
          trusts: [
            "1001/cny/money_maker_1"
            "1001/cny/gateway_dst"
          ]
          offers: [
            [
              "1000000"
              "1/cny/gateway_dst"
              # []
            ]
            [
              "1/cny/gateway_dst"
              "1000000"
              # []
            ]
            [
              "318000/cny/gateway_dst"
              "53000000000"
              # ["sell"]
            ]
            [
              "209000000"
              "4.18/cny/money_maker_2"
              # []
            ]
            [
              "990000/cny/money_maker_1"
              "10000000000"
              # ["sell"]
            ]
            [
              "9990000/cny/money_maker_1"
              "10000000000"
              # ["sell"]
            ]
            [
              "8870000/cny/gateway_dst"
              "10000000000"
              # ["sell"]
            ]
            [
              "232000000"
              "5.568/cny/money_maker_2"
              # []
            ]
          ]

        a1:
          balance: [
            # "240.997150"
            "1240.997150"
            "0.0000000119761/cny/money_maker_1"
            "33.047994/cny/gateway_dst"
          ]
          trusts: [
            "1000000/cny/money_maker_1"
            "100000/usd/money_maker_1"
            "10000/btc/money_maker_1"
            "1000/usd/gateway_dst"
            "1000/cny/gateway_dst"
          ]
          offers: []

        a2:
          balance: [
            "14115.046893"
            "209.3081873019994/cny/money_maker_1"
            "694.6251706504019/cny/gateway_dst"
          ]
          trusts: [
            "3000/cny/money_maker_1"
            "3000/cny/gateway_dst"
          ]
          offers: [
            [
              "2000000000"
              "66.8/cny/money_maker_1"
              # []
            ]
            [
              "1200000000"
              "42/cny/gateway_dst"
              # []
            ]
            [
              "43.2/cny/money_maker_1"
              "900000000"
              # ["sell"]
            ]
          ]

        a3:
          balance: [
            "512087.883181"
            "23.617050013581/cny/money_maker_1"
            "70.999614649799/cny/gateway_dst"
          ]
          trusts: [
            "10000/cny/money_maker_1"
            "10000/cny/gateway_dst"
          ]
          offers: [[
            "2240/cny/money_maker_1"
            "50000000000"
            # ["sell"]
          ]]


  "path tests (bitstamp + snapswap account holders | liquidity provider with no offers)":
    ledger:
      accounts:
        g1bs:
          balance: ["1000.0"]
        g2sw:
          balance: ["1000.0"]
        a1:
          balance: ["1000.0", "1000/hkd/g1bs"]
          trusts: ["2000/hkd/g1bs"]
        a2:
          balance: ["1000.0", "1000/hkd/g2sw"]
          trusts: ["2000/hkd/g2sw"]
        m1:
          # snapswap wants to be able to set trust line quality settings so they
          # can charge a fee when transactions ripple across. liquitidy
          # provider, via trusting/holding both accounts
          balance: ["11000.0",
                   "1200/hkd/g1bs",
                   "5000/hkd/g2sw"
          ]
          trusts: ["100000/hkd/g1bs", "100000/hkd/g2sw"]
          # we haven't got any offers

    paths_expected: {
      bs:
        p1:
          debug: false
          src: "a1", dst: "a2", send: "10/hkd/a2", via: "hkd"
          n_alternatives: 1
        p2:
          debug: false
          src: "a2", dst: "a1", send: "10/hkd/a1", via: "hkd"
          n_alternatives: 1
        p3:
          debug: false
          src: "g1bs", dst: "a2", send: "10/hkd/a2", via: "hkd"
          alternatives: [
            amount: "10/hkd/g1bs",
            paths: [["hkd/m1|m1", "hkd/g2sw|g2sw"]]
          ]
        p5:
          debug: false
          src: "m1",
          send: "10/hkd/m1",
          dst: "g1bs",
          via: "hkd"
        p4:
          debug: false
          src: "g2sw", send: "10/hkd/a1", dst: "a1", via: "hkd"
          alternatives: [
            amount: "10/hkd/g2sw",
            paths: [["hkd/m1|m1", "hkd/g1bs|g1bs"]]
          ]
    }
  "path tests #4 (non-xrp to non-xrp, same currency)": {
    ledger:
      accounts:
        g1: balance: ["1000.0"]
        g2: balance: ["1000.0"]
        g3: balance: ["1000.0"]
        g4: balance: ["1000.0"]
        a1:
          balance: ["1000.0", "1000/hkd/g1"]
          trusts:  ["2000/hkd/g1"]
        a2:
          balance: ["1000.0", "1000/hkd/g2"]
          trusts:  ["2000/hkd/g2"]
        a3:
          balance: ["1000.0", "1000/hkd/g1"]
          trusts:  ["2000/hkd/g1"]
        a4:
          balance: ["10000.0"]
        m1:
          balance: ["11000.0", "1200/hkd/g1", "5000/hkd/g2"]
          trusts:  ["100000/hkd/g1", "100000/hkd/g2"]
          offers:  [
            ["1000/hkd/g1", "1000/hkd/g2"]
          ]
        m2:
          balance: ["11000.0", "1200/hkd/g1", "5000/hkd/g2"]
          trusts:  ["100000/hkd/g1", "100000/hkd/g2"]
          offers:  [
            ["10000.0", "1000/hkd/g2"]
            ["1000/hkd/g1", "10000.0"]
          ]

    paths_expected: {
      t4:
        "a) borrow or repay":
          comment: 'source -> destination (repay source issuer)'
          src: "a1", send: "10/hkd/g1", dst: "g1", via: "hkd"
          alternatives: [amount: "10/hkd/a1", paths: []]

        "a2) borrow or repay":
          comment: 'source -> destination (repay destination issuer)'
          src: "a1", send: "10/hkd/a1", dst: "g1", via: "hkd"
          alternatives: [amount: "10/hkd/a1", paths: []]

        "b) common gateway":
          comment: 'source -> ac -> destination'
          src: "a1", send: "10/hkd/a3", dst: "a3", via: "hkd"
          alternatives: [amount: "10/hkd/a1", paths: [["hkd/g1|g1"]]]

        "c) gateway to gateway":
          comment: 'source -> ob -> destination'
          src: "g1", send: "10/hkd/g2", dst: "g2", via: "hkd"
          debug: false
          alternatives: [
            amount: "10/hkd/g1"
            paths: [["hkd/m2|m2"],
                    ["hkd/m1|m1"],
                    ["hkd/g2|$"]
                    ["xrp|$", "hkd/g2|$"]
                  ]
          ]

        "d) user to unlinked gateway via order book":
          comment: 'source -> ac -> ob -> destination'
          src: "a1", send: "10/hkd/g2", dst: "g2", via: "hkd"
          debug: false
          alternatives: [
            amount: "10/hkd/a1"
            paths: [
              ["hkd/g1|g1", "hkd/g2|$"],                                   # <--
              ["hkd/g1|g1", "hkd/m2|m2"],
              ["hkd/g1|g1", "hkd/m1|m1"],
              ["hkd/g1|g1", "xrp|$", "hkd/g2|$"]
            ]
          ]

        "i4) xrp bridge":
          comment: 'source -> ac -> ob to xrp -> ob from xrp -> ac -> destination'
          src: "a1", send: "10/hkd/a2", dst: "a2", via: "hkd"
          debug: false
          alternatives: [
            amount: "10/hkd/a1",
            paths: [
              # focus
              ["hkd/g1|g1", "hkd/g2|$", "hkd/g2|g2"            ],
              ["hkd/g1|g1", "xrp|$",    "hkd/g2|$", "hkd/g2|g2"],          # <--
              # incidental
              ["hkd/g1|g1", "hkd/m1|m1", "hkd/g2|g2"],
              ["hkd/g1|g1", "hkd/m2|m2", "hkd/g2|g2"]
            ]
          ]

    }
  },
  "path tests #2 (non-xrp to non-xrp, same currency)": {
    ledger:
      accounts:
        g1: balance: ["1000.0"]
        g2: balance: ["1000.0"]
        a1:
          balance: ["1000.0", "1000/hkd/g1"]
          trusts:  ["2000/hkd/g1"]
        a2:
          balance: ["1000.0", "1000/hkd/g2"]
          trusts:  ["2000/hkd/g2"]
        a3:
          balance: ["1000.0"]
          trusts:  ["2000/hkd/a2"]
        m1:
          balance: ["11000.0", "5000/hkd/g1", "5000/hkd/g2"]
          trusts:  ["100000/hkd/g1", "100000/hkd/g2"]
          offers:  [
            ["1000/hkd/g1",    "1000/hkd/g2"]
            # ["2000/hkd/g2",  "2000/hkd/g1"]
            # ["2000/hkd/m1",  "2000/hkd/g1"]
            # ["100.0",        "1000/hkd/g1"]
            # ["1000/hkd/g1",        "100.0"]
          ]

    paths_expected: {
      t4:
        "e) gateway to user":
          ledger: false
          comment: 'source -> ob -> ac -> destination'
          # comment: 'gateway -> ob -> gateway 2 -> user'
          src: "g1", send: "10/hkd/a2", dst: "a2", via: "hkd"
          debug: false
          alternatives: [
            amount: "10/hkd/g1"
            paths: [
              ["hkd/g2|$", "hkd/g2|g2"],
              ["hkd/m1|m1", "hkd/g2|g2"]
            ]
          ]

        "f) different gateways, ripple  _skip":
          comment: 'source -> ac -> ac -> destination'

        "g) different users of different gateways, ripple  _skip":
          comment: 'source -> ac -> ac -> ac -> destination'

        "h) different gateways, order book  _skip":
          comment: 'source -> ac -> ob -> ac -> destination'

        "i1) xrp bridge  _skip":
          comment: 'source -> ob to xrp -> ob from xrp -> destination'
          src: "a4", send: "10/hkd/g2", dst: "g2", via: "xrp"
          debug: true

        "i2) xrp bridge  _skip":
          comment: 'source -> ac -> ob to xrp -> ob from xrp -> destination'

        "i3) xrp bridge  _skip":
          comment: 'source -> ob to xrp -> ob from xrp -> ac -> destination'
    }
  }

################################# define suites ################################

define_suites(path_finding_cases)