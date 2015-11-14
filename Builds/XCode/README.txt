place xcode project file here!

1) to build in you xcode, you need to: 

mkdir -p build/proto; 
protoc -isrc/ripple/proto --cpp_out=build/proto src/ripple/proto/ripple.proto

2) to run you vpal by default configuration, you need to copy your vpal.cfg to:
~/.config/ripple/

3) to run vapl by default, you need to mkdir:
mkdir -p ~/vpal/db
