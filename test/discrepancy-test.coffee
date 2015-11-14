################################### requires ###################################

assert                     = require 'assert'
async                      = require 'async'

{
  amount
  meta
}                          = require 'ripple-lib'

testutils                  = require './testutils'

{
  pretty_json
  server_setup_teardown
}                          = require './batmans-belt'

#################################### config ####################################

config = testutils.init_config()

#################################### helpers ###################################

tx_blob_submit_factory = (remote, txn) ->
  (next) ->
    req = remote.request_submit()
    req.message.tx_blob = txn
    req.once 'error', -> assert false, "unexpected error submitting txns"
    req.once 'success', (m) ->
      remote.once 'ledger_closed', -> next()
      remote.ledger_accept()
    req.request()

offer_amounts = (fields) ->
  [amount.from_json(fields.takerpays),
   amount.from_json(fields.takergets)]

executed_offers = (meta) ->
  offers = {}

  meta.nodes.foreach (n, i) ->
    if n.entrytype == 'offer'
      [prev_pays, prev_gets] = offer_amounts(n.fieldsprev)
      [final_pays, final_gets] = offer_amounts(n.fields)

      summary=
        pays_executed: prev_pays.subtract(final_pays).to_text_full()
        gets_executed: prev_gets.subtract(final_gets).to_text_full()
        pays_final: final_pays.to_text_full()
        gets_final: final_gets.to_text_full()
        owner: n.fields.account

      offers[n.ledgerindex] = summary
  offers

build_tx_blob_submission_series = (remote, txns_to_submit) ->
  series = []
  while (txn = txns_to_submit.shift())
    series.push tx_blob_submit_factory remote, txn
  series

compute_xrp_discrepancy = (fee, meta) ->
  before = amount.from_json(0)
  after = amount.from_json(fee)

  meta.nodes.foreach (n, i) ->
    if n.entrytype == 'accountroot'
      prev = n.fieldsprev?.balance || n.fields.balance
      final  = n.fieldsfinal?.balance || n.fields.balance

      before = before.add(amount.from_json(prev))
      after = after.add(amount.from_json(final))

  before.subtract(after)

