################################### requires ###################################

extend                     = require 'extend'
fs                         = require 'fs'
assert                     = require 'assert'
async                      = require 'async'

{
  remote
  uint160
  transaction
  amount
}                          = require 'ripple-lib'

testutils                  = require './testutils'

{
  ledgerstate
  ledgerverifier
  testaccount
}                          = require './ledger-state'

{
  pretty_json
  server_setup_teardown
  skip_or_only
  submit_for_final
  suite_test_bailer
}                          = require './batmans-belt'


################################ freeze overview ###############################
'''

freeze feature overview
=======================

a frozen line prevents funds from being transferred to anyone except back to the
issuer, yet does not prohibit acquisition of more of the issuer's assets, via
receipt of a payment or placing offers to buy them.

a trust line's flags now assigns two bits, for toggling the freeze status of
each side of a trustline.

globalfreeze
------------

there is also, a global freeze, toggled by a bit in the accountroot flags, which
freezes all trust lines for an account. 

offers can not be created to buy or sell assets issued by an account with
globalfreeze set.

use cases (via (david (joelkatz) schwartz)):

  there are two basic cases. one is a case where some kind of bug or flaw causes
  a large amount of an asset to somehow be created and the gateway hasn't yet
  decided how it's going to handle it.
  
  the other is a reissue where one asset is replaced by another. in a community
  credit case, say someone tricks you into issuing a huge amount of an asset,
  but you set the no freeze flag. you can still set global freeze to protect
  others from trading valuable assets for assets you issued that are now,
  unfortunately, worthless. and if you're an honest guy, you can set up a new
  account and re-issue to people who are legitimate holders

nofreeze
--------

nofreeze, is a set only flag bit in the account root.

when this bit is set:
  an account may not freeze it's side of a trustline

  the nofreeze bit can not be cleared

  the globalfreeze flag bit can not cleared
    globalfreeze can be used as a matter of last resort

flag definitions
================

ledgerentry flags
-----------------

ripplestate
  
  lowfreeze     0x00400000
  highfreeze    0x00800000

accountroot

  nofreeze      0x00200000
  globalfeeze   0x00400000

transaction flags
-----------------

trustset (used with flags)

  setfreeze     0x00100000
  clearfreeze   0x00200000

accountset (used with setflag/clearflag)

    nofreeze              6
    globalfreeze          7

api implications
================

transaction.payment
-------------------

any offers containing frozen funds found in the process of a tessuccess will be
removed from the books.

transaction.offercreate
-----------------------

selling an asset from a globally frozen issuer fails with tecfrozen
selling an asset from a frozen line fails with tecunfunded_offer

any offers containing frozen funds found in the process of a tessuccess will be
removed from the books.

request.book_offers
-------------------

all offers selling assets from a frozen line/acount (offers created before a
freeze) will be filtered, except where in a global freeze situation where: 

  takergets.issuer == account ($frozen_account)

request.path_find & transaction.payment
---------------------------------------

no path may contain frozen trustlines, or offers (placed, prior to freezing) of
assets from frozen lines.

request.account_offers
-----------------------

these offers are unfiltered, merely walking the owner directory and reporting
all offers.

'''

################################################################################

flags =
  sle:
    accountroot:
      passwordspent:   0x00010000
      requiredesttag:  0x00020000
      requireauth:     0x00040000
      disallowxrp:     0x00080000
      nofreeze:        0x00200000
      globalfreeze:    0x00400000

    ripplestate:
      lowfreeze:      0x00400000
      highfreeze:     0x00800000
  tx:
    setflag:
      accountroot:
        nofreeze:     6
        globalfreeze: 7

    trustset:
      # new flags
      setfreeze:      0x00100000
      clearfreeze:    0x00200000


transaction.flags.trustset ||= {};
# monkey patch setfreeze and clearfreeze into old version of ripple-lib
transaction.flags.trustset.setfreeze = flags.tx.trustset.setfreeze
transaction.flags.trustset.clearfreeze = flags.tx.trustset.clearfreeze

globalfreeze = flags.tx.setflag.accountroot.globalfreeze
nofreeze     = flags.tx.setflag.accountroot.nofreeze

#################################### config ####################################

config = testutils.init_config()

#################################### helpers ###################################

get_lines = (remote, acc, done) ->
  remote.request_account_lines acc, null, 'validated', (err, lines) ->
    done(lines)

