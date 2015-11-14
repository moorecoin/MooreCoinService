# rippled

# use the ubuntu base image
from ubuntu
maintainer roberto catini roberto.catini@gmail.com

# make sure the package repository is up to date
run apt-get update
run apt-get -y upgrade

# install the dependencies
run apt-get -y install git scons pkg-config protobuf-compiler libprotobuf-dev libssl-dev libboost1.55-all-dev

# download source code from official repository
run git clone https://github.com/ripple/rippled.git src; cd src/; git checkout master

# compile
run cd src/; scons build/rippled

# move to root directory and strip
run cp src/build/rippled rippled; strip rippled

# copy default config
run cp src/doc/rippled-example.cfg rippled.cfg

# clean source
run rm -r src

# launch rippled when launching the container
entrypoint ./rippled
