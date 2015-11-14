from __future__ import absolute_import, division, print_function, unicode_literals

from ripple.util import configfile

from unittest import testcase

class test_configfile(testcase):
    def test_trivial(self):
        self.assertequals(configfile.read(''), {})

    def test_full(self):
        self.assertequals(configfile.read(full.splitlines()), result)

result = {
    'websocket_port': '6206',
    'database_path': '/development/alpha/db',
    'sntp_servers':
        ['time.windows.com', 'time.apple.com', 'time.nist.gov', 'pool.ntp.org'],
    'validation_seed': 'sh1t8t9yguv7jb6dphqszdu2s5lcv',
    'node_size': 'medium',
    'rpc_startup': {
        'command': 'log_level',
        'severity': 'debug'},
    'ips': ['r.ripple.com', '51235'],
    'node_db': {
        'file_size_mult': '2',
        'file_size_mb': '8',
        'cache_mb': '256',
        'path': '/development/alpha/db/rocksdb',
        'open_files': '2000',
        'type': 'rocksdb',
        'filter_bits': '12'},
    'peer_port': '53235',
    'ledger_history': 'full',
    'rpc_ip': '127.0.0.1',
    'websocket_public_ip': '0.0.0.0',
    'rpc_allow_remote': '0',
    'validators':
         [['n949f75evchwgyp4fpvgahqnhxuvn15psjez3b3hnxpcpjczaoy7', 'rl1'],
          ['n9md5h24qrqqiybc8aeqqcwvpibiyq3jxsr91uidvmrkyhrdyluj', 'rl2'],
          ['n9l81uncapgtujfahh89gmdvxkamst5gdsw2g1ipwapkahw5nm4c', 'rl3'],
          ['n9kiym9cgnglvtrcqhzwgc2gjpdazcccbt3vboxinfckuwfvujzs', 'rl4'],
          ['n9ldgetkmgb9e2h3k4vp7iguakuq23zr32ehxiu8fwy7xoxbwtsa', 'rl5']],
    'debug_logfile': '/development/alpha/debug.log',
    'websocket_public_port': '5206',
    'peer_ip': '0.0.0.0',
    'rpc_port': '5205',
    'validation_quorum': '3',
    'websocket_ip': '127.0.0.1'}

full = """
[ledger_history]
full

# allow other peers to connect to this server.
#
[peer_ip]
0.0.0.0

[peer_port]
53235

# allow untrusted clients to connect to this server.
#
[websocket_public_ip]
0.0.0.0

[websocket_public_port]
5206

# provide trusted websocket admin access to the localhost.
#
[websocket_ip]
127.0.0.1

[websocket_port]
6206

# provide trusted json-rpc admin access to the localhost.
#
[rpc_ip]
127.0.0.1

[rpc_port]
5205

[rpc_allow_remote]
0

[node_size]
medium

# this is primary persistent datastore for rippled.  this includes transaction
# metadata, account states, and ledger headers.  helpful information can be
# found here: https://ripple.com/wiki/nodebackend
[node_db]
type=rocksdb
path=/development/alpha/db/rocksdb
open_files=2000
filter_bits=12
cache_mb=256
file_size_mb=8
file_size_mult=2

[database_path]
/development/alpha/db

# this needs to be an absolute directory reference, not a relative one.
# modify this value as required.
[debug_logfile]
/development/alpha/debug.log

[sntp_servers]
time.windows.com
time.apple.com
time.nist.gov
pool.ntp.org

# where to find some other servers speaking the ripple protocol.
#
[ips]
r.ripple.com 51235

# the latest validators can be obtained from
# https://ripple.com/ripple.txt
#
[validators]
n949f75evchwgyp4fpvgahqnhxuvn15psjez3b3hnxpcpjczaoy7	rl1
n9md5h24qrqqiybc8aeqqcwvpibiyq3jxsr91uidvmrkyhrdyluj	rl2
n9l81uncapgtujfahh89gmdvxkamst5gdsw2g1ipwapkahw5nm4c	rl3
n9kiym9cgnglvtrcqhzwgc2gjpdazcccbt3vboxinfckuwfvujzs	rl4
n9ldgetkmgb9e2h3k4vp7iguakuq23zr32ehxiu8fwy7xoxbwtsa	rl5

# ditto.
[validation_quorum]
3

[validation_seed]
sh1t8t9yguv7jb6dphqszdu2s5lcv

# turn down default logging to save disk space in the long run.
# valid values here are trace, debug, info, warning, error, and fatal
[rpc_startup]
{ "command": "log_level", "severity": "debug" }

# configure ssl for websockets.  not enabled by default because not everybody
# has an ssl cert on their server, but if you uncomment the following lines and
# set the path to the ssl certificate and private key the websockets protocol
# will be protected by ssl/tls.
#[websocket_secure]
#1

#[websocket_ssl_cert]
#/etc/ssl/certs/server.crt

#[websocket_ssl_key]
#/etc/ssl/private/server.key

# defaults to 0 ("no") so that you can use self-signed ssl certificates for
# development, or internally.
#[ssl_verify]
#0
""".strip()
