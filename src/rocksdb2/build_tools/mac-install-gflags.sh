#!/bin/sh
# install gflags for mac developers.

set -e

dir=`mktemp -d /tmp/rocksdb_gflags_xxxx`

cd $dir
wget https://gflags.googlecode.com/files/gflags-2.0.tar.gz
tar xvfz gflags-2.0.tar.gz
cd gflags-2.0

./configure
make
make install

# add include/lib path for g++
echo 'export library_path+=":/usr/local/lib"' >> ~/.bash_profile
echo 'export cpath+=":/usr/local/include"' >> ~/.bash_profile

echo ""
echo "-----------------------------------------------------------------------------"
echo "|                         installation completed                            |"
echo "-----------------------------------------------------------------------------"
echo "please run \`. ~/.bash_profile\` to be able to compile with gflags"
