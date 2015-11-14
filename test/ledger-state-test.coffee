################################################################################

async                      = require 'async'
simple_assert              = require 'assert'
deep_eq                    = require 'deep-equal'
testutils                  = require './testutils'

{
  ledgerverifier
  balance
}                          = require './ledger-state'

#################################### config ####################################

config = testutils.init_config()

#################################### helpers ###################################

assert = simple_assert
prettyj = pretty_json =  (v) -> json.stringify(v, undefined, 2)

describe 'balance', ->
  it 'parses native balances', ->
    bal = new balance("1.000")
    assert.equal bal.is_native, true
    assert.equal bal.limit, null

  it 'parses iou balances', ->
    bal = new balance("1.000/usd/bob")
    assert.equal bal.is_native, false
    assert.equal bal.limit, null
    assert.equal bal.amount.currency().to_json(), 'usd'

  it 'parses iou balances with limits', ->
    bal = new balance("1-500/usd/bob")
    assert.equal bal.is_native, false
    assert.equal bal.amount.currency().to_json(), 'usd'
    assert.equal bal.limit.to_json().value, '500'
    assert.equal bal.amount.to_json().value, '1'

describe 'ledgerverifier', ->
  lv = null

  declaration=
    accounts:
      bob:
        balance: ['100.0', '200-500/usd/alice']
        offers: [['89.0', '100/usd/alice'], ['89.0', '100/usd/alice']]

  # we are using this because mocha and coffee-script is a retarded combination
  # unfortunately, which terminates the program silently upon any require time
  # exceptions. todo: investigate obviously, but for the moment this is an
  # acceptable workaround.
  suitesetup ->
    remote_dummy = {set_secret: (->)}
    lv = new ledgerverifier(declaration, remote_dummy, config, assert)

  it 'tracks xrp balances', ->
    assert.equal lv.xrp_balances['bob'].to_json(), '100000000'

  it 'tracks iou balances', ->
    assert.equal lv.iou_balances['bob']['usd/alice'].to_json().value, '200'

  it 'tracks iou trust limits', ->
    assert.equal lv.trusts['bob']['usd/alice'].to_json().value, '500'

  it 'can verify', ->
    account_offers = [
      {
          "account": "bob",
          "offers": [
            {
              "flags": 65536,
              "seq": 2,
              "taker_gets": {
                "currency": "usd",
                "issuer": "alice",
                "value": "100"
              },
              "taker_pays": "88000000"
            }
          ]
        }
    ]

    account_lines = [{
      "account": "bob",
      "lines": [
        {
          "account": "alice",
          "balance": "201",
          "currency": "usd",
          "limit": "500",
          "limit_peer": "0",
          "quality_in": 0,
          "quality_out": 0
        },
      ]
    }]

    account_infos = [{
      "account_data": {
        "account": "bob",
        "balance": "999"+ "999"+ "970",
        "flags": 0,
        "ledgerentrytype": "accountroot",
        "ownercount": 0,
        "previoustxnid": "3d7823b577a5af5860273b3dd13ca82d072b63b3b095de1784604a5b41d7dd1d",
        "previoustxnlgrseq": 5,
        "sequence": 3,
        "index": "59bea57d1a27b6a560eca226abd10de80c3adc6961039908087acdfa92f71489"
      },
      "ledger_current_index": 8
    }]

    errors = lv.verify account_infos, account_lines, account_offers

    assert.equal errors.bob.balance['usd/alice'].expected, '200'
    assert.equal errors.bob.balance['usd/alice'].actual, '201'

    assert.equal errors.bob.balance['xrp'].expected, '100'
    assert.equal errors.bob.balance['xrp'].actual, '999.99997'

    assert.equal errors.bob.offers[0].taker_pays.actual, '88/xrp'
    assert.equal errors.bob.offers[0].taker_pays.expected, '89/xrp'

    # {"expected":["89.0","100/usd/alice"],"actual":"missing"}
    assert.equal errors.bob.offers[1].actual, 'missing'

    expected = {
      "bob": {
        "balance": {
          "xrp": {
            "actual": "999.99997",
            "expected": "100"
          },
          "usd/alice": {
            "actual": "201",
            "expected": "200"
          }
        },
        "offers": [
          {
            "taker_pays": {
              "expected": "89/xrp",
              "actual": "88/xrp"
            }
          },
          "missing"
        ]
      }
    }