suite 'discrepancy test', ->
  suite 'xrp discrepancy', ->
    get_context = server_setup_teardown({server_opts: {ledger_file: 'ledger-6226713.json'}})
    test '01030e435c2eebe2689fed7494bc159a4c9b98d0df0b23f7dfce223822237e8c', (done) ->
      {remote} = get_context()
      txns_to_submit = [
        # this is the nasty one ;)
        '1200002200020000240000124e61d5438d7ea4c680000000000000000000000000004a50590000000000e5c92828261dbaac933b6309c6f5c72af020afd468400000000000000a69d4d3e7809b4814c8000000000000000000000000434e59000000000041c8be2c0a6aa17471b9f6d0af92aab1c94d5a25732103fc5f96ea61889691ec7a56fb2b859b600de68c0255bf580d5c22d02eb97afce474473045022100d14b60bc6e01e5c19471f87eb00a4bfca16d039bb91aee12da1142e8c4cae7c2022020e2809cf24de2bc0c3dcf1a07c469db415f880485b2b323e5b5aa1d9f6f22d48114afd96601692a6c6416dba294f0da684675a824b28314afd96601692a6c6416dba294f0da684675a824b20112300000000000000000000000004a50590000000000e5c92828261dbaac933b6309c6f5c72af020afd401e5c92828261dbaac933b6309c6f5c72af020afd4ff100000000000000000000000000000000000000000300000000000000000000000004a50590000000000e5c92828261dbaac933b6309c6f5c72af020afd401e5c92828261dbaac933b6309c6f5c72af020afd4ff01a034782e2dbac4fb82b601cd50421e8ef24f3a00100000000000000000000000000000000000000000300000000000000000000000004a50590000000000e5c92828261dbaac933b6309c6f5c72af020afd401e5c92828261dbaac933b6309c6f5c72af020afd400'
      ]
      series = build_tx_blob_submission_series remote, txns_to_submit
      async.series series, ->
        hash = '01030e435c2eebe2689fed7494bc159a4c9b98d0df0b23f7dfce223822237e8c'
        remote.request_tx hash, (e, m) ->
          meta = new meta(m.meta)
          zero = amount.from_json(0)
          discrepancy = compute_xrp_discrepancy(m.fee, meta)
          assert discrepancy.equals(zero), discrepancy.to_text_full()
          done()

  suite 'ripd 304', ->
    get_context = server_setup_teardown({server_opts: {ledger_file: 'ledger-7145315.json'}})
    test 'b1a305038d43bcdf3ea1d096e6a0acc5fb0ecae0c8f5d3a54ad76a2aa1e20ec4', (done) ->
      {remote} = get_context()

      txns_to_submit = [
        '120008228000000024000030fd2019000030f0201b006d076b68400000000000000f732103325eb29a014dde22289d0ea989861d481d54d54c727578ab6c2f18bc342d3829744630440220668d06c44144c284e0346ee7785eb41b72edbb244fe6ee02f317011a07023c63022067a52367ac01941a3fe19477d7f588c862704a44a8a771bcad6b7a9119b71e9e8114a7c1c74dadb3693c199888a901fc2b7fd0884ee1'
        '1200002200020000240000163161d551c37937e080000000000000000000000000004a50590000000000e5c92828261dbaac933b6309c6f5c72af020afd468400000000000000a69d4d0bec6a319514d00000000000000000000000055534400000000000a20b3c85f482532a9578dbb3950b85ca06594d173210342d49ade3dbc5e8e25f02b4fbb169448157b016ba203a268c3e8ccc9df1ae39f74463044022069a2b0f79a042cc65c7ccfdf610dead8fda12f53e43061f9f75fad5b398e657a02200a4a45bb4f31e922a52f843d5ce96d83446992a13393871c31fcd8a52ae4329f81148c4be4dbaa81f7bc66720e5874ebd2d90c9563ea83148c4be4dbaa81f7bc66720e5874ebd2d90c9563ea0112100000000000000000000000000000000000000000300000000000000000000000004a50590000000000e5c92828261dbaac933b6309c6f5c72af020afd401e5c92828261dbaac933b6309c6f5c72af020afd4ff300000000000000000000000004a50590000000000e5c92828261dbaac933b6309c6f5c72af020afd401e5c92828261dbaac933b6309c6f5c72af020afd4ff01585e1f3bd02a15d6185f8bb9b57cc60deddb37c101dd39c650a96eda48334e70cc4a85b8b2e8502cd3300000000000000000000000004a50590000000000e5c92828261dbaac933b6309c6f5c72af020afd401e5c92828261dbaac933b6309c6f5c72af020afd400'
        '1200082200000000240019b1a520190019b1a46840000000000000327321025718736160fa6632f48ea4354a35ab0340f8d7dc7083799b9c57c3e937d7185174463044022048b3669edca795a1897da3c7328c8526940708dbb3ffad88ca5dc22d0398a67502206b37796a743105de05ee1a11be017404b4f41fa17e6449e390c36f69d8907c078114affdcc86d33c153da185156eb32608accf0bc713'
        '1200072200000000240019b1a664d550af2d90a009d80000000000000000000000004a50590000000000e5c92828261dbaac933b6309c6f5c72af020afd46540000002540be4006840000000000000327321025718736160fa6632f48ea4354a35ab0340f8d7dc7083799b9c57c3e937d71851744630440220509f09b609573bc8addd55449dbd5201a40f6c1c3aa2d5d984acb54e0f651f2e022019e6af2937a5e76d8c9a2b5b0c4704d6be637aac17f2ee135da449b0892b728b8114affdcc86d33c153da185156eb32608accf0bc713'
        # this is the nasty one ;)
        '1200002200020000240000163261d551c37937e080000000000000000000000000004a50590000000000e5c92828261dbaac933b6309c6f5c72af020afd468400000000000000a69d4d0bec6a319514d00000000000000000000000055534400000000000a20b3c85f482532a9578dbb3950b85ca06594d173210342d49ade3dbc5e8e25f02b4fbb169448157b016ba203a268c3e8ccc9df1ae39f74473045022100c79c86bd18bbbc0343f0eb268a7770fdaec30748eccb9a6483e2b11488749dc00220544a5ff2d085fa5dd2a003aa9c3f031b8fe3fd4a443b659b9ee84e165795bc0581148c4be4dbaa81f7bc66720e5874ebd2d90c9563ea83148c4be4dbaa81f7bc66720e5874ebd2d90c9563ea0112100000000000000000000000000000000000000000300000000000000000000000004a50590000000000e5c92828261dbaac933b6309c6f5c72af020afd401e5c92828261dbaac933b6309c6f5c72af020afd4ff300000000000000000000000004a50590000000000e5c92828261dbaac933b6309c6f5c72af020afd401e5c92828261dbaac933b6309c6f5c72af020afd4ff01585e1f3bd02a15d6185f8bb9b57cc60deddb37c101dd39c650a96eda48334e70cc4a85b8b2e8502cd3300000000000000000000000004a50590000000000e5c92828261dbaac933b6309c6f5c72af020afd401e5c92828261dbaac933b6309c6f5c72af020afd400'
      ]

      series = build_tx_blob_submission_series remote, txns_to_submit

      async.series series, ->
        hash = 'b1a305038d43bcdf3ea1d096e6a0acc5fb0ecae0c8f5d3a54ad76a2aa1e20ec4'
        remote.request_tx hash, (e, m) ->
          meta = new meta(m.meta)

          expected = {
            "003313896da56cfa0996b36af066589ef0e689230e67da01d13320289c834a93": {
              "pays_executed": "955.967853/xrp",
              "gets_executed": "445.6722130686/jpy/rmaz5znk73nynul4foavaxdreczckg3va6",
              "pays_final": "245,418.978522/xrp",
              "gets_final": "114414.3277869564/jpy/rmaz5znk73nynul4foavaxdreczckg3va6",
              "owner": "rqlevsnbgpcuxktu8vvszupx4scukcjuzb"

            },
            "9847793d6b936812907ed58455fba4195205abcacbe28df9584c3a195a221e59": {
              "pays_executed": "4.19284145965/usd/rvyafwj5gh67ov6fw32zzp3aw4eubs59b",
              "gets_executed": "955.967853/xrp",
              "pays_final": "13630.84998220238/usd/rvyafwj5gh67ov6fw32zzp3aw4eubs59b",
              "gets_final": "3,107,833.795934/xrp",
              "owner": "rehkzcz5ndjm9bzzmmkrtvhxpnswbyssdv"
            }
          }
          ## rhsxr2aaddyckx5izctebt4padxv6iwdxb
          assert.deepequal executed_offers(meta), expected
          done()