account_set_factory = (remote, ledger, alias_for) ->
  (acc, fields, done) ->
    tx = remote.transaction()
    tx.account_set(acc)
    extend tx.tx_json, fields

    tx.on 'error', (err) ->
      assert !err, ("unexpected error #{ledger.pretty_json err}\n" +
                    "don't use this helper if expecting an error")

    submit_for_final tx, (m) ->
      assert.equal m.metadata?.transactionresult, 'tessuccess'
      affected_root  = m.metadata.affectednodes[0].modifiednode
      assert.equal alias_for(affected_root.finalfields.account), acc
      done(affected_root)

make_payment_factory = (remote, ledger) ->
  (src, dst, amount, path, on_final) ->

    if typeof path == 'function'
      on_final = path
      path = undefined

    src_account = uint160.json_rewrite src
    dst_account = uint160.json_rewrite dst
    dst_amount  = amount.from_json amount

    tx = remote.transaction().payment(src_account, dst_account, dst_amount)
    if not path?
      tx.build_path(true)
    else
      tx.path_add path.path
      tx.send_max path.send_max

    tx.on 'error', (err) ->
      if err.engine_result?.slice(0,3) == 'tec'
        # we can handle this in the `final`
        return
      assert !err, ("unexpected error #{ledger.pretty_json err}\n" +
                    "don't use this helper if expecting an error")

    submit_for_final tx, (m) ->
      on_final m

create_offer_factory = (remote, ledger) ->
  (acc, pays, gets, on_final) ->
    tx = remote.transaction().offer_create(acc, pays, gets)

    tx.on 'error', (err) ->
      if err.engine_result?.slice(0,3) == 'tec'
        # we can handle this in the `final`
        return
      assert !err, ("unexpected error #{ledger.pretty_json err}\n" +
                    "don't use this helper if expecting an error")
    submit_for_final tx, (m) ->
      on_final m

ledger_state_setup = (pre_ledger) ->
  post_setup = (context, done) ->
    context.ledger = new ledgerstate(pre_ledger,
                                     assert,
                                     context.remote,
                                     config)

    context.ledger.setup(
      #-> # noop logging function
      ->
      ->
        context.ledger.verifier().do_verify (errors) ->
          assert object.keys(errors).length == 0,
                 "pre_ledger errors:\n"+ pretty_json errors
          done()
    )

verify_ledger_state = (ledger, remote, pre_state, done) ->
  {config, assert, am} = ledger
  verifier = new ledgerverifier(pre_state, remote, config, assert, am)

  verifier.do_verify (errors) ->
    assert object.keys(errors).length == 0,
           "ledger_state errors:\n"+ pretty_json errors
    done()


book_offers_factory = (remote) ->
  (pays, gets, on_book) ->
    asset = (a) ->
      if typeof a == 'string'
        ret = {}
        [ret['currency'], ret['issuer']] = a.split('/')
        ret
      else
        a

    book=
      pays: asset(pays)
      gets: asset(gets)

    remote.request_book_offers book, (err, book) ->
      if err
        assert !err, "error with request_book_offers #{err}"

      on_book(book)

suite_setup = (state) ->
  '''
  
  @state
  
    the ledger state to setup, after starting the server
  
  '''
  opts =
    setup_func: suitesetup
    teardown_func: suiteteardown
    post_setup: ledger_state_setup(state)

  get_context = server_setup_teardown(opts)

  helpers      = null
  helpers_factory = ->
    context = {ledger, remote} = get_context()

    alog      = (obj) -> console.log ledger.pretty_json obj
    lines_for = (acc) -> get_lines(remote, arguments...)
    alias_for = (acc) -> ledger.am.lookup_alias(acc)

    verify_ledger_state_before_suite = (pre) ->
      suitesetup (done) -> verify_ledger_state(ledger, remote, pre, done)

    {
      context:      context
      remote:       remote
      ledger:       ledger
      lines_for:    lines_for
      alog:         alog
      alias_for:    alias_for
      book_offers:  book_offers_factory(remote)
      create_offer: create_offer_factory(remote, ledger, alias_for)
      account_set:  account_set_factory(remote, ledger, alias_for)
      make_payment: make_payment_factory(remote, ledger, alias_for)
      verify_ledger_state_before_suite: verify_ledger_state_before_suite
    }

  get_helpers = -> (helpers = helpers ? helpers_factory())

  {
    get_context: get_context
    get_helpers: get_helpers
  }

##################################### tests ####################################

execute_if_enabled = (fn) ->
  enforced = true # freeze tests enforced
  fn(global.suite, enforced)

