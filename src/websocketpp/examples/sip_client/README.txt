

checkout the project from git

at the top level, run cmake:

  cmake -g 'unix makefiles' \
     -d build_examples=on \
     -d websocketpp_root=/tmp/cm1 \
     -d enable_cpp11=off .

and then make the example:

  make -c examples/sip_client

now run it:

  bin/sip_client ws://ws-server:80

it has been tested against the repro sip proxy from resiprocate

  http://www.resiprocate.org/webrtc_and_sip_over_websockets