conditional_test_factory = ->
  test = suite_test_bailer()
  test_if = (b, args...) ->
    if b
      test(args...)
    else
      test.skip(args...)
  [test, test_if]

execute_if_enabled (suite, enforced) ->
  suite 'freeze feature', ->
    suite 'ripplestate freeze', ->
      # test = suite_test_bailer()
      [test, test_if] = conditional_test_factory()
      h = null

      {get_helpers} = suite_setup
        accounts:
          g1: balance: ['1000.0']

          bob:
            balance: ['1000.0', '10-100/usd/g1']

          alice:
            balance: ['1000.0', '100/usd/g1']
            offers: [['500.0', '100/usd/g1']]

      suite 'account with line unfrozen (proving operations normally work)', ->
        test 'can make payment on that line',  (done) ->
          {remote} = h = get_helpers()

          h.make_payment 'alice', 'bob', '1/usd/g1', (m) ->
            assert.equal m.metadata?.transactionresult, 'tessuccess'
            done()

        test 'can receive payment on that line',  (done) ->
          h.make_payment 'bob', 'alice', '1/usd/g1', (m) ->
            assert.equal m.metadata?.transactionresult, 'tessuccess'
            done()

      suite 'is created via a trustset with setfreeze flag', ->
        test 'sets lowfreeze | highfreeze flags', (done) ->
          {remote} = h

          tx = remote.transaction()
          tx.ripple_line_set('g1', '0/usd/bob')
          tx.set_flags('setfreeze')

          submit_for_final tx, (m) ->
            assert.equal m.metadata?.transactionresult, 'tessuccess'
            affected = m.metadata.affectednodes
            assert affected.length > 1, "only one affected node implies a noop"
            ripple_state = affected[1].modifiednode
            final = ripple_state.finalfields
            assert.equal h.alias_for(final.lowlimit.issuer), 'g1'
            assert final.flags & flags.sle.ripplestate.lowfreeze
            assert !(final.flags & flags.sle.ripplestate.highfreeze)

            done()

      suite 'account with line frozen by issuer', ->
        test 'can buy more assets on that line', (done) ->
          h.create_offer 'bob', '5/usd/g1', '25.0', (m) ->
            meta = m.metadata
            assert.equal meta.transactionresult, 'tessuccess'
            line = meta.affectednodes[3]['modifiednode'].finalfields
            assert.equal h.alias_for(line.highlimit.issuer), 'bob'
            assert.equal line.balance.value, '-15' # highlimit means balance inverted
            done()

        test_if enforced, 'can not sell assets from that line', (done) ->
          h.create_offer 'bob', '1.0', '5/usd/g1', (m) ->
            assert.equal m.engine_result, 'tecunfunded_offer'
            done()

        test 'can receive payment on that line',  (done) ->
          h.make_payment 'alice', 'bob', '1/usd/g1', (m) ->
            assert.equal m.metadata?.transactionresult, 'tessuccess'
            done()

        test_if enforced, 'can not make payment from that line', (done) ->
          h.make_payment 'bob', 'alice', '1/usd/g1', (m) ->
            assert.equal m.engine_result, 'tecpath_dry'
            done()

      suite 'request_account_lines', ->
        test 'shows `freeze_peer` and `freeze` respectively', (done) ->
          async.parallel [
            (next) ->
                h.lines_for 'g1', (lines) ->
                  for line in lines.lines
                    if h.alias_for(line.account) == 'bob'
                      assert.equal line.freeze, true
                      assert.equal line.balance, '-16'
                      # unless we get here, the test will hang alerting us to
                      # something amiss
                      next() # setimmediate ;)
                      break

            (next) ->
              h.lines_for 'bob', (lines) ->
                for line in lines.lines
                  if h.alias_for(line.account) == 'g1'
                    assert.equal line.freeze_peer, true
                    assert.equal line.balance, '16'
                    next()
                    break
          ], ->
            done()

      suite 'is cleared via a trustset with clearfreeze flag', ->
        test 'sets lowfreeze | highfreeze flags', (done) ->
          {remote} = h

          tx = remote.transaction()
          tx.ripple_line_set('g1', '0/usd/bob')
          tx.set_flags('clearfreeze')

          submit_for_final tx, (m) ->
            assert.equal m.metadata?.transactionresult, 'tessuccess'
            affected = m.metadata.affectednodes
            ripple_state = affected[1].modifiednode
            final = ripple_state.finalfields
            assert.equal h.alias_for(final.lowlimit.issuer), 'g1'
            assert !(final.flags & flags.sle.ripplestate.lowfreeze)
            assert !(final.flags & flags.sle.ripplestate.highfreeze)

            done()

    suite 'global (accountroot) freeze', ->
      # nofreeze:        0x00200000
      # globalfreeze:    0x00400000

      # test = suite_test_bailer()
      [test, test_if] = conditional_test_factory()
      h = null

      {get_helpers} = suite_setup
        accounts:
          g1:
            balance: ['12000.0']
            offers: [['10000.0', '100/usd/g1'], ['100/usd/g1', '10000.0']]

          a1:
            balance: ['1000.0', '1000/usd/g1']
            offers: [['10000.0', '100/usd/g1']]
            trusts: ['1200/usd/g1']

          a2:
            balance: ['20000.0', '100/usd/g1']
            trusts: ['200/usd/g1']
            offers: [['100/usd/g1', '10000.0']]

          a3:
            balance: ['20000.0', '100/btc/g1']

          a4:
            balance: ['20000.0', '100/btc/g1']

      suite 'is toggled via accountset using setflag and clearflag', ->
        test 'setflag globalfreeze should set 0x00400000 in flags', (done) ->
          {remote} = h = get_helpers()

          h.account_set 'g1', setflag: globalfreeze, (root) ->
            new_flags  = root.finalfields.flags

            assert !(new_flags & flags.sle.accountroot.nofreeze)
            assert (new_flags & flags.sle.accountroot.globalfreeze)

            done()

        test 'clearflag globalfreeze should clear 0x00400000 in flags', (done) ->
          {remote} = h = get_helpers()

          h.account_set 'g1', clearflag: globalfreeze, (root) ->
            new_flags  = root.finalfields.flags

            assert !(new_flags & flags.sle.accountroot.nofreeze)
            assert !(new_flags & flags.sle.accountroot.globalfreeze)

            done()

      suite 'account without globalfreeze (proving operations normally work)', ->
        suite 'have visible offers', ->
          test 'where taker_gets is $unfrozen_issuer', (done) ->
            {remote} = h = get_helpers()

            h.book_offers 'xrp', 'usd/g1', (book) ->
              assert.equal book.offers.length, 2

              aliases = (h.alias_for(o.account) for o in book.offers).sort()

              assert.equal aliases[0], 'a1'
              assert.equal aliases[1], 'g1'

              done()

          test 'where taker_pays is $unfrozen_issuer', (done) ->
            h.book_offers 'usd/g1', 'xrp', (book) ->

              assert.equal book.offers.length, 2
              aliases = (h.alias_for(o.account) for o in book.offers).sort()

              assert.equal aliases[0], 'a2'
              assert.equal aliases[1], 'g1'

              done()

        suite 'it\'s assets can be', ->

          test 'bought on the market', (next) ->
            h.create_offer 'a3', '1/btc/g1', '1.0', (m) ->
              assert.equal m.metadata?.transactionresult, 'tessuccess'
              next()

          test 'sold on the market', (next) ->
            h.create_offer 'a4', '1.0', '1/btc/g1', (m) ->
              assert.equal m.metadata?.transactionresult, 'tessuccess'
              next()

        suite 'payments', ->
          test 'direct issues can be sent',  (done) ->
            {remote} = h = get_helpers()

            h.make_payment 'g1', 'a2', '1/usd/g1', (m) ->
              assert.equal m.metadata?.transactionresult, 'tessuccess'
              done()

          test 'direct redemptions can be sent',  (done) ->
            h.make_payment 'a2', 'g1', '1/usd/g1', (m) ->
              assert.equal m.metadata?.transactionresult, 'tessuccess'
              done()

          test 'via rippling can be sent',  (done) ->
            h.make_payment 'a2', 'a1', '1/usd/g1', (m) ->
              assert.equal m.metadata?.transactionresult, 'tessuccess'
              done()

          test 'via rippling can be sent back', (done) ->
            h.make_payment 'a2', 'a1', '1/usd/g1', (m) ->
              assert.equal m.metadata?.transactionresult, 'tessuccess'
              done()

      suite 'account with globalfreeze', ->
        suite 'needs to set globalfreeze first', ->
          test 'setflag globalfreeze will toggle back to freeze', (done) ->
            h.account_set 'g1', setflag: globalfreeze, (root) ->
              new_flags  = root.finalfields.flags

              assert !(new_flags & flags.sle.accountroot.nofreeze)
              assert (new_flags  & flags.sle.accountroot.globalfreeze)

              done()

        suite 'it\'s assets can\'t be', ->
          test_if enforced, 'bought on the market', (next) ->
            h.create_offer 'a3', '1/btc/g1', '1.0', (m) ->
              assert.equal m.engine_result, 'tecfrozen'
              next()

          test_if enforced, 'sold on the market', (next) ->
            h.create_offer 'a4', '1.0', '1/btc/g1', (m) ->
              assert.equal m.engine_result, 'tecfrozen'
              next()

        suite 'it\'s offers are filtered', ->
          test_if enforced, 'account_offers always '+
                            'shows their own offers', (done) ->
            {remote} = h = get_helpers()

            args = { account: 'g1', ledger: 'validated' }

            remote.request_account_offers args, (err, res) ->
              assert.equal res.offers.length, 2
              done()

          test.skip 'books_offers(*, $frozen_account/*) shows offers '+
               'owned by $frozen_account only', (done) ->

            h.book_offers 'xrp', 'usd/g1', (book) ->
              # h.alog book.offers
              assert.equal book.offers.length, 1
              done()

          test.skip 'books_offers($frozen_account/*, *) shows '+
                    'no offers', (done) ->

            h.book_offers 'usd/g1', 'xrp', (book) ->
              assert.equal book.offers.length, 0
              done()

          test_if enforced, 'books_offers(*, $frozen_account/*) shows offers '+
               'owned by $frozen_account only (broken) ', (done) ->

            h.book_offers 'xrp', 'usd/g1', (book) ->
              # h.alog book.offers
              assert.equal book.offers.length, 2
              done()

          test_if enforced, 'books_offers($frozen_account/*, *) '+
                            'shows no offers (broken)', (done) ->

            h.book_offers 'usd/g1', 'xrp', (book) ->
              assert.equal book.offers.length, 2
              done()

        suite 'payments', ->
          test 'direct issues can be sent',  (done) ->
            {remote} = h = get_helpers()

            h.make_payment 'g1', 'a2', '1/usd/g1', (m) ->
              assert.equal m.metadata?.transactionresult, 'tessuccess'
              done()

          test 'direct redemptions can be sent',  (done) ->
            h.make_payment 'a2', 'g1', '1/usd/g1', (m) ->
              assert.equal m.metadata?.transactionresult, 'tessuccess'
              done()

          test_if enforced, 'via rippling cant be sent',  (done) ->
            h.make_payment 'a2', 'a1', '1/usd/g1', (m) ->
              assert.equal m.engine_result, 'tecpath_dry'
              done()

    suite  'accounts with nofreeze', ->
      test = suite_test_bailer()
      h = null

      {get_helpers} = suite_setup
        accounts:
          g1:  balance: ['12000.0']
          a1:  balance: ['1000.0', '1000/usd/g1']

      suite 'trustset nofreeze', ->
        test 'should set 0x00200000 in flags', (done) ->
          h = get_helpers()

          h.account_set 'g1', setflag: nofreeze, (root) ->
            new_flags  = root.finalfields.flags

            assert (new_flags & flags.sle.accountroot.nofreeze)
            assert !(new_flags & flags.sle.accountroot.globalfreeze)

            done()

        test 'can not be cleared', (done) ->
          h.account_set 'g1', clearflag: nofreeze, (root) ->
            new_flags  = root.finalfields.flags

            assert (new_flags & flags.sle.accountroot.nofreeze)
            assert !(new_flags & flags.sle.accountroot.globalfreeze)

            done()

      suite 'globalfreeze', ->
        test 'can set globalfreeze', (done) ->
          h.account_set 'g1', setflag: globalfreeze, (root) ->
            new_flags  = root.finalfields.flags

            assert (new_flags & flags.sle.accountroot.nofreeze)
            assert (new_flags & flags.sle.accountroot.globalfreeze)

            done()

        test 'can not unset globalfreeze', (done) ->
          h.account_set 'g1', clearflag: globalfreeze, (root) ->
            new_flags  = root.finalfields.flags

            assert (new_flags & flags.sle.accountroot.nofreeze)
            assert (new_flags & flags.sle.accountroot.globalfreeze)

            done()

      suite 'their trustlines', ->
        test 'can\'t be frozen', (done) ->
          {remote} = h = get_helpers()

          tx = remote.transaction()
          tx.ripple_line_set('g1', '0/usd/a1')
          tx.set_flags('setfreeze')

          submit_for_final tx, (m) ->
            assert.equal m.metadata?.transactionresult, 'tessuccess'
            affected = m.metadata.affectednodes
            assert.equal affected.length, 1
            affected_type = affected[0]['modifiednode'].ledgerentrytype
            assert.equal affected_type, 'accountroot'

            done()

    suite 'offers for frozen trustlines (not globalfreeze)', ->
      # test = suite_test_bailer()
      [test, test_if] = conditional_test_factory()
      remote = h = null

      {get_helpers} = suite_setup
        accounts:
          g1:
            balance: ['1000.0']
          a2:
            balance: ['2000.0']
            trusts: ['1000/usd/g1']
          a3:
            balance: ['1000.0', '2000/usd/g1']
            offers: [['1000.0', '1000/usd/g1']]

          a4:
            balance: ['1000.0', '2000/usd/g1']

      suite 'will be removed by payment with tessuccess', ->
        test 'can normally make a payment partially consuming offer', (done) ->
          {remote} = h = get_helpers()

          path =
            path: [{"currency": "usd", "issuer": "g1"}]
            send_max: '1.0'

          h.make_payment 'a2', 'g1', '1/usd/g1', path, (m) ->
            assert.equal m.metadata?.transactionresult, 'tessuccess'
            done()

        test 'offer was only partially consumed', (done) ->
          args = { account: 'a3', ledger: 'validated' }

          remote.request_account_offers args, (err, res) ->
            assert res.offers.length == 1
            assert res.offers[0].taker_gets.value, '999'
            done()

        test 'someone else creates an offer providing liquidity', (done) ->
          h.create_offer 'a4', '999.0', '999/usd/g1', (m) ->
            assert.equal m.metadata?.transactionresult, 'tessuccess'
            affected = m.metadata.affectednodes
            done()

        test 'owner of partially consumed offer\'s line is frozen', (done) ->
          tx = remote.transaction()
          tx.ripple_line_set('g1', '0/usd/a3')
          tx.set_flags('setfreeze')

          submit_for_final tx, (m) ->
            assert.equal m.metadata?.transactionresult, 'tessuccess'
            affected = m.metadata.affectednodes
            ripple_state = affected[1].modifiednode
            final = ripple_state.finalfields
            assert.equal h.alias_for(final.highlimit.issuer), 'g1'
            assert !(final.flags & flags.sle.ripplestate.lowfreeze)
            assert (final.flags & flags.sle.ripplestate.highfreeze)

            done()

        test 'can make a payment via the new offer', (done) ->
          path =
            path: [{"currency": "usd", "issuer": "g1"}]
            send_max: '1.0'

          h.make_payment 'a2', 'g1', '1/usd/g1', path, (m) ->
            # assert.equal m.engine_result, 'tecpath_partial' # tecpath_dry
            assert.equal m.metadata.transactionresult, 'tessuccess' # tecpath_dry
            done()

        test_if enforced, 'partially consumed offer was removed by tes* payment', (done) ->
          args = { account: 'a3', ledger: 'validated' }

          remote.request_account_offers args, (err, res) ->
            assert res.offers.length == 0
            done()

      suite 'will be removed by offercreate with tessuccess', ->
        test_if enforced, 'freeze the new offer', (done) ->
          tx = remote.transaction()
          tx.ripple_line_set('g1', '0/usd/a4')
          tx.set_flags('setfreeze')

          submit_for_final tx, (m) ->
            assert.equal m.metadata?.transactionresult, 'tessuccess'
            affected = m.metadata.affectednodes
            ripple_state = affected[0].modifiednode
            final = ripple_state.finalfields
            assert.equal h.alias_for(final.lowlimit.issuer), 'g1'
            assert (final.flags & flags.sle.ripplestate.lowfreeze)
            assert !(final.flags & flags.sle.ripplestate.highfreeze)

            done()

        test_if enforced, 'can no longer create a crossing offer', (done) ->
          h.create_offer 'a2', '999/usd/g1', '999.0', (m) ->
            assert.equal m.metadata?.transactionresult, 'tessuccess'
            affected = m.metadata.affectednodes
            created = affected[5].creatednode
            new_fields = created.newfields
            assert.equal h.alias_for(new_fields.account), 'a2'
            done()

        test_if enforced, 'offer was removed by offer_create', (done) ->
          args = { account: 'a4', ledger: 'validated' }
          
          remote.request_account_offers args, (err, res) ->
            assert res.offers.length == 0
            done